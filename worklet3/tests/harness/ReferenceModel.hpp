#ifndef WORKLET3_REFERENCE_MODEL_HPP
#define WORKLET3_REFERENCE_MODEL_HPP

#include <map>
#include <vector>

#include "MatchingEngine.hpp"

struct RefTrade {
    int buyOrderID;
    int sellOrderID;
    int price;
    int quantity;

    bool operator==(const RefTrade& other) const {
        return buyOrderID == other.buyOrderID &&
               sellOrderID == other.sellOrderID &&
               price == other.price &&
               quantity == other.quantity;
    }
};

class ReferenceModel {
public:
    std::vector<int> liveIds() const;
    bool cancel(int id);
    bool modify(int id, int price, int quantity);
    bool submit(int id, Side side, OrderType type, int price, int quantity);

    const std::vector<RefTrade>& trades() const;
    bool hasBid() const;
    bool hasAsk() const;
    int bestBid() const;
    int bestAsk() const;
    int volumeAt(int price, Side side) const;
    std::vector<int> queueAt(int price, Side side) const;

private:
    struct OrderState {
        Side side;
        int price;
        int quantity;
    };

    std::map<int, OrderState> orders_;
    std::map<int, std::vector<int>> bids_;
    std::map<int, std::vector<int>> asks_;
    std::vector<RefTrade> trades_;

    std::map<int, std::vector<int>>& levels(Side side);
    const std::map<int, std::vector<int>>& levels(Side side) const;
    void removeFromLevel(int id, const OrderState& order);
};

#endif
