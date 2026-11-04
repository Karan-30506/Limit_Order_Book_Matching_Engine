#ifndef GUI_CSV_WORKER_HPP
#define GUI_CSV_WORKER_HPP

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QStringList>

#include <atomic>
#include <cstdint>
#include <functional>
#include <vector>

struct CsvPageReply {
    std::uint64_t requestID = 0;
    bool success = false;
    QString error;
    QStringList headers;
    std::vector<QStringList> rows;
    std::size_t totalRows = 0;
    int page = 0;
};

Q_DECLARE_METATYPE(CsvPageReply)

class CsvWorker final : public QObject {
    Q_OBJECT

public:
    explicit CsvWorker(QObject* parent = nullptr);
    void loadPage(QString path, QString filter, int page, int pageSize,
                  std::uint64_t requestID);
    void cancelPending();

signals:
    void pageReady(const CsvPageReply& reply);
    void progressChanged(std::uint64_t requestID, int percent);

private:
    bool post(std::function<void()> operation);
    CsvPageReply readPage(const QString& path, const QString& filter,
                          int page, int pageSize, std::uint64_t requestID);

    std::atomic<std::uint64_t> latestRequestID_{0};
    QString indexedPath_;
    qint64 indexedFileSize_ = -1;
    QDateTime indexedModified_;
    QStringList headers_;
    std::vector<qint64> rowOffsets_;
    QString indexedError_;
};

#endif
