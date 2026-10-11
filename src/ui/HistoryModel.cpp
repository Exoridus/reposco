#include "HistoryModel.h"

namespace reposco {
int HistoryModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}
QVariant HistoryModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const auto& row = rows_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case EntryIdRole:
        return QVariant::fromValue(static_cast<qlonglong>(row.id));
    case ExcerptRole:
        return row.excerpt;
    case CopiedAtRole:
        return QVariant::fromValue(static_cast<qlonglong>(row.copiedAt));
    case PayloadBytesRole:
        return QVariant::fromValue(static_cast<qlonglong>(row.payloadBytes));
    case KindRole:
        return row.kind;
    default:
        return {};
    }
}
QHash<int, QByteArray> HistoryModel::roleNames() const {
    return {{EntryIdRole, "entryId"},
            {ExcerptRole, "excerpt"},
            {CopiedAtRole, "copiedAt"},
            {PayloadBytesRole, "payloadBytes"},
            {KindRole, "kind"}};
}
void HistoryModel::replace(std::vector<HistorySummary> rows) {
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
}
} // namespace reposco
