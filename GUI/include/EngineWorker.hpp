#ifndef GUI_ENGINE_WORKER_HPP
#define GUI_ENGINE_WORKER_HPP

#include <QObject>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "EngineService.hpp"

struct OrderCommand {
    int id = 0;
    int price = 0;
    int quantity = 0;
    long long timestamp = 0;
    Side side = Side::Buy;
    OrderType type = OrderType::Limit;
};

struct OrderOutcomeSnapshot {
    int id = 0;
    Side side = Side::Buy;
    OrderType type = OrderType::Limit;
    int requestedQuantity = 0;
    int executedQuantity = 0;
    int remainingQuantity = 0;
    bool accepted = false;
    QString reason;
};

struct ReplayEventSnapshot {
    std::uint64_t sequenceID = 0;
    EventType type = EventType::NEW_ORDER;
    Side side = Side::Buy;
    int orderID = 0;
    int buyOrderID = 0;
    int sellOrderID = 0;
    int price = 0;
    int quantity = 0;
};

struct EngineReply {
    std::uint64_t requestID = 0;
    bool success = false;
    QString message;
    BookSnapshot snapshot;
    std::vector<double> latencyMs;
    std::size_t acceptedCount = 0;
    std::size_t submittedCount = 0;
    bool batch = false;
    bool fatalError = false;
    std::vector<QString> differences;
    std::vector<OrderOutcomeSnapshot> outcomes;
    std::vector<OrderCommand> parsedOrders;
    std::vector<QString> parseWarnings;
    ReplaySnapshot replaySnapshot;
    std::vector<ReplayEventSnapshot> events;
    std::uint64_t replaySequence = 0;
    std::size_t replayCursor = 0;
    std::size_t replayEventCount = 0;
    bool hasReplaySnapshot = false;
};

Q_DECLARE_METATYPE(BookSnapshot)
Q_DECLARE_METATYPE(EngineReply)
Q_DECLARE_METATYPE(std::vector<OrderCommand>)

class EngineWorker final : public QObject {
    Q_OBJECT

public:
    explicit EngineWorker(QObject* parent = nullptr);

    void initialize(bool fullSystemMode);
    void reset(bool fullSystemMode, std::uint64_t requestID);
    void submit(OrderCommand command, std::uint64_t requestID);
    void submitBatch(std::vector<OrderCommand> commands, std::uint64_t requestID);
    void cancel(int orderID, std::uint64_t requestID);
    void modify(int orderID, int price, int quantity, std::uint64_t requestID);
    void parseOrders(QString path, std::uint64_t requestID);
    void loadReplay(QString path, std::uint64_t requestID);
    void stepReplay(std::uint64_t requestID);
    void validateReplay(std::uint64_t requestID);
    void flushEventLog(std::uint64_t requestID = 0);
    void requestSnapshot();

signals:
    void ready(bool success, const QString& error, const BookSnapshot& snapshot);
    void operationCompleted(const EngineReply& reply);
    void snapshotReady(const BookSnapshot& snapshot);

private:
    void post(std::function<void()> operation);

    std::unique_ptr<EngineService> service_;
    std::vector<Event> replayEvents_;
    std::unique_ptr<ReplayEngine> replayEngine_;
    std::size_t replayCursor_ = 0;
};

#endif
