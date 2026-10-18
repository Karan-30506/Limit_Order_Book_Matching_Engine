#ifndef LIMIT_NODE_HPP
#define LIMIT_NODE_HPP
#include "OrderQueue.hpp"

// One price level: AVL links organize prices and the queue organizes time priority.
class LimitNode {
public:
    int price;
    int height;

    LimitNode* parent;
    LimitNode* left;
    LimitNode* right;

    OrderQueue queue;

    explicit LimitNode(int price);
    LimitNode(const LimitNode&) = delete;
    LimitNode& operator=(const LimitNode&) = delete;

    bool isEmpty();
    int getOrderCount();  // queue.totalOrders
    int getTotalVolume(); // queue.totalQuantity
};
#endif
