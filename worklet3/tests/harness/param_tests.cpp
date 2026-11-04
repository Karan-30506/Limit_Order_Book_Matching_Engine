// Parametrized (table-driven) tests: each row is one case with a label.
#include <climits>
#include <string>
#include <vector>

#include "Book.hpp"
#include "MatchingEngine.hpp"
#include "OrderValidator.hpp"
#include "mini_test.hpp"

namespace {

struct ValidatorCase {
    std::string label;
    int id, price, qty;
    Side side;
    bool expected;
};

const std::vector<ValidatorCase> kValidatorCases = {
    {"normal buy",          1, 100, 10, Side::Buy,  true},
    {"normal sell",         1, 100, 10, Side::Sell, true},
    {"id zero",             0, 100, 10, Side::Buy,  false},
    {"id negative",        -1, 100, 10, Side::Buy,  false},
    {"qty zero",            1, 100,  0, Side::Buy,  false},
    {"qty negative",        1, 100, -5, Side::Sell, false},
    {"qty one (min)",       1, 100,  1, Side::Buy,  true},
    {"qty INT_MAX",         1, 100, INT_MAX, Side::Buy, true},
    {"id INT_MAX",    INT_MAX, 100, 10, Side::Sell, true},
    {"price ignored here",  1,  -1, 10, Side::Buy,  true},  // price is checked by MatchingEngine
};

struct AcceptCase {
    std::string label;
    OrderType type;
    Side side;
    int id, price, qty;
    bool expected;
};

const std::vector<AcceptCase> kAcceptCases = {
    {"limit buy ok",          OrderType::Limit,  Side::Buy,  1, 100, 5, true},
    {"limit sell ok",         OrderType::Limit,  Side::Sell, 1, 100, 5, true},
    {"limit price 0",         OrderType::Limit,  Side::Buy,  1,   0, 5, false},
    {"limit price negative",  OrderType::Limit,  Side::Sell, 1,  -9, 5, false},
    {"limit qty 0",           OrderType::Limit,  Side::Buy,  1, 100, 0, false},
    {"limit id 0",            OrderType::Limit,  Side::Buy,  0, 100, 5, false},
    {"market buy price 0 ok", OrderType::Market, Side::Buy,  1,   0, 5, true},
    {"market sell ok",        OrderType::Market, Side::Sell, 1,   0, 5, true},
    {"market qty 0",          OrderType::Market, Side::Buy,  1,   0, 0, false},
    {"market id negative",    OrderType::Market, Side::Sell, -4,  0, 5, false},
};

struct CrossCase {
    std::string label;
    int restingAsk, buyLimit, buyQty, restingQty;
    int expectedTrades, expectedFilled, expectedBidVolume, expectedAskVolume;
};

const std::vector<CrossCase> kCrossCases = {
    {"buy below ask: no trade",          100,  99, 5, 5, 0, 0, 5, 5},
    {"buy equals ask: trade at ask",     100, 100, 5, 5, 1, 5, 0, 0},
    {"buy above ask: trade at ask",      100, 105, 5, 5, 1, 5, 0, 0},
    {"buy smaller than resting",         100, 100, 2, 5, 1, 2, 0, 3},
    {"buy larger than resting",          100, 100, 8, 5, 1, 5, 3, 0},
    {"1-lot against 1-lot",              100, 100, 1, 1, 1, 1, 0, 0},
};

} 

PARAM_TEST(Validator, Table, kValidatorCases, [](const ValidatorCase& c) {
    Order o(c.id, c.price, c.qty, 1, c.side);
    CHECK_EQ(OrderValidator::isValid(o), c.expected);
});

PARAM_TEST(Engine, AcceptanceTable, kAcceptCases, [](const AcceptCase& c) {
    Book book;
    MatchingEngine engine(book);
    Order o(c.id, c.price, c.qty, 1, c.side);
    CHECK_EQ(engine.submitOrder(o, c.type), c.expected);
});

PARAM_TEST(Engine, CrossingTable, kCrossCases, [](const CrossCase& c) {
    Book book;
    MatchingEngine engine(book);
    Order ask(1, c.restingAsk, c.restingQty, 1, Side::Sell);
    CHECK(engine.submitOrder(ask, OrderType::Limit));
    Order buy(2, c.buyLimit, c.buyQty, 2, Side::Buy);
    CHECK(engine.submitOrder(buy, OrderType::Limit));

    CHECK_EQ((int)engine.getTradeCount(), c.expectedTrades);
    if (c.expectedTrades) {
        CHECK_EQ(engine.getTrades()[0].price, c.restingAsk); // resting order's price
        CHECK_EQ(engine.getTrades()[0].quantity, c.expectedFilled);
    }
    CHECK_EQ(book.getVolumeAt(c.buyLimit, Side::Buy), c.expectedBidVolume);
    CHECK_EQ(book.getVolumeAt(c.restingAsk, Side::Sell), c.expectedAskVolume);
});

// ---------- Mirror image for sells (symmetry check over a price grid) ----------
struct GridCase { std::string label; int bid, sellLimit; bool shouldTrade; };

static std::vector<GridCase> makeGrid() {
    std::vector<GridCase> v;
    for (int bid : {90, 100, 110})
        for (int sell : {89, 90, 91, 100, 109, 110, 111})
            v.push_back({"bid=" + std::to_string(bid) + " sell=" + std::to_string(sell), bid, sell, sell <= bid});
    return v;
}
static const std::vector<GridCase> kGrid = makeGrid();

PARAM_TEST(Engine, SellCrossingGrid, kGrid, [](const GridCase& c) {
    Book book;
    MatchingEngine engine(book);
    Order bid(1, c.bid, 10, 1, Side::Buy);
    CHECK(engine.submitOrder(bid, OrderType::Limit));
    Order sell(2, c.sellLimit, 10, 2, Side::Sell);
    CHECK(engine.submitOrder(sell, OrderType::Limit));
    CHECK_EQ(engine.getTradeCount() == 1, c.shouldTrade);
    if (c.shouldTrade) CHECK_EQ(engine.getTrades()[0].price, c.bid);
});
