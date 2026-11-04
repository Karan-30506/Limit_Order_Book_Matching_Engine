#ifndef EVENT_LOGGER_HPP
#define EVENT_LOGGER_HPP

#include "MarketDataFeed.hpp"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Event.hpp"
#include "Trade.hpp"

class EventLogger {
public:
    explicit EventLogger(const std::string& fileName);

    ~EventLogger();

    EventLogger(const EventLogger&) = delete;
    EventLogger& operator=(const EventLogger&) = delete;

    void logOrder(const Order& order, OrderType type);
    void logCancel(const Order& order);
    void logModify(const Order& order, int newPrice, int newQuantity);
    void logTrade(const Trade& trade);

    std::vector<Event> getEvents() const;
    std::uint64_t getLastSequence() const;

    FeedStatus getNextEvent(
        std::uint64_t& nextSequence,
        Event& event
    );

    static bool readEvents(
        const std::string& fileName,
        std::vector<Event>& events,
        std::string& error
    );

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;

    std::vector<Event> events_;
    std::deque<std::string> writeQueue_;

    std::ofstream file_;
    std::thread writer_;

    std::uint64_t lastSequence_ = 0;
    std::uint64_t lastTimestamp_ = 0;

    bool stopping_ = false;
    bool writeFailed_ = false;

    void appendEvent(Event event);
    void writerLoop();

    static std::uint64_t currentTimestamp();
    static std::uint64_t calculateChecksum(const Event& event);
    static std::string serialize(const Event& event);
    static bool deserialize(
        const std::string& line,
        Event& event
    );
};

#endif