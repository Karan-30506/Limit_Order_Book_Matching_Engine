#ifndef BOOK_HPP
#define BOOK_HPP

#include "AVLTree.hpp"
#include "HashMap.hpp"
#include "Order.hpp"

class Book {    
public:
    AVLTree buyTree;
    AVLTree sellTree;

    // Cache pointers
    LimitNode* highestBuy;     // pointer to buyTree's current max
    LimitNode* lowestSell;       // pointer to sellTree's current min

    // hashmaps for fast lookups
    HashMap<int, Order*> orderMap;              
    HashMap<int, LimitNode*> buyLimitMap;            
    HashMap<int, LimitNode*> sellLimitMap; 

    Book();                                        
    ~Book();

    // inserts a LIMIT order (existing or new price level)
    bool insertOrder(Order* order);  
    
    // O(1) expected — removes an order from wherever it rests
    bool cancelOrder(int orderId);

    // cancel + re-insert
    bool modifyOrder(int orderId, int newPrice, int newQuantity);

    // API for Worklet 2
    // O(1) — returns highestBuy
    LimitNode* getBestBid(); 

    // O(1) — returns lowestSell
    LimitNode* getBestAsk();

    // O(log M) — get the next best limit node
    LimitNode* getNextLevel(LimitNode* level);
    
    // O(1) — 0 if no LimitNode exists there
    int getVolumeAt(int price);

    // O(1) — nullptr if not found
    Order* getOrder(int orderId);

    // O(1) — pops the oldest order at a level for matching
    Order* popHeadOrder(LimitNode* level);

    // duplicate id / non-positive qty / non-positive limit price
    bool isValidNewOrder(int orderId, int price, int quantity);
};

#endif