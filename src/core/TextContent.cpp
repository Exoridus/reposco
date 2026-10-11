#include "TextContent.h"
#include "ContentPolicy.h"
#include <QCryptographicHash>

namespace reposco {
std::optional<TextContent> normalizeText(const QString& text) {
    if (text.size() > static_cast<qsizetype>(kMaxTextBytes) || !text.isValidUtf16() ||
        text.contains(QChar('\0'))) {
        return std::nullopt;
    }
    const auto original = text.toUtf8();
    if (!acceptsText(
            std::string_view(original.constData(), static_cast<std::size_t>(original.size())),
            false)) {
        return std::nullopt;
    }
    auto canonicalText = text;
    canonicalText.replace("\r\n", "\n").replace('\r', '\n');
    const auto canonical =
        canonicalText.normalized(QString::NormalizationForm_C).trimmed().toUtf8();
    if (canonical.isEmpty() || canonical.size() > static_cast<qsizetype>(kMaxTextBytes)) {
        return std::nullopt;
    }
    QByteArray identity("text:v1\0", 8);
    identity += canonical;
    return TextContent{original, canonical,
                       QCryptographicHash::hash(identity, QCryptographicHash::Sha256)};
}
} // namespace reposco
