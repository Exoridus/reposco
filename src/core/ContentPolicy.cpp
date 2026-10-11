#include "ContentPolicy.h"

namespace reposco {
bool acceptsText(std::string_view text, bool excluded) {
    return !excluded && !text.empty() && text.size() <= kMaxTextBytes &&
           text.find_first_not_of(" \t\r\n\v\f") != std::string_view::npos;
}
} // namespace reposco
