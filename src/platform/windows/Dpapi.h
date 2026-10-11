#pragma once

#include <QByteArray>
#include <optional>

namespace reposco {
// User-scope protection. Empty inputs and native failures return no value.
[[nodiscard]] std::optional<QByteArray> protectKey(const QByteArray& key);
[[nodiscard]] std::optional<QByteArray> unprotectKey(const QByteArray& envelope);
} // namespace reposco
