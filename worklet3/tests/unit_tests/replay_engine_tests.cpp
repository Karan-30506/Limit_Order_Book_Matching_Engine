
#include "replayEngine.hpp"
#include "Book.hpp"

#include <iostream>
#include <cassert>
#include <string>
#include <vector>

namespace {
Event newOrder(std::uint64_t sequence, int id, Side side, OrderType type, int price, int quantity) {
    Event event;
    event.sequenceID = sequence;
    event.type = EventType::NEW_ORDER;
    event.orderID = id;
    event.side = side;
    event.orderType = type;
    event.price = price;
    event.quantity = quantity;
    event.orderTimestamp = static_cast<long long>(sequence);
    return event;
}

Event trade(std::uint64_t sequence, int id, int buyID, int sellID, int price, int quantity) {
    Event event;
    event.sequenceID = sequence;
    event.type = EventType::TRADE;
    event.tradeID = id;
    event.buyOrderID = buyID;
    event.sellOrderID = sellID;
    event.price = price;
    event.quantity = quantity;
    return event;
}

Event cancel(std::uint64_t sequence, int id) {
    Event event;
    event.sequenceID = sequence;
    event.type = EventType::CANCEL_ORDER;
    event.orderID = id;
    return event;
}

Event modify(std::uint64_t sequence, int id, Side side, int price, int quantity) {
    Event event;
    event.sequenceID = sequence;
    event.type = EventType::MODIFY_ORDER;
    event.orderID = id;
    event.side = side;
    event.price = price;
    event.quantity = quantity;
    event.orderTimestamp = static_cast<long long>(sequence);
    return event;
}

bool contains(const std::vector<std::string>& messages, const std::string& text) {
    for (const auto& message : messages) {
        if (message.find(text) != std::string::npos) return true;
    }
    return false;
}

bool sameSnapshot(const ReplaySnapshot& left, const ReplaySnapshot& right) {
    if (left.hasBestBid != right.hasBestBid || left.bestBid != right.bestBid ||
        left.hasBestAsk != right.hasBestAsk || left.bestAsk != right.bestAsk ||
        left.orders.size() != right.orders.size() ||
        left.levels.size() != right.levels.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.orders.size(); ++i) {
        const ReplayOrder& a = left.orders[i];
        const ReplayOrder& b = right.orders[i];
        if (a.id != b.id || a.side != b.side || a.price != b.price ||
            a.quantity != b.quantity || a.timestamp != b.timestamp ||
            a.orderType != b.orderType) {
            return false;
        }
    }
    for (std::size_t i = 0; i < left.levels.size(); ++i) {
        const ReplayLevel& a = left.levels[i];
        const ReplayLevel& b = right.levels[i];
        if (a.side != b.side || a.price != b.price ||
            a.totalQuantity != b.totalQuantity || a.orderIDs != b.orderIDs) {
            return false;
        }
    }
    return true;
}
}

void runReplayEngineTests() {
    ReplayEngine empty{std::vector<Event>()};
    assert(empty.replayTo().success);
    assert(empty.snapshot().orders.empty());

    std::vector<Event> simpleEvents;
    simpleEvents.push_back(newOrder(1, 1, Side::Buy, OrderType::Limit, 100, 5));
    ReplayEngine simple(simpleEvents);
    assert(simple.replayTo().success);
    assert(simple.snapshot().orders.size() == 1);
    assert(simple.snapshot().bestBid == 100);

    std::vector<Event> events;
    events.push_back(newOrder(1, 10, Side::Sell, OrderType::Limit, 100, 5));
    events.push_back(newOrder(2, 11, Side::Sell, OrderType::Limit, 100, 5));
    events.push_back(newOrder(3, 20, Side::Buy, OrderType::Limit, 105, 12));
    events.push_back(trade(4, 1, 20, 10, 100, 5));
    events.push_back(trade(5, 2, 20, 11, 100, 5));

    ReplayEngine engine(events);
    ReplayResult result = engine.replayTo();
    assert(result.success);
    ReplaySnapshot snapshot = engine.snapshot();
    assert(snapshot.orders.size() == 1);
    assert(snapshot.orders[0].id == 20 && snapshot.orders[0].quantity == 2);
    assert(snapshot.levels.size() == 1 && snapshot.levels[0].orderIDs[0] == 20);

    result = engine.replayTo(2);
    assert(result.success);
    snapshot = engine.snapshot();
    assert(snapshot.orders.size() == 2);
    assert(snapshot.levels[0].orderIDs[0] == 10);
    assert(snapshot.levels[0].orderIDs[1] == 11);

    result = engine.replayTo(99);
    assert(result.success && result.sequence == 5);
    assert(result.message.find("beyond") != std::string::npos);

    engine.reset();
    result = engine.replayTo();
    assert(result.success && engine.snapshot().orders[0].quantity == 2);

    std::vector<Event> marketEvents;
    marketEvents.push_back(newOrder(1, 30, Side::Sell, OrderType::Limit, 101, 3));
    marketEvents.push_back(newOrder(2, 31, Side::Buy, OrderType::Market, 0, 7));
    marketEvents.push_back(trade(3, 3, 31, 30, 101, 3));
    ReplayEngine market(marketEvents);
    assert(market.replayTo().success && market.snapshot().orders.empty());

    std::vector<Event> levels;
    levels.push_back(newOrder(1, 40, Side::Buy, OrderType::Limit, 99, 2));
    levels.push_back(newOrder(2, 41, Side::Buy, OrderType::Limit, 101, 3));
    levels.push_back(newOrder(3, 42, Side::Sell, OrderType::Limit, 110, 4));
    ReplayEngine levelEngine(levels);
    assert(levelEngine.replayTo().success);
    snapshot = levelEngine.snapshot();
    assert(snapshot.bestBid == 101 && snapshot.bestAsk == 110);
    assert(snapshot.levels.size() == 3);
    assert(snapshot.levels[0].price == 101 && snapshot.levels[1].price == 99);

    std::vector<Event> cancelEvents;
    cancelEvents.push_back(newOrder(1, 50, Side::Sell, OrderType::Limit, 120, 6));
    cancelEvents.push_back(cancel(2, 50));
    ReplayEngine cancelEngine(cancelEvents);
    assert(cancelEngine.replayTo().success && cancelEngine.snapshot().orders.empty());

    std::vector<Event> modifyEvents;
    modifyEvents.push_back(newOrder(1, 60, Side::Buy, OrderType::Limit, 100, 2));
    modifyEvents.push_back(newOrder(2, 61, Side::Buy, OrderType::Limit, 100, 2));
    modifyEvents.push_back(modify(3, 60, Side::Buy, 100, 7));
    ReplayEngine modifyEngine(modifyEvents);
    assert(modifyEngine.replayTo().success);
    snapshot = modifyEngine.snapshot();
    assert(snapshot.levels[0].orderIDs.size() == 2);
    assert(snapshot.levels[0].orderIDs[0] == 61 && snapshot.levels[0].orderIDs[1] == 60);

    std::vector<Event> partialEvents;
    partialEvents.push_back(newOrder(1, 70, Side::Sell, OrderType::Limit, 100, 5));
    partialEvents.push_back(newOrder(2, 71, Side::Buy, OrderType::Limit, 105, 5));
    partialEvents.push_back(trade(3, 10, 71, 70, 100, 2));
    ReplayEngine partialEngine(partialEvents);
    assert(partialEngine.replayTo().success);
    snapshot = partialEngine.snapshot();
    assert(snapshot.orders.size() == 2);
    assert(snapshot.orders[0].quantity == 3 || snapshot.orders[1].quantity == 3);

    std::vector<Event> fullEvents;
    fullEvents.push_back(newOrder(1, 80, Side::Sell, OrderType::Limit, 100, 5));
    fullEvents.push_back(newOrder(2, 81, Side::Buy, OrderType::Limit, 105, 5));
    fullEvents.push_back(trade(3, 11, 81, 80, 100, 5));
    ReplayEngine fullEngine(fullEvents);
    assert(fullEngine.replayTo().success && fullEngine.snapshot().orders.empty());

    ReplayBook atomicBook;
    assert(atomicBook.apply(newOrder(1, 140, Side::Buy, OrderType::Limit, 100, 5)).success);
    assert(atomicBook.apply(newOrder(2, 141, Side::Sell, OrderType::Limit, 110, 3)).success);
    const ReplaySnapshot beforeInvalidCancel = atomicBook.snapshot();
    assert(atomicBook.hasPendingOrder() && atomicBook.pendingOrderID() == 141);
    assert(!atomicBook.apply(cancel(3, 999)).success);
    assert(sameSnapshot(beforeInvalidCancel, atomicBook.snapshot()));
    assert(atomicBook.hasPendingOrder() && atomicBook.pendingOrderID() == 141);

    const ReplaySnapshot beforeInvalidModify = atomicBook.snapshot();
    assert(!atomicBook.apply(modify(4, 999, Side::Buy, 101, 2)).success);
    assert(sameSnapshot(beforeInvalidModify, atomicBook.snapshot()));
    assert(atomicBook.hasPendingOrder() && atomicBook.pendingOrderID() == 141);

    assert(atomicBook.apply(cancel(5, 140)).success);
    assert(!atomicBook.hasPendingOrder());
    assert(atomicBook.apply(modify(6, 141, Side::Sell, 111, 4)).success);
    assert(atomicBook.finalize().success);
    assert(atomicBook.snapshot().orders.size() == 1);
    assert(atomicBook.snapshot().orders[0].id == 141);
    assert(atomicBook.apply(cancel(7, 141)).success);
    assert(atomicBook.snapshot().orders.empty());

    ReplayBook pendingCancelBook;
    assert(pendingCancelBook.apply(
        newOrder(1, 142, Side::Buy, OrderType::Limit, 99, 3)
    ).success);
    assert(pendingCancelBook.hasPendingOrder());
    assert(pendingCancelBook.apply(cancel(2, 142)).success);
    assert(!pendingCancelBook.hasPendingOrder());
    assert(pendingCancelBook.snapshot().orders.empty());

    ReplayBook pendingMarketBook;
    assert(pendingMarketBook.apply(
        newOrder(1, 147, Side::Buy, OrderType::Market, 0, 3)
    ).success);
    const ReplaySnapshot beforePendingMarketCancel =
        pendingMarketBook.snapshot();
    assert(!pendingMarketBook.apply(cancel(2, 147)).success);
    assert(sameSnapshot(
        beforePendingMarketCancel,
        pendingMarketBook.snapshot()
    ));
    assert(pendingMarketBook.hasPendingOrder());
    assert(pendingMarketBook.pendingOrderID() == 147);
    assert(!pendingMarketBook.apply(
        modify(3, 147, Side::Buy, 100, 2)
    ).success);
    assert(sameSnapshot(
        beforePendingMarketCancel,
        pendingMarketBook.snapshot()
    ));
    assert(pendingMarketBook.hasPendingOrder());
    assert(pendingMarketBook.pendingOrderID() == 147);

    ReplayBook filledPendingBook;
    assert(filledPendingBook.apply(
        newOrder(1, 150, Side::Sell, OrderType::Limit, 100, 2)
    ).success);
    assert(filledPendingBook.apply(
        newOrder(2, 151, Side::Buy, OrderType::Limit, 105, 2)
    ).success);
    assert(filledPendingBook.apply(
        trade(3, 16, 151, 150, 100, 2)
    ).success);
    const ReplaySnapshot beforeFilledPendingChange =
        filledPendingBook.snapshot();
    assert(filledPendingBook.hasPendingOrder());
    assert(filledPendingBook.pendingOrderID() == 151);
    assert(!filledPendingBook.apply(cancel(4, 151)).success);
    assert(!filledPendingBook.apply(
        modify(5, 151, Side::Buy, 105, 1)
    ).success);
    assert(sameSnapshot(
        beforeFilledPendingChange,
        filledPendingBook.snapshot()
    ));
    assert(filledPendingBook.hasPendingOrder());
    assert(filledPendingBook.pendingOrderID() == 151);

    ReplayBook crossingTradeBook;
    assert(crossingTradeBook.apply(
        newOrder(1, 148, Side::Sell, OrderType::Limit, 100, 3)
    ).success);
    assert(crossingTradeBook.apply(
        newOrder(2, 149, Side::Buy, OrderType::Limit, 99, 2)
    ).success);
    const ReplaySnapshot beforeCrossingTrade =
        crossingTradeBook.snapshot();
    assert(!crossingTradeBook.apply(
        trade(3, 15, 149, 148, 100, 1)
    ).success);
    assert(sameSnapshot(
        beforeCrossingTrade,
        crossingTradeBook.snapshot()
    ));
    assert(crossingTradeBook.hasPendingOrder());
    assert(crossingTradeBook.pendingOrderID() == 149);
    assert(crossingTradeBook.finalize().success);
    const ReplaySnapshot afterCrossingTrade =
        crossingTradeBook.snapshot();
    assert(afterCrossingTrade.orders.size() == 2);
    assert(afterCrossingTrade.orders[0].id == 148);
    assert(afterCrossingTrade.orders[0].quantity == 3);
    assert(afterCrossingTrade.orders[1].id == 149);
    assert(afterCrossingTrade.orders[1].quantity == 2);

    ReplayBook duplicateOrderBook;
    assert(duplicateOrderBook.apply(
        newOrder(1, 143, Side::Buy, OrderType::Limit, 99, 3)
    ).success);
    const ReplaySnapshot beforeDuplicateOrder = duplicateOrderBook.snapshot();
    assert(!duplicateOrderBook.apply(
        newOrder(2, 143, Side::Sell, OrderType::Limit, 101, 2)
    ).success);
    assert(sameSnapshot(beforeDuplicateOrder, duplicateOrderBook.snapshot()));
    assert(duplicateOrderBook.hasPendingOrder());
    assert(duplicateOrderBook.pendingOrderID() == 143);

    ReplayBook invalidFieldsBook;
    const ReplaySnapshot beforeInvalidFields = invalidFieldsBook.snapshot();
    assert(!invalidFieldsBook.apply(
        newOrder(1, 144, Side::Buy, OrderType::Limit, 100, 0)
    ).success);
    assert(sameSnapshot(beforeInvalidFields, invalidFieldsBook.snapshot()));
    assert(!invalidFieldsBook.hasPendingOrder());

    ReplayBook invalidModifyFieldsBook;
    assert(invalidModifyFieldsBook.apply(
        newOrder(1, 145, Side::Buy, OrderType::Limit, 99, 3)
    ).success);
    assert(invalidModifyFieldsBook.apply(
        newOrder(2, 146, Side::Sell, OrderType::Limit, 110, 2)
    ).success);
    const ReplaySnapshot beforeInvalidModifyFields =
        invalidModifyFieldsBook.snapshot();
    assert(!invalidModifyFieldsBook.apply(
        modify(3, 145, Side::Buy, 100, 0)
    ).success);
    assert(sameSnapshot(
        beforeInvalidModifyFields,
        invalidModifyFieldsBook.snapshot()
    ));
    assert(invalidModifyFieldsBook.hasPendingOrder());
    assert(invalidModifyFieldsBook.pendingOrderID() == 146);

    std::vector<Event> invalidEvents;
    invalidEvents.push_back(newOrder(1, 90, Side::Sell, OrderType::Limit, 100, 5));
    invalidEvents.push_back(newOrder(3, 91, Side::Buy, OrderType::Limit, 105, 5));
    ReplayEngine invalidEngine(invalidEvents);
    result = invalidEngine.replayTo(2);
    assert(!result.success && invalidEngine.snapshot().orders.empty());

    std::vector<Event> duplicateEvents;
    duplicateEvents.push_back(newOrder(1, 100, Side::Buy, OrderType::Limit, 100, 5));
    duplicateEvents.push_back(newOrder(1, 101, Side::Buy, OrderType::Limit, 101, 5));
    ReplayEngine duplicateEngine(duplicateEvents);
    assert(!duplicateEngine.replayTo().success && duplicateEngine.snapshot().orders.empty());

    std::vector<Event> boundaryEvents;
    boundaryEvents.push_back(newOrder(1, 110, Side::Sell, OrderType::Limit, 100, 5));
    boundaryEvents.push_back(newOrder(2, 111, Side::Buy, OrderType::Limit, 105, 5));
    boundaryEvents.push_back(trade(3, 12, 111, 110, 100, 5));
    ReplayEngine boundaryEngine(boundaryEvents);
    result = boundaryEngine.replayTo(2);
    assert(!result.success && boundaryEngine.snapshot().orders.empty());

    std::vector<Event> badTradeEvents;
    badTradeEvents.push_back(newOrder(1, 120, Side::Sell, OrderType::Limit, 100, 5));
    badTradeEvents.push_back(newOrder(2, 121, Side::Buy, OrderType::Limit, 105, 5));
    badTradeEvents.push_back(trade(3, 13, 121, 120, 100, 6));
    ReplayEngine badTradeEngine(badTradeEvents);
    assert(!badTradeEngine.replayTo().success && badTradeEngine.snapshot().orders.empty());

    std::vector<Event> duplicateTradeEvents;
    duplicateTradeEvents.push_back(newOrder(1, 130, Side::Sell, OrderType::Limit, 100, 5));
    duplicateTradeEvents.push_back(newOrder(2, 131, Side::Buy, OrderType::Limit, 105, 10));
    duplicateTradeEvents.push_back(trade(3, 14, 131, 130, 100, 5));
    duplicateTradeEvents.push_back(trade(4, 14, 131, 130, 100, 1));
    ReplayEngine duplicateTradeEngine(duplicateTradeEvents);
    assert(!duplicateTradeEngine.replayTo().success && duplicateTradeEngine.snapshot().orders.empty());

    Book liveBook;
    assert(liveBook.insertOrder(new Order(200, 100, 5, 1, Side::Buy)));
    ReplayEngine comparisonEngine(std::vector<Event>{newOrder(1, 200, Side::Buy, OrderType::Limit, 100, 5)});
    assert(comparisonEngine.replayTo().success);
    assert(comparisonEngine.compareWith(liveBook).empty());
    assert(liveBook.modifyOrder(200, 100, 4));
    assert(contains(comparisonEngine.compareWith(liveBook), "active order 200 differs"));

    Book fifoBook;
    assert(fifoBook.insertOrder(new Order(202, 100, 1, 1, Side::Buy)));
    assert(fifoBook.insertOrder(new Order(201, 100, 1, 2, Side::Buy)));
    ReplayEngine fifoEngine(std::vector<Event>{
        newOrder(1, 201, Side::Buy, OrderType::Limit, 100, 1),
        newOrder(2, 202, Side::Buy, OrderType::Limit, 100, 1)
    });
    assert(fifoEngine.replayTo().success);
    assert(contains(fifoEngine.compareWith(fifoBook), "FIFO order differs"));

        std::cout << "Replay Engine tests passed.\n";

}
