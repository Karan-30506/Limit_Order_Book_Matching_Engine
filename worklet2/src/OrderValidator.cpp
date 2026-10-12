
#include "OrderValidator.hpp"

bool OrderValidator::isValid(const Order& order) {
    // Order ID must be positive.
    if (order.id <= 0) {
        return false;
    }

    // Quantity must be positive.
    if (order.quantity <= 0) {
        return false;
    }

    // Side must be valid.
    if (order.side != Side::Buy &&
        order.side != Side::Sell) {
        return false;
    }

    // Price validation depends on the order type.
    // Limit orders require a positive price.
    // Market orders do not use their submitted price.
    // Therefore, price validation is handled by MatchingEngine.

    return true;
}