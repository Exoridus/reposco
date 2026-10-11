#pragma once

#include "ClipboardContent.h"
#include <QTextDocument>
#include <QVariantList>

namespace reposco {
// All resource types, including stylesheet and local/UNC image references, stay offline.
class OfflineDocument final : public QTextDocument {
  protected:
    QVariant loadResource(int, const QUrl&) override;
};
struct ContentPreview {
    QString text;
    QString kind = QStringLiteral("Text");
    QString darkImage;
    QString lightImage;
    QVariantList rows;
    bool truncated = false;
};
[[nodiscard]] bool looksLikeMarkdown(const QString& text);
[[nodiscard]] ContentPreview makeContentPreview(const ClipboardContent& content);
} // namespace reposco
Q_DECLARE_METATYPE(reposco::ContentPreview)
