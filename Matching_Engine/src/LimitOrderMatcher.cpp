#include "LimitOrderMatcher.hpp"

LimitOrderMatcher::LimitOrderMatcher(
    Book& orderBook,
    TradeRecorder& recorder,
    std::uint64_t& tradeID
)
    : BaseOrderMatcher(orderBook, recorder, tradeID) {}

bool LimitOrderMatcher::validateSpecific(const Order& order) const {
    return order.price > 0;
}

bool LimitOrderMatcher::canMatchPrice(
    const Order& incoming,
    const LimitNode* level,
    Side side
) const {
    if (side == Side::Buy) {
        return level->price <= incoming.price;
    } else {
        return level->price >= incoming.price;
    }
}

bool LimitOrderMatcher::handleUnfilled(
    const Order& order,
    int remainingQuantity,
    Side side
) {
    Order* remainder = new Order(
        order.id,
        order.price,
        remainingQuantity,
        order.timestamp,
        side
    );

    if (!book.insertOrder(remainder)) {
        delete remainder;
        return false;
    }

    return true;
}

bool LimitOrderMatcher::matchBuy(const Order& order) {
    return executeMatch(order, Side::Buy);
}

bool LimitOrderMatcher::matchSell(const Order& order) {
    return executeMatch(order, Side::Sell);
}