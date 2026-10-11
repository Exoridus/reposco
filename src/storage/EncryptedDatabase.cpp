#include "EncryptedDatabase.h"

#include <sqlite3.h>

#include <filesystem>
#include <limits>

namespace reposco {
void EncryptedDatabase::Close::operator()(sqlite3* database) const {
    sqlite3_close(database);
}
EncryptedDatabase::EncryptedDatabase(const std::string& path, std::span<const unsigned char> key) {
    if (key.empty() || key.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("A nonempty bounded database key is required");
    }
    const std::filesystem::path file{std::u8string(path.begin(), path.end())};
    // SQLite treats an existing empty file as a new database; history must fail closed instead.
    if (std::filesystem::exists(file) &&
        (!std::filesystem::is_regular_file(file) || std::filesystem::file_size(file) < 4096)) {
        throw std::runtime_error("Existing encrypted database is truncated or unavailable");
    }
    sqlite3* raw = nullptr;
    const int opened =
        sqlite3_open_v2(path.c_str(), &raw,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, nullptr);
    database_.reset(raw);
    if (opened != SQLITE_OK ||
        sqlite3_key(database_.get(), key.data(), static_cast<int>(key.size())) != SQLITE_OK) {
        throw std::runtime_error("Encrypted database could not be opened");
    }
    if (!cipherVersion().starts_with("4.19.0") || cipherProvider() != "openssl") {
        throw std::runtime_error("The approved SQLCipher provider is unavailable");
    }
    // Force an authenticated page read before any configuration can write to an existing file.
    (void)scalar("SELECT count(*) FROM sqlite_master");
    execute("PRAGMA temp_store=MEMORY");
    execute("PRAGMA secure_delete=ON");
    execute("PRAGMA cipher_memory_security=ON");
    execute("PRAGMA synchronous=FULL");
    execute("PRAGMA foreign_keys=ON");
    if (scalar("PRAGMA journal_mode=DELETE") != "delete") {
        throw std::runtime_error("The approved database journal mode is unavailable");
    }
}
EncryptedDatabase::~EncryptedDatabase() = default;
void EncryptedDatabase::execute(const std::string& sql) {
    if (sqlite3_exec(database_.get(), sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Encrypted database statement failed");
    }
}
std::string EncryptedDatabase::scalar(const std::string& sql) const {
    sqlite3_stmt* raw = nullptr;
    const int prepared = sqlite3_prepare_v2(database_.get(), sql.c_str(), -1, &raw, nullptr);
    const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw,
                                                                               sqlite3_finalize);
    if (prepared != SQLITE_OK || !statement) {
        throw std::runtime_error("Encrypted database query could not be prepared");
    }
    const int stepped = sqlite3_step(statement.get());
    if (stepped == SQLITE_DONE) {
        return {};
    }
    if (stepped != SQLITE_ROW) {
        throw std::runtime_error("Encrypted database query failed");
    }
    const auto* text = sqlite3_column_text(statement.get(), 0);
    return text ? std::string(reinterpret_cast<const char*>(text),
                              static_cast<std::size_t>(sqlite3_column_bytes(statement.get(), 0)))
                : std::string{};
}
std::string EncryptedDatabase::cipherVersion() const {
    return scalar("PRAGMA cipher_version");
}
std::string EncryptedDatabase::cipherProvider() const {
    return scalar("PRAGMA cipher_provider");
}
} // namespace reposco
