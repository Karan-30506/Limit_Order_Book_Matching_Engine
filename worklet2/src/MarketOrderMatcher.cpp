#include "MarketOrderMatcher.hpp"

MarketOrderMatcher::MarketOrderMatcher(MatchingCore& sharedCore)
    : core(&sharedCore) {}

bool MarketOrderMatcher::matchMarketBuy(const Order& order) {
    return core->match(order, Side::Buy, false);
}

bool MarketOrderMatcher::matchMarketSell(const Order& order) {
    return core->match(order, Side::Sell, false);
}