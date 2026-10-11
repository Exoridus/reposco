#pragma once

#include "core/TextContent.h"
#include <QImage>
#include <QMetaType>

namespace reposco {
inline constexpr qsizetype kMaxClipboardBytes = 32 * 1024 * 1024;
inline constexpr qsizetype kMaxMarkupBytes = 1024 * 1024;

struct ClipboardContent {
    QString text{};
    QByteArray html{};
    QByteArray rtf{};
    QByteArray png{};
    QByteArray dib{};
    QByteArray dibV5{};
    [[nodiscard]] qsizetype payloadBytes() const;
};
struct NormalizedClipboard {
    TextContent text;
    QByteArray formats;
    QString kind;
    qsizetype payloadBytes = 0;
};

// Rejects malformed/oversize representations and exact codes before constructing an identity.
[[nodiscard]] std::optional<NormalizedClipboard>
normalizeClipboard(const ClipboardContent& content, bool excludeSixDigitCodes = true,
                   bool excludeEightDigitCodes = false);
[[nodiscard]] std::optional<ClipboardContent> decodeFormats(const QByteArray& formats,
                                                            const QByteArray& originalText);
[[nodiscard]] std::optional<QString> htmlFragment(const QByteArray& html);
[[nodiscard]] QImage clipboardImage(const ClipboardContent& content);
} // namespace reposco
Q_DECLARE_METATYPE(reposco::ClipboardContent)
