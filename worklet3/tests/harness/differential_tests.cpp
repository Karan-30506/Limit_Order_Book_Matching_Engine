// Randomized differential test: the real engine and the naive ReferenceModel
// receive identical random command streams (one stream per seed) and must
// agree on every trade, every acceptance decision, best bid/ask, and queues.
#include <random>
#include <string>
#include <vector>

#include "Book.hpp"
#include "MatchingEngine.hpp"
#include "ReferenceModel.hpp"
#include "mini_test.hpp"

namespace {

struct Profile {
    std::string label;
    unsigned seed;
    int steps;
    int priceLo, priceHi;   // narrow range => dense crossing
    int maxQty;
    int pCancel, pModify, pMarket; // percentages; remainder = limit orders
};

const std::vector<Profile> kProfiles = {
    {"tight band, heavy crossing", 1,   3000, 98, 102, 10, 10,  5, 10},
    {"tight band, seed 2",         2,   3000, 98, 102, 10, 10,  5, 10},
    {"wide band, mostly resting",  3,   3000, 50, 150, 20, 10,  5,  5},
    {"cancel heavy",               4,   3000, 95, 105, 10, 35,  5,  5},
    {"modify heavy",               5,   3000, 95, 105, 10, 10, 30,  5},
    {"market heavy",               6,   3000, 95, 105,  5,  5,  5, 40},
    {"qty 1 only",                 7,   3000, 99, 101,  1, 10, 10, 10},
    {"long run",                   8,  20000, 90, 110, 15, 15, 10, 10},
};

std::string describe(const std::string& what, int step) {
    return what + " at step " + std::to_string(step);
}

} // namespace

PARAM_TEST(Differential, EngineMatchesReference, kProfiles, [](const Profile& p) {
    Book book;
    MatchingEngine engine(book);
    ReferenceModel ref;
    std::mt19937 rng(p.seed);
    auto pick = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };

    int nextId = 1;
    std::size_t compared = 0;
    long long ts = 0;

    for (int step = 0; step < p.steps; ++step) {
        const int roll = pick(0, 99);
        const auto ids = ref.liveIds();

        if (roll < p.pCancel && !ids.empty()) {
            const int id = ids[pick(0, (int)ids.size() - 1)];
            CHECK_EQ(book.cancelOrder(id), ref.cancel(id));
        } else if (roll < p.pCancel + p.pModify && !ids.empty()) {
            const int id = ids[pick(0, (int)ids.size() - 1)];
            const int price = pick(p.priceLo, p.priceHi), qty = pick(1, p.maxQty);
            CHECK_EQ(book.modifyOrder(id, price, qty), ref.modify(id, price, qty));
        } else {
            const bool market = roll < p.pCancel + p.pModify + p.pMarket;
            const Side side = pick(0, 1) ? Side::Buy : Side::Sell;
            const int price = market ? 0 : pick(p.priceLo, p.priceHi);
            const int qty = pick(1, p.maxQty);
            const int id = nextId++;
            const OrderType type = market ? OrderType::Market : OrderType::Limit;
            Order o(id, price, qty, ++ts, side);
            CHECK_EQ(engine.submitOrder(o, type), ref.submit(id, side, type, price, qty));
        }

        // Modify can leave the book crossed (Book does no matching); the reference
        // model does the same, so a crossed state is "equal" - but the next incoming
        // order then matches against it in both. Compare trades as we go:
        const auto& et = engine.getTrades();
        const auto& rt = ref.trades();
        if (et.size() != rt.size()) { mt::fail(__FILE__, __LINE__, describe("trade count diverged (engine=" + std::to_string(et.size()) + ", ref=" + std::to_string(rt.size()) + ")", step)); return; }
        for (; compared < et.size(); ++compared) {
            const RefTrade got{et[compared].buyOrderID, et[compared].sellOrderID, et[compared].price, et[compared].quantity};
            if (!(got == rt[compared])) { mt::fail(__FILE__, __LINE__, describe("trade #" + std::to_string(compared) + " differs", step)); return; }
        }

        // Cheap invariants every step.
        LimitNode* bb = book.getBestBid();
        LimitNode* ba = book.getBestAsk();
        CHECK_EQ(bb != nullptr, ref.hasBid());
        CHECK_EQ(ba != nullptr, ref.hasAsk());
        if (bb && ref.hasBid()) CHECK_EQ(bb->price, ref.bestBid());
        if (ba && ref.hasAsk()) CHECK_EQ(ba->price, ref.bestAsk());
        if (mt::state().failures) { std::fprintf(stderr, "    (stopped at step %d)\n", step); return; }
    }

    // Final deep comparison: every live order, FIFO queue and volume.
    for (int id : ref.liveIds()) {
        CHECK(book.getOrder(id) != nullptr);
    }
    for (int price = p.priceLo; price <= p.priceHi; ++price) {
        for (Side s : {Side::Buy, Side::Sell}) {
            CHECK_EQ(book.getVolumeAt(price, s), ref.volumeAt(price, s));
            std::vector<int> got;
            LimitNode* lvl = (s == Side::Buy ? book.buyLimitMap : book.sellLimitMap).get(price);
            if (lvl) for (Order* o = lvl->queue.head; o; o = o->next) got.push_back(o->id);
            CHECK(got == ref.queueAt(price, s));
        }
    }
});
