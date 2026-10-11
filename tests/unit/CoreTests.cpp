#include "CapturePolicy.h"
#include "ContentPolicy.h"

#include <iostream>
#include <string>

namespace {
class SyntheticTextSource final : public reposco::ClipboardTextSource {
  public:
    bool excluded = false;
    bool permissionPresent = false;
    std::optional<std::uint32_t> permission = 1;
    std::optional<std::u16string> text = u"synthetic";
    int reads = 0;
    bool hasExclusionMarker() const override {
        return excluded;
    }
    bool hasHistoryPermission() const override {
        return permissionPresent;
    }
    std::optional<std::uint32_t> historyPermission() override {
        return permission;
    }
    std::optional<std::u16string> readText() override {
        ++reads;
        return text;
    }
};
} // namespace

int main() {
    const auto require = [](bool condition, const char* name) {
        if (!condition) {
            std::cerr << name << '\n';
        }
        return condition;
    };
    bool passed = require(reposco::acceptsText("Synthetic text", false), "accept plain text");
    passed &= require(!reposco::acceptsText("synthetic secret", true), "reject excluded text");
    passed &= require(!reposco::acceptsText("", false), "reject empty text");
    passed &= require(!reposco::acceptsText(" \t\r\n", false), "reject ASCII whitespace");
    passed &= require(reposco::acceptsText("\xC3\xA9", false), "preserve UTF-8 bytes");
    passed &= require(reposco::acceptsText(std::string(reposco::kMaxTextBytes, 'a'), false),
                      "accept exact budget");
    passed &= require(!reposco::acceptsText(std::string(reposco::kMaxTextBytes + 1, 'a'), false),
                      "reject oversized text");
    SyntheticTextSource source;
    source.excluded = true;
    passed &= require(reposco::captureAllowedText(source).decision ==
                              reposco::TextCaptureDecision::Excluded &&
                          source.reads == 0,
                      "exclude before reading text");
    source.excluded = false;
    source.permissionPresent = true;
    source.permission = 0;
    passed &= require(reposco::captureAllowedText(source).decision ==
                              reposco::TextCaptureDecision::Excluded &&
                          source.reads == 0,
                      "history opt-out before reading text");
    source.permission.reset();
    passed &= require(reposco::captureAllowedText(source).decision ==
                              reposco::TextCaptureDecision::Excluded &&
                          source.reads == 0,
                      "malformed history marker fails closed");
    source.permission = 1;
    passed &= require(reposco::captureAllowedText(source).text == u"synthetic",
                      "explicit history permission accepted");
    source.permissionPresent = false;
    passed &= require(reposco::captureAllowedText(source).decision ==
                          reposco::TextCaptureDecision::Accepted,
                      "cloud opt-out alone does not exclude local history");
    source.text = std::u16string(1, static_cast<char16_t>(0xD800));
    passed &= require(reposco::captureAllowedText(source).decision ==
                          reposco::TextCaptureDecision::Invalid,
                      "reject unpaired Unicode surrogate");
    source.text = std::u16string(reposco::kMaxTextBytes + 1, u'a');
    passed &= require(reposco::captureAllowedText(source).decision ==
                          reposco::TextCaptureDecision::Oversize,
                      "reject oversized native text");
    source.text.reset();
    passed &= require(reposco::captureAllowedText(source).decision ==
                          reposco::TextCaptureDecision::Unsupported,
                      "missing Unicode payload ignored");
    return passed ? 0 : 1;
}
