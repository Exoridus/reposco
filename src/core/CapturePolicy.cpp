#include "CapturePolicy.h"

#include "ContentPolicy.h"

namespace reposco {
TextCapture captureAllowedText(ClipboardTextSource& source) {
    if (source.hasExclusionMarker() ||
        (source.hasHistoryPermission() && source.historyPermission() != 1U)) {
        return {TextCaptureDecision::Excluded, {}};
    }
    auto text = source.readText();
    if (!text) {
        return {TextCaptureDecision::Unsupported, {}};
    }
    std::size_t utf8Bytes = 0;
    for (std::size_t index = 0; index < text->size(); ++index) {
        const auto character = static_cast<std::uint32_t>((*text)[index]);
        if (character >= 0xD800 && character <= 0xDBFF) {
            if (++index >= text->size() || (*text)[index] < 0xDC00 || (*text)[index] > 0xDFFF) {
                return {TextCaptureDecision::Invalid, {}};
            }
            utf8Bytes += 4;
        } else if (character == 0 || (character >= 0xDC00 && character <= 0xDFFF)) {
            return {TextCaptureDecision::Invalid, {}};
        } else {
            utf8Bytes += character <= 0x7F ? 1 : (character <= 0x7FF ? 2 : 3);
        }
        if (utf8Bytes > kMaxTextBytes) {
            return {TextCaptureDecision::Oversize, {}};
        }
    }
    return {TextCaptureDecision::Accepted, std::move(*text)};
}
} // namespace reposco
