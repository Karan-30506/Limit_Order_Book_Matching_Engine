#ifndef ORDER_QUEUE_HPP
#define ORDER_QUEUE_HPP
#include "Order.hpp"

// Non-owning FIFO queue. Members must remain alive until detached.
class OrderQueue {
public:
    Order* head;
    Order* tail;
    int totalOrders;
    int totalQuantity;

    OrderQueue();
    ~OrderQueue(); // detaches, never deletes orders
    OrderQueue(const OrderQueue&) = delete;
    OrderQueue& operator=(const OrderQueue&) = delete;

    // owner must be the LimitNode containing this queue; the order must be detached.
    void pushBack(Order* order, LimitNode* owner);
    Order* popFront();
    void remove(Order* order);       // null/foreign/detached: no-op
    void reduceQuantity(int shares); // reduces head, delegates accounting to Order
};
#endif
