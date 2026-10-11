#ifndef ORDER_HPP
#define ORDER_HPP

class LimitNode;

class Order {
public:
    int id;
    int price;
    int quantity;
    long long timestamp;

    Order* prev;
    Order* next;
    LimitNode* parentLimit;      // back pointer to the limit node

    Order(int _id, int _price, int _quantity, long long _timestamp);

    bool isFullyFilled();

    // required for partially filled
    void reduceQuantity(int filledShares);
};

#endif
