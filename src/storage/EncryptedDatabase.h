#pragma once

#include <memory>
#include <span>
#include <stdexcept>
#include <string>

struct sqlite3;

namespace reposco {
// Synchronous connection. Own and use it exclusively on a storage worker, never the GUI thread.
// A key is required. Opening an unreadable existing database fails without replacing it.
class EncryptedDatabase final {
  public:
    EncryptedDatabase(const std::string& path, std::span<const unsigned char> key);
    ~EncryptedDatabase();
    EncryptedDatabase(const EncryptedDatabase&) = delete;
    EncryptedDatabase& operator=(const EncryptedDatabase&) = delete;
    void execute(const std::string& sql);
    [[nodiscard]] std::string scalar(const std::string& sql) const;
    [[nodiscard]] std::string cipherVersion() const;
    [[nodiscard]] std::string cipherProvider() const;

  private:
    struct Close {
        void operator()(sqlite3* database) const;
    };
    std::unique_ptr<sqlite3, Close> database_;
};
} // namespace reposco
