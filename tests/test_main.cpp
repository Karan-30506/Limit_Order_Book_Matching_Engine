#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "AVLTree.hpp"
#include "Book.hpp"
#include "EventLogger.hpp"
#include "EventLoggerIntegration.hpp"
#include "Hashmap.hpp"
#include "MatchingEngine.hpp"
#include "MarketDataFeed.hpp"
#include "OrderParser.hpp"
#include "PerformanceMetrics.hpp"
#include "replayEngine.hpp"

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testHashMapAndAvlBook() {
    HashMap<int, int> map(2);
    map.insert(1, 10);
    map.insert(3, 30);
    map.insert(-1, 40);
    require(map.getSize() == 3 && map.getCapacity() >= 4,
            "hash map did not grow");
    require(map.get(3) == 30 && map.contains(-1),
            "hash map lookup failed after resize");
    map.remove(3);
    require(!map.contains(3), "hash map removal failed");

    AVLTree tree;
    std::vector<LimitNode*> nodes;
    for (int price = 1; price <= 100; ++price) {
        nodes.push_back(tree.insert(price));
    }
    require(tree.getHeightOfTree() < 10, "AVL tree is not balanced");
    require(tree.successor(nodes[49]) == nodes[50],
            "AVL successor traversal failed");
    for (int price = 0; price < 100; ++price) {
        LimitNode* level = nodes[static_cast<std::size_t>(price)];
        tree.remove(level);
    }
    require(tree.isEmpty(), "AVL deletion failed to empty the tree");

    Book book;
    Order first(1, 100, 4, 1, Side::Sell);
    Order second(2, 100, 5, 2, Side::Sell);
    require(book.insertOrder(new Order(first.id, first.price, first.quantity,
                                       first.timestamp, first.side)),
            "book rejected first resting order");
    require(book.insertOrder(new Order(second.id, second.price, second.quantity,
                                       second.timestamp, second.side)),
            "book rejected second resting order");
    require(book.getBestAsk()->queue.head->id == 1 &&
            book.getVolumeAt(100, Side::Sell) == 9,
            "book FIFO or aggregate volume is wrong");
    require(book.modifyOrder(2, 100, 3), "same-price modify failed");
    require(book.getBestAsk()->queue.tail->id == 2 &&
            book.getVolumeAt(100, Side::Sell) == 7,
            "modified order did not lose priority or update volume");
    require(book.cancelOrder(1) && book.getBestAsk()->queue.head->id == 2,
            "book cancellation failed");
}

void testIntegratedMatchingFeedReplayAndPersistence() {
    const std::string eventFile = "lob_tests_events.log";
    std::remove(eventFile.c_str());
    std::vector<Event> loggedEvents;
    {
        Book book;
        MatchingEngine engine(book);
        EventLogger logger(eventFile, false);
        MarketDataFeed feed(32);
        const std::uint64_t consumer = feed.registerConsumer();
        EventLoggerIntegration integration(book, engine, logger, feed);

        Order ask1(201, 100, 2, 1, Side::Sell);
        Order ask2(202, 100, 3, 2, Side::Sell);
        Order buy(301, 101, 4, 3, Side::Buy);
        require(integration.submitOrder(ask1, OrderType::Limit),
                "first ask submission failed");
        require(integration.submitOrder(ask2, OrderType::Limit),
                "second ask submission failed");
        require(integration.submitOrder(buy, OrderType::Limit),
                "crossing buy submission failed");
        require(engine.getTradeCount() == 2 &&
                engine.getTrades()[0].sellOrderID == 201 &&
                engine.getTrades()[1].sellOrderID == 202 &&
                engine.getTrades()[1].quantity == 2,
                "matching did not preserve price-time priority");
        require(book.getOrder(202) != nullptr &&
                book.getOrder(202)->quantity == 1,
                "partial fill did not leave the correct remainder");

        require(integration.modifyOrder(202, 99, 4),
                "integrated modification failed");
        require(integration.cancelOrder(202),
                "integrated cancellation failed");
        Order market(401, 0, 5, 4, Side::Buy);
        require(integration.submitOrder(market, OrderType::Market),
                "market order with no liquidity should be accepted");
        require(book.getOrder(401) == nullptr,
                "unfilled market order incorrectly rested in the book");
        Order invalid(402, 0, 1, 5, Side::Buy);
        const SubmissionResult rejected =
            integration.submitOrderDetailed(invalid, OrderType::Limit);
        require(!rejected.success &&
                rejected.reason == "INVALID_LIMIT_PRICE",
                "engine-level rejection did not return its reason");

        loggedEvents = logger.getEvents();
        logger.flush();
        ReplayEngine replay(loggedEvents);
        const ReplayResult result = replay.replayTo();
        require(result.success, "event replay failed: " + result.message);
        require(replay.compareWith(book).empty(),
                "replay and live book diverged");

        Event event;
        std::size_t readCount = 0;
        while (feed.getNextEvent(consumer, event) == FeedStatus::EVENT_AVAILABLE) {
            ++readCount;
        }
        require(readCount == loggedEvents.size(),
                "market data feed missed events without overflowing");
        ConsumerStats stats{};
        require(feed.getConsumerStats(consumer, stats) &&
                stats.eventsRead == loggedEvents.size(),
                "feed consumer statistics are incorrect");
    }

    std::vector<Event> persisted;
    std::string error;
    require(EventLogger::readEvents(eventFile, persisted, error),
            "persisted event log did not validate: " + error);
    require(persisted.size() == loggedEvents.size(),
            "persisted event count differs from in-memory log");
    ReplayEngine persistedReplay(persisted);
    require(persistedReplay.replayTo().success,
            "persisted event log could not be replayed");
    std::remove(eventFile.c_str());
}

void testParserRejectsWrongJsonTypes() {
    const std::string inputFile = "lob_tests_bad_input.json";
    {
        std::ofstream file(inputFile.c_str());
        file << "[{\"id\":1,\"type\":5,\"side\":\"BUY\","
                "\"price\":100,\"quantity\":1}]";
    }

    std::vector<OrderRequest> requests;
    std::vector<ParserRejection> rejections;
    std::string error;
    const bool parsed = OrderParser::parseFile(
        inputFile, requests, rejections, error);
    std::remove(inputFile.c_str());
    require(parsed, "structurally valid JSON should be parsed");
    require(requests.empty() && rejections.size() == 1 &&
            rejections.front().reason.find("'type' must be a string") !=
                std::string::npos,
            "wrong JSON field type did not become a record rejection");
}

void testReplayDetectsPriorityViolation() {
    std::vector<Event> events(4);
    events[0].sequenceID = 1;
    events[0].type = EventType::NEW_ORDER;
    events[0].orderID = 10;
    events[0].side = Side::Sell;
    events[0].price = 100;
    events[0].quantity = 1;
    events[1].sequenceID = 2;
    events[1].type = EventType::NEW_ORDER;
    events[1].orderID = 11;
    events[1].side = Side::Sell;
    events[1].price = 101;
    events[1].quantity = 1;
    events[2].sequenceID = 3;
    events[2].type = EventType::NEW_ORDER;
    events[2].orderID = 12;
    events[2].side = Side::Buy;
    events[2].orderType = OrderType::Market;
    events[2].quantity = 1;
    events[3].sequenceID = 4;
    events[3].type = EventType::TRADE;
    events[3].tradeID = 1;
    events[3].buyOrderID = 12;
    events[3].sellOrderID = 11;
    events[3].price = 101;
    events[3].quantity = 1;

    ReplayEngine replay(events);
    const ReplayResult result = replay.replayTo();
    require(!result.success &&
            result.message.find("price-time priority") != std::string::npos,
            "replay accepted a trade that bypassed the best ask");

    events.resize(3);
    events[2].orderType = OrderType::Limit;
    events[2].price = 101;
    ReplayEngine crossedLimitWithoutTrade(events);
    const ReplayResult crossedResult = crossedLimitWithoutTrade.replayTo();
    require(!crossedResult.success &&
            crossedResult.message.find("marketable remainder") != std::string::npos,
            "replay accepted an unexecuted marketable limit order");
}

void testMarketDataFeedOverflowRecovery() {
    MarketDataFeed feed(2);
    const std::uint64_t consumer = feed.registerConsumer();
    for (std::uint64_t sequence = 1; sequence <= 3; ++sequence) {
        Event event;
        event.sequenceID = sequence;
        const PublishStatus status = feed.publish(event);
        require(status == (sequence == 3
                    ? PublishStatus::BUFFER_OVERWRITE
                    : PublishStatus::PUBLISHED),
                "feed publish/overwrite status is incorrect");
    }

    Event event;
    require(feed.getNextEvent(consumer, event) == FeedStatus::OVERFLOW,
            "slow feed consumer was not notified of overflow");
    require(feed.recoverConsumer(consumer),
            "feed consumer overflow recovery failed");
    require(feed.getNextEvent(consumer, event) == FeedStatus::EVENT_AVAILABLE &&
            event.sequenceID == 2,
            "feed did not resume at the oldest retained event");
    require(feed.getNextEvent(consumer, event) == FeedStatus::EVENT_AVAILABLE &&
            event.sequenceID == 3,
            "feed did not return the latest retained event");
    ConsumerStats stats{};
    require(feed.getConsumerStats(consumer, stats) &&
            stats.missedEvents == 1 && stats.overflowCount == 1,
            "feed overflow statistics are incorrect");
}

void testPerformanceMetrics() {
    PerformanceMetrics metrics;
    metrics.recordOrder({0, 1, true, "", 100});
    metrics.recordOrder({1, 2, false, "rejected", 200});
    const PerformanceSummary summary = metrics.summarize(1'000'000'000, 5);
    require(summary.orderCount == 2 && summary.acceptedCount == 1 &&
            summary.rejectedCount == 1,
            "performance order counts are incorrect");
    require(summary.meanLatencyNanoseconds == 150 &&
            summary.p50LatencyNanoseconds == 100 &&
            summary.p95LatencyNanoseconds == 200 &&
            summary.p99LatencyNanoseconds == 200 &&
            summary.maxLatencyNanoseconds == 200,
            "latency summary is incorrect");
    require(summary.ordersPerSecond == 2.0 &&
            summary.tradesPerSecond == 5.0,
            "throughput metrics are incorrect");
}

void testWideTradeIDPersistence() {
    const std::string eventFile = "lob_tests_wide_trade_id.log";
    std::remove(eventFile.c_str());
    const std::uint64_t expectedID =
        std::numeric_limits<std::uint64_t>::max();
    {
        EventLogger logger(eventFile, false);
        Trade trade(expectedID, 1, 2, 100, 1);
        logger.logTrade(trade);
        logger.flush();
    }

    std::vector<Event> events;
    std::string error;
    require(EventLogger::readEvents(eventFile, events, error) &&
            events.size() == 1 && events.front().tradeID == expectedID,
            "64-bit trade ID did not survive event-log round trip: " + error);
    std::remove(eventFile.c_str());
}
}

int main() {
    try {
        testHashMapAndAvlBook();
        testIntegratedMatchingFeedReplayAndPersistence();
        testParserRejectsWrongJsonTypes();
        testReplayDetectsPriorityViolation();
        testMarketDataFeedOverflowRecovery();
        testPerformanceMetrics();
        testWideTradeIDPersistence();
        std::cout << "All consolidated matching-engine tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
