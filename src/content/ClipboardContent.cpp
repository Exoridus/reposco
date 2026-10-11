#include "ClipboardContent.h"
#include "ContentPreview.h"
#include "core/ContentPolicy.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QImageReader>
#include <QStringDecoder>
#include <QtEndian>
#include <array>
#include <charconv>

namespace reposco {
namespace {
constexpr qint64 kMaxImagePixels = 16 * 1024 * 1024;
QByteArray boundedPng(const QByteArray& bytes) {
    if (!bytes.startsWith(QByteArray("\x89PNG\r\n\x1a\n", 8))) {
        return {};
    }
    qsizetype offset = 8;
    for (int chunks = 0; chunks < 4096 && bytes.size() - offset >= 12; ++chunks) {
        const auto length = qFromBigEndian<quint32>(bytes.constData() + offset);
        if (length > quint32(bytes.size() - offset - 12)) {
            return {};
        }
        const bool end = bytes.mid(offset + 4, 4) == "IEND";
        offset += qsizetype(length) + 12;
        if (end) {
            return length == 0 ? bytes.first(offset) : QByteArray{};
        }
    }
    return {};
}
bool boundedAttributes(QStringView tag) {
    qsizetype offset = 0;
    while (offset < tag.size() && tag[offset].isLetterOrNumber()) {
        ++offset;
    }
    bool rowSpan = false;
    bool columnSpan = false;
    while (offset < tag.size()) {
        if (tag[offset] == '/' && offset == tag.size() - 1) {
            return true;
        }
        if (!tag[offset].isSpace()) {
            return false;
        }
        while (offset < tag.size() && tag[offset].isSpace()) {
            ++offset;
        }
        if (offset == tag.size()) {
            return true;
        }
        if (tag[offset] == '/' && offset == tag.size() - 1) {
            return true;
        }
        const auto start = offset;
        while (offset < tag.size() && (tag[offset].isLetterOrNumber() || tag[offset] == '-' ||
                                       tag[offset] == '_' || tag[offset] == ':')) {
            ++offset;
        }
        if (offset == start) {
            return false;
        }
        const auto key = tag.sliced(start, offset - start).toString().toLower();
        const auto nameEnd = offset;
        while (offset < tag.size() && tag[offset].isSpace()) {
            ++offset;
        }
        if (offset == tag.size() || tag[offset] != '=') {
            if (key == "rowspan" || key == "colspan" || key == "color" || key == "bgcolor" ||
                key == "bordercolor") {
                return false;
            }
            offset = nameEnd;
            continue;
        }
        ++offset;
        while (offset < tag.size() && tag[offset].isSpace()) {
            ++offset;
        }
        if (offset == tag.size()) {
            return false;
        }
        QChar quote;
        if (tag[offset] == '\'' || tag[offset] == '"') {
            quote = tag[offset++];
        }
        const auto valueStart = offset;
        while (offset < tag.size() &&
               (quote.isNull() ? !tag[offset].isSpace() : tag[offset] != quote)) {
            ++offset;
        }
        const auto value = tag.sliced(valueStart, offset - valueStart).toString();
        if (!quote.isNull()) {
            if (offset == tag.size()) {
                return false;
            }
            ++offset;
        }
        if (key == "rowspan" || key == "colspan") {
            auto& seen = key == "rowspan" ? rowSpan : columnSpan;
            if (seen || value.size() != 1 || value[0] < '1' || value[0] > '6') {
                return false;
            }
            seen = true;
        }
        // Qt includes invalid color values in warnings, so reject before importing HTML.
        if ((key == "color" || key == "bgcolor" || key == "bordercolor") &&
            !QColor::fromString(value).isValid()) {
            return false;
        }
    }
    return true;
}
bool boundedHtml(const QString& html) {
    int tags = 0;
    int depth = 0;
    qsizetype offset = 0;
    while ((offset = html.indexOf('<', offset)) >= 0) {
        qsizetype end = offset + 1;
        QChar quote;
        for (; end < html.size() && end - offset <= 4096; ++end) {
            const auto character = html[end];
            if (!quote.isNull()) {
                if (character == quote) {
                    quote = QChar{};
                }
            } else if (character == '\'' || character == '"') {
                quote = character;
            } else if (character == '>') {
                break;
            }
        }
        if (end == html.size() || !quote.isNull()) {
            return false;
        }
        if (end < 0 || end - offset > 4096 || ++tags > 512) {
            return false;
        }
        const auto tag = QStringView(html).sliced(offset + 1, end - offset - 1).trimmed();
        if (tag.isEmpty() || tag.front() == '!' || tag.front() == '?' || tag.contains('\\')) {
            return false;
        }
        {
            if (tag.front() == '/') {
                const auto closing = tag.sliced(1).trimmed();
                if (closing.isEmpty()) {
                    return false;
                }
                for (const auto character : closing) {
                    if (!character.isLetterOrNumber() && character != '-' && character != ':') {
                        return false;
                    }
                }
                depth = qMax(0, depth - 1);
            } else {
                if (!boundedAttributes(tag)) {
                    return false;
                }
                qsizetype nameEnd = 0;
                while (nameEnd < tag.size() && tag[nameEnd].isLetterOrNumber()) {
                    ++nameEnd;
                }
                const auto name = tag.first(nameEnd).toString().toLower();
                const bool empty = tag.endsWith('/') || name == "br" || name == "hr" ||
                                   name == "img" || name == "input" || name == "meta" ||
                                   name == "link" || name == "wbr" || name == "source";
                if (!empty && ++depth > 32) {
                    return false;
                }
            }
        }
        offset = end + 1;
    }
    return true;
}
bool boundedImage(const QSize& size) {
    return size.width() > 0 && size.height() > 0 && size.width() <= 8192 && size.height() <= 8192 &&
           qint64(size.width()) * size.height() <= kMaxImagePixels;
}
QImage decodeImage(QByteArray bytes, const QByteArray& format) {
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::ReadOnly)) {
        return {};
    }
    QImageReader reader(&buffer, format);
    reader.setAutoDetectImageFormat(false);
    return boundedImage(reader.size()) ? reader.read() : QImage{};
}
QByteArray bitmapFile(const QByteArray& dib) {
    if (dib.size() < 40 || dib.size() > kMaxClipboardBytes) {
        return {};
    }
    const auto* data = reinterpret_cast<const uchar*>(dib.constData());
    const auto header = qFromLittleEndian<quint32>(data);
    if (header != 40 && header != 108 && header != 124) {
        return {};
    }
    if (dib.size() < header) {
        return {};
    }
    const auto width = qFromLittleEndian<qint32>(data + 4);
    const auto height = qFromLittleEndian<qint32>(data + 8);
    const auto bits = qFromLittleEndian<quint16>(data + 14);
    const auto compression = qFromLittleEndian<quint32>(data + 16);
    if (qFromLittleEndian<quint16>(data + 12) != 1 || height == INT_MIN ||
        !boundedImage(QSize(width, qAbs(height))) || (bits != 16 && bits != 24 && bits != 32) ||
        (compression != 0 && compression != 3) || (compression == 3 && bits == 24) ||
        qFromLittleEndian<quint32>(data + 32) != 0) {
        return {};
    }
    if (header == 124 && (qFromLittleEndian<quint32>(data + 112) != 0 ||
                          qFromLittleEndian<quint32>(data + 116) != 0)) {
        return {};
    }
    const quint64 offset = header + (header == 40 && compression == 3 ? 12 : 0);
    const quint64 stride = (quint64(width) * bits + 31) / 32 * 4;
    const auto required = offset + stride * quint64(qAbs(height));
    if (required > quint64(dib.size()) || required > quint64(kMaxClipboardBytes)) {
        return {};
    }
    QByteArray file(14, '\0');
    file[0] = 'B';
    file[1] = 'M';
    qToLittleEndian<quint32>(quint32(required + 14), file.data() + 2);
    qToLittleEndian<quint32>(quint32(offset + 14), file.data() + 10);
    file += dib.first(qsizetype(required));
    return file;
}
std::array<const QByteArray*, 5> representations(const ClipboardContent& content) {
    return {&content.html, &content.rtf, &content.png, &content.dib, &content.dibV5};
}
void appendPart(QByteArray& result, quint8 type, const QByteArray& bytes) {
    result.append(char(type));
    const auto start = result.size();
    result.resize(start + 4);
    qToLittleEndian<quint32>(quint32(bytes.size()), result.data() + start);
    result.append(bytes);
}
QByteArray encodeFormats(const ClipboardContent& content) {
    QByteArray result("RCF1");
    quint8 type = 0;
    for (const auto* bytes : representations(content)) {
        ++type;
        if (!bytes->isEmpty()) {
            appendPart(result, type, *bytes);
        }
    }
    return result.size() == 4 ? QByteArray{} : result;
}
bool validRtf(const QByteArray& bytes) {
    if (!bytes.startsWith("{\\rtf") || bytes.size() > kMaxMarkupBytes || bytes.contains('\0')) {
        return false;
    }
    int depth = 0;
    for (qsizetype i = 0; i < bytes.size(); ++i) {
        const char c = bytes[i];
        if (c == '\\') {
            if (++i >= bytes.size()) {
                return false;
            }
            if (bytes[i] == '{' || bytes[i] == '}' || bytes[i] == '\\') {
                continue;
            }
            const auto start = i;
            while (i < bytes.size() && bytes[i] >= 'a' && bytes[i] <= 'z') {
                ++i;
            }
            const auto word = bytes.mid(start, i - start);
            if (word == "bin" || word == "object" || word == "objdata" || word == "filetbl" ||
                word == "field") {
                return false;
            }
            --i;
        } else if (c == '{') {
            if (++depth > 64) {
                return false;
            }
        } else if (c == '}' && --depth < 0) {
            return false;
        }
    }
    return depth == 0 && bytes.trimmed().endsWith('}');
}
} // namespace
qsizetype ClipboardContent::payloadBytes() const {
    qsizetype total = text.toUtf8().size();
    for (const auto* bytes : representations(*this)) {
        total += bytes->size();
    }
    return total;
}
std::optional<QString> htmlFragment(const QByteArray& html) {
    if (html.isEmpty() || html.size() > kMaxMarkupBytes || html.contains('\0')) {
        return std::nullopt;
    }
    const auto header = html.first(qMin(qsizetype(4096), html.size()));
    std::array<qint64, 4> values{};
    constexpr std::array<const char*, 4> keys = {
        "StartHTML:", "EndHTML:", "StartFragment:", "EndFragment:"};
    qsizetype headerEnd = 0;
    for (std::size_t n = 0; n < keys.size(); ++n) {
        const QByteArray key(keys[n]);
        const auto position = header.indexOf(key);
        if (position < 0 || (position > 0 && header[position - 1] != '\n') ||
            header.indexOf(key, position + key.size()) >= 0) {
            return std::nullopt;
        }
        const auto end = header.indexOf('\n', position);
        if (end < 0) {
            return std::nullopt;
        }
        const auto number =
            header.mid(position + key.size(), end - position - key.size()).trimmed();
        const auto parsed =
            std::from_chars(number.constData(), number.constData() + number.size(), values[n]);
        if (parsed.ec != std::errc{} || parsed.ptr != number.constData() + number.size()) {
            return std::nullopt;
        }
        headerEnd = qMax(headerEnd, end + 1);
    }
    const auto [start, end, fragmentStart, fragmentEnd] = values;
    if (fragmentStart < headerEnd || fragmentEnd < fragmentStart || fragmentEnd > html.size() ||
        !((start == -1 && end == -1) || (start >= headerEnd && start <= fragmentStart &&
                                         end >= fragmentEnd && end <= html.size()))) {
        return std::nullopt;
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString result = decoder(html.sliced(fragmentStart, fragmentEnd - fragmentStart));
    return decoder.hasError() || !boundedHtml(result) ? std::nullopt : std::optional(result);
}
QImage clipboardImage(const ClipboardContent& content) {
    if (!content.png.isEmpty()) {
        if (content.png.size() > kMaxClipboardBytes ||
            !content.png.startsWith(QByteArray("\x89PNG\r\n\x1a\n", 8))) {
            return {};
        }
        return decodeImage(content.png, "PNG");
    }
    const auto file = bitmapFile(content.dibV5.isEmpty() ? content.dib : content.dibV5);
    return file.isEmpty() ? QImage{} : decodeImage(file, "BMP");
}
std::optional<ClipboardContent> decodeFormats(const QByteArray& formats,
                                              const QByteArray& originalText) {
    if (formats.size() > kMaxClipboardBytes + 29 ||
        originalText.size() > qsizetype(kMaxTextBytes)) {
        return std::nullopt;
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    ClipboardContent content;
    content.text = decoder(originalText);
    if (decoder.hasError()) {
        return std::nullopt;
    }
    if (formats.isEmpty()) {
        return content;
    }
    if (!formats.startsWith("RCF1")) {
        return std::nullopt;
    }
    const std::array<QByteArray*, 5> fields = {&content.html, &content.rtf, &content.png,
                                               &content.dib, &content.dibV5};
    qsizetype offset = 4;
    quint8 previous = 0;
    while (offset < formats.size()) {
        if (formats.size() - offset < 5) {
            return std::nullopt;
        }
        const auto type = quint8(formats[offset++]);
        const auto length = qFromLittleEndian<quint32>(formats.constData() + offset);
        offset += 4;
        if (type <= previous || type > fields.size() || length == 0 ||
            length > quint32(kMaxClipboardBytes) || qsizetype(length) > formats.size() - offset) {
            return std::nullopt;
        }
        *fields[type - 1] = formats.sliced(offset, length);
        offset += length;
        previous = type;
    }
    return content.payloadBytes() <= kMaxClipboardBytes ? std::optional(content) : std::nullopt;
}
std::optional<NormalizedClipboard> normalizeClipboard(const ClipboardContent& content,
                                                      bool excludeSixDigitCodes,
                                                      bool excludeEightDigitCodes) {
    if (content.text.size() > qsizetype(kMaxTextBytes) || content.payloadBytes() <= 0 ||
        content.payloadBytes() > kMaxClipboardBytes) {
        return std::nullopt;
    }
    auto normalized = normalizeText(content.text, excludeSixDigitCodes, excludeEightDigitCodes);
    if (!content.text.trimmed().isEmpty() && !normalized) {
        return std::nullopt;
    }
    if (!content.text.isValidUtf16() || content.text.contains(QChar::Null)) {
        return std::nullopt;
    }
    QByteArray identity("clipboard:v1\0", 13);
    QString kind =
        looksLikeMarkdown(content.text) ? QStringLiteral("Markdown") : QStringLiteral("Text");
    if (!content.html.isEmpty()) {
        const auto fragment = htmlFragment(content.html);
        if (!fragment) {
            return std::nullopt;
        }
        OfflineDocument document;
        document.setHtml(*fragment);
        const auto derived = document.toPlainText();
        const auto admitted = normalizeText(derived, excludeSixDigitCodes, excludeEightDigitCodes);
        if (!derived.trimmed().isEmpty() && !admitted) {
            return std::nullopt;
        }
        if (!normalized && admitted) {
            normalized = admitted;
            normalized->original = content.text.toUtf8();
        }
        kind = QStringLiteral("HTML");
        appendPart(identity, 1, fragment->toUtf8());
        if (fragment->contains("<table", Qt::CaseInsensitive)) {
            kind = QStringLiteral("Table");
        }
    }
    if (!content.rtf.isEmpty()) {
        if (!validRtf(content.rtf)) {
            return std::nullopt;
        }
        appendPart(identity, 2, content.rtf);
        if (kind == "Text") {
            kind = QStringLiteral("RTF");
        }
        if (!normalized) {
            return std::nullopt;
        }
    }
    if (!content.png.isEmpty() || !content.dib.isEmpty() || !content.dibV5.isEmpty()) {
        const auto image = clipboardImage(content).convertToFormat(QImage::Format_RGBA8888);
        if (image.isNull()) {
            return std::nullopt;
        }
        for (const auto* bytes : {&content.dib, &content.dibV5}) {
            if (!bytes->isEmpty() && bitmapFile(*bytes).isEmpty()) {
                return std::nullopt;
            }
        }
        kind = QStringLiteral("Image");
        if (!normalized) {
            normalized =
                normalizeText(QString("Image %1 x %2").arg(image.width()).arg(image.height()));
            normalized->original = content.text.toUtf8();
        }
        appendPart(identity, 3,
                   QByteArray::number(image.width()) + ':' + QByteArray::number(image.height()));
        QCryptographicHash pixels(QCryptographicHash::Sha256);
        for (int row = 0; row < image.height(); ++row) {
            pixels.addData(QByteArrayView(reinterpret_cast<const char*>(image.constScanLine(row)),
                                          image.width() * 4));
        }
        appendPart(identity, 4, pixels.result());
    }
    if (!normalized) {
        return std::nullopt;
    }
    auto stored = content;
    if (!stored.png.isEmpty()) {
        stored.png = boundedPng(stored.png);
        if (stored.png.isEmpty()) {
            return std::nullopt;
        }
    }
    for (auto* bytes : {&stored.dib, &stored.dibV5}) {
        if (!bytes->isEmpty()) {
            *bytes = bitmapFile(*bytes).sliced(14);
        }
    }
    const auto formats = encodeFormats(stored);
    if (!formats.isEmpty()) {
        appendPart(identity, 0, normalized->canonical);
        normalized->fingerprint = QCryptographicHash::hash(identity, QCryptographicHash::Sha256);
    }
    return NormalizedClipboard{*normalized, formats, kind, stored.payloadBytes()};
}
} // namespace reposco
