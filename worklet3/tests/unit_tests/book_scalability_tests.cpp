#include "Book.hpp"

#include <cassert>
#include <cstddef>
#include <iostream>

void runBookScalabilityTests() {
    constexpr int priceLevelCount = 1000;
    constexpr int ordersPerLevel = 100;
    constexpr int orderCount = priceLevelCount * ordersPerLevel;
    constexpr int firstPrice = 10000;

    Book book;
    for (int id = 1; id <= orderCount; ++id) {
        const int price = firstPrice + (id - 1) / ordersPerLevel;
        assert(book.insertOrder(new Order(id, price, 1, id, Side::Buy)));
    }

    assert(book.getBestBid() != nullptr);
    assert(book.getBestBid()->price == firstPrice + priceLevelCount - 1);
    assert(book.getBestAsk() == nullptr);

    std::size_t levelsSeen = 0;
    std::size_t ordersSeen = 0;
    for (LimitNode* level = book.getBestBid(); level != nullptr;
         level = book.getNextLevel(level)) {
        assert(level->getOrderCount() == ordersPerLevel);
        assert(level->getTotalVolume() == ordersPerLevel);
        ordersSeen += static_cast<std::size_t>(level->getOrderCount());
        ++levelsSeen;
    }

    assert(levelsSeen == priceLevelCount);
    assert(ordersSeen == orderCount);
    for (int id = 1; id <= orderCount; ++id) {
        const Order* order = book.getOrder(id);
        assert(order != nullptr);
        assert(order->price == firstPrice + (id - 1) / ordersPerLevel);
    }

    std::cout << "Book scalability test passed: " << orderCount
              << " resting orders across " << levelsSeen << " price levels.\n";
}
