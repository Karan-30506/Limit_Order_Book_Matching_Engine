#include "BaseOrderMatcher.hpp"

#include <algorithm>
#include <limits>

BaseOrderMatcher::BaseOrderMatcher(
    Book& orderBook,
    TradeRecorder& recorder,
    std::uint64_t& tradeID
)
    : book(orderBook),
      tradeRecorder(recorder),
      nextTradeID(tradeID) {}

bool BaseOrderMatcher::executeMatch(const Order& order, Side expectedSide) {
    // 1. Common validation using OrderValidator
    if (!OrderValidator::isValid(order) || order.side != expectedSide) {
        return false;
    }

    // 2. Order-specific validation hook (e.g., limit price > 0)
    if (!validateSpecific(order)) {
        return false;
    }

    const std::uint64_t maxTradeID = std::numeric_limits<std::uint64_t>::max();

    int remainingQuantity = order.quantity;

    // 3. Execution loop against opposing liquidity
    while (remainingQuantity > 0) {
        LimitNode* level = (expectedSide == Side::Buy)
            ? book.getBestAsk()
            : book.getBestBid();

        if (level == nullptr) {
            break;
        }

        // Custom price-crossing check hook
        if (!canMatchPrice(order, level, expectedSide)) {
            break;
        }

        Order* restingOrder = level->queue.head;
        if (restingOrder == nullptr || restingOrder->quantity <= 0) {
            return false;
        }

        if (nextTradeID == maxTradeID) {
            return false;
        }

        const int fillQuantity = std::min(
            remainingQuantity,
            restingOrder->quantity
        );

        Trade trade(
            nextTradeID,
            expectedSide == Side::Buy ? order.id : restingOrder->id,
            expectedSide == Side::Buy ? restingOrder->id : order.id,
            restingOrder->price,
            fillQuantity
        );

        if (fillQuantity == restingOrder->quantity) {
            Order* filledOrder = book.popHeadOrder(level);
            if (filledOrder != restingOrder) {
                return false;
            }
            delete filledOrder;
        } else {
            restingOrder->reduceQuantity(fillQuantity);
        }

        tradeRecorder.recordTrade(trade);
        ++nextTradeID;
        remainingQuantity -= fillQuantity;
    }

    // 4. Handle unfilled remainder hook (e.g., limit orders rest in book, market orders drop)
    if (remainingQuantity > 0) {
        return handleUnfilled(order, remainingQuantity, expectedSide);
    }

    return true;
}
