#include "CsvTableModel.hpp"

#include <QBrush>
#include <QColor>

CsvTableModel::CsvTableModel(QObject* parent) : QAbstractTableModel(parent) {}

int CsvTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int CsvTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : headers_.size();
}

QVariant CsvTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 ||
        static_cast<std::size_t>(index.row()) >= rows_.size() ||
        index.column() < 0 || index.column() >= headers_.size()) return {};

    const QString& value = rows_[static_cast<std::size_t>(index.row())][index.column()];
    if (role == Qt::DisplayRole) return value;
    if (role == Qt::ForegroundRole &&
        headers_[index.column()].compare(QStringLiteral("side"), Qt::CaseInsensitive) == 0) {
        if (value.compare(QStringLiteral("BUY"), Qt::CaseInsensitive) == 0) {
            return QBrush(QColor("#f38ba8"));
        }
        if (value.compare(QStringLiteral("SELL"), Qt::CaseInsensitive) == 0) {
            return QBrush(QColor("#a6e3a1"));
        }
    }
    return {};
}

QVariant CsvTableModel::headerData(int section, Qt::Orientation orientation,
                                   int role) const {
    if (role != Qt::DisplayRole) return {};
    if (orientation == Qt::Horizontal) {
        return section >= 0 && section < headers_.size() ? headers_[section] : QVariant{};
    }
    return section + 1;
}

Qt::ItemFlags CsvTableModel::flags(const QModelIndex& index) const {
    return index.isValid() ? QAbstractTableModel::flags(index) & ~Qt::ItemIsEditable
                           : Qt::NoItemFlags;
}

void CsvTableModel::setPage(QStringList headers, std::vector<QStringList> rows) {
    beginResetModel();
    headers_ = std::move(headers);
    rows_ = std::move(rows);
    endResetModel();
}
