#include "csv_cases.hpp"
#include "Book.hpp"
#include <climits>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>

// This executable alone replaces allocation so real bad_alloc rollback can be tested.
// The normal runner and benchmark do not link these replacements.
namespace Fault {
thread_local int allocationsUntilFailure = -1;

void* allocate(std::size_t size) {

    if (allocationsUntilFailure == 0) {
        allocationsUntilFailure = -1;
        throw std::bad_alloc();
    }
    if (allocationsUntilFailure > 0) {
        --allocationsUntilFailure;
    }
    if (void* memory = std::malloc(size ? size : 1)) {
        return memory;
    }
    throw std::bad_alloc();
}

template <typename Function>
bool failsAt(int offset, Function operation) {

    allocationsUntilFailure = offset;
    try {
        operation();
        allocationsUntilFailure = -1;
        return false;
    } catch (const std::bad_alloc&) {
        allocationsUntilFailure = -1;
        return true;
    } catch (...) {
        allocationsUntilFailure = -1;
        throw;
    }
}

} // namespace Fault

void* operator new(std::size_t size) {

    return Fault::allocate(size);
}

void* operator new[](std::size_t size) {

    return Fault::allocate(size);
}

void operator delete(void* memory) noexcept {

    std::free(memory);
}

void operator delete[](void* memory) noexcept {

    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {

    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {

    std::free(memory);
}

namespace {

void queueRollback(TestFramework::TestSuite& suite) {

    Order first(1, 100, INT_MAX, 0, Side::Buy);
    Order extra(2, 100, 1, 0, Side::Buy);
    LimitNode level(100);
    LimitNode foreign(100);
    level.queue.pushBack(&first, &level);
    ASSERT_THROWS(level.queue.pushBack(&extra, &level), std::overflow_error);
    ASSERT_FALSE(extra.isAttached());
    ASSERT_NULL(extra.prev);
    ASSERT_NULL(extra.next);
    ASSERT_NULL(extra.parentLimit);
    ASSERT_EQ(level.getTotalVolume(), INT_MAX);
    ASSERT_EQ(level.getOrderCount(), 1);
    ASSERT_EQ(level.queue.head, &first);
    ASSERT_EQ(level.queue.tail, &first);
    ASSERT_THROWS(foreign.queue.pushBack(&first, &foreign), std::invalid_argument);
    foreign.queue.remove(&first);
    ASSERT_EQ(first.parentLimit, &level);
    ASSERT_THROWS(first.reduceQuantity(-1), std::invalid_argument);
    ASSERT_EQ(first.quantity, INT_MAX);
    ASSERT_EQ(level.getTotalVolume(), INT_MAX);
    level.queue.popFront();
    ASSERT_THROWS(level.queue.reduceQuantity(0), std::logic_error);
}

void bookRollback(TestFramework::TestSuite& suite) {

    Book book;
    ASSERT_FALSE(book.insertOrder(nullptr));
    Order invalidSide(9, 100, 1, 0, static_cast<Side>(42));
    ASSERT_FALSE(book.insertOrder(&invalidSide));
    ASSERT_FALSE(invalidSide.isAttached());
    ASSERT_TRUE(book.buyTree.isEmpty());
    auto buy = std::make_unique<Order>(1, 100, INT_MAX, 0, Side::Buy);
    ASSERT_TRUE(book.insertOrder(buy.get()));
    Order* original = buy.release();
    auto sell = std::make_unique<Order>(2, 100, INT_MAX, 0, Side::Sell);
    ASSERT_TRUE(book.insertOrder(sell.get()));
    sell.release();
    ASSERT_THROWS(book.getVolumeAt(100), std::overflow_error);
    ASSERT_EQ(book.getVolumeAt(100, Side::Buy), INT_MAX);
    ASSERT_EQ(book.getVolumeAt(100, Side::Sell), INT_MAX);
    ASSERT_THROWS(book.getVolumeAt(100, static_cast<Side>(42)), std::invalid_argument);
    Order overflow(3, 100, 1, 0, Side::Buy);
    ASSERT_FALSE(book.insertOrder(&overflow));
    ASSERT_FALSE(overflow.isAttached());
    ASSERT_FALSE(book.modifyOrder(1, 100, -1));
    ASSERT_EQ(book.getOrder(1), original);
    ASSERT_EQ(original->quantity, INT_MAX);
    LimitNode foreign(100);
    ASSERT_NULL(book.popHeadOrder(&foreign));
    ASSERT_NULL(book.getNextLevel(&foreign));
    ASSERT_FALSE(book.insertOrder(original));
    ASSERT_EQ(book.orderMap.getSize(), 2);
}

void avlTransplant(TestFramework::TestSuite& suite) {

    Order order(1, 45, 1, 0, Side::Buy);
    AVLTree tree;
    std::map<int, LimitNode*> nodes;
    for (int price : {40, 20, 60, 10, 30, 50, 70, 45, 55}) {
        nodes[price] = tree.insert(price);
    }
    nodes.at(45)->queue.pushBack(&order, nodes.at(45));
    ASSERT_THROWS(tree.remove(nodes.at(45)), std::logic_error);
    LimitNode foreign(40);
    tree.remove(&foreign);
    ASSERT_NULL(tree.successor(&foreign));
    tree.remove(nodes.at(40));
    nodes.erase(40);
    for (const auto& item : nodes) {
        ASSERT_EQ(tree.insert(item.first), item.second);
    }
    ASSERT_EQ(order.parentLimit, nodes.at(45));
    ASSERT_EQ(nodes.at(45)->queue.head, &order);
    nodes.at(45)->queue.popFront();
    for (const auto& item : nodes) {
        tree.remove(item.second);
    }
    ASSERT_TRUE(tree.isEmpty());
}

std::uint64_t mixed(std::uint64_t key) {

    key ^= key >> 30;
    key *= UINT64_C(0xbf58476d1ce4e5b9);
    key ^= key >> 27;
    key *= UINT64_C(0x94d049bb133111eb);
    return key ^ (key >> 31);
}

void hashCollisionsAndLimits(TestFramework::TestSuite& suite) {

    HashMap<long long, int> signedKeys;
    signedKeys.insert(LLONG_MIN, 1);
    signedKeys.insert(LLONG_MAX, 2);
    signedKeys.insert(0, 3);
    ASSERT_EQ(signedKeys.get(LLONG_MIN), 1);
    ASSERT_EQ(signedKeys.get(LLONG_MAX), 2);
    HashMap<unsigned long long, int> unsignedKeys;
    unsignedKeys.insert(ULLONG_MAX, 4);
    ASSERT_EQ(unsignedKeys.get(ULLONG_MAX), 4);
    HashMap<int, int> chains(4096);
    std::vector<int> colliding;
    for (int key = 1; colliding.size() < 32; ++key) {
        if (mixed(static_cast<std::uint64_t>(key)) % 4096 == 0) {
            colliding.push_back(key);
            chains.insert(key, key);
        }
    }
    ASSERT_EQ(chains.getCapacity(), 4096);
    for (size_t i : {size_t(31), size_t(16), size_t(0)}) {
        chains.remove(colliding[i]);
        ASSERT_FALSE(chains.contains(colliding[i]));
    }
    for (size_t i = 1; i < 31; ++i) {
        if (i != 16) {
            ASSERT_EQ(chains.get(colliding[i]), colliding[i]);
        }
    }
    ASSERT_EQ(chains.getSize(), 29);
}

void hashAllocationRollback(TestFramework::TestSuite& suite) {

    int failures = 0;
    bool succeeded = false;
    for (int offset = 0; offset < 16 && !succeeded; ++offset) {
        HashMap<int, int> map(4);
        for (int i = 1; i <= 3; ++i) map.insert(i, i);
        const bool failed = Fault::failsAt(offset, [&]() {

            map.insert(4, 4);
        });
        if (failed) {
            ++failures;
            ASSERT_EQ(map.getSize(), 3);
            ASSERT_EQ(map.getCapacity(), 4);
            ASSERT_FALSE(map.contains(4));
        } else {
            succeeded = true;
            ASSERT_EQ(map.getSize(), 4);
            ASSERT_EQ(map.getCapacity(), 8);
        }
        for (int i = 1; i <= 3; ++i) ASSERT_EQ(map.get(i), i);
    }
    ASSERT_EQ(failures, 2);
    ASSERT_TRUE(succeeded);
}

void insertAllocationRollback(TestFramework::TestSuite& suite) {

    for (Side side : {Side::Buy, Side::Sell}) {
        int failures = 0;
        bool succeeded = false;
        for (int offset = 0; offset < 16 && !succeeded; ++offset) {
            Book book;
            std::vector<Order*> previous;
            for (int i = 1; i <= 12; ++i) {
                auto order = std::make_unique<Order>(i, i, 1, 0, side);
                ASSERT_TRUE(book.insertOrder(order.get()));
                previous.push_back(order.release());
            }
            auto candidate = std::make_unique<Order>(13, 13, 1, 0, side);
            bool inserted = false;
            const bool failed = Fault::failsAt(offset, [&]() {

                inserted = book.insertOrder(candidate.get());
            });
            auto& map = side == Side::Buy ? book.buyLimitMap : book.sellLimitMap;
            if (failed) {
                ++failures;
                ASSERT_EQ(book.orderMap.getSize(), 12);
                ASSERT_EQ(map.getSize(), 12);
                ASSERT_NULL(map.get(13));
                ASSERT_NULL(book.getOrder(13));
                ASSERT_FALSE(candidate->isAttached());
            } else {
                ASSERT_TRUE(inserted);
                candidate.release();
                succeeded = true;
                ASSERT_EQ(book.orderMap.getSize(), 13);
            }
            for (auto* order : previous) {
                ASSERT_EQ(book.getOrder(order->id), order);
                ASSERT_EQ(order->quantity, 1);
                ASSERT_EQ(order->parentLimit, map.get(order->price));
            }
            ASSERT_EQ(book.getBestBid(), side == Side::Buy ? map.get(failed ? 12 : 13) : nullptr);
            ASSERT_EQ(book.getBestAsk(), side == Side::Sell ? map.get(1) : nullptr);
        }
        ASSERT_TRUE(failures >= 5);
        ASSERT_TRUE(succeeded);
    }
}

void modifyAllocationRollback(TestFramework::TestSuite& suite) {

    for (Side side : {Side::Buy, Side::Sell}) {
        int failures = 0;
        bool succeeded = false;
        for (int offset = 0; offset < 16 && !succeeded; ++offset) {
            Book book;
            Order* original = nullptr;
            for (int i = 1; i <= 12; ++i) {
                auto order = std::make_unique<Order>(i, i, 1, 0, side);
                ASSERT_TRUE(book.insertOrder(order.get()));
                if (i == 1) original = order.get();
                order.release();
            }
            LimitNode* originalLevel = original->parentLimit;
            bool modified = false;
            const bool failed = Fault::failsAt(offset, [&]() {

                modified = book.modifyOrder(1, 13, 5);
            });
            auto& map = side == Side::Buy ? book.buyLimitMap : book.sellLimitMap;
            ASSERT_EQ(book.getOrder(1), original);
            ASSERT_EQ(book.orderMap.getSize(), 12);
            ASSERT_EQ(map.getSize(), 12);
            if (failed) {
                ++failures;
                ASSERT_EQ(original->price, 1);
                ASSERT_EQ(original->quantity, 1);
                ASSERT_EQ(original->parentLimit, originalLevel);
                ASSERT_EQ(originalLevel->queue.head, original);
                ASSERT_NULL(map.get(13));
                ASSERT_EQ(book.getVolumeAt(1, side), 1);
            } else {
                ASSERT_TRUE(modified);
                succeeded = true;
                ASSERT_EQ(original->price, 13);
                ASSERT_EQ(original->quantity, 5);
                ASSERT_NULL(map.get(1));
                ASSERT_EQ(original->parentLimit, map.get(13));
            }
        }
        ASSERT_TRUE(failures >= 3);
        ASSERT_TRUE(succeeded);
    }
}

} // namespace

int main() {

    TestFramework::TestSuite suite("Failure and allocation rollback tests");
    CsvCases::run(suite, "Failure", {
        {"queueRollback", queueRollback},
        {"bookRollback", bookRollback},
        {"avlTransplant", avlTransplant},
        {"hashCollisionsAndLimits", hashCollisionsAndLimits},
        {"hashAllocationRollback", hashAllocationRollback},
        {"insertAllocationRollback", insertAllocationRollback},
        {"modifyAllocationRollback", modifyAllocationRollback}
    });
    suite.printSummary();
    return suite.allPassed() ? 0 : 1;
}
