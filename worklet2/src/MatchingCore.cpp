
#include "MatchingCore.hpp"

#include <algorithm>
#include <limits>

MatchingCore::MatchingCore(
    Book& orderBook,
    TradeRecorder& recorder,
    std::uint64_t& tradeID
)
    : book(orderBook),
      tradeRecorder(recorder),
      nextTradeID(tradeID) {}

bool MatchingCore::match(
    const Order& order,
    Side expectedSide,
    bool isLimitOrder
) {
    // Validate the incoming order.
    if (order.side != expectedSide ||
        order.id <= 0 ||
        order.quantity <= 0) {
        return false;
    }

    if (isLimitOrder && order.price <= 0) {
        return false;
    }

    const std::uint64_t maxTradeID =
        std::numeric_limits<std::uint64_t>::max();

    // Preserve the existing limit-order validation behavior.
    if (isLimitOrder && nextTradeID == maxTradeID) {
        return false;
    }

    int remainingQuantity = order.quantity;

    while (remainingQuantity > 0) {
        // Find the best opposing price level.
        LimitNode* level =
            (expectedSide == Side::Buy)
                ? book.getBestAsk()
                : book.getBestBid();

        if (level == nullptr) {
            break;
        }

        // Limit orders must not execute beyond their limit price.
        if (isLimitOrder) {
            if (expectedSide == Side::Buy &&
                level->price > order.price) {
                break;
            }

            if (expectedSide == Side::Sell &&
                level->price < order.price) {
                break;
            }
        }

        Order* restingOrder = level->queue.head;

        if (restingOrder == nullptr ||
            restingOrder->quantity <= 0) {
            return false;
        }

        // Market orders check the ID only when a trade is needed.
        if (!isLimitOrder && nextTradeID == maxTradeID) {
            return false;
        }

        const int fillQuantity = std::min(
            remainingQuantity,
            restingOrder->quantity
        );

        // The resting order determines the execution price.
        Trade trade(
            nextTradeID,
            expectedSide == Side::Buy
                ? order.id
                : restingOrder->id,
            expectedSide == Side::Buy
                ? restingOrder->id
                : order.id,
            restingOrder->price,
            fillQuantity
        );

        // Remove a fully filled resting order, or reduce its
        // quantity while preserving its FIFO position.
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

        // Avoid wrapping the counter while a limit order still
        // has quantity to process.
        if (isLimitOrder &&
            remainingQuantity > 0 &&
            nextTradeID == maxTradeID) {
            return false;
        }
    }

    // Only limit orders can leave a remainder in the book.
    if (isLimitOrder && remainingQuantity > 0) {
        Order* remainder = new Order(
            order.id,
            order.price,
            remainingQuantity,
            order.timestamp,
            expectedSide
        );

        if (!book.insertOrder(remainder)) {
            delete remainder;
            return false;
        }
    }

    // Any unfilled market-order quantity is discarded.
    return true;
}