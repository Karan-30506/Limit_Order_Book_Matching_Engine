#include "EngineWorker.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QTemporaryFile>
#include <QThread>
#include <QTimer>

#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool waitForReply(QEventLoop& loop, bool& received) {
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
    return received;
}

}

int main(int argc, char* argv[]) {
    try {
        QCoreApplication application(argc, argv);
        QThread thread;
        auto* worker = new EngineWorker;
        worker->moveToThread(&thread);
        QObject::connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
        thread.start();

        try {
            QEventLoop readyLoop;
            bool ready = false;
            bool readySuccess = false;
            QObject::connect(worker, &EngineWorker::ready, &readyLoop,
                [&](bool success, const QString&, const BookSnapshot&) {
                    ready = true;
                    readySuccess = success;
                    readyLoop.quit();
                });
            worker->initialize(false);
            require(waitForReply(readyLoop, ready) && readySuccess,
                    "worker did not initialize asynchronously");

            QEventLoop operationLoop;
            bool received = false;
            EngineReply reply;
            QObject::connect(worker, &EngineWorker::operationCompleted, &operationLoop,
                [&](const EngineReply& result) {
                    reply = result;
                    received = true;
                    operationLoop.quit();
                });

            worker->submit(
                {1, 100, 5, 1, Side::Buy, OrderType::Limit, {}, {}, false}, 1);
            require(waitForReply(operationLoop, received) && reply.success &&
                    reply.snapshot.openOrders.size() == 1,
                    "worker submission did not return a value snapshot");

            received = false;
            worker->submitBatch({
                {2, 99, 2, 2, Side::Sell, OrderType::Limit, {}, {}, false},
                {3, 101, 4, 3, Side::Buy, OrderType::Limit, {}, {}, false}
            }, 2);
            require(waitForReply(operationLoop, received) && reply.batch &&
                    reply.submittedCount == 2 && reply.acceptedCount == 2 &&
                    reply.snapshot.trades.size() == 1 && reply.outcomes.size() == 2,
                    "worker batch did not produce expected trade and order outcomes");

            received = false;
            worker->cancel(3, 3);
            require(waitForReply(operationLoop, received) && reply.success &&
                    reply.snapshot.openOrders.size() == 1,
                    "worker cancellation did not update its snapshot");

            received = false;
            worker->modify(1, 102, 2, 4);
            require(waitForReply(operationLoop, received) && reply.success &&
                    reply.snapshot.openOrders.size() == 1 &&
                    reply.snapshot.openOrders.front().price == 102 &&
                    reply.snapshot.openOrders.front().quantity == 2,
                    "worker modification did not update its snapshot");

            QTemporaryFile invalidOrderFile(
                QDir::tempPath() + QStringLiteral("/lob-invalid-order-XXXXXX.json"));
            require(invalidOrderFile.open(),
                    "could not create invalid-order parser fixture");
            const QByteArray invalidOrderJson =
                "[{\"id\":-9001,\"type\":\"ICEBERG-42\",\"side\":\"AUCTION-SIDE\","
                "\"price\":-86,\"quantity\":-3},"
                "{\"id\":-9002,\"type\":\"\",\"side\":\"\","
                "\"price\":-87,\"quantity\":-4}]";
            require(invalidOrderFile.write(invalidOrderJson) == invalidOrderJson.size(),
                    "could not write invalid-order parser fixture");
            require(invalidOrderFile.flush(),
                    "could not flush invalid-order parser fixture");

            received = false;
            worker->parseOrders(invalidOrderFile.fileName(), 5);
            require(waitForReply(operationLoop, received) && reply.success &&
                    reply.parsedOrders.size() == 2,
                    "order parser did not return the invalid order");
            require(reply.parsedOrders.front().originalType == QStringLiteral("ICEBERG-42") &&
                    reply.parsedOrders.front().originalSide == QStringLiteral("AUCTION-SIDE") &&
                    reply.parsedOrders.front().hasOriginalInput &&
                    reply.parsedOrders.back().originalType.isEmpty() &&
                    reply.parsedOrders.back().originalSide.isEmpty() &&
                    reply.parsedOrders.back().hasOriginalInput,
                    "order parser did not preserve arbitrary type and side values");
            require(reply.parsedOrders.front().id == -9001 &&
                    reply.parsedOrders.front().price == -86 &&
                    reply.parsedOrders.front().quantity == -3,
                    "order parser did not preserve invalid numeric values");

            received = false;
            worker->submitBatch(reply.parsedOrders, 6);
            require(waitForReply(operationLoop, received) && reply.batch &&
                    reply.outcomes.size() == 2 && !reply.outcomes.front().accepted &&
                    reply.outcomes.front().originalType == QStringLiteral("ICEBERG-42") &&
                    reply.outcomes.front().originalSide == QStringLiteral("AUCTION-SIDE") &&
                    reply.outcomes.front().hasOriginalInput &&
                    reply.outcomes.front().id == -9001 &&
                    reply.outcomes.front().price == -86 &&
                    reply.outcomes.front().requestedQuantity == -3,
                    "rejected order outcome did not preserve original input fields");
            require(reply.outcomes.back().originalType.isEmpty() &&
                    reply.outcomes.back().originalSide.isEmpty() &&
                    reply.outcomes.back().hasOriginalInput,
                    "rejected order outcome replaced empty original fields");
        } catch (...) {
            thread.quit();
            thread.wait();
            throw;
        }

        thread.quit();
        thread.wait();
    } catch (const std::exception& error) {
        qCritical("EngineWorker test failed: %s", error.what());
        return 1;
    }
    return 0;
}
