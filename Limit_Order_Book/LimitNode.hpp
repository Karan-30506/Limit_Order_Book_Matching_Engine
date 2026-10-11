#ifndef LIMIT_NODE_HPP
#define LIMIT_NODE_HPP

#include "OrderQueue.hpp"

class LimitNode {
public:
    int price;
    int height;

    LimitNode* parent;
    LimitNode* left;
    LimitNode* right;

    OrderQueue queue;

    // construcutor
    explicit LimitNode(int _price);

    // methods
    bool isEmpty();       // --> return queue.count == 0;
    int getOrderCount();  // --> return queue.count;
    int getTotalVolume(); // --> return queue.totalVolume;
};

#endif