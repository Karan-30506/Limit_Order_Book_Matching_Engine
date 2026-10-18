#include "EventLogger.hpp"
#include <algorithm>
#include <chrono>
#include <stdexcept>

EventLogger::EventLogger(const std::string& name) {
    std::vector<Event> old;
    std::string error;

    if (!readEvents(name, old, error)) {
        std::ifstream check(name);
        if (check.good()) throw std::runtime_error(error);
    }

    for (const auto& e : old) {
        events_.push_back(e);
        lastSequence_ = e.sequenceID;
        lastTimestamp_ = e.timestamp;
    }

    file_.open(name, std::ios::out | std::ios::app | std::ios::binary);
    if (!file_) throw std::runtime_error("Cannot open event log.");

    writer_ = std::thread(&EventLogger::writerLoop, this);
}

EventLogger::~EventLogger() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    condition_.notify_one();
    if (writer_.joinable()) writer_.join();
    file_.flush();
    file_.close();
}

std::uint64_t EventLogger::currentTimestamp() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(
        steady_clock::now().time_since_epoch()).count();
}

void EventLogger::appendEvent(Event e) {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (writeFailed_ || stopping_)
            throw std::runtime_error("Event logger unavailable.");

        e.sequenceID = ++lastSequence_;
        e.timestamp = std::max(currentTimestamp(), lastTimestamp_ + 1);
        lastTimestamp_ = e.timestamp;
        e.checksum = calculateChecksum(e);

        events_.push_back(e);

        writeQueue_.push_back(
            serialize(e) + '\t' + std::to_string(e.checksum));
    }
    condition_.notify_one();
}

void EventLogger::logOrder(const Order& o, OrderType type) {
    Event e;
    e.type = EventType::NEW_ORDER;
    e.orderID = o.id;
    e.side = o.side;
    e.orderType = type;
    e.price = o.price;
    e.quantity = o.quantity;
    e.orderTimestamp = o.timestamp;
    appendEvent(e);
}

void EventLogger::logCancel(const Order& o) {
    Event e;
    e.type = EventType::CANCEL_ORDER;
    e.orderID = o.id;
    e.side = o.side;
    e.price = o.price;
    e.quantity = o.quantity;
    e.orderTimestamp = o.timestamp;
    appendEvent(e);
}

void EventLogger::logModify(const Order& o, int price, int quantity) {
    Event e;
    e.type = EventType::MODIFY_ORDER;
    e.orderID = o.id;
    e.side = o.side;
    e.price = price;
    e.quantity = quantity;
    e.orderTimestamp = o.timestamp;
    appendEvent(e);
}

void EventLogger::logTrade(const Trade& t) {
    Event e;
    e.type = EventType::TRADE;
    e.tradeID = t.tradeID;
    e.buyOrderID = t.buyOrderID;
    e.sellOrderID = t.sellOrderID;
    e.price = t.price;
    e.quantity = t.quantity;
    appendEvent(e);
}

std::vector<Event> EventLogger::getEvents() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_;
}

std::uint64_t EventLogger::getLastSequence() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastSequence_;
}

FeedStatus EventLogger::getNextEvent(
    std::uint64_t& nextSequence,
    Event& event
) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (nextSequence == 0) {
        nextSequence = 1;
    }

    if (nextSequence > lastSequence_) {
        return FeedStatus::NO_EVENT;
    }

    const std::size_t index = static_cast<std::size_t>(nextSequence - 1);
    if (index >= events_.size() ||
        events_[index].sequenceID != nextSequence) {
        return FeedStatus::OVERFLOW;
    }

    event = events_[index];
    ++nextSequence;
    return FeedStatus::EVENT_AVAILABLE;
}
