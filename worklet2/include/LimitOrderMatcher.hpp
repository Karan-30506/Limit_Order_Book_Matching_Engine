#ifndef LIMIT_ORDER_MATCHER_HPP
#define LIMIT_ORDER_MATCHER_HPP

#include "Book.hpp"
#include "TradeRecorder.hpp"

class LimitOrderMatcher {
private:
    Book& book;
    TradeRecorder& tradeRecorder;
    int& nextTradeID;

public:
    LimitOrderMatcher(
        Book& orderBook,
        TradeRecorder& recorder,
        int& tradeID
    );

    bool matchLimitBuy(const Order& order);
    bool matchLimitSell(const Order& order);
};

#endif
