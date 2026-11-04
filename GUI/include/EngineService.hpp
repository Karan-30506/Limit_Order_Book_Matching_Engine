#ifndef GUI_ENGINE_SERVICE_HPP
#define GUI_ENGINE_SERVICE_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "EventLogger.hpp"
#include "EventLoggerIntegration.hpp"
#include "Book.hpp"
#include "MarketDataFeed.hpp"
#include "MatchingEngine.hpp"
#include "replayEngine.hpp"

struct OrderSnapshot {
    int id = 0;
    Side side = Side::Buy;
    int price = 0;
    int quantity = 0;
    long long timestamp = 0;
    std::size_t fifoPosition = 0;
};

struct PriceLevelSnapshot {
    Side side = Side::Buy;
    int price = 0;
    int totalQuantity = 0;
    std::vector<OrderSnapshot> orders;
    int left = -1;
    int right = -1;
};

struct BookSnapshot {
    std::vector<PriceLevelSnapshot> buyLevels;
    std::vector<PriceLevelSnapshot> sellLevels;
    std::vector<OrderSnapshot> openOrders;
    std::vector<Trade> trades;
    int buyRoot = -1;
    int sellRoot = -1;
    std::uint64_t lastEventSequence = 0;
    bool feedActive = false;
    bool hasBestBid = false;
    bool hasBestAsk = false;
    int bestBid = 0;
    int bestAsk = 0;
    FeedStats feedStats;
    ConsumerStats consumerStats;
};

enum class FeedDrainStatus {
    Inactive,
    Drained,
    RecoveredOverflow,
    InvalidConsumer,
    RecoveryFailed
};

class EngineService {
public:
    EngineService();
    ~EngineService();

    EngineService(const EngineService&) = delete;
    EngineService& operator=(const EngineService&) = delete;

    bool reset(bool fullSystemMode, std::string& error);
    SubmissionResult submitOrder(const Order& order, OrderType type);
    bool cancelOrder(int orderID);
    bool modifyOrder(int orderID, int newPrice, int newQuantity);

    BookSnapshot snapshot() const;
    std::vector<std::string> compareWithLiveBook(const ReplayEngine& replay) const;
    FeedDrainStatus drainFeed();

    void flushEventLog();
    std::uint64_t lastEventSequence() const;
    bool getFeedStats(FeedStats& feedStats, ConsumerStats& consumerStats) const;
    std::size_t tradeCount() const;
    bool fullSystemMode() const;

private:
    int captureLevel(LimitNode* node, Side side,
                     std::vector<PriceLevelSnapshot>& levels,
                     std::unordered_set<LimitNode*>& visited) const;

    std::unique_ptr<Book> book_;
    std::unique_ptr<MatchingEngine> engine_;
    std::unique_ptr<EventLogger> eventLogger_;
    std::unique_ptr<MarketDataFeed> feed_;
    std::unique_ptr<EventLoggerIntegration> integration_;
    std::uint64_t feedConsumer_ = 0;
    bool fullSystemMode_ = false;
};

#endif
