#pragma once

#include <cstddef>
#include <string_view>

namespace reposco {
inline constexpr std::size_t kMaxTextBytes = 16 * 1024 * 1024;
[[nodiscard]] bool acceptsText(std::string_view text, bool excluded);
} // namespace reposco
