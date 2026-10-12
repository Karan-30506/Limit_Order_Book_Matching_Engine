#ifndef ORDER_HPP
#define ORDER_HPP

class LimitNode;
class OrderQueue;
class Book;

enum class Side { Buy, Sell };

// Intrusive queue member. Book owns accepted orders; links do not own adjacent orders.
class Order {
public:
    int id;
    int price;
    int quantity; // remaining quantity
    long long timestamp;
    Side side;

    // Queue position determines FIFO priority; timestamp is retained metadata.
    Order* prev;
    Order* next;
    LimitNode* parentLimit;

    Order(int id, int price, int quantity, long long timestamp, Side side);
    // Compatibility overload: explicitly defaults to Buy.
    Order(int id, int price, int quantity, long long timestamp);
    Order(const Order&) = delete;
    Order& operator=(const Order&) = delete;

    bool isFullyFilled();
    bool isAttached() const;
    // Updates an attached queue's volume once; a zero remainder stays alive and attached.
    void reduceQuantity(int filledShares);

private:
    // Identifies the actual queue in O(1), including singleton orders with null links.
    OrderQueue* queueOwner_;
    friend class OrderQueue;
    friend class Book;
};
#endif
