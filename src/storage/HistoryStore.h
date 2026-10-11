#pragma once

#include "EncryptedDatabase.h"
#include "core/TextContent.h"

#include <cstdint>
#include <vector>

namespace reposco {
struct HistorySummary {
    std::int64_t id = 0;
    QString excerpt;
    std::int64_t copiedAt = 0;
    std::int64_t payloadBytes = 0;
};
struct HistoryLimits {
    std::int64_t maxItems = 1000;
    std::int64_t maxBytes = 1024LL * 1024 * 1024;
    std::int64_t retentionMs = 90LL * 24 * 60 * 60 * 1000;
};
class HistoryStore final {
  public:
    HistoryStore(const std::string& path, std::span<const unsigned char> key,
                 const HistoryLimits& limits = {});
    void record(const TextContent& text, std::int64_t copiedAt);
    void prune(std::int64_t now);
    void clear();
    void setRetentionDays(int days, std::int64_t now);
    [[nodiscard]] std::vector<HistorySummary> summaries(const QString& search = {}) const;
    [[nodiscard]] QString preview(std::int64_t id) const;
    [[nodiscard]] QByteArray original(std::int64_t id) const;

  private:
    EncryptedDatabase database_;
    HistoryLimits limits_;
};
} // namespace reposco
