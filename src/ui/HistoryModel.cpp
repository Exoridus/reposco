#include "HistoryModel.h"

#include <algorithm>

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
    for (int row = rowCount() - 1; row >= 0; --row) {
        const auto id = rows_[static_cast<std::size_t>(row)].id;
        if (std::ranges::none_of(rows,
                                 [id](const HistorySummary& value) { return value.id == id; })) {
            beginRemoveRows({}, row, row);
            rows_.erase(rows_.begin() + row);
            endRemoveRows();
        }
    }
    for (std::size_t row = 0; row < rows.size(); ++row) {
        const auto id = rows[row].id;
        const auto first = rows_.begin() + static_cast<std::ptrdiff_t>(row);
        const auto found = std::find_if(
            first, rows_.end(), [id](const HistorySummary& value) { return value.id == id; });
        const auto target = static_cast<int>(row);
        if (found == rows_.end()) {
            beginInsertRows({}, target, target);
            rows_.insert(first, std::move(rows[row]));
            endInsertRows();
        } else {
            if (found != first) {
                const auto source = static_cast<int>(std::distance(rows_.begin(), found));
                beginMoveRows({}, source, source, {}, target);
                std::rotate(first, found, found + 1);
                endMoveRows();
            }
            auto& previous = rows_[row];
            const auto& next = rows[row];
            if (previous.excerpt != next.excerpt || previous.copiedAt != next.copiedAt ||
                previous.payloadBytes != next.payloadBytes || previous.kind != next.kind) {
                previous = std::move(rows[row]);
                emit dataChanged(index(target, 0), index(target, 0));
            }
        }
    }
    emit summariesChanged();
}
} // namespace reposco
