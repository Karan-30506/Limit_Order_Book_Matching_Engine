
#ifndef LIMIT_ORDER_MATCHER_HPP
#define LIMIT_ORDER_MATCHER_HPP

#include <memory>

#include "MatchingCore.hpp"

class LimitOrderMatcher {
private:
    MatchingCore* core;

public:
    // Constructor used by MatchingEngine to share one core.
    explicit LimitOrderMatcher(MatchingCore& sharedCore);

    bool matchLimitBuy(const Order& order);
    bool matchLimitSell(const Order& order);
};

#endif