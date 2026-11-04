#ifndef GUI_CSV_TABLE_MODEL_HPP
#define GUI_CSV_TABLE_MODEL_HPP

#include <QAbstractTableModel>
#include <QStringList>

#include <vector>

class CsvTableModel final : public QAbstractTableModel {
public:
    explicit CsvTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setPage(QStringList headers, std::vector<QStringList> rows);

private:
    QStringList headers_;
    std::vector<QStringList> rows_;
};

#endif
