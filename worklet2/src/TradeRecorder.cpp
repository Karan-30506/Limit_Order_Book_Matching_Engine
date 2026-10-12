#include "TradeRecorder.hpp"

void TradeRecorder::recordTrade(const Trade& trade) {
    trades.push_back(trade);
}

const std::vector<Trade>& TradeRecorder::getTrades() const {
    return trades;
}

std::size_t TradeRecorder::getTradeCount() const {
    return trades.size();
}

void TradeRecorder::clear() {
    trades.clear();
}