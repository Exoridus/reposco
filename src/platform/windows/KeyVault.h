#pragma once

#include <QString>
#include <array>
#include <memory>
#include <span>

namespace reposco {
class DatabaseKey final {
  public:
    DatabaseKey() = default;
    ~DatabaseKey();
    DatabaseKey(const DatabaseKey&) = delete;
    DatabaseKey& operator=(const DatabaseKey&) = delete;
    [[nodiscard]] std::span<const unsigned char> bytes() const;

  private:
    friend std::unique_ptr<DatabaseKey> loadDatabaseKey(const QString&, bool);
    std::array<unsigned char, 32> bytes_{};
};
// Missing or malformed envelopes beside existing history fail without replacing either file.
[[nodiscard]] std::unique_ptr<DatabaseKey> loadDatabaseKey(const QString& directory,
                                                           bool historyExists);
} // namespace reposco
