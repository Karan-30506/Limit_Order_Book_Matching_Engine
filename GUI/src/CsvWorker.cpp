#include "CsvWorker.hpp"

#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QTextStream>

#include <algorithm>
#include <exception>
#include <utility>

#include "CsvReader.hpp"

CsvWorker::CsvWorker(QObject* parent) : QObject(parent) {
    qRegisterMetaType<CsvPageReply>();
}

bool CsvWorker::post(std::function<void()> operation) {
    return QMetaObject::invokeMethod(this, std::move(operation), Qt::QueuedConnection);
}

void CsvWorker::loadPage(QString path, QString filter, int page, int pageSize,
                         std::uint64_t requestID) {
    latestRequestID_.store(requestID, std::memory_order_relaxed);
    const bool queued = post([this, path = std::move(path), filter = std::move(filter),
          page, pageSize, requestID] {
        try {
            CsvPageReply reply = readPage(path, filter, page, pageSize, requestID);
            if (latestRequestID_.load(std::memory_order_relaxed) == requestID) {
                emit pageReady(reply);
            }
        } catch (const std::exception& error) {
            CsvPageReply reply;
            reply.requestID = requestID;
            reply.error = QString::fromUtf8(error.what());
            if (latestRequestID_.load(std::memory_order_relaxed) == requestID) {
                emit pageReady(reply);
            }
        }
    });
    if (!queued) {
        CsvPageReply reply;
        reply.requestID = requestID;
        reply.error = QStringLiteral("Could not queue CSV loading on the worker thread.");
        emit pageReady(reply);
    }
}

void CsvWorker::cancelPending() {
    latestRequestID_.fetch_add(1, std::memory_order_relaxed);
}

CsvPageReply CsvWorker::readPage(const QString& path, const QString& filter,
                                 int page, int pageSize, std::uint64_t requestID) {
    CsvPageReply reply;
    reply.requestID = requestID;
    reply.page = page;
    const QFileInfo fileInfo(path);
    if (path != indexedPath_ || fileInfo.size() != indexedFileSize_ ||
        fileInfo.lastModified() != indexedModified_) {
        indexedPath_ = path;
        indexedFileSize_ = fileInfo.size();
        indexedModified_ = fileInfo.lastModified();
        headers_.clear();
        rowOffsets_.clear();
        indexedError_.clear();

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            indexedError_ = QStringLiteral("Could not open CSV: %1").arg(file.errorString());
        } else if (file.atEnd()) {
            indexedError_ = QStringLiteral("CSV file is empty.");
        } else {
            QStringList header;
            if (!parseCsvRow(csvLineText(file.readLine()), header) ||
                header.isEmpty()) {
                indexedError_ = QStringLiteral("CSV header is malformed.");
            } else {
                headers_ = std::move(header);
                const qint64 fileSize = std::max<qint64>(1, file.size());
                std::size_t lineNumber = 1;
                while (!file.atEnd()) {
                    if (latestRequestID_.load(std::memory_order_relaxed) != requestID) {
                        reply.error = QStringLiteral("CSV load superseded.");
                        return reply;
                    }
                    const qint64 offset = file.pos();
                    const QByteArray bytes = file.readLine();
                    ++lineNumber;
                    if (bytes.trimmed().isEmpty()) continue;
                    QStringList row;
                    if (!parseCsvRow(csvLineText(bytes), row) ||
                        row.size() != headers_.size()) {
                        indexedError_ = QStringLiteral("Malformed CSV record at line %1.")
                            .arg(lineNumber);
                        rowOffsets_.clear();
                        break;
                    }
                    rowOffsets_.push_back(offset);
                    if (rowOffsets_.size() % 10000 == 0) {
                        emit progressChanged(requestID, static_cast<int>(
                            (100 * file.pos()) / fileSize));
                    }
                }
            }
        }
    }

    if (!indexedError_.isEmpty()) {
        reply.error = indexedError_;
        return reply;
    }
    reply.headers = headers_;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        reply.error = QStringLiteral("Could not reopen CSV: %1").arg(file.errorString());
        return reply;
    }

    std::vector<qint64> matches;
    const QString needle = filter.trimmed();
    matches.reserve(rowOffsets_.size());
    for (std::size_t index = 0; index < rowOffsets_.size(); ++index) {
        if (latestRequestID_.load(std::memory_order_relaxed) != requestID) {
            reply.error = QStringLiteral("CSV filter superseded.");
            return reply;
        }
        if (!file.seek(rowOffsets_[index])) {
            reply.error = QStringLiteral("Could not seek to CSV row %1.").arg(index + 2);
            return reply;
        }
        QStringList row;
        if (!parseCsvRow(csvLineText(file.readLine()), row) ||
            row.size() != headers_.size()) {
            reply.error = QStringLiteral("CSV changed or became malformed at row %1.")
                .arg(index + 2);
            return reply;
        }
        bool matchesFilter = needle.isEmpty();
        for (const QString& field : row) {
            if (field.contains(needle, Qt::CaseInsensitive)) {
                matchesFilter = true;
                break;
            }
        }
        if (matchesFilter) matches.push_back(rowOffsets_[index]);
        if (index % 10000 == 0) {
            emit progressChanged(requestID, static_cast<int>(
                (100 * (index + 1)) / std::max<std::size_t>(1, rowOffsets_.size())));
        }
    }

    reply.totalRows = matches.size();
    const std::size_t start = std::min<std::size_t>(
        matches.size(), static_cast<std::size_t>(std::max(0, page)) *
                            static_cast<std::size_t>(std::max(1, pageSize)));
    const std::size_t end = std::min(matches.size(), start +
        static_cast<std::size_t>(std::max(1, pageSize)));
    reply.rows.reserve(end - start);
    for (std::size_t index = start; index < end; ++index) {
        if (!file.seek(matches[index])) {
            reply.error = QStringLiteral("Could not seek to displayed CSV row.");
            reply.rows.clear();
            return reply;
        }
        QStringList row;
        if (!parseCsvRow(csvLineText(file.readLine()), row) ||
            row.size() != headers_.size()) {
            reply.error = QStringLiteral("CSV changed or became malformed while reading the page.");
            reply.rows.clear();
            return reply;
        }
        reply.rows.push_back(std::move(row));
    }
    reply.success = true;
    return reply;
}
