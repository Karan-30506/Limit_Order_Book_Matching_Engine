#include "EventLoggerIntegration.hpp"

#include <stdexcept>

EventLoggerIntegration::EventLoggerIntegration(
    Book& book,
    MatchingEngine& engine,
    EventLogger& logger
)
    : book_(book), engine_(engine), logger_(logger), feed_(nullptr) {
}

EventLoggerIntegration::EventLoggerIntegration(
    Book& book,
    MatchingEngine& engine,
    EventLogger& logger,
    MarketDataFeed& feed
)
    : book_(book), engine_(engine), logger_(logger), feed_(&feed) {
}

void EventLoggerIntegration::publishLastEvent() {
    if (feed_ == nullptr) {
        return;
    }

    Event event;
    if (!logger_.getLastEvent(event) ||
        feed_->publish(event) == PublishStatus::INVALID_SEQUENCE) {
        throw std::runtime_error("Unable to publish logged event to feed.");
    }
}

bool EventLoggerIntegration::submitOrder(
    const Order& order,
    OrderType type
) {
    return submitOrderDetailed(order, type).success;
}

SubmissionResult EventLoggerIntegration::submitOrderDetailed(
    const Order& order,
    OrderType type
) {
    const std::size_t oldTradeCount = engine_.getTradeCount();

    const SubmissionResult result = engine_.submitOrderDetailed(order, type);
    if (!result.success) {
        return result;
    }

    // Log the accepted incoming order first.
    logger_.logOrder(order, type);
    publishLastEvent();

    // Then log each execution produced by this submission.
    const std::vector<Trade>& trades = engine_.getTrades();

    for (std::size_t i = oldTradeCount; i < trades.size(); ++i) {
        logger_.logTrade(trades[i]);
        publishLastEvent();
    }

    return result;
}

bool EventLoggerIntegration::cancelOrder(int orderID) {
    Order* order = book_.getOrder(orderID);

    if (order == nullptr) {
        return false;
    }

    Order canceled(order->id, order->price, order->quantity,
                   order->timestamp, order->side);

    if (!book_.cancelOrder(orderID)) {
        return false;
    }

    logger_.logCancel(canceled);
    publishLastEvent();

    return true;
}

bool EventLoggerIntegration::modifyOrder(
    int orderID,
    int newPrice,
    int newQuantity
) {
    Order* order = book_.getOrder(orderID);

    if (order == nullptr) {
        return false;
    }

    if (!book_.modifyOrder(orderID, newPrice, newQuantity)) {
        return false;
    }

    logger_.logModify(*order, newPrice, newQuantity);
    publishLastEvent();

    return true;
}