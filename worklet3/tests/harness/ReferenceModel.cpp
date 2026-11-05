// This simple order book is used to compare results with the real engine.
#include "ReferenceModel.hpp"

#include <algorithm>

std::map<int, std::vector<int>>& ReferenceModel::levels(Side side) {
    return side == Side::Buy ? bids_ : asks_;
}

const std::map<int, std::vector<int>>& ReferenceModel::levels(Side side) const {
    return side == Side::Buy ? bids_ : asks_;
}

void ReferenceModel::removeFromLevel(int id, const OrderState& order) {
    auto& sideLevels = levels(order.side);
    auto level = sideLevels.find(order.price);
    if (level == sideLevels.end()) {
        return;
    }
    auto& queue = level->second;
    queue.erase(std::remove(queue.begin(), queue.end(), id), queue.end());
    if (queue.empty()) {
        sideLevels.erase(level);
    }
}

std::vector<int> ReferenceModel::liveIds() const {
    std::vector<int> ids;
    ids.reserve(orders_.size());
    for (const auto& entry : orders_) {
        ids.push_back(entry.first);
    }
    return ids;
}

bool ReferenceModel::cancel(int id) {
    const auto found = orders_.find(id);
    if (found == orders_.end()) {
        return false;
    }
    removeFromLevel(id, found->second);
    orders_.erase(found);
    return true;
}

bool ReferenceModel::modify(int id, int price, int quantity) {
    auto found = orders_.find(id);
    if (found == orders_.end() || price <= 0 || quantity <= 0) {
        return false;
    }
    const Side side = found->second.side;
    removeFromLevel(id, found->second);
    found->second.price = price;
    found->second.quantity = quantity;
    levels(side)[price].push_back(id);
    return true;
}

bool ReferenceModel::submit(
    int id,
    Side side,
    OrderType type,
    int price,
    int quantity
) {
    if (id <= 0 || quantity <= 0 ||
        (side != Side::Buy && side != Side::Sell) ||
        (type != OrderType::Limit && type != OrderType::Market) ||
        (type == OrderType::Limit && price <= 0) ||
        orders_.find(id) != orders_.end()) {
        return false;
    }

    int remaining = quantity;
    while (remaining > 0) {
        auto& opposing = levels(side == Side::Buy ? Side::Sell : Side::Buy);
        if (opposing.empty()) {
            break;
        }

        auto level = side == Side::Buy
            ? opposing.begin()
            : std::prev(opposing.end());
        const int restingPrice = level->first;
        if (type == OrderType::Limit &&
            ((side == Side::Buy && restingPrice > price) ||
             (side == Side::Sell && restingPrice < price))) {
            break;
        }

        const int restingID = level->second.front();
        auto resting = orders_.find(restingID);
        const int fill = std::min(remaining, resting->second.quantity);
        trades_.push_back({
            side == Side::Buy ? id : restingID,
            side == Side::Buy ? restingID : id,
            restingPrice,
            fill
        });
        remaining -= fill;
        resting->second.quantity -= fill;
        if (resting->second.quantity == 0) {
            level->second.erase(level->second.begin());
            if (level->second.empty()) {
                opposing.erase(level);
            }
            orders_.erase(resting);
        }
    }

    if (type == OrderType::Limit && remaining > 0) {
        orders_.emplace(id, OrderState{side, price, remaining});
        levels(side)[price].push_back(id);
    }
    return true;
}

const std::vector<RefTrade>& ReferenceModel::trades() const {
    return trades_;
}

bool ReferenceModel::hasBid() const {
    return !bids_.empty();
}

bool ReferenceModel::hasAsk() const {
    return !asks_.empty();
}

int ReferenceModel::bestBid() const {
    return bids_.rbegin()->first;
}

int ReferenceModel::bestAsk() const {
    return asks_.begin()->first;
}

int ReferenceModel::volumeAt(int price, Side side) const {
    const auto level = levels(side).find(price);
    if (level == levels(side).end()) {
        return 0;
    }
    int total = 0;
    for (int id : level->second) {
        total += orders_.at(id).quantity;
    }
    return total;
}

std::vector<int> ReferenceModel::queueAt(int price, Side side) const {
    const auto level = levels(side).find(price);
    return level == levels(side).end() ? std::vector<int>() : level->second;
}
