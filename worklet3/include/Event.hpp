#ifndef EVENT_HPP
#define EVENT_HPP

#include <cstdint>

#include "Order.hpp"
#include "MatchingEngine.hpp"

enum class EventType {
    NEW_ORDER = 1,
    CANCEL_ORDER = 2,
    MODIFY_ORDER = 3,
    TRADE = 4
};

struct Event {
    std::uint64_t sequenceID = 0;
    std::uint64_t timestamp = 0;

    EventType type = EventType::NEW_ORDER;

    int orderID = 0;
    int tradeID = 0;
    int buyOrderID = 0;
    int sellOrderID = 0;

    Side side = Side::Buy;
    OrderType orderType = OrderType::Limit;

    int price = 0;
    int quantity = 0;
    long long orderTimestamp = 0;

    std::uint64_t checksum = 0;
};

#endif