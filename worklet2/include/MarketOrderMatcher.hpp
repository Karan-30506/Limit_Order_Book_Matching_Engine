
#ifndef MARKET_ORDER_MATCHER_HPP
#define MARKET_ORDER_MATCHER_HPP

#include <memory>

#include "MatchingCore.hpp"

class MarketOrderMatcher {
private:
    MatchingCore* core;

public:
    // Constructor used by MatchingEngine to share one core.
    explicit MarketOrderMatcher(MatchingCore& sharedCore);

    bool matchMarketBuy(const Order& order);
    bool matchMarketSell(const Order& order);
};

#endif