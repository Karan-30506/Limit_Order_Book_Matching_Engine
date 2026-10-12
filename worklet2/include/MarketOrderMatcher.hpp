#ifndef MARKET_ORDER_MATCHER_HPP
#define MARKET_ORDER_MATCHER_HPP

#include "Book.hpp"
#include "TradeRecorder.hpp"

class MarketOrderMatcher {
private:
    Book& book;
    TradeRecorder& tradeRecorder;
    int& nextTradeID;

public:
    MarketOrderMatcher(
        Book& orderBook,
        TradeRecorder& recorder,
        int& tradeID
    );

    bool matchMarketBuy(const Order& order);
    bool matchMarketSell(const Order& order);
};

#endif