
#ifndef TRADE_HPP
#define TRADE_HPP

#include <cstdint>

struct Trade {
    std::uint64_t tradeID;
    int buyOrderID;
    int sellOrderID;
    int price;
    int quantity;

    Trade(
        std::uint64_t id,
        int buyID,
        int sellID,
        int executionPrice,
        int tradedQuantity
    )
        : tradeID(id),
          buyOrderID(buyID),
          sellOrderID(sellID),
          price(executionPrice),
          quantity(tradedQuantity) {}
};

#endif