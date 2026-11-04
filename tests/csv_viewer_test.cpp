#include "CsvTableModel.hpp"
#include "CsvWorker.hpp"

#include <QBrush>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTemporaryFile>
#include <QThread>
#include <QTimer>

#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

CsvPageReply requestPage(CsvWorker* worker, const QString& path,
                         const QString& filter, int page, std::uint64_t requestID) {
    QEventLoop loop;
    CsvPageReply result;
    bool received = false;
    QObject::connect(worker, &CsvWorker::pageReady, &loop,
        [&](const CsvPageReply& reply) {
            if (reply.requestID == requestID) {
                result = reply;
                received = true;
                loop.quit();
            }
        });
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    worker->loadPage(path, filter, page, 3, requestID);
    loop.exec();
    require(received, "timed out waiting for CSV worker");
    return result;
}

}

int main(int argc, char* argv[]) {
    try {
        QCoreApplication application(argc, argv);
        QTemporaryFile csv;
        require(csv.open(), "could not create CSV fixture");
        csv.write("\xEF\xBB\xBFid,name,side\n"
                  "1,\"alpha,one\",BUY\n"
                  "2,beta,SELL\n"
                  "3,gamma,BUY\n"
                  "4,needle,SELL\n"
                  "5,delta,BUY\n"
                  "6,epsilon,SELL\n"
                  "7,zeta,BUY\n");
        csv.flush();

        QThread thread;
        auto* worker = new CsvWorker;
        worker->moveToThread(&thread);
        QObject::connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
        thread.start();

        CsvPageReply page;
        try {
            page = requestPage(worker, csv.fileName(), QString(), 1, 1);
            require(page.success && page.totalRows == 7 && page.rows.size() == 3 &&
                    page.rows.front()[0] == QStringLiteral("4") &&
                    page.rows.front()[1] == QStringLiteral("needle"),
                    "CSV page did not return the correct rows or preserve quoted commas");

            const CsvPageReply filtered =
                requestPage(worker, csv.fileName(), QStringLiteral("needle"), 0, 2);
            require(filtered.success && filtered.totalRows == 1 &&
                    filtered.rows.size() == 1 &&
                    filtered.rows.front()[0] == QStringLiteral("4"),
                    "CSV filter did not search the full file");

            CsvTableModel model;
            model.setPage(filtered.headers, filtered.rows);
            require(model.rowCount() == 1 && model.columnCount() == 3 &&
                    model.data(model.index(0, 1)).toString() == QStringLiteral("needle"),
                    "CSV table model returned incorrect row data");
            const QBrush sideBrush =
                qvariant_cast<QBrush>(model.data(model.index(0, 2), Qt::ForegroundRole));
            require(sideBrush.color() == QColor("#a6e3a1"),
                    "CSV table model did not color the sell side");
        } catch (...) {
            thread.quit();
            thread.wait();
            throw;
        }
        thread.quit();
        thread.wait();
    } catch (const std::exception& error) {
        qCritical("CSV viewer test failed: %s", error.what());
        return 1;
    }
    return 0;
}
