
#include "LimitOrderMatcher.hpp"

LimitOrderMatcher::LimitOrderMatcher(MatchingCore& sharedCore)
    : core(&sharedCore) {}

bool LimitOrderMatcher::matchLimitBuy(const Order& order) {
    return core->match(order, Side::Buy, true);
}

bool LimitOrderMatcher::matchLimitSell(const Order& order) {
    return core->match(order, Side::Sell, true);
}