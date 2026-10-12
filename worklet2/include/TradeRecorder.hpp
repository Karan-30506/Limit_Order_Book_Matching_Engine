#ifndef TRADE_RECORDER_HPP
#define TRADE_RECORDER_HPP

#include <cstddef>
#include <vector>

#include "Trade.hpp"

class TradeRecorder {
private:
    std::vector<Trade> trades;

public:
    TradeRecorder() = default;

    void recordTrade(const Trade& trade);

    const std::vector<Trade>& getTrades() const;

    std::size_t getTradeCount() const;

    void clear();
};

#endif