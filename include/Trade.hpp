#ifndef TRADE_HPP
#define TRADE_HPP

struct Trade {
    int tradeID;
    int buyOrderID;
    int sellOrderID;
    int price;
    int quantity;

    Trade(
        int id,
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