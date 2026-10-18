#include "Book.hpp"
#include <limits>
#include <stdexcept>

namespace {
bool validSide(Side side) {
    return side == Side::Buy || side == Side::Sell;
}

bool canFitQuantity(LimitNode* level, int quantity, int replacedQuantity = 0) {
    // Same-level modification replaces the old remainder rather than adding to it.
    return !level || quantity <= std::numeric_limits<int>::max() -
                                     (level->queue.totalQuantity - replacedQuantity);
}
} // namespace

Book::Book() : highestBuy(nullptr), lowestSell(nullptr) {
}

Book::~Book() {
    deleteOrders(buyTree.root);
    deleteOrders(sellTree.root);
}

void Book::deleteOrders(LimitNode* level) {
    if (!level) {
        return;
    }
    deleteOrders(level->left);
    deleteOrders(level->right);
    // Orders must be detached/deleted before tree member destructors free the levels.
    while (Order* order = level->queue.popFront()) {
        delete order;
    }
}

LimitNode* Book::ensureLevel(int price, Side side) {
    auto& levelMap = side == Side::Buy ? buyLimitMap : sellLimitMap;
    auto& levelTree = side == Side::Buy ? buyTree : sellTree;
    if (auto* level = levelMap.get(price)) {
        return level;
    }
    LimitNode* level = levelTree.insert(price);
    // If map allocation fails, undo tree insertion before publishing a best-price cache.
    try {
        levelMap.insert(price, level);
    } catch (...) {
        levelTree.remove(level);
        throw;
    }
    if (side == Side::Buy) {
        if (!highestBuy || price > highestBuy->price) {
            highestBuy = level;
        }
    } else if (!lowestSell || price < lowestSell->price) {
        lowestSell = level;
    }
    return level;
}

void Book::removeEmptyLevel(LimitNode* level, Side side) {
    if (!level->isEmpty()) {
        return;
    }
    auto& levelTree = side == Side::Buy ? buyTree : sellTree;
    auto& levelMap = side == Side::Buy ? buyLimitMap : sellLimitMap;
    // Read neighbors and erase borrowed aliases while the level is still alive.
    if (side == Side::Buy && highestBuy == level) {
        highestBuy = levelTree.predecessor(level);
    }
    if (side == Side::Sell && lowestSell == level) {
        lowestSell = levelTree.successor(level);
    }
    levelMap.remove(level->price);
    levelTree.remove(level);
}

bool Book::isValidNewOrder(int id, int price, int quantity) {
    return price > 0 && quantity > 0 && !orderMap.contains(id);
}

bool Book::insertOrder(Order* order) {
    // Reject invalid inputs before creating a level or taking ownership of the order.
    if (!order || !validSide(order->side) || order->isAttached() || order->prev || order->next ||
        order->parentLimit || !isValidNewOrder(order->id, order->price, order->quantity)) {
        return false;
    }
    auto& levelMap = order->side == Side::Buy ? buyLimitMap : sellLimitMap;
    if (!canFitQuantity(levelMap.get(order->price), order->quantity)) {
        return false;
    }
    LimitNode* level = ensureLevel(order->price, order->side);
    // Roll back a newly created empty level if indexing the order cannot allocate.
    try {
        orderMap.insert(order->id, order);
    } catch (...) {
        removeEmptyLevel(level, order->side);
        throw;
    }
    // No allocation or failed precondition remains after validation.
    level->queue.pushBack(order, level);
    return true;
}

bool Book::cancelOrder(int id) {
    Order* order = orderMap.get(id);
    if (!order) {
        return false;
    }
    LimitNode* level = order->parentLimit;
    const Side side = order->side;
    level->queue.remove(order);
    orderMap.remove(id);
    delete order;
    removeEmptyLevel(level, side);
    return true;
}

bool Book::modifyOrder(int id, int price, int quantity) {
    Order* order = orderMap.get(id);
    if (!order || price <= 0 || quantity <= 0) {
        return false;
    }
    LimitNode* originalLevel = order->parentLimit;
    auto& levelMap = order->side == Side::Buy ? buyLimitMap : sellLimitMap;
    LimitNode* destination = levelMap.get(price);
    if (!canFitQuantity(destination, quantity,
                        destination == originalLevel ? order->quantity : 0)) {
        return false;
    }
    // Allocate/index the destination before touching the original order.
    destination = ensureLevel(price, order->side);
    // Unlink/reappend even at the same price: every successful modification loses priority.
    originalLevel->queue.remove(order);
    order->price = price;
    order->quantity = quantity;
    destination->queue.pushBack(order, destination);
    // A same-level modification must retain the level we just appended to.
    if (originalLevel != destination) {
        removeEmptyLevel(originalLevel, order->side);
    }
    return true;
}

LimitNode* Book::getBestBid() {
    return highestBuy;
}

LimitNode* Book::getBestAsk() {
    return lowestSell;
}

bool Book::levelSide(LimitNode* level, Side& side) {
    if (!level) {
        return false;
    }
    // Identity in separate maps disambiguates equal prices on opposite sides.
    if (buyLimitMap.get(level->price) == level) {
        side = Side::Buy;
        return true;
    }
    if (sellLimitMap.get(level->price) == level) {
        side = Side::Sell;
        return true;
    }
    return false;
}

LimitNode* Book::getNextLevel(LimitNode* level) {
    Side side = Side::Buy;
    if (!levelSide(level, side)) {
        return nullptr;
    }
    return side == Side::Buy ? buyTree.predecessor(level) : sellTree.successor(level);
}

int Book::getVolumeAt(int price, Side side) {
    if (!validSide(side)) {
        throw std::invalid_argument("invalid side");
    }
    auto* level = (side == Side::Buy ? buyLimitMap : sellLimitMap).get(price);
    return level ? level->getTotalVolume() : 0;
}

int Book::getVolumeAt(int price) {
    const int buy = getVolumeAt(price, Side::Buy);
    const int sell = getVolumeAt(price, Side::Sell);
    if (sell > std::numeric_limits<int>::max() - buy) {
        throw std::overflow_error("combined volume exceeds int");
    }
    return buy + sell;
}

Order* Book::getOrder(int id) {
    return orderMap.get(id);
}

Order* Book::popHeadOrder(LimitNode* level) {
    Side side = Side::Buy;
    if (!levelSide(level, side)) {
        return nullptr;
    }
    Order* order = level->queue.popFront();
    if (order) {
        orderMap.remove(order->id);
    }
    // This may delete level, but the detached Order remains valid for the caller.
    removeEmptyLevel(level, side);
    return order;
}
