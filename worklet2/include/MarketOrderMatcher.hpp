#ifndef MARKET_ORDER_MATCHER_HPP
#define MARKET_ORDER_MATCHER_HPP

#include "BaseOrderMatcher.hpp"

class MarketOrderMatcher : public BaseOrderMatcher {
protected:
    // Hooks customizing BaseOrderMatcher for Market Orders
    bool canMatchPrice(const Order& incoming, const LimitNode* level, Side side) const override;
    bool handleUnfilled(const Order& order, int remainingQuantity, Side side) override;
    bool validateSpecific(const Order& order) const override;

public:
    MarketOrderMatcher(
        Book& orderBook,
        TradeRecorder& recorder,
        std::uint64_t& tradeID
    );

    bool matchBuy(const Order& order) override;
    bool matchSell(const Order& order) override;

    // Backwards-compatible convenience methods
    bool matchMarketBuy(const Order& order) { return matchBuy(order); }
    bool matchMarketSell(const Order& order) { return matchSell(order); }
};

#endif