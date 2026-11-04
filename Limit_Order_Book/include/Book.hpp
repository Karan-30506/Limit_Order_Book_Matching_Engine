#ifndef BOOK_HPP
#define BOOK_HPP
#include "AVLTree.hpp"
#include "HashMap.hpp"
#include "Order.hpp"

// Resting-order storage; Worklet 2 supplies matching and crossing decisions.
class Book {
public:
    // Each side owns independent price levels, including at the same numeric price.
    AVLTree buyTree;
    AVLTree sellTree;

    // Borrowed cache pointers: O(1) best-price access, null for an empty side.
    LimitNode* highestBuy;
    LimitNode* lowestSell;

    HashMap<int, Order*> orderMap;
    HashMap<int, LimitNode*> buyLimitMap;
    HashMap<int, LimitNode*> sellLimitMap;

    Book();
    ~Book();
    Book(const Book&) = delete;
    Book& operator=(const Book&) = delete;

    // Ownership transfers only on success; false leaves the order with the caller.
    bool insertOrder(Order* order);
    // Cancellation deletes the order; removing its last level adds O(log M) cleanup.
    bool cancelOrder(int orderId);
    // Preserves the object, ID and side but appends at the destination tail.
    bool modifyOrder(int orderId, int newPrice, int newQuantity);

    LimitNode* getBestBid();
    LimitNode* getBestAsk();
    LimitNode* getNextLevel(LimitNode* level); // next worse on the same side
    int getVolumeAt(int price);                // combined buy + sell remaining quantity
    int getVolumeAt(int price, Side side);
    Order* getOrder(int orderId);
    // Transfers a detached order to the caller. Last-order pop invalidates level.
    Order* popHeadOrder(LimitNode* level);
    bool isValidNewOrder(int orderId, int price, int quantity);

private:
    LimitNode* ensureLevel(int price, Side side);
    void removeEmptyLevel(LimitNode* level, Side side);
    bool levelSide(LimitNode* level, Side& side);
    static void deleteOrders(LimitNode* level);
};
#endif
