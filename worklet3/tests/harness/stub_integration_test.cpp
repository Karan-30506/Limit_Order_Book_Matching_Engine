// Stub test #2: EventLoggerIntegration in isolation.
// EventLogger is an in-memory stub (no files, no writer thread) with failure
// injection; Book, MatchingEngine and MarketDataFeed are the real ones.
#include <string>
#include <vector>
#include "Book.hpp"
#include "EventLoggerIntegration.hpp"
#include "MarketDataFeed.hpp"
#include "MatchingEngine.hpp"
#include "mini_test.hpp"
#include "stubs/StubControl.hpp"

namespace {
struct Rig {
    Book book;
    MatchingEngine engine{book};
    EventLogger logger{"unused.log"};   // stub: never touches the disk
    MarketDataFeed feed{64};
    EventLoggerIntegration sys{book, engine, logger, feed};
    std::uint64_t consumer = feed.registerConsumer();
    Rig() { gStubLogger.reset(); }
    std::vector<Event> drain() { return feed.getBatch(consumer, 1000); }
};
} 

TEST(StubIntegration, NoDiskIoHappens) {
    gStubLogger.constructed = 0;
    Rig rig;
    CHECK_EQ(gStubLogger.constructed, 1);
}

TEST(StubIntegration, RestingOrderEmitsOneNewOrderEvent) {
    Rig rig;
    Order o(1, 100, 10, 1, Side::Buy);
    CHECK(rig.sys.submitOrder(o, OrderType::Limit));
    const auto ev = rig.drain();
    CHECK_EQ((int)ev.size(), 1);
    if (ev.size() == 1) {
        CHECK(ev[0].type == EventType::NEW_ORDER);
        CHECK_EQ(ev[0].orderID, 1);
        CHECK_EQ(ev[0].price, 100);
    }
}

TEST(StubIntegration, CrossingOrderEmitsOrderThenTradesInSequence) {
    Rig rig;
    Order a(1, 100, 5, 1, Side::Sell), b(2, 101, 5, 2, Side::Sell), buy(3, 101, 8, 3, Side::Buy);
    CHECK(rig.sys.submitOrder(a, OrderType::Limit));
    CHECK(rig.sys.submitOrder(b, OrderType::Limit));
    CHECK(rig.sys.submitOrder(buy, OrderType::Limit));
    const auto ev = rig.drain();
    // 3 NEW_ORDER + 2 TRADE, strictly increasing sequence ids.
    CHECK_EQ((int)ev.size(), 5);
    for (std::size_t i = 1; i < ev.size(); ++i) CHECK(ev[i].sequenceID == ev[i - 1].sequenceID + 1);
    if (ev.size() == 5) {
        CHECK(ev[3].type == EventType::TRADE);
        CHECK_EQ(ev[3].quantity, 5);
        CHECK(ev[4].type == EventType::TRADE);
        CHECK_EQ(ev[4].quantity, 3);
    }
}

TEST(StubIntegration, RejectedOrderEmitsNothing) {
    Rig rig;
    Order bad(1, 0, 5, 1, Side::Buy);
    CHECK(!rig.sys.submitOrder(bad, OrderType::Limit));
    CHECK_EQ((int)rig.drain().size(), 0);
}

TEST(StubIntegration, CancelAndModifyEmitEventsOnlyOnSuccess) {
    Rig rig;
    Order o(1, 100, 5, 1, Side::Buy);
    rig.sys.submitOrder(o, OrderType::Limit);
    CHECK(!rig.sys.cancelOrder(99));
    CHECK(!rig.sys.modifyOrder(99, 100, 5));
    CHECK(!rig.sys.modifyOrder(1, 0, 5));            // invalid price rejected by Book
    CHECK(rig.sys.modifyOrder(1, 101, 7));
    CHECK(rig.sys.cancelOrder(1));
    const auto ev = rig.drain();
    CHECK_EQ((int)ev.size(), 3);                     // NEW, MODIFY, CANCEL
    if (ev.size() == 3) {
        CHECK(ev[1].type == EventType::MODIFY_ORDER);
        CHECK_EQ(ev[1].price, 101);
        CHECK(ev[2].type == EventType::CANCEL_ORDER);
    }
}

TEST(StubIntegration, LoggerFailureSurfacesAsException) {
    Rig rig;
    gStubLogger.failOnNextLog = true;
    Order o(1, 100, 5, 1, Side::Buy);
    bool threw = false;
    try { rig.sys.submitOrder(o, OrderType::Limit); } catch (const std::exception&) { threw = true; }
    CHECK(threw);
    // Observed behaviour worth knowing: the engine already applied the order,
    // so a logging failure leaves the book ahead of the log.
    CHECK(rig.book.getOrder(1) != nullptr);
    CHECK_EQ((int)rig.drain().size(), 0);
}

int main() {
    std::printf("Stub test: EventLoggerIntegration (EventLogger stubbed in memory)\n\n");
    return mt::runAll();
}
