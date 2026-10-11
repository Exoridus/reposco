#pragma once

#include <QByteArray>
#include <QString>
#include <optional>

namespace reposco {
struct TextContent {
    QByteArray original;
    QByteArray canonical;
    QByteArray fingerprint;
};
[[nodiscard]] std::optional<TextContent> normalizeText(const QString& text,
                                                       bool excludeSixDigitCodes = true,
                                                       bool excludeEightDigitCodes = false);
} // namespace reposco
