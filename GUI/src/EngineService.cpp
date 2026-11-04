#include "EngineService.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <utility>

EngineService::EngineService() {
    std::string error;
    if (!reset(false, error)) {
        throw std::runtime_error(error);
    }
}

EngineService::~EngineService() {
    flushEventLog();
}

bool EngineService::reset(bool fullSystemMode, std::string& error) {
    flushEventLog();
    integration_.reset();
    feed_.reset();
    eventLogger_.reset();
    engine_.reset();
    book_.reset();
    feedConsumer_ = 0;

    try {
        book_ = std::make_unique<Book>();
        engine_ = std::make_unique<MatchingEngine>(*book_);
        if (fullSystemMode) {
            eventLogger_ = std::make_unique<EventLogger>("lob_gui_events.log", false);
            feed_ = std::make_unique<MarketDataFeed>(4096);
            feedConsumer_ = feed_->registerConsumer();
            integration_ = std::make_unique<EventLoggerIntegration>(
                *book_, *engine_, *eventLogger_, *feed_);
        }
        fullSystemMode_ = fullSystemMode;
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        integration_.reset();
        feed_.reset();
        eventLogger_.reset();
        engine_.reset();
        book_.reset();
        fullSystemMode_ = false;
        error = exception.what();
        return false;
    }
}

SubmissionResult EngineService::submitOrder(const Order& order, OrderType type) {
    return integration_ ? integration_->submitOrderDetailed(order, type)
                        : engine_->submitOrderDetailed(order, type);
}

bool EngineService::cancelOrder(int orderID) {
    return integration_ ? integration_->cancelOrder(orderID) : book_->cancelOrder(orderID);
}

bool EngineService::modifyOrder(int orderID, int newPrice, int newQuantity) {
    return integration_ ? integration_->modifyOrder(orderID, newPrice, newQuantity)
                        : book_->modifyOrder(orderID, newPrice, newQuantity);
}

int EngineService::captureLevel(LimitNode* node, Side side,
                                std::vector<PriceLevelSnapshot>& levels,
                                std::unordered_set<LimitNode*>& visited) const {
    if (!node) return -1;
    if (!visited.insert(node).second) {
        throw std::runtime_error("Price-level tree contains a cycle or shared node.");
    }
    const int index = static_cast<int>(levels.size());
    levels.push_back({side, node->price, node->queue.totalQuantity, {}, -1, -1});
    std::vector<OrderSnapshot>& orders = levels[static_cast<std::size_t>(index)].orders;
    std::unordered_set<Order*> visitedOrders;
    std::size_t fifoPosition = 1;
    for (Order* order = node->queue.head; order; order = order->next) {
        if (!visitedOrders.insert(order).second) {
            throw std::runtime_error("Price-level queue contains a cycle.");
        }
        orders.push_back(
            {order->id, order->side, order->price, order->quantity, order->timestamp,
             fifoPosition++});
    }
    const int left = captureLevel(node->left, side, levels, visited);
    const int right = captureLevel(node->right, side, levels, visited);
    levels[static_cast<std::size_t>(index)].left = left;
    levels[static_cast<std::size_t>(index)].right = right;
    return index;
}

BookSnapshot EngineService::snapshot() const {
    BookSnapshot result;
    std::unordered_set<LimitNode*> visited;
    result.buyRoot = captureLevel(book_->buyTree.root, Side::Buy, result.buyLevels, visited);
    visited.clear();
    result.sellRoot = captureLevel(book_->sellTree.root, Side::Sell, result.sellLevels, visited);

    for (const PriceLevelSnapshot& level : result.buyLevels) {
        result.openOrders.insert(result.openOrders.end(), level.orders.begin(), level.orders.end());
    }
    for (const PriceLevelSnapshot& level : result.sellLevels) {
        result.openOrders.insert(result.openOrders.end(), level.orders.begin(), level.orders.end());
    }
    std::stable_sort(result.openOrders.begin(), result.openOrders.end(),
        [](const OrderSnapshot& left, const OrderSnapshot& right) {
            if (left.side != right.side) return left.side == Side::Buy;
            if (left.price != right.price) {
                return left.side == Side::Buy ? left.price > right.price
                                              : left.price < right.price;
            }
            return false;
        });
    result.trades = engine_->getTrades();
    for (const PriceLevelSnapshot& level : result.buyLevels) {
        if (!result.hasBestBid || level.price > result.bestBid) {
            result.hasBestBid = true;
            result.bestBid = level.price;
        }
    }
    for (const PriceLevelSnapshot& level : result.sellLevels) {
        if (!result.hasBestAsk || level.price < result.bestAsk) {
            result.hasBestAsk = true;
            result.bestAsk = level.price;
        }
    }
    result.lastEventSequence = lastEventSequence();
    result.feedActive = feed_ != nullptr;
    if (feed_) {
        result.feedStats = feed_->getFeedStats();
        feed_->getConsumerStats(feedConsumer_, result.consumerStats);
    }
    return result;
}

std::vector<std::string> EngineService::compareWithLiveBook(
    const ReplayEngine& replay) const {
    return replay.compareWith(*book_);
}

FeedDrainStatus EngineService::drainFeed() {
    if (!feed_) return FeedDrainStatus::Inactive;
    Event event;
    while (true) {
        const FeedStatus status = feed_->getNextEvent(feedConsumer_, event);
        if (status == FeedStatus::EVENT_AVAILABLE) continue;
        if (status == FeedStatus::NO_EVENT) return FeedDrainStatus::Drained;
        if (status == FeedStatus::BUFFER_OVERFLOW) {
            return feed_->recoverConsumer(feedConsumer_)
                ? FeedDrainStatus::RecoveredOverflow
                : FeedDrainStatus::RecoveryFailed;
        }
        return FeedDrainStatus::InvalidConsumer;
    }
}

void EngineService::flushEventLog() {
    if (eventLogger_) eventLogger_->flush();
}

std::uint64_t EngineService::lastEventSequence() const {
    return eventLogger_ ? eventLogger_->getLastSequence() : 0;
}

bool EngineService::getFeedStats(FeedStats& feedStats,
                                 ConsumerStats& consumerStats) const {
    if (!feed_) return false;
    feedStats = feed_->getFeedStats();
    return feed_->getConsumerStats(feedConsumer_, consumerStats);
}

std::size_t EngineService::tradeCount() const {
    return engine_ ? engine_->getTradeCount() : 0;
}

bool EngineService::fullSystemMode() const {
    return fullSystemMode_;
}
