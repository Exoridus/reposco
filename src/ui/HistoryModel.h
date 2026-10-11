#pragma once

#include "storage/HistoryStore.h"
#include <QAbstractListModel>

namespace reposco {
class HistoryModel final : public QAbstractListModel {
    Q_OBJECT
  public:
    enum Role : std::uint16_t {
        EntryIdRole = Qt::UserRole + 1,
        ExcerptRole,
        CopiedAtRole,
        PayloadBytesRole
    };
    using QAbstractListModel::QAbstractListModel;
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    void replace(std::vector<HistorySummary> rows);

  private:
    std::vector<HistorySummary> rows_;
};
} // namespace reposco
