#include "HistoryStore.h"

#include "core/ContentPolicy.h"
#include <limits>
#include <sqlite3.h>

namespace reposco {
namespace {
class Statement final {
  public:
    Statement(sqlite3* database, const char* sql) {
        sqlite3_stmt* raw = nullptr;
        const int status = sqlite3_prepare_v2(database, sql, -1, &raw, nullptr);
        statement_.reset(raw);
        if (status != SQLITE_OK || !statement_) {
            throw std::runtime_error("History statement could not be prepared");
        }
    }
    void bind(int index, const QByteArray& value, bool text = false) {
        const int status =
            text ? sqlite3_bind_text(statement_.get(), index, value.constData(),
                                     static_cast<int>(value.size()), SQLITE_TRANSIENT)
                 : sqlite3_bind_blob(statement_.get(), index, value.constData(),
                                     static_cast<int>(value.size()), SQLITE_TRANSIENT);
        if (status != SQLITE_OK) {
            throw std::runtime_error("History value could not be bound");
        }
    }
    void bind(int index, std::int64_t value) {
        if (sqlite3_bind_int64(statement_.get(), index, value) != SQLITE_OK) {
            throw std::runtime_error("History value could not be bound");
        }
    }
    [[nodiscard]] bool next() {
        const int status = sqlite3_step(statement_.get());
        if (status != SQLITE_ROW && status != SQLITE_DONE) {
            throw std::runtime_error("History statement failed");
        }
        return status == SQLITE_ROW;
    }
    [[nodiscard]] std::int64_t integer(int column) const {
        return sqlite3_column_int64(statement_.get(), column);
    }
    [[nodiscard]] QByteArray bytes(int column) const {
        const auto* value = static_cast<const char*>(sqlite3_column_blob(statement_.get(), column));
        return QByteArray(value, sqlite3_column_bytes(statement_.get(), column));
    }

  private:
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement_{nullptr,
                                                                          sqlite3_finalize};
};

class Transaction final {
  public:
    explicit Transaction(EncryptedDatabase& database) : database_(database) {
        database_.execute("BEGIN IMMEDIATE");
    }
    ~Transaction() {
        if (!committed_) {
            try {
                database_.execute("ROLLBACK");
            } catch (const std::exception&) {
                // The worker locks history after a failed operation; never replace its files.
            }
        }
    }
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    void commit() {
        database_.execute("COMMIT");
        committed_ = true;
    }

  private:
    EncryptedDatabase& database_;
    bool committed_ = false;
};

void evict(sqlite3* database, const HistoryLimits& limits, std::int64_t now) {
    Statement expired(database, "DELETE FROM history WHERE last_copied_ms < ?1");
    expired.bind(1, now - limits.retentionMs);
    (void)expired.next();
    while (true) {
        Statement budget(database, "SELECT count(*), coalesce(sum(payload_bytes),0) FROM history");
        if (!budget.next()) {
            throw std::runtime_error("History budget could not be checked");
        }
        if (budget.integer(0) <= limits.maxItems && budget.integer(1) <= limits.maxBytes) {
            return;
        }
        Statement oldest(database, "DELETE FROM history WHERE id=(SELECT id FROM history "
                                   "ORDER BY ordinal ASC LIMIT 1)");
        (void)oldest.next();
    }
}
} // namespace

HistoryStore::HistoryStore(const std::string& path, std::span<const unsigned char> key,
                           const HistoryLimits& limits)
    : database_(path, key), limits_(limits) {
    if (limits.maxItems <= 0 || limits.maxItems > 1000 || limits.maxBytes <= 0 ||
        limits.maxBytes > 1024LL * 1024 * 1024 || limits.retentionMs <= 0) {
        throw std::invalid_argument("History limits exceed the supported safety caps");
    }
    const auto version = database_.scalar("PRAGMA user_version");
    if (version == "0" && database_.scalar("SELECT count(*) FROM sqlite_master") == "0") {
        Transaction transaction(database_);
        database_.execute("CREATE TABLE schema_info(version INTEGER NOT NULL, "
                          "canonicalization_version INTEGER NOT NULL)");
        database_.execute("INSERT INTO schema_info VALUES(1,1)");
        database_.execute(
            "CREATE TABLE history(id INTEGER PRIMARY KEY AUTOINCREMENT, fingerprint BLOB NOT NULL "
            "UNIQUE, canonical TEXT NOT NULL, original BLOB NOT NULL, "
            "first_seen_ms INTEGER NOT NULL, last_copied_ms INTEGER NOT NULL, "
            "ordinal INTEGER NOT NULL UNIQUE, payload_bytes INTEGER NOT NULL)");
        database_.execute("CREATE INDEX history_retention ON history(last_copied_ms)");
        database_.execute("PRAGMA user_version=1");
        transaction.commit();
    } else if (version != "1" ||
               database_.scalar(
                   "SELECT version || ':' || canonicalization_version FROM schema_info") != "1:1") {
        throw std::runtime_error("History schema is unsupported; original files were preserved");
    }
}
void HistoryStore::record(const TextContent& text, std::int64_t copiedAt) {
    if (text.original.isEmpty() || text.original.size() > static_cast<qsizetype>(kMaxTextBytes) ||
        text.canonical.isEmpty() || text.canonical.size() > static_cast<qsizetype>(kMaxTextBytes) ||
        text.fingerprint.size() != 32 || copiedAt < 0) {
        throw std::invalid_argument("History text does not meet the bounded content contract");
    }
    Transaction transaction(database_);
    auto* database = database_.database_.get();
    {
        Statement existing(database, "SELECT canonical FROM history WHERE fingerprint=?1");
        existing.bind(1, text.fingerprint);
        if (existing.next() && existing.bytes(0) != text.canonical) {
            throw std::runtime_error("History comparison identity is ambiguous");
        }
    }
    std::int64_t ordinal = 0;
    {
        Statement newest(database, "SELECT coalesce(max(ordinal),0) FROM history");
        if (!newest.next() || newest.integer(0) == std::numeric_limits<std::int64_t>::max()) {
            throw std::runtime_error("History sequence could not be advanced");
        }
        ordinal = newest.integer(0) + 1;
    }
    {
        Statement write(database,
                        "INSERT INTO history(fingerprint,canonical,original,first_seen_ms,"
                        "last_copied_ms,ordinal,payload_bytes) VALUES(?1,?2,?3,?4,?4,?5,?6) "
                        "ON CONFLICT(fingerprint) DO UPDATE SET original=excluded.original,"
                        "last_copied_ms=excluded.last_copied_ms,ordinal=excluded.ordinal,"
                        "payload_bytes=excluded.payload_bytes");
        write.bind(1, text.fingerprint);
        write.bind(2, text.canonical, true);
        write.bind(3, text.original);
        write.bind(4, copiedAt);
        write.bind(5, ordinal);
        write.bind(6, static_cast<std::int64_t>(text.original.size()));
        (void)write.next();
    }
    evict(database, limits_, copiedAt);
    transaction.commit();
}
void HistoryStore::prune(std::int64_t now) {
    if (now < 0) {
        throw std::invalid_argument("History retention timestamp must be nonnegative");
    }
    Transaction transaction(database_);
    evict(database_.database_.get(), limits_, now);
    transaction.commit();
}
void HistoryStore::setRetentionDays(int days, std::int64_t now) {
    if (days < 1 || days > 90 || now < 0) {
        throw std::invalid_argument("Retention must be between 1 and 90 days");
    }
    limits_.retentionMs = static_cast<std::int64_t>(days) * 86400000;
    prune(now);
}
void HistoryStore::clear() {
    Transaction transaction(database_);
    database_.execute("DELETE FROM history");
    transaction.commit();
    database_.execute("PRAGMA shrink_memory");
}
std::vector<HistorySummary> HistoryStore::summaries(const QString& search) const {
    Statement query(database_.database_.get(),
                    "SELECT id,substr(canonical,1,200),last_copied_ms,payload_bytes "
                    "FROM history WHERE instr(lower(canonical),lower(?1))>0 "
                    "ORDER BY ordinal DESC LIMIT 100");
    query.bind(1, search.left(256).toUtf8(), true);
    std::vector<HistorySummary> result;
    while (query.next()) {
        result.push_back({query.integer(0), QString::fromUtf8(query.bytes(1)), query.integer(2),
                          query.integer(3)});
    }
    return result;
}
QString HistoryStore::preview(std::int64_t id) const {
    Statement query(database_.database_.get(),
                    "SELECT substr(CAST(original AS TEXT),1,8192) FROM history WHERE id=?1");
    query.bind(1, id);
    return query.next() ? QString::fromUtf8(query.bytes(0)) : QString{};
}
QByteArray HistoryStore::original(std::int64_t id) const {
    Statement query(database_.database_.get(), "SELECT original FROM history WHERE id=?1");
    query.bind(1, id);
    return query.next() ? query.bytes(0) : QByteArray{};
}
} // namespace reposco
