#ifndef BASE_ORDER_MATCHER_HPP
#define BASE_ORDER_MATCHER_HPP

#include <cstdint>
#include "Book.hpp"
#include "Trade.hpp"
#include "TradeRecorder.hpp"
#include "OrderValidator.hpp"

class BaseOrderMatcher {
protected:
    Book& book;
    TradeRecorder& tradeRecorder;
    std::uint64_t& nextTradeID;

    // Universal matching loop implementing order crossing, FIFO reduction, and trade creation.
    bool executeMatch(const Order& order, Side expectedSide);

    // Hooks customized by concrete matchers (Template Method Pattern)
    virtual bool canMatchPrice(const Order& incoming, const LimitNode* level, Side side) const = 0;
    virtual bool handleUnfilled(const Order& order, int remainingQuantity, Side side) = 0;
    virtual bool validateSpecific(const Order& order) const = 0;

public:
    BaseOrderMatcher(
        Book& orderBook,
        TradeRecorder& recorder,
        std::uint64_t& tradeID
    );

    virtual ~BaseOrderMatcher() = default;

    virtual bool matchBuy(const Order& order) = 0;
    virtual bool matchSell(const Order& order) = 0;

    // Universal entry point
    virtual bool match(const Order& order) {
        return (order.side == Side::Buy) ? matchBuy(order) : matchSell(order);
    }
};

#endif
