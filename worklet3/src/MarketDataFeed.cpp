#include "MarketDataFeed.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>

// creates a feed using the given configuration.
MarketDataFeed::MarketDataFeed(const FeedConfig& config)
    : MarketDataFeed(config.capacity) {
}

// initializes the buffer and statistics.
MarketDataFeed::MarketDataFeed(size_t bufferCapacity)
    : buffer(bufferCapacity),
      capacity(bufferCapacity),
      lastSequence(0),
      nextConsumerID(1),
      totalMissedEvents(0),
      totalOverflowCount(0),
      totalPublishTimeNs(0),
      maxPublishTimeNs(0) {

    if (capacity == 0) {
        throw std::invalid_argument(
            "Buffer capacity cannot be zero"
        );
    }
}

// finds the oldest event still available in the buffer.
uint64_t MarketDataFeed::oldestAvailableSequence() const {
    if (lastSequence == 0) {
        return 1;
    }

    uint64_t retained = std::min(
        lastSequence,
        static_cast<uint64_t>(capacity)
    );

    return lastSequence - retained + 1;
}

// updates statistics when a consumer misses events.
void MarketDataFeed::recordOverflow(
    ConsumerStats& stats,
    uint64_t newSequence) {

    if (newSequence <= stats.nextSequence) {
        return;
    }

    uint64_t missed = newSequence - stats.nextSequence;

    stats.missedEvents += missed;
    stats.overflowCount++;

    totalMissedEvents += missed;
    totalOverflowCount++;

    stats.nextSequence = newSequence;
    stats.overflowed = true;
}

// registers a consumer and returns its ID.
uint64_t MarketDataFeed::registerConsumer() {
    std::lock_guard<std::mutex> lock(feedMutex);

    if (nextConsumerID == 0) {
        throw std::overflow_error("Consumer ID limit reached");
    }

    uint64_t id = nextConsumerID++;

    ConsumerStats stats;
    stats.nextSequence = lastSequence + 1;

    consumerStats.emplace(id, stats);

    return id;
}

// removes a registered consumer.
bool MarketDataFeed::unregisterConsumer(uint64_t consumerID) {
    std::lock_guard<std::mutex> lock(feedMutex);

    return consumerStats.erase(consumerID) > 0;
}

// publishes an event and tracks overflow and publishing time.
PublishStatus MarketDataFeed::publish(const Event& event) {
    auto start = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(feedMutex);

    if (event.sequenceID == 0 ||
        event.sequenceID != lastSequence + 1) {
        return PublishStatus::INVALID_SEQUENCE;
    }

    bool overwritten = false;

    if (event.sequenceID > capacity) {
        uint64_t overwrittenSequence =
            event.sequenceID - capacity;

        for (auto& item : consumerStats) {
            ConsumerStats& stats = item.second;

            if (stats.nextSequence <= overwrittenSequence) {
                recordOverflow(
                    stats,
                    overwrittenSequence + 1
                );

                overwritten = true;
            }
        }
    }

    size_t index = static_cast<size_t>(
        (event.sequenceID - 1) % capacity
    );

    buffer[index] = event;
    lastSequence = event.sequenceID;

    auto end = std::chrono::steady_clock::now();

    uint64_t elapsed = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            end - start
        ).count()
    );

    totalPublishTimeNs += elapsed;

    if (elapsed > maxPublishTimeNs) {
        maxPublishTimeNs = elapsed;
    }

    return overwritten
        ? PublishStatus::BUFFER_OVERWRITE
        : PublishStatus::PUBLISHED;
}

// reads the next event for a consumer.
FeedStatus MarketDataFeed::getNextEvent(
    uint64_t consumerID,
    Event& event) {

    std::lock_guard<std::mutex> lock(feedMutex);

    auto it = consumerStats.find(consumerID);

    if (it == consumerStats.end()) {
        return FeedStatus::INVALID_CONSUMER;
    }

    ConsumerStats& stats = it->second;

    if (stats.overflowed) {
        stats.overflowed = false;
        return FeedStatus::OVERFLOW;
    }

    if (stats.nextSequence > lastSequence) {
        return FeedStatus::NO_EVENT;
    }

    uint64_t oldest = oldestAvailableSequence();

    if (stats.nextSequence < oldest) {
        recordOverflow(stats, oldest);
        stats.overflowed = false;
        return FeedStatus::OVERFLOW;
    }

    size_t index = static_cast<size_t>(
        (stats.nextSequence - 1) % capacity
    );

    if (buffer[index].sequenceID != stats.nextSequence) {
        recordOverflow(stats, oldest);
        stats.overflowed = false;
        return FeedStatus::OVERFLOW;
    }

    event = buffer[index];

    stats.nextSequence++;
    stats.eventsRead++;

    return FeedStatus::EVENT_AVAILABLE;
}

// reads multiple events for a consumer.
std::vector<Event> MarketDataFeed::getBatch(
    uint64_t consumerID,
    size_t maxEvents) {

    std::vector<Event> events;
    events.reserve(maxEvents);

    for (size_t i = 0; i < maxEvents; i++) {
        Event event{};

        FeedStatus status = getNextEvent(
            consumerID,
            event
        );

        if (status != FeedStatus::EVENT_AVAILABLE) {
            break;
        }

        events.push_back(event);
    }

    return events;
}

// resets a consumer's position after overflow.
bool MarketDataFeed::recoverConsumer(uint64_t consumerID) {
    std::lock_guard<std::mutex> lock(feedMutex);

    auto it = consumerStats.find(consumerID);

    if (it == consumerStats.end()) {
        return false;
    }

    ConsumerStats& stats = it->second;

    stats.nextSequence = oldestAvailableSequence();
    stats.overflowed = false;

    return true;
}

// retrieves statistics for a consumer.
bool MarketDataFeed::getConsumerStats(
    uint64_t consumerID,
    ConsumerStats& stats) const {

    std::lock_guard<std::mutex> lock(feedMutex);

    auto it = consumerStats.find(consumerID);

    if (it == consumerStats.end()) {
        return false;
    }

    stats = it->second;

    return true;
}

// retrieves overall feed statistics.
FeedStats MarketDataFeed::getFeedStats() const {
    std::lock_guard<std::mutex> lock(feedMutex);

    FeedStats stats{};

    stats.eventsPublished = lastSequence;
    stats.latestSequence = lastSequence;
    stats.capacity = capacity;
    stats.consumers = consumerStats.size();

    stats.eventsStored = static_cast<size_t>(
        std::min(
            lastSequence,
            static_cast<uint64_t>(capacity)
        )
    );

    stats.totalMissedEvents = totalMissedEvents;
    stats.overflowCount = totalOverflowCount;
    stats.maxPublishTimeNs =
        static_cast<double>(maxPublishTimeNs);

    if (lastSequence > 0) {
        stats.averagePublishTimeNs =
            static_cast<double>(totalPublishTimeNs) /
            static_cast<double>(lastSequence);
    }

    return stats;
}

// returns the latest published sequence number.
uint64_t MarketDataFeed::getLatestSequence() const {
    std::lock_guard<std::mutex> lock(feedMutex);

    return lastSequence;
}

// returns the buffer capacity.
size_t MarketDataFeed::getCapacity() const {
    return capacity;
}

// returns the number of registered consumers.
size_t MarketDataFeed::getConsumerCount() const {
    std::lock_guard<std::mutex> lock(feedMutex);

    return consumerStats.size();
}

// resets the feed's statistics.
void MarketDataFeed::resetStatistics() {
    std::lock_guard<std::mutex> lock(feedMutex);

    totalMissedEvents = 0;
    totalOverflowCount = 0;
    totalPublishTimeNs = 0;
    maxPublishTimeNs = 0;

    for (auto& item : consumerStats) {
        item.second.eventsRead = 0;
        item.second.missedEvents = 0;
        item.second.overflowCount = 0;
    }
}