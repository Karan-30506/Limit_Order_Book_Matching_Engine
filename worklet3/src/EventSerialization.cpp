
#include "EventLogger.hpp"

#include <charconv>
#include <sstream>
#include <string>
#include <system_error>

namespace {
constexpr std::uint64_t OFFSET = 14695981039346656037ULL;
constexpr std::uint64_t PRIME = 1099511628211ULL;

template <typename Integer>
bool parseInteger(const std::string& field, Integer& value) {
    if (field.empty()) {
        return false;
    }

    const char* begin = field.data();
    const char* end = begin + field.size();
    const auto result = std::from_chars(begin, end, value, 10);
    return result.ec == std::errc() && result.ptr == end;
}
}

std::string EventLogger::serialize(const Event& e) {
    std::ostringstream out;

    out << e.sequenceID << '\t'
        << e.timestamp << '\t'
        << static_cast<int>(e.type) << '\t'
        << e.orderID << '\t'
        << e.tradeID << '\t'
        << e.buyOrderID << '\t'
        << e.sellOrderID << '\t'
        << static_cast<int>(e.side) << '\t'
        << static_cast<int>(e.orderType) << '\t'
        << e.price << '\t'
        << e.quantity << '\t'
        << e.orderTimestamp;

    return out.str();
}

std::uint64_t EventLogger::calculateChecksum(const Event& e) {
    std::uint64_t hash = OFFSET;

    for (unsigned char c : serialize(e))
        hash = (hash ^ c) * PRIME;

    return hash;
}

bool EventLogger::deserialize(const std::string& line, Event& e) {
    std::istringstream in(line);
    std::string f[13];

    for (std::size_t i = 0; i < 12; ++i) {
        if (!std::getline(in, f[i], '\t')) {
            return false;
        }
    }
    if (!std::getline(in, f[12], '\t')) {
        return false;
    }
    std::string extraField;
    if (std::getline(in, extraField, '\t')) {
        return false;
    }

    std::uint64_t sequenceID = 0;
    std::uint64_t timestamp = 0;
    int type = 0;
    int orderID = 0;
    int tradeID = 0;
    int buyOrderID = 0;
    int sellOrderID = 0;
    int side = 0;
    int orderType = 0;
    int price = 0;
    int quantity = 0;
    long long orderTimestamp = 0;
    std::uint64_t checksum = 0;

    if (!parseInteger(f[0], sequenceID) ||
        !parseInteger(f[1], timestamp) ||
        !parseInteger(f[2], type) ||
        !parseInteger(f[3], orderID) ||
        !parseInteger(f[4], tradeID) ||
        !parseInteger(f[5], buyOrderID) ||
        !parseInteger(f[6], sellOrderID) ||
        !parseInteger(f[7], side) ||
        !parseInteger(f[8], orderType) ||
        !parseInteger(f[9], price) ||
        !parseInteger(f[10], quantity) ||
        !parseInteger(f[11], orderTimestamp) ||
        !parseInteger(f[12], checksum)) {
        return false;
    }

    e.sequenceID = sequenceID;
    e.timestamp = timestamp;
    e.type = static_cast<EventType>(type);
    e.orderID = orderID;
    e.tradeID = tradeID;
    e.buyOrderID = buyOrderID;
    e.sellOrderID = sellOrderID;
    e.side = static_cast<Side>(side);
    e.orderType = static_cast<OrderType>(orderType);
    e.price = price;
    e.quantity = quantity;
    e.orderTimestamp = orderTimestamp;
    e.checksum = checksum;

    return e.sequenceID != 0 &&
           type >= 1 && type <= 4 &&
           (e.side == Side::Buy || e.side == Side::Sell) &&
           (e.orderType == OrderType::Limit ||
            e.orderType == OrderType::Market) &&
           e.checksum == calculateChecksum(e);
}
