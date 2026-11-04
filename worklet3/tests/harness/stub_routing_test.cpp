// Stub test #1: MatchingEngine in isolation.
// Validator and both matchers are stubs (see stubs/StubMatchers.cpp); the Book
// is real. We assert ONLY what MatchingEngine itself decides: which collaborator
// it calls, in what situations it short-circuits, and that results pass through.
#include <string>
#include <vector>

#include "Book.hpp"
#include "MatchingEngine.hpp"
#include "mini_test.hpp"
#include "stubs/StubControl.hpp"

namespace {
struct RouteCase {
    std::string label;
    OrderType type;
    Side side;
    int price;
    std::string expectedCall;
};
const std::vector<RouteCase> kRoutes = {
    {"limit buy  -> LimitOrderMatcher::matchLimitBuy",   OrderType::Limit,  Side::Buy,  100, "limitBuy:7"},
    {"limit sell -> LimitOrderMatcher::matchLimitSell",  OrderType::Limit,  Side::Sell, 100, "limitSell:7"},
    {"market buy -> MarketOrderMatcher::matchMarketBuy", OrderType::Market, Side::Buy,    0, "marketBuy:7"},
    {"market sell-> MarketOrderMatcher::matchMarketSell",OrderType::Market, Side::Sell,   0, "marketSell:7"},
};
} 

PARAM_TEST(StubRouting, DispatchesToCorrectMatcher, kRoutes, [](const RouteCase& c) {
    gStubMatchers.reset();
    Book book;
    MatchingEngine engine(book);
    Order o(7, c.price, 3, 1, c.side);
    CHECK(engine.submitOrder(o, c.type));
    CHECK_EQ((int)gStubMatchers.calls.size(), 1);
    if (!gStubMatchers.calls.empty()) CHECK_EQ(gStubMatchers.calls[0], c.expectedCall);
});

TEST(StubRouting, InvalidOrderNeverReachesMatcher) {
    gStubMatchers.reset();
    gStubMatchers.validatorResult = false;
    Book book;
    MatchingEngine engine(book);
    Order o(1, 100, 5, 1, Side::Buy);
    CHECK(!engine.submitOrder(o, OrderType::Limit));
    CHECK_EQ(gStubMatchers.validatorCalls, 1);
    CHECK(gStubMatchers.calls.empty());
}

TEST(StubRouting, LimitPriceGuardLivesInEngineNotValidator) {
    gStubMatchers.reset();          // validator stub says "valid"
    Book book;
    MatchingEngine engine(book);
    Order zero(1, 0, 5, 1, Side::Buy), neg(2, -5, 5, 1, Side::Sell);
    CHECK(!engine.submitOrder(zero, OrderType::Limit));
    CHECK(!engine.submitOrder(neg, OrderType::Limit));
    CHECK(gStubMatchers.calls.empty());
    // ...but market orders ignore price entirely.
    CHECK(engine.submitOrder(zero, OrderType::Market));
    CHECK_EQ((int)gStubMatchers.calls.size(), 1);
}

TEST(StubRouting, DuplicateRestingIdRejectedBeforeMatcher) {
    gStubMatchers.reset();
    Book book;
    MatchingEngine engine(book);
    CHECK(book.insertOrder(new Order(5, 100, 10, 1, Side::Buy))); // already resting
    Order dup(5, 101, 1, 2, Side::Sell);
    CHECK(!engine.submitOrder(dup, OrderType::Limit));
    CHECK(gStubMatchers.calls.empty());
}

TEST(StubRouting, MatcherFailureIsPropagated) {
    gStubMatchers.reset();
    gStubMatchers.matcherResult = false;
    Book book;
    MatchingEngine engine(book);
    Order o(1, 100, 5, 1, Side::Buy);
    CHECK(!engine.submitOrder(o, OrderType::Limit));
    CHECK_EQ((int)gStubMatchers.calls.size(), 1);   // it was called, and its "false" came back
}

TEST(StubRouting, StubsRecordNoTrades) {
    gStubMatchers.reset();
    Book book;
    MatchingEngine engine(book);
    Order o(1, 100, 5, 1, Side::Buy);
    engine.submitOrder(o, OrderType::Limit);
    CHECK_EQ((int)engine.getTradeCount(), 0);        // trades come from real matchers only
    engine.clearTrades();
    CHECK_EQ((int)engine.getTrades().size(), 0);
}

int main() {
    std::printf("Stub test: MatchingEngine routing (validator + matchers stubbed)\n\n");
    return mt::runAll();
}
