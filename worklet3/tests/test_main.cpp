#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cmath>

#include "Book.hpp"
#include "MatchingEngine.hpp"
#include "EventLogger.hpp"
#include "EventLoggerIntegration.hpp"
#include "MarketDataFeed.hpp"
#include "Benchmark.hpp"
#include "replay_tests.hpp"
#include "replayEngine.hpp"

void testBenchmark() {
    using worklet3::calculateLatencyStats;

    std::cout << "\nbenchmark tests:\n";

    const auto stats =
        calculateLatencyStats({1.0, 2.0, 3.0, 4.0, 100.0});

    std::cout << "Sample count: " << stats.sampleCount << '\n';
    std::cout << "Average latency: " << stats.averageNs << " ns\n";
    std::cout << "P50 latency: " << stats.p50Ns << " ns\n";
    std::cout << "P95 latency: " << stats.p95Ns << " ns\n";
    std::cout << "P99 latency: " << stats.p99Ns << " ns\n";
    std::cout << "Maximum latency: " << stats.maxNs << " ns\n";

    assert(stats.sampleCount == 5);
    assert(std::fabs(stats.averageNs - 22.0) < 0.0001);
    assert(stats.p50Ns == 3.0);
    assert(stats.p95Ns == 100.0);
    assert(stats.maxNs == 100.0);

    worklet3::BenchmarkConfig config;
    config.operations = 5;
    config.warmupOperations = 0;

    const auto result = worklet3::runBenchmark(
        "test",
        config,
        [](std::size_t i) { return i % 2 == 0; },
        []() { return true; }
    );

    std::cout << "\nBenchmark run:\n";
    std::cout << "Operations attempted: " << result.attempted << '\n';
    std::cout << "Operations succeeded: " << result.succeeded << '\n';
    std::cout << "Operations failed: " << result.failed << '\n';
    std::cout << "Correctness: "
              << (result.correctnessPassed ? "PASSED" : "FAILED")
              << '\n';

    assert(result.attempted == 5);
    assert(result.succeeded == 3);
    assert(result.failed == 2);
    assert(result.correctnessPassed);

    std::cout << "Benchmark tests passed.\n";
}

void testMarketDataFeed(const std::vector<Event>& events) {
    std::cout << "\nmarket data feed tests:\n";

    MarketDataFeed feed(16);
    const std::uint64_t consumer = feed.registerConsumer();

    for (const Event& event : events) {
        assert(feed.publish(event) == PublishStatus::PUBLISHED);

        std::cout << "Published event:"
                  << " sequence=" << event.sequenceID
                  << ", type=" << static_cast<int>(event.type)
                  << ", orderID=" << event.orderID
                  << ", tradeID=" << event.tradeID
                  << ", price=" << event.price
                  << ", quantity=" << event.quantity << '\n';
    }

    for (const Event& expected : events) {
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

        std::cout << "Consumed event:"
                  << " sequence=" << received.sequenceID
                  << ", orderID=" << received.orderID
                  << ", price=" << received.price
                  << ", quantity=" << received.quantity << '\n';
    }

    const FeedStats stats = feed.getFeedStats();

    std::cout << "\nFeed statistics:\n";
    std::cout << "Events published: " << stats.eventsPublished << '\n';
    std::cout << "Buffer capacity: " << stats.capacity << '\n';
    std::cout << "Events stored: " << stats.eventsStored << '\n';
    std::cout << "Consumers: " << stats.consumers << '\n';

    assert(stats.eventsPublished == events.size());
    assert(stats.capacity == 16);
    assert(stats.consumers == 1);

    std::cout << "Market Data Feed tests passed.\n";
}

void testEventLoggerAndMatching() {
    std::cout << "\nevent logger and matching tests:\n";

    const std::string logFile = "event_log_test.txt";
    std::remove(logFile.c_str());

    Book book;
    MatchingEngine engine(book);

    {
        EventLogger logger(logFile);
        EventLoggerIntegration integration(book, engine, logger);

        Order buyOrder(1, 100, 10, 1, Side::Buy);
        Order sellOrder(2, 100, 5, 2, Side::Sell);

        assert(integration.submitOrder(buyOrder, OrderType::Limit));
        std::cout << "Buy order submitted: ID=1, price=100, quantity=10\n";

        assert(integration.submitOrder(sellOrder, OrderType::Limit));
        std::cout << "Sell order submitted: ID=2, price=100, quantity=5\n";

        const std::vector<Event> events = logger.getEvents();

        std::cout << "\nIn-memory events: " << events.size() << '\n';

        for (const Event& event : events) {
            std::cout << "Sequence=" << event.sequenceID
                      << ", type=" << static_cast<int>(event.type)
                      << ", orderID=" << event.orderID
                      << ", tradeID=" << event.tradeID
                      << ", buyOrderID=" << event.buyOrderID
                      << ", sellOrderID=" << event.sellOrderID
                      << ", price=" << event.price
                      << ", quantity=" << event.quantity << '\n';
        }

        assert(events.size() == 3);
        assert(events[0].sequenceID == 1);
        assert(events[1].sequenceID == 2);
        assert(events[2].sequenceID == 3);
        assert(events[2].type == EventType::TRADE);
        assert(events[2].quantity == 5);

        testMarketDataFeed(events);
    }

    std::vector<Event> diskEvents;
    std::string error;

    assert(EventLogger::readEvents(logFile, diskEvents, error));
    assert(error.empty());
    assert(diskEvents.size() == 3);
    assert(diskEvents[2].type == EventType::TRADE);
    assert(diskEvents[2].quantity == 5);

    std::cout << "\nEvents recovered from disk: "
              << diskEvents.size() << '\n';

    for (const Event& event : diskEvents) {
        std::cout << "Sequence=" << event.sequenceID
                  << ", type=" << static_cast<int>(event.type)
                  << ", orderID=" << event.orderID
                  << ", price=" << event.price
                  << ", quantity=" << event.quantity << '\n';
    }

    std::cout << "Event Logger and disk persistence tests passed.\n";
    std::remove(logFile.c_str());
}

int main() {
    
    std::cout << " all tests:\n";
    
    testBenchmark();

    std::cout << "\nRunning benchmark utility tests:\n";
    runBenchmarkTests();


    // Existing Event Logger, matching, and Market Data Feed tests
    testEventLoggerAndMatching();

    // Full Market Data Feed test suite
    std::cout << "\nRunning Market Data Feed tests:\n";
    runMarketDataFeedTests();

    // Existing Replay Engine tests
    std::cout << "\nreplay engine tests:\n";
    runReplayEngineTests();

    // Existing Event Logger Replay Integration tests
    std::cout << "\nreplay integration tests:\n";
    runEventLoggerReplayIntegrationTests();

    std::cout << "\nfull Worklet 1/2/3 workflow tests:\n";
    runFullWorkflowTests();


    return 0;
}
