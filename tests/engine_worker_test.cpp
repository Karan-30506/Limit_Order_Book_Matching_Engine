#include "EngineWorker.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
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

            worker->submit({1, 100, 5, 1, Side::Buy, OrderType::Limit}, 1);
            require(waitForReply(operationLoop, received) && reply.success &&
                    reply.snapshot.openOrders.size() == 1,
                    "worker submission did not return a value snapshot");

            received = false;
            worker->submitBatch({
                {2, 99, 2, 2, Side::Sell, OrderType::Limit},
                {3, 101, 4, 3, Side::Buy, OrderType::Limit}
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
