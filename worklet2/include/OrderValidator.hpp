#ifndef ORDER_VALIDATOR_HPP
#define ORDER_VALIDATOR_HPP

#include "Order.hpp"

class OrderValidator {
public:
    static bool isValid(const Order& order);
};

#endif