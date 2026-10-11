#include "ContentPolicy.h"

#include <iostream>
#include <string>

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
    return passed ? 0 : 1;
}
