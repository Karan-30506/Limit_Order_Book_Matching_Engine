// Link-time stub for EventLogger: pure in-memory, no file, no writer thread,
// plus failure injection. Linked INSTEAD of worklet3/src/EventLogger.cpp.
#include <stdexcept>

#include "EventLogger.hpp"
#include "StubControl.hpp"

StubLoggerControl gStubLogger;

EventLogger::EventLogger(const std::string&) { ++gStubLogger.constructed; }
EventLogger::~EventLogger() {}

static void maybeFail() {
    if (gStubLogger.failOnNextLog) {
        gStubLogger.failOnNextLog = false;
        throw std::runtime_error("stub logger: injected failure");
    }
}

void EventLogger::appendEvent(Event e) {
    maybeFail();
    e.sequenceID = ++lastSequence_;
    e.timestamp = ++lastTimestamp_;
    events_.push_back(e);
}

void EventLogger::logOrder(const Order& o, OrderType t) {
    Event e; e.type = EventType::NEW_ORDER; e.orderID = o.id; e.side = o.side;
    e.orderType = t; e.price = o.price; e.quantity = o.quantity; e.orderTimestamp = o.timestamp;
    appendEvent(e);
}
void EventLogger::logCancel(const Order& o) {
    Event e; e.type = EventType::CANCEL_ORDER; e.orderID = o.id; e.side = o.side;
    e.price = o.price; e.quantity = o.quantity; e.orderTimestamp = o.timestamp;
    appendEvent(e);
}
void EventLogger::logModify(const Order& o, int p, int q) {
    Event e; e.type = EventType::MODIFY_ORDER; e.orderID = o.id; e.side = o.side;
    e.price = p; e.quantity = q; e.orderTimestamp = o.timestamp;
    appendEvent(e);
}
void EventLogger::logTrade(const Trade& t) {
    Event e; e.type = EventType::TRADE; e.tradeID = t.tradeID; e.buyOrderID = t.buyOrderID;
    e.sellOrderID = t.sellOrderID; e.price = t.price; e.quantity = t.quantity;
    appendEvent(e);
}
std::vector<Event> EventLogger::getEvents() const { return events_; }
std::uint64_t EventLogger::getLastSequence() const { return lastSequence_; }
FeedStatus EventLogger::getNextEvent(std::uint64_t& next, Event& e) {
    if (next == 0) next = 1;
    if (next > lastSequence_) return FeedStatus::NO_EVENT;
    e = events_[next - 1];
    ++next;
    return FeedStatus::EVENT_AVAILABLE;
}
bool EventLogger::readEvents(const std::string&, std::vector<Event>& out, std::string&) {
    out.clear();
    return true;
}
