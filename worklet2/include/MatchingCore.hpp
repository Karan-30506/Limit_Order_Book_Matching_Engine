
#ifndef MATCHING_CORE_HPP
#define MATCHING_CORE_HPP

#include <cstdint>

#include "Book.hpp"
#include "TradeRecorder.hpp"

class MatchingCore {
private:
    Book& book;
    TradeRecorder& tradeRecorder;
    std::uint64_t& nextTradeID;

public:
    MatchingCore(
        Book& orderBook,
        TradeRecorder& recorder,
        std::uint64_t& tradeID
    );

    bool match(
        const Order& order,
        Side expectedSide,
        bool isLimitOrder
    );
};

#endif
