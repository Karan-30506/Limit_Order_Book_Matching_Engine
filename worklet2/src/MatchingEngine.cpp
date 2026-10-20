
#include "MatchingEngine.hpp"

#include "OrderValidator.hpp"

MatchingEngine::MatchingEngine(Book& orderBook)
    : book(orderBook),
      tradeRecorder(),
      nextTradeID(1),
      matchingCore(book, tradeRecorder, nextTradeID),
      limitMatcher(matchingCore),
      marketMatcher(matchingCore) {}

bool MatchingEngine::submitOrder(
    const Order& order,
    OrderType type
) {
    // Step 1: Validate common order fields.
    if (!OrderValidator::isValid(order)) {
        return false;
    }

    // Step 2: Reject an order ID already active in the book.
    if (book.getOrder(order.id) != nullptr) {
        return false;
    }

    // Step 3: Route to the appropriate matcher.
    if (type == OrderType::Limit) {
        if (order.price <= 0) {
            return false;
        }

        if (order.side == Side::Buy) {
            return limitMatcher.matchLimitBuy(order);
        }

        if (order.side == Side::Sell) {
            return limitMatcher.matchLimitSell(order);
        }

        return false;
    }

    if (type == OrderType::Market) {
        if (order.side == Side::Buy) {
            return marketMatcher.matchMarketBuy(order);
        }

        if (order.side == Side::Sell) {
            return marketMatcher.matchMarketSell(order);
        }

        return false;
    }

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