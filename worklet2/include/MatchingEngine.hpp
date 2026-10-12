#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP

#include <cstddef>
#include <vector>

#include "Book.hpp"
#include "Trade.hpp"
#include "TradeRecorder.hpp"

enum class OrderType {
    Limit,
    Market
};

class MatchingEngine {
private:
    Book& book;
    TradeRecorder tradeRecorder;
    int nextTradeID;

public:
    explicit MatchingEngine(Book& orderBook);

    bool submitOrder(const Order& order, OrderType type);

    const std::vector<Trade>& getTrades() const;

    std::size_t getTradeCount() const;

    void clearTrades();
};

#endif