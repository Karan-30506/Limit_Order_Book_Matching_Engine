#include "OrderQueue.hpp"
#include "LimitNode.hpp"
#include <limits>
#include <stdexcept>

OrderQueue::OrderQueue() : head(nullptr), tail(nullptr), totalOrders(0), totalQuantity(0) {
}

OrderQueue::~OrderQueue() {
    while (head) {
        popFront();
    }
}

void OrderQueue::pushBack(Order* order, LimitNode* owner) {
    if (!order || !owner || &owner->queue != this || order->isAttached() || order->prev ||
        order->next || order->parentLimit || order->quantity < 0) {
        throw std::invalid_argument("invalid queue attachment");
    }
    if (order->quantity > std::numeric_limits<int>::max() - totalQuantity ||
        totalOrders == std::numeric_limits<int>::max()) {
        throw std::overflow_error("queue aggregate overflow");
    }
    // Publish membership only after validation, then append without scanning.
    order->prev = tail;
    order->next = nullptr;
    order->parentLimit = owner;
    order->queueOwner_ = this;
    if (tail) {
        tail->next = order;
    } else {
        head = order;
    }
    tail = order;
    ++totalOrders;
    totalQuantity += order->quantity;
}

Order* OrderQueue::popFront() {
    Order* order = head;
    remove(order);
    return order;
}

void OrderQueue::remove(Order* order) {
    // Links alone cannot distinguish a detached order from an attached singleton.
    if (!order || order->queueOwner_ != this) {
        return;
    }

    // Repair both neighbors and the endpoints before clearing the removed links.
    if (order->prev) {
        order->prev->next = order->next;
    } else {
        head = order->next;
    }
    if (order->next) {
        order->next->prev = order->prev;
    } else {
        tail = order->prev;
    }
    --totalOrders;
    totalQuantity -= order->quantity;

    // Leave the live order ready for reinsertion or transfer to its caller.
    order->prev = order->next = nullptr;
    order->parentLimit = nullptr;
    order->queueOwner_ = nullptr;
}

void OrderQueue::reduceQuantity(int shares) {
    if (!head) {
        throw std::logic_error("cannot reduce an empty queue");
    }
    head->reduceQuantity(shares);
}
