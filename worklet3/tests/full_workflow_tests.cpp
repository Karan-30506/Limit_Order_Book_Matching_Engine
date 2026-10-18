#include "EventLoggerIntegration.hpp"
#include "replayEngine.hpp"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

void runFullWorkflowTests() {
    const std::string logFile = "full_workflow_test.tsv";
    std::remove(logFile.c_str());

    Book book;
    MatchingEngine engine(book);
    MarketDataFeed feed(16);
    const std::uint64_t consumer = feed.registerConsumer();
    std::vector<Event> loggedEvents;

    {
        EventLogger logger(logFile);
        EventLoggerIntegration integration(book, engine, logger, feed);

        const Order restingSell(500, 100, 10, 1, Side::Sell);
        const Order incomingBuy(501, 105, 6, 2, Side::Buy);

        assert(integration.submitOrder(restingSell, OrderType::Limit));
        assert(integration.submitOrder(incomingBuy, OrderType::Limit));

        assert(engine.getTradeCount() == 1);
        const Trade& trade = engine.getTrades().front();
        assert(trade.tradeID == 1);
        assert(trade.buyOrderID == 501);
        assert(trade.sellOrderID == 500);
        assert(trade.price == 100);
        assert(trade.quantity == 6);

        const Order* remainingSell = book.getOrder(500);
        assert(remainingSell != nullptr);
        assert(remainingSell->quantity == 4);
        assert(book.getOrder(501) == nullptr);

        assert(integration.cancelOrder(500));
        assert(book.getOrder(500) == nullptr);
        assert(book.getBestAsk() == nullptr);

        loggedEvents = logger.getEvents();
        assert(loggedEvents.size() == 4);
        assert(loggedEvents[0].type == EventType::NEW_ORDER);
        assert(loggedEvents[0].orderID == 500);
        assert(loggedEvents[1].type == EventType::NEW_ORDER);
        assert(loggedEvents[1].orderID == 501);
        assert(loggedEvents[2].type == EventType::TRADE);
        assert(loggedEvents[2].tradeID == 1);
        assert(loggedEvents[2].quantity == 6);
        assert(loggedEvents[3].type == EventType::CANCEL_ORDER);
        assert(loggedEvents[3].orderID == 500);
        assert(loggedEvents[3].quantity == 4);
    }

    for (const Event& expected : loggedEvents) {
        Event received{};
        assert(feed.getNextEvent(consumer, received) ==
               FeedStatus::EVENT_AVAILABLE);
        assert(received.sequenceID == expected.sequenceID);
        assert(received.type == expected.type);
        assert(received.orderID == expected.orderID);
        assert(received.tradeID == expected.tradeID);
        assert(received.buyOrderID == expected.buyOrderID);
        assert(received.sellOrderID == expected.sellOrderID);
        assert(received.price == expected.price);
        assert(received.quantity == expected.quantity);
    }
    Event noMoreEvents{};
    assert(feed.getNextEvent(consumer, noMoreEvents) == FeedStatus::NO_EVENT);

    std::vector<Event> persistedEvents;
    std::string error;
    assert(EventLogger::readEvents(logFile, persistedEvents, error));
    assert(error.empty());
    assert(persistedEvents.size() == loggedEvents.size());

    ReplayEngine replay(persistedEvents);
    const ReplayResult replayResult = replay.replayTo();
    assert(replayResult.success);
    assert(replay.compareWith(book).empty());
    const ReplaySnapshot snapshot = replay.snapshot();
    assert(snapshot.orders.empty());
    assert(snapshot.levels.empty());

    std::remove(logFile.c_str());
    std::cout << "Full Worklet 1/2/3 workflow tests passed.\n";
}