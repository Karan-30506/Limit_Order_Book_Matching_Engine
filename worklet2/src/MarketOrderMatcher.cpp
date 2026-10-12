
#include "MarketOrderMatcher.hpp"

#include <algorithm>
#include <limits>

MarketOrderMatcher::MarketOrderMatcher(
    Book& orderBook,
    TradeRecorder& recorder,
    int& tradeID
)
    : book(orderBook),
      tradeRecorder(recorder),
      nextTradeID(tradeID) {}

bool MarketOrderMatcher::matchMarketBuy(const Order& order) {
    if (order.side != Side::Buy ||
        order.id <= 0 ||
        order.quantity <= 0) {
        return false;
    }

    int remainingQuantity = order.quantity;

    while (remainingQuantity > 0) {
        LimitNode* level = book.getBestAsk();

        // No sell orders remain.
        if (level == nullptr) {
            break;
        }

        Order* restingOrder = level->queue.head;

        if (restingOrder == nullptr ||
            restingOrder->quantity <= 0) {
            return false;
        }

        // Do not execute a trade if no valid trade ID is available.
        if (nextTradeID <= 0 ||
            nextTradeID == std::numeric_limits<int>::max()) {
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
            // Remove the fully filled resting order from the book.
            Order* filledOrder = book.popHeadOrder(level);

            if (filledOrder != restingOrder) {
                return false;
            }

            delete filledOrder;
        } else {
            // Partial fill: preserve the resting order's FIFO position.
            restingOrder->reduceQuantity(fillQuantity);
        }

        tradeRecorder.recordTrade(trade);
        ++nextTradeID;

        remainingQuantity -= fillQuantity;
    }

    // Any unfilled market-buy quantity is discarded.
    return true;
}

bool MarketOrderMatcher::matchMarketSell(const Order& order) {
    if (order.side != Side::Sell ||
        order.id <= 0 ||
        order.quantity <= 0) {
        return false;
    }

    int remainingQuantity = order.quantity;

    while (remainingQuantity > 0) {
        LimitNode* level = book.getBestBid();

        // No buy orders remain.
        if (level == nullptr) {
            break;
        }

        Order* restingOrder = level->queue.head;

        if (restingOrder == nullptr ||
            restingOrder->quantity <= 0) {
            return false;
        }

        // Do not execute a trade if no valid trade ID is available.
        if (nextTradeID <= 0 ||
            nextTradeID == std::numeric_limits<int>::max()) {
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
            // Remove the fully filled resting order from the book.
            Order* filledOrder = book.popHeadOrder(level);

            if (filledOrder != restingOrder) {
                return false;
            }

            delete filledOrder;
        } else {
            // Partial fill: preserve the resting order's FIFO position.
            restingOrder->reduceQuantity(fillQuantity);
        }

        tradeRecorder.recordTrade(trade);
        ++nextTradeID;

        remainingQuantity -= fillQuantity;
    }

    // Any unfilled market-sell quantity is discarded.
    return true;
}