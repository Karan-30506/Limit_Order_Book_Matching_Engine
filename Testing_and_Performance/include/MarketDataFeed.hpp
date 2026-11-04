#ifndef MARKET_DATA_FEED_HPP
#define MARKET_DATA_FEED_HPP

#include "Event.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

// result of reading an event.
enum class FeedStatus {
    EVENT_AVAILABLE,
    NO_EVENT,
    OVERFLOW,
    INVALID_CONSUMER
};

// result of publishing an event.
enum class PublishStatus {
    PUBLISHED,
    INVALID_SEQUENCE,
    BUFFER_OVERWRITE
};

struct FeedConfig {
    size_t capacity = 1024;
};

// tracks each consumer's reading progress.
struct ConsumerStats {
    uint64_t nextSequence = 1;
    uint64_t eventsRead = 0;
    uint64_t missedEvents = 0;
    uint64_t overflowCount = 0;
    bool overflowed = false;
};

// stores overall feed statistics.
struct FeedStats {
    uint64_t eventsPublished = 0;
    uint64_t totalMissedEvents = 0;
    uint64_t overflowCount = 0;
    uint64_t latestSequence = 0;
    size_t capacity = 0;
    size_t consumers = 0;
    size_t eventsStored = 0;
    double averagePublishTimeNs = 0.0;
    double maxPublishTimeNs = 0.0;
};

class MarketDataFeed {
private:
    std::vector<Event> buffer;
    size_t capacity;

    uint64_t lastSequence;
    uint64_t nextConsumerID;

    uint64_t totalMissedEvents;
    uint64_t totalOverflowCount;
    uint64_t totalPublishTimeNs;
    uint64_t maxPublishTimeNs;

    std::map<uint64_t, ConsumerStats> consumerStats;

    mutable std::mutex feedMutex;

    // finds the oldest available event.
    uint64_t oldestAvailableSequence() const;

    // records events missed due to overflow.
    void recordOverflow(
        ConsumerStats& stats,
        uint64_t newSequence
    );

public:
    // creates a feed with the given configuration.
    explicit MarketDataFeed(const FeedConfig& config);

    // creates a feed with the given capacity.
    explicit MarketDataFeed(size_t bufferCapacity);

    // registers a consumer and returns its ID.
    uint64_t registerConsumer();

    bool unregisterConsumer(uint64_t consumerID);

    // publishes an event to the buffer.
    PublishStatus publish(const Event& event);

    // reads the next event for a consumer.
    FeedStatus getNextEvent(
        uint64_t consumerID,
        Event& event
    );

    // reads multiple events at once.
    std::vector<Event> getBatch(
        uint64_t consumerID,
        size_t maxEvents
    );

    bool recoverConsumer(uint64_t consumerID);

    bool getConsumerStats(
        uint64_t consumerID,
        ConsumerStats& stats
    ) const;

    FeedStats getFeedStats() const;

    uint64_t getLatestSequence() const;

    size_t getCapacity() const;

    size_t getConsumerCount() const;

    void resetStatistics();
};

#endif