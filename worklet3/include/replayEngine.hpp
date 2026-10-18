#ifndef REPLAY_ENGINE_HPP
#define REPLAY_ENGINE_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Event.hpp"

class Book;

struct ReplayOrder {
    int id = 0;
    Side side = Side::Buy;
    int price = 0;
    int quantity = 0;
    long long timestamp = 0;
    OrderType orderType = OrderType::Limit;
};

struct ReplayLevel {
    Side side = Side::Buy;
    int price = 0;
    int totalQuantity = 0;
    std::vector<int> orderIDs;
};

struct ReplaySnapshot {
    std::vector<ReplayOrder> orders;
    std::vector<ReplayLevel> levels;
    bool hasBestBid = false;
    int bestBid = 0;
    bool hasBestAsk = false;
    int bestAsk = 0;
};

struct ReplayResult {
    bool success = false;
    std::uint64_t sequence = 0;
    std::string message;
};

class ReplayBook {
public:
    ReplayBook();
    ~ReplayBook();

    void reset();
    ReplayResult apply(const Event& event);
    ReplayResult finalize();
    bool hasPendingOrder() const;
    int pendingOrderID() const;

    ReplaySnapshot snapshot() const;
    std::vector<std::string> compareWith(Book& liveBook) const;

private:
    struct State;
    std::unique_ptr<State> state_;

    ReplayResult finishPending();
    ReplayResult fail(const Event& event, const std::string& message) const;
};

class ReplayEngine {
public:
    explicit ReplayEngine(const std::vector<Event>& events);

    template <typename EventSource>
    explicit ReplayEngine(const EventSource& source)
        : events_(source.getEvents()) {}

    ~ReplayEngine();

    ReplayResult replayTo(std::uint64_t sequence = UINT64_MAX);
    void reset();

    const ReplayBook& book() const;
    ReplaySnapshot snapshot() const;
    std::vector<std::string> compareWith(Book& liveBook) const;

private:
    std::vector<Event> events_;
    ReplayBook book_;
};

#endif
