#include "LimitNode.hpp"

LimitNode::LimitNode(int levelPrice)
    : price(levelPrice), height(1), parent(nullptr), left(nullptr), right(nullptr), queue() {
}

bool LimitNode::isEmpty() {
    return queue.totalOrders == 0;
}

int LimitNode::getOrderCount() {
    return queue.totalOrders;
}

int LimitNode::getTotalVolume() {
    return queue.totalQuantity;
}
