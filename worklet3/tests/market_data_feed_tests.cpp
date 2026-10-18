#include "MarketDataFeed.hpp"

#include <cassert>
#include <iostream>
#include <thread>

Event makeEvent(uint64_t sequence, int orderID) {
    Event event{};

    event.sequenceID = sequence;
    event.orderID = orderID;
    event.type = EventType::NEW_ORDER;
    event.price = 100 + orderID;
    event.quantity = 10;

    return event;
}

void testPublishAndConsume() {
    MarketDataFeed feed(5);

    uint64_t consumer = feed.registerConsumer();

    Event sent = makeEvent(1, 101);
    assert(feed.publish(sent) == PublishStatus::PUBLISHED);

    Event received{};

    assert(
        feed.getNextEvent(consumer, received) ==
        FeedStatus::EVENT_AVAILABLE
    );

    assert(received.sequenceID == 1);
    assert(received.orderID == 101);
    assert(received.price == 201);
}

void testEmptyFeed() {
    MarketDataFeed feed(5);

    uint64_t consumer = feed.registerConsumer();
    Event received{};

    assert(
        feed.getNextEvent(consumer, received) ==
        FeedStatus::NO_EVENT
    );
}

void testFIFOOrder() {
    MarketDataFeed feed(5);

    uint64_t consumer = feed.registerConsumer();

    for (uint64_t i = 1; i <= 3; i++) {
        Event event = makeEvent(i, static_cast<int>(i));

        assert(
            feed.publish(event) == PublishStatus::PUBLISHED
        );
    }

    auto events = feed.getBatch(consumer, 3);

    assert(events.size() == 3);
    assert(events[0].sequenceID == 1);
    assert(events[1].sequenceID == 2);
    assert(events[2].sequenceID == 3);
}

void testMultipleConsumers() {
    MarketDataFeed feed(5);

    uint64_t consumer1 = feed.registerConsumer();
    uint64_t consumer2 = feed.registerConsumer();

    Event event = makeEvent(1, 101);
    feed.publish(event);

    Event first{};
    Event second{};

    assert(
        feed.getNextEvent(consumer1, first) ==
        FeedStatus::EVENT_AVAILABLE
    );

    assert(
        feed.getNextEvent(consumer2, second) ==
        FeedStatus::EVENT_AVAILABLE
    );

    assert(first.sequenceID == second.sequenceID);
}

void testOverflow() {
    MarketDataFeed feed(3);

    uint64_t consumer = feed.registerConsumer();

    for (uint64_t i = 1; i <= 4; i++) {
        Event event = makeEvent(i, static_cast<int>(i));
        const PublishStatus status = feed.publish(event);
        assert(status == (i <= 3
            ? PublishStatus::PUBLISHED
            : PublishStatus::BUFFER_OVERWRITE));
    }

    Event received{};

    assert(
        feed.getNextEvent(consumer, received) ==
        FeedStatus::OVERFLOW
    );

    assert(
        feed.getNextEvent(consumer, received) ==
        FeedStatus::EVENT_AVAILABLE
    );

    assert(received.sequenceID == 2);
}

void testInvalidConsumer() {
    MarketDataFeed feed(5);

    Event received{};

    assert(
        feed.getNextEvent(999, received) ==
        FeedStatus::INVALID_CONSUMER
    );
}

void testConsumerRemoval() {
    MarketDataFeed feed(5);

    uint64_t consumer = feed.registerConsumer();

    assert(feed.getConsumerCount() == 1);
    assert(feed.unregisterConsumer(consumer));
    assert(feed.getConsumerCount() == 0);
    assert(!feed.unregisterConsumer(consumer));
}

void testInvalidSequence() {
    MarketDataFeed feed(5);

    Event event = makeEvent(2, 102);

    assert(
        feed.publish(event) == PublishStatus::INVALID_SEQUENCE
    );

    assert(feed.getLatestSequence() == 0);
}

void testStatistics() {
    MarketDataFeed feed(5);

    uint64_t consumer = feed.registerConsumer();

    Event event = makeEvent(1, 101);
    feed.publish(event);

    Event received{};
    feed.getNextEvent(consumer, received);

    ConsumerStats consumerStats{};
    assert(feed.getConsumerStats(consumer, consumerStats));
    assert(consumerStats.eventsRead == 1);

    FeedStats stats = feed.getFeedStats();

    assert(stats.eventsPublished == 1);
    assert(stats.capacity == 5);
    assert(stats.eventsStored == 1);
    assert(stats.consumers == 1);
}

void testConcurrentPublishers() {
    constexpr uint64_t eventCount = 400;
    constexpr uint64_t publisherCount = 4;
    MarketDataFeed feed(eventCount);
    const uint64_t consumer = feed.registerConsumer();
    std::vector<std::thread> publishers;

    for (uint64_t publisher = 0; publisher < publisherCount; ++publisher) {
        publishers.emplace_back([&, publisher] {
            for (uint64_t sequence = publisher + 1;
                 sequence <= eventCount;
                 sequence += publisherCount) {
                const Event event = makeEvent(
                    sequence,
                    static_cast<int>(sequence)
                );
                PublishStatus status;
                do {
                    status = feed.publish(event);
                    if (status == PublishStatus::INVALID_SEQUENCE) {
                        std::this_thread::yield();
                    }
                } while (status == PublishStatus::INVALID_SEQUENCE);

                assert(status == PublishStatus::PUBLISHED);
            }
        });
    }

    for (std::thread& publisher : publishers) {
        publisher.join();
    }

    const std::vector<Event> received = feed.getBatch(consumer, eventCount);
    assert(received.size() == eventCount);
    for (uint64_t i = 0; i < eventCount; ++i) {
        assert(received[i].sequenceID == i + 1);
        assert(received[i].orderID == static_cast<int>(i + 1));
    }
    assert(feed.getLatestSequence() == eventCount);
}

void runMarketDataFeedTests() {
    testPublishAndConsume();
    testEmptyFeed();
    testFIFOOrder();
    testMultipleConsumers();
    testOverflow();
    testInvalidConsumer();
    testConsumerRemoval();
    testInvalidSequence();
    testStatistics();
    testConcurrentPublishers();

    std::cout << "All Market Data Feed tests passed\n";
}