#include "Order.hpp"
#include "LimitNode.hpp"
#include <stdexcept>

Order::Order(int orderId, int limitPrice, int remainingQuantity, long long creationTimestamp,
             Side orderSide)
    : id(orderId), price(limitPrice), quantity(remainingQuantity), timestamp(creationTimestamp),
      side(orderSide), prev(nullptr), next(nullptr), parentLimit(nullptr), queueOwner_(nullptr) {
}

Order::Order(int orderId, int limitPrice, int remainingQuantity, long long creationTimestamp)
    : Order(orderId, limitPrice, remainingQuantity, creationTimestamp, Side::Buy) {
}

bool Order::isFullyFilled() {
    return quantity == 0;
}

bool Order::isAttached() const {
    return queueOwner_ != nullptr;
}

void Order::reduceQuantity(int shares) {
    if (shares < 0 || shares > quantity) {
        throw std::invalid_argument("reduction outside remaining quantity");
    }
    quantity -= shares;
    // OrderQueue::reduceQuantity delegates here, so aggregate accounting has one owner.
    if (queueOwner_) {
        queueOwner_->totalQuantity -= shares;
    }
}
