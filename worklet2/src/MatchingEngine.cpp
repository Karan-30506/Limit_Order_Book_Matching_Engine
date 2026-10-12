
#include "MatchingEngine.hpp"

#include "OrderValidator.hpp"
#include "LimitOrderMatcher.hpp"
#include "MarketOrderMatcher.hpp"

MatchingEngine::MatchingEngine(Book& orderBook)
    : book(orderBook),
      tradeRecorder(),
      nextTradeID(1) {}

bool MatchingEngine::submitOrder(
    const Order& order,
    OrderType type
) {
    // Step 1: Validate common order fields.
    if (!OrderValidator::isValid(order)) {
        return false;
    }

    // Step 2: Reject an order ID that is already active in the book.
    if (book.getOrder(order.id) != nullptr) {
        return false;
    }

    // Step 3: Route limit orders.
    if (type == OrderType::Limit) {
        // Limit orders must have a positive price.
        if (order.price <= 0) {
            return false;
        }

        LimitOrderMatcher matcher(
            book,
            tradeRecorder,
            nextTradeID
        );

        if (order.side == Side::Buy) {
            return matcher.matchLimitBuy(order);
        }

        if (order.side == Side::Sell) {
            return matcher.matchLimitSell(order);
        }

        return false;
    }

    // Step 4: Route market orders.
    if (type == OrderType::Market) {
        MarketOrderMatcher matcher(
            book,
            tradeRecorder,
            nextTradeID
        );

        if (order.side == Side::Buy) {
            return matcher.matchMarketBuy(order);
        }

        if (order.side == Side::Sell) {
            return matcher.matchMarketSell(order);
        }

        return false;
    }

    // Reject an unsupported order type.
    return false;
}

const std::vector<Trade>& MatchingEngine::getTrades() const {
    return tradeRecorder.getTrades();
}

std::size_t MatchingEngine::getTradeCount() const {
    return tradeRecorder.getTradeCount();
}

void MatchingEngine::clearTrades() {
    tradeRecorder.clear();
}