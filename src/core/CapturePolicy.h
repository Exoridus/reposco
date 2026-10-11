#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace reposco {
enum class TextCaptureDecision : std::uint8_t {
    Accepted,
    Excluded,
    Unsupported,
    Invalid,
    Oversize
};

class ClipboardTextSource {
  public:
    virtual ~ClipboardTextSource() = default;
    [[nodiscard]] virtual bool hasExclusionMarker() const = 0;
    [[nodiscard]] virtual bool hasHistoryPermission() const = 0;
    [[nodiscard]] virtual std::optional<std::uint32_t> historyPermission() = 0;
    // Return at most the native text budget, without a trailing terminator.
    [[nodiscard]] virtual std::optional<std::u16string> readText() = 0;
};

struct TextCapture {
    TextCaptureDecision decision = TextCaptureDecision::Unsupported;
    std::u16string text;
};

[[nodiscard]] TextCapture captureAllowedText(ClipboardTextSource& source);
} // namespace reposco
