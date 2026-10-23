#include "MarketOrderMatcher.hpp"

MarketOrderMatcher::MarketOrderMatcher(
    Book& orderBook,
    TradeRecorder& recorder,
    std::uint64_t& tradeID
)
    : BaseOrderMatcher(orderBook, recorder, tradeID) {}

bool MarketOrderMatcher::validateSpecific(const Order& order) const {
    (void)order;
    // Market orders do not validate price here.
    return true;
}

bool MarketOrderMatcher::canMatchPrice(
    const Order& incoming,
    const LimitNode* level,
    Side side
) const {
    (void)incoming;
    (void)level;
    (void)side;
    // Market orders match at any available price level.
    return true;
}

bool MarketOrderMatcher::handleUnfilled(
    const Order& order,
    int remainingQuantity,
    Side side
) {
    (void)order;
    (void)remainingQuantity;
    (void)side;
    // Any unfilled market-order quantity is discarded (cannot rest in book).
    return true;
}

bool MarketOrderMatcher::matchBuy(const Order& order) {
    return executeMatch(order, Side::Buy);
}

bool MarketOrderMatcher::matchSell(const Order& order) {
    return executeMatch(order, Side::Sell);
}