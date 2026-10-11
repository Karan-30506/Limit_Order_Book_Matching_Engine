#ifndef ORDER_QUEUE_HPP
#define ORDER_QUEUE_HPP

#include "Order.hpp"

class LimitNode; 

class OrderQueue {
public:
    Order* head;
    Order* tail;
    int totalOrders;
    int totalQuantity;

    OrderQueue();

    void pushBack(Order* order, LimitNode* owner);   

    // remove by matching algorithm
    Order* popFront();   

    // remove by user
    void remove(Order* order);                  
    
    // partially reduce volume of an order by user
    void reduceQuantity(int shares);                         
};

#endif
