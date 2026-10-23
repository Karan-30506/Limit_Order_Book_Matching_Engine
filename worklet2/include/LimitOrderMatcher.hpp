#ifndef LIMIT_ORDER_MATCHER_HPP
#define LIMIT_ORDER_MATCHER_HPP

#include "BaseOrderMatcher.hpp"

class LimitOrderMatcher : public BaseOrderMatcher {
protected:
    // Hooks customizing BaseOrderMatcher for Limit Orders
    bool canMatchPrice(const Order& incoming, const LimitNode* level, Side side) const override;
    bool handleUnfilled(const Order& order, int remainingQuantity, Side side) override;
    bool validateSpecific(const Order& order) const override;

public:
    LimitOrderMatcher(
        Book& orderBook,
        TradeRecorder& recorder,
        std::uint64_t& tradeID
    );

    bool matchBuy(const Order& order) override;
    bool matchSell(const Order& order) override;

    // Backwards-compatible convenience methods
    bool matchLimitBuy(const Order& order) { return matchBuy(order); }
    bool matchLimitSell(const Order& order) { return matchSell(order); }
};

#endif