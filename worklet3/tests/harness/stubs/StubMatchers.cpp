// Link-time stubs for OrderValidator, LimitOrderMatcher and MarketOrderMatcher.
// Linked INSTEAD of the real worklet2 files so MatchingEngine can be tested in
// isolation: we only verify its own routing/guard logic.
#include "LimitOrderMatcher.hpp"
#include "MarketOrderMatcher.hpp"
#include "OrderValidator.hpp"
#include "StubControl.hpp"

StubMatcherControl gStubMatchers;

bool OrderValidator::isValid(const Order&) {
    ++gStubMatchers.validatorCalls;
    return gStubMatchers.validatorResult;
}

LimitOrderMatcher::LimitOrderMatcher(MatchingCore& sharedCore)
    : core(&sharedCore) {}
bool LimitOrderMatcher::matchLimitBuy(const Order& o) {
    gStubMatchers.calls.push_back("limitBuy:" + std::to_string(o.id));
    return gStubMatchers.matcherResult;
}
bool LimitOrderMatcher::matchLimitSell(const Order& o) {
    gStubMatchers.calls.push_back("limitSell:" + std::to_string(o.id));
    return gStubMatchers.matcherResult;
}

MarketOrderMatcher::MarketOrderMatcher(MatchingCore& sharedCore)
    : core(&sharedCore) {}
bool MarketOrderMatcher::matchMarketBuy(const Order& o) {
    gStubMatchers.calls.push_back("marketBuy:" + std::to_string(o.id));
    return gStubMatchers.matcherResult;
}
bool MarketOrderMatcher::matchMarketSell(const Order& o) {
    gStubMatchers.calls.push_back("marketSell:" + std::to_string(o.id));
    return gStubMatchers.matcherResult;
}
