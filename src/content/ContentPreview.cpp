#include "ContentPreview.h"
#include <QBuffer>
#include <QPainter>
#include <QTextCursor>
#include <QTextTable>
#include <QUrl>
#include <cmath>

namespace reposco {
namespace {
QString imageUrl(const QImage& image) {
    if (image.isNull()) {
        return {};
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
        return {};
    }
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64());
}
QVariantList tableRows(QTextFrame* frame) {
    for (auto* child : frame->childFrames()) {
        if (auto* table = qobject_cast<QTextTable*>(child)) {
            QVariantList result;
            for (int row = 0; row < qMin(table->rows(), 12); ++row) {
                QStringList cells;
                for (int column = 0; column < qMin(table->columns(), 6); ++column) {
                    const auto cell = table->cellAt(row, column);
                    auto cursor = cell.firstCursorPosition();
                    cursor.setPosition(cell.lastCursorPosition().position(),
                                       QTextCursor::KeepAnchor);
                    cells.append(cursor.selectedText().left(128));
                }
                result.append(cells);
            }
            return result;
        }
        const auto nested = tableRows(child);
        if (!nested.isEmpty()) {
            return nested;
        }
    }
    return {};
}
QString renderDocument(OfflineDocument& document, bool dark, bool& truncated) {
    document.setTextWidth(580);
    document.setDefaultStyleSheet(dark ? "body {color:#e5e9f1;} a {color:#a7c9ff;}"
                                       : "body {color:#202c3c;} a {color:#245a9b;}");
    const auto height = document.size().height();
    if (!std::isfinite(height) || height <= 0) {
        return {};
    }
    truncated = truncated || height > 2048;
    QImage image(580, qMin(2048, int(qMin(height + 1, qreal(2048)))),
                 QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QTextCursor cursor(&document);
    cursor.select(QTextCursor::Document);
    QTextCharFormat contrast;
    contrast.setForeground(dark ? QColor("#e5e9f1") : QColor("#202c3c"));
    cursor.mergeCharFormat(contrast);
    QPainter painter(&image);
    document.drawContents(&painter, QRectF(0, 0, image.width(), image.height()));
    return imageUrl(image);
}
void initializeDocument(OfflineDocument& document, const QString& source, bool html, bool dark) {
    document.setDocumentMargin(0);
    document.setDefaultFont(QFont(QStringLiteral("Segoe UI"), 10));
    document.setDefaultStyleSheet(dark ? "body {color:#e5e9f1;} a {color:#a7c9ff;}"
                                       : "body {color:#202c3c;} a {color:#245a9b;}");
    if (html) {
        document.setHtml(source);
    } else {
        document.setMarkdown(source,
                             QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub) |
                                 QTextDocument::MarkdownNoHTML);
    }
}
} // namespace
QVariant OfflineDocument::loadResource(int, const QUrl&) {
    return QByteArray{};
}
bool looksLikeMarkdown(const QString& text) {
    const auto bounded = text.first(qMin(qsizetype(8192), text.size()));
    return bounded.startsWith("# ") || bounded.startsWith("## ") || bounded.contains("\n# ") ||
           bounded.contains("```") || bounded.contains("**") || bounded.startsWith("- ") ||
           (bounded.contains("|") && bounded.contains("---"));
}
ContentPreview makeContentPreview(const ClipboardContent& content) {
    ContentPreview result;
    result.text = content.text.left(8192);
    result.truncated = content.text.size() > 8192;
    if (!content.png.isEmpty() || !content.dib.isEmpty() || !content.dibV5.isEmpty()) {
        result.kind = QStringLiteral("Image");
        const auto image = clipboardImage(content);
        result.darkImage =
            imageUrl(image.scaled(QSize(960, 960), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        result.lightImage = result.darkImage;
        if (result.text.isEmpty() && !image.isNull()) {
            result.text = QString("Image %1 x %2").arg(image.width()).arg(image.height());
        }
        return result;
    }
    const bool html = !content.html.isEmpty();
    const bool markdown = looksLikeMarkdown(content.text);
    if (html || markdown) {
        const auto fragment = html ? htmlFragment(content.html) : std::optional(content.text);
        if (!fragment) {
            return result;
        }
        const auto source = fragment->left(32768);
        result.truncated = result.truncated || fragment->size() > source.size();
        OfflineDocument dark;
        initializeDocument(dark, source, html, true);
        result.kind = html ? QStringLiteral("HTML") : QStringLiteral("Markdown");
        if (result.text.isEmpty()) {
            result.text = dark.toPlainText().left(8192);
        }
        result.rows = tableRows(dark.rootFrame());
        if (!result.rows.isEmpty()) {
            result.kind = QStringLiteral("Table");
        } else {
            result.darkImage = renderDocument(dark, true, result.truncated);
            OfflineDocument light;
            initializeDocument(light, source, html, false);
            result.lightImage = renderDocument(light, false, result.truncated);
        }
        return result;
    }
    if (content.text.contains('\t')) {
        const auto lines = result.text.split('\n');
        if (lines.size() > 1) {
            const auto columns = lines.front().split('\t').size();
            if (columns >= 2 && columns <= 6) {
                for (const auto& line : lines) {
                    if (line.isEmpty()) {
                        continue;
                    }
                    auto cells = line.split('\t');
                    if (cells.size() != columns) {
                        result.rows.clear();
                        break;
                    }
                    for (auto& cell : cells) {
                        cell = cell.left(128);
                    }
                    result.rows.append(cells);
                    if (result.rows.size() == 12) {
                        result.truncated = lines.size() > 12;
                        break;
                    }
                }
                if (!result.rows.isEmpty()) {
                    result.kind = QStringLiteral("Table");
                }
            }
        }
    }
    if (result.kind == "Text" && !content.rtf.isEmpty()) {
        result.kind = QStringLiteral("RTF");
    }
    return result;
}
} // namespace reposco
