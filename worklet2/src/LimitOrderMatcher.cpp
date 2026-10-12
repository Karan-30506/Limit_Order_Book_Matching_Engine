#include "LimitOrderMatcher.hpp"

#include <algorithm>
#include <limits>

LimitOrderMatcher::LimitOrderMatcher(
    Book& orderBook,
    TradeRecorder& recorder,
    int& tradeID
)
    : book(orderBook),
      tradeRecorder(recorder),
      nextTradeID(tradeID) {}

bool LimitOrderMatcher::matchLimitBuy(const Order& order) {
    if (order.side != Side::Buy ||
        order.id <= 0 ||
        order.price <= 0 ||
        order.quantity <= 0 ||
        nextTradeID <= 0 ||
        nextTradeID == std::numeric_limits<int>::max()) {
        return false;
    }

    int remainingQuantity = order.quantity;

    while (remainingQuantity > 0) {
        LimitNode* level = book.getBestAsk();

        if (level == nullptr || level->price > order.price) {
            break;
        }

        Order* restingOrder = level->queue.head;

        if (restingOrder == nullptr || restingOrder->quantity <= 0) {
            return false;
        }

        const int fillQuantity = std::min(
            remainingQuantity,
            restingOrder->quantity
        );

        Trade trade(
            nextTradeID,
            order.id,
            restingOrder->id,
            restingOrder->price,
            fillQuantity
        );

        if (fillQuantity == restingOrder->quantity) {
            Order* filledOrder = book.popHeadOrder(level);

            if (filledOrder != restingOrder) {
                // This indicates an unexpected inconsistency in Book.
                // Do not delete an unexpected pointer here.
                return false;
            }

            delete filledOrder;
        } else {
            // Preserve the resting order's FIFO position.
            restingOrder->reduceQuantity(fillQuantity);
        }

        tradeRecorder.recordTrade(trade);
        ++nextTradeID;
        remainingQuantity -= fillQuantity;

        // Guard against exhausting the trade ID range mid-order.
        if (remainingQuantity > 0 &&
            nextTradeID == std::numeric_limits<int>::max()) {
            return false;
        }
    }

    if (remainingQuantity > 0) {
        Order* remainder = new Order(
            order.id,
            order.price,
            remainingQuantity,
            order.timestamp,
            Side::Buy
        );

        if (!book.insertOrder(remainder)) {
            delete remainder;
            return false;
        }
    }

    return true;
}

bool LimitOrderMatcher::matchLimitSell(const Order& order) {
    if (order.side != Side::Sell ||
        order.id <= 0 ||
        order.price <= 0 ||
        order.quantity <= 0 ||
        nextTradeID <= 0 ||
        nextTradeID == std::numeric_limits<int>::max()) {
        return false;
    }

    int remainingQuantity = order.quantity;

    while (remainingQuantity > 0) {
        LimitNode* level = book.getBestBid();

        if (level == nullptr || level->price < order.price) {
            break;
        }

        Order* restingOrder = level->queue.head;

        if (restingOrder == nullptr || restingOrder->quantity <= 0) {
            return false;
        }

        const int fillQuantity = std::min(
            remainingQuantity,
            restingOrder->quantity
        );

        Trade trade(
            nextTradeID,
            restingOrder->id,
            order.id,
            restingOrder->price,
            fillQuantity
        );

        if (fillQuantity == restingOrder->quantity) {
            Order* filledOrder = book.popHeadOrder(level);

            if (filledOrder != restingOrder) {
                // This indicates an unexpected inconsistency in Book.
                // Do not delete an unexpected pointer here.
                return false;
            }

            delete filledOrder;
        } else {
            // Preserve the resting order's FIFO position.
            restingOrder->reduceQuantity(fillQuantity);
        }

        tradeRecorder.recordTrade(trade);
        ++nextTradeID;
        remainingQuantity -= fillQuantity;

        if (remainingQuantity > 0 &&
            nextTradeID == std::numeric_limits<int>::max()) {
            return false;
        }
    }

    if (remainingQuantity > 0) {
        Order* remainder = new Order(
            order.id,
            order.price,
            remainingQuantity,
            order.timestamp,
            Side::Sell
        );

        if (!book.insertOrder(remainder)) {
            delete remainder;
            return false;
        }
    }

    return true;
}
