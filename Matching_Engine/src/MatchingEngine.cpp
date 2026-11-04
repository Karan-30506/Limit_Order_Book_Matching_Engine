#include "MatchingEngine.hpp"

#include "OrderValidator.hpp"

MatchingEngine::MatchingEngine(Book& orderBook)
    : book(orderBook),
      tradeRecorder(),
      nextTradeID(1),
      limitMatcher(book, tradeRecorder, nextTradeID),
      marketMatcher(book, tradeRecorder, nextTradeID) {}

SubmissionResult MatchingEngine::submitOrderDetailed(
    const Order& order,
    OrderType type
) {
    // 1. Validate the order type.
    if (type != OrderType::Limit &&
        type != OrderType::Market) {
        return {false, "UNSUPPORTED_ORDER_TYPE"};
    }

    // 2. Validate the order side.
    if (order.side != Side::Buy &&
        order.side != Side::Sell) {
        return {false, "INVALID_ORDER_SIDE"};
    }

    // 3. Validate common order attributes.
    if (!OrderValidator::isValid(order)) {
        return {false, "INVALID_ORDER_ATTRIBUTES"};
    }

    // 4. Reject duplicate active order IDs.
    if (book.getOrder(order.id) != nullptr) {
        return {false, "DUPLICATE_ACTIVE_ORDER_ID"};
    }

    // 5. Validate the price according to the order type.
    if (type == OrderType::Limit && order.price <= 0) {
        return {false, "INVALID_LIMIT_PRICE"};
    }

    // 6. Route polymorphically to the appropriate matcher.
    BaseOrderMatcher* matcher = nullptr;
    if (type == OrderType::Limit) {
        matcher = &limitMatcher;
    } else {
        matcher = &marketMatcher;
    }

    bool success = (order.side == Side::Buy)
        ? matcher->matchBuy(order)
        : matcher->matchSell(order);

    if (!success) {
        return {false, "MATCHING_OPERATION_FAILED"};
    }

    return {true, ""};
}

// Preserve the existing API for other callers and tests.
bool MatchingEngine::submitOrder(
    const Order& order,
    OrderType type
) {
    return submitOrderDetailed(order, type).success;
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