#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>

#include "Book.hpp"
#include "Trade.hpp"
#include "TradeRecorder.hpp"
#include "BaseOrderMatcher.hpp"
#include "LimitOrderMatcher.hpp"
#include "MarketOrderMatcher.hpp"

enum class OrderType {
    Limit,
    Market
};

struct SubmissionResult {
    bool success;
    std::string reason;
};

class MatchingEngine {
private:
    Book& book;
    TradeRecorder tradeRecorder;
    std::uint64_t nextTradeID;

    // Polymorphic matchers inheriting from BaseOrderMatcher
    LimitOrderMatcher limitMatcher;
    MarketOrderMatcher marketMatcher;

public:
    explicit MatchingEngine(Book& orderBook);

    bool submitOrder(const Order& order, OrderType type);

    SubmissionResult submitOrderDetailed(const Order& order, OrderType type);

    const std::vector<Trade>& getTrades() const;

    std::size_t getTradeCount() const;

    void clearTrades();
};

#endif