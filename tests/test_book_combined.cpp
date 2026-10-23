#include "test_framework.hpp"
#include "Book.hpp"
#include "Order.hpp"
#include "LimitNode.hpp"
#include <vector>

namespace TestBookCombined {

void testEmptyBook(TestFramework::TestSuite& suite) {
    Book book;
    ASSERT_NULL(book.getBestBid());
    ASSERT_NULL(book.getBestAsk());
    ASSERT_EQ(book.getVolumeAt(100), 0);
    ASSERT_EQ(book.getVolumeAt(100, Side::Buy), 0);
    ASSERT_EQ(book.getVolumeAt(100, Side::Sell), 0);
    ASSERT_NULL(book.getOrder(1));
    ASSERT_NULL(book.getNextLevel(nullptr));
}

void testOrderValidation(TestFramework::TestSuite& suite) {
    Book book;
    ASSERT_TRUE(book.isValidNewOrder(1, 100, 10));
    ASSERT_FALSE(book.isValidNewOrder(1, 0, 10));     // price == 0
    ASSERT_FALSE(book.isValidNewOrder(1, -5, 10));    // price < 0
    ASSERT_FALSE(book.isValidNewOrder(1, 100, 0));    // quantity == 0
    ASSERT_FALSE(book.isValidNewOrder(1, 100, -10));  // quantity < 0

    // Insert order 1
    Order* o1 = new Order(1, 100, 10, 1000LL, Side::Buy);
    ASSERT_TRUE(book.insertOrder(o1));

    // Reject duplicate ID
    ASSERT_FALSE(book.isValidNewOrder(1, 100, 10));
    Order* duplicateOrder = new Order(1, 100, 10, 1001LL, Side::Buy);
    ASSERT_FALSE(book.insertOrder(duplicateOrder));
    delete duplicateOrder; // caller retains ownership on failed insert
}

void testInsertAndPriceTimePriority(TestFramework::TestSuite& suite) {
    Book book;

    // Insert Bids: 100, 102, 101
    // At price 102, insert two orders to test FIFO queue
    Order* b1 = new Order(1, 100, 10, 1001LL, Side::Buy);
    Order* b2 = new Order(2, 102, 20, 1002LL, Side::Buy);
    Order* b3 = new Order(3, 101, 15, 1003LL, Side::Buy);
    Order* b4 = new Order(4, 102, 35, 1004LL, Side::Buy);

    ASSERT_TRUE(book.insertOrder(b1));
    ASSERT_TRUE(book.insertOrder(b2));
    ASSERT_TRUE(book.insertOrder(b3));
    ASSERT_TRUE(book.insertOrder(b4));

    // Best bid must be highest price: 102
    LimitNode* bestBid = book.getBestBid();
    ASSERT_NOT_NULL(bestBid);
    ASSERT_EQ(bestBid->price, 102);
    ASSERT_EQ(bestBid->getOrderCount(), 2);
    ASSERT_EQ(bestBid->getTotalVolume(), 55);

    // FIFO priority at level 102: b2 came before b4
    ASSERT_EQ(bestBid->queue.head, b2);
    ASSERT_EQ(bestBid->queue.tail, b4);
    ASSERT_EQ(b2->next, b4);
    ASSERT_EQ(b4->prev, b2);

    // Traversal from best bid down to lower bids
    LimitNode* nextBid1 = book.getNextLevel(bestBid);
    ASSERT_NOT_NULL(nextBid1);
    ASSERT_EQ(nextBid1->price, 101);

    LimitNode* nextBid2 = book.getNextLevel(nextBid1);
    ASSERT_NOT_NULL(nextBid2);
    ASSERT_EQ(nextBid2->price, 100);

    LimitNode* nextBid3 = book.getNextLevel(nextBid2);
    ASSERT_NULL(nextBid3);

    // Insert Asks: 105, 103, 107
    Order* a1 = new Order(11, 105, 50, 2001LL, Side::Sell);
    Order* a2 = new Order(12, 103, 40, 2002LL, Side::Sell);
    Order* a3 = new Order(13, 107, 60, 2003LL, Side::Sell);

    ASSERT_TRUE(book.insertOrder(a1));
    ASSERT_TRUE(book.insertOrder(a2));
    ASSERT_TRUE(book.insertOrder(a3));

    // Best ask must be lowest price: 103
    LimitNode* bestAsk = book.getBestAsk();
    ASSERT_NOT_NULL(bestAsk);
    ASSERT_EQ(bestAsk->price, 103);
    ASSERT_EQ(bestAsk->getTotalVolume(), 40);

    // Traversal from best ask up to higher asks
    LimitNode* nextAsk1 = book.getNextLevel(bestAsk);
    ASSERT_NOT_NULL(nextAsk1);
    ASSERT_EQ(nextAsk1->price, 105);

    LimitNode* nextAsk2 = book.getNextLevel(nextAsk1);
    ASSERT_NOT_NULL(nextAsk2);
    ASSERT_EQ(nextAsk2->price, 107);

    LimitNode* nextAsk3 = book.getNextLevel(nextAsk2);
    ASSERT_NULL(nextAsk3);

    // Volume lookups
    ASSERT_EQ(book.getVolumeAt(102, Side::Buy), 55);
    ASSERT_EQ(book.getVolumeAt(102, Side::Sell), 0);
    ASSERT_EQ(book.getVolumeAt(102), 55);

    ASSERT_EQ(book.getVolumeAt(103, Side::Buy), 0);
    ASSERT_EQ(book.getVolumeAt(103, Side::Sell), 40);
    ASSERT_EQ(book.getVolumeAt(103), 40);
}

void testCancelOrderAndLevelPruning(TestFramework::TestSuite& suite) {
    Book book;

    Order* b1 = new Order(1, 100, 10, 1001LL, Side::Buy);
    Order* b2 = new Order(2, 102, 20, 1002LL, Side::Buy);
    Order* b3 = new Order(3, 102, 30, 1003LL, Side::Buy);

    book.insertOrder(b1);
    book.insertOrder(b2);
    book.insertOrder(b3);

    ASSERT_EQ(book.getBestBid()->price, 102);
    ASSERT_EQ(book.getVolumeAt(102, Side::Buy), 50);

    // Cancel middle/first order at level 102
    ASSERT_TRUE(book.cancelOrder(2));
    ASSERT_NULL(book.getOrder(2));
    ASSERT_EQ(book.getVolumeAt(102, Side::Buy), 30);
    ASSERT_EQ(book.getBestBid()->price, 102);
    ASSERT_EQ(book.getBestBid()->queue.head, b3);

    // Cancel b3: level 102 becomes empty and is pruned!
    ASSERT_TRUE(book.cancelOrder(3));
    ASSERT_NULL(book.getOrder(3));
    ASSERT_EQ(book.getVolumeAt(102, Side::Buy), 0);

    // Best bid automatically moves to remaining price level: 100
    ASSERT_NOT_NULL(book.getBestBid());
    ASSERT_EQ(book.getBestBid()->price, 100);

    // Cancel non-existent order returns false
    ASSERT_FALSE(book.cancelOrder(999));
}

void testModifyOrder(TestFramework::TestSuite& suite) {
    Book book;

    Order* o1 = new Order(1, 100, 10, 1001LL, Side::Buy);
    Order* o2 = new Order(2, 100, 20, 1002LL, Side::Buy);
    book.insertOrder(o1);
    book.insertOrder(o2);

    LimitNode* level = book.getBestBid();
    ASSERT_EQ(level->queue.head, o1);
    ASSERT_EQ(level->queue.tail, o2);

    // 1. Same-level quantity modification:
    // Order loses time priority and must be appended to the TAIL!
    ASSERT_TRUE(book.modifyOrder(1, 100, 15));
    ASSERT_EQ(level->queue.head, o2); // o2 now has priority!
    ASSERT_EQ(level->queue.tail, o1); // o1 moved to tail!
    ASSERT_EQ(o1->quantity, 15);
    ASSERT_EQ(level->getTotalVolume(), 35);

    // 2. Price change modification:
    // Move o1 from 100 to higher price 105 -> new best bid!
    ASSERT_TRUE(book.modifyOrder(1, 105, 25));
    ASSERT_EQ(book.getBestBid()->price, 105);
    ASSERT_EQ(book.getBestBid()->getTotalVolume(), 25);
    ASSERT_EQ(book.getVolumeAt(100, Side::Buy), 20); // only o2 left at 100

    // 3. Move o2 from 100 to 105:
    // Price level 100 should be completely pruned!
    ASSERT_TRUE(book.modifyOrder(2, 105, 30));
    ASSERT_EQ(book.getVolumeAt(100, Side::Buy), 0);
    ASSERT_EQ(book.getVolumeAt(105, Side::Buy), 55);
    // Traversal from 105 should have no next level
    ASSERT_NULL(book.getNextLevel(book.getBestBid()));

    // 4. Invalid modification inputs return false
    ASSERT_FALSE(book.modifyOrder(1, -10, 20)); // invalid price
    ASSERT_FALSE(book.modifyOrder(1, 105, -5)); // invalid quantity
    ASSERT_FALSE(book.modifyOrder(999, 105, 10)); // invalid ID
}

void testPopHeadOrderMatchingSimulation(TestFramework::TestSuite& suite) {
    Book book;

    // Populate asks (sellers waiting at 100, 101)
    Order* a1 = new Order(1, 100, 10, 1000LL, Side::Sell);
    Order* a2 = new Order(2, 100, 15, 1001LL, Side::Sell);
    Order* a3 = new Order(3, 101, 50, 1002LL, Side::Sell);

    book.insertOrder(a1);
    book.insertOrder(a2);
    book.insertOrder(a3);

    // Simulate market taker buy order of 10 shares arriving:
    // Matches head of best ask (100) -> a1
    LimitNode* bestAsk = book.getBestAsk();
    ASSERT_EQ(bestAsk->price, 100);

    Order* matched1 = book.popHeadOrder(bestAsk);
    ASSERT_EQ(matched1, a1);
    ASSERT_FALSE(matched1->isAttached());
    ASSERT_NULL(book.getOrder(1)); // unindexed from book!

    // Level 100 still exists with a2
    ASSERT_NOT_NULL(book.getBestAsk());
    ASSERT_EQ(book.getBestAsk()->price, 100);
    ASSERT_EQ(book.getBestAsk()->getTotalVolume(), 15);

    delete matched1; // caller responsibility

    // Next match consumes remaining 15 shares of level 100
    Order* matched2 = book.popHeadOrder(book.getBestAsk());
    ASSERT_EQ(matched2, a2);
    ASSERT_FALSE(matched2->isAttached());
    ASSERT_NULL(book.getOrder(2));

    delete matched2;

    // Level 100 is now empty and was pruned; best ask automatically advances to 101!
    ASSERT_NOT_NULL(book.getBestAsk());
    ASSERT_EQ(book.getBestAsk()->price, 101);
    ASSERT_EQ(book.getBestAsk()->getTotalVolume(), 50);

    // Consume level 101
    Order* matched3 = book.popHeadOrder(book.getBestAsk());
    ASSERT_EQ(matched3, a3);
    delete matched3;

    // Book is now completely empty on sell side
    ASSERT_NULL(book.getBestAsk());
}

void testCrossedBookSupport(TestFramework::TestSuite& suite) {
    Book book;

    // Crossed book condition: Bid (110) > Ask (100)
    // Worklet 1 allows crossed books until Worklet 2 matching is integrated.
    Order* bid = new Order(1, 110, 20, 1000LL, Side::Buy);
    Order* ask = new Order(2, 100, 15, 1001LL, Side::Sell);

    ASSERT_TRUE(book.insertOrder(bid));
    ASSERT_TRUE(book.insertOrder(ask));

    ASSERT_EQ(book.getBestBid()->price, 110);
    ASSERT_EQ(book.getBestAsk()->price, 100);

    // Orders at the exact same price on both sides
    Order* bidAt105 = new Order(3, 105, 5, 1002LL, Side::Buy);
    Order* askAt105 = new Order(4, 105, 8, 1003LL, Side::Sell);

    ASSERT_TRUE(book.insertOrder(bidAt105));
    ASSERT_TRUE(book.insertOrder(askAt105));

    ASSERT_EQ(book.getVolumeAt(105, Side::Buy), 5);
    ASSERT_EQ(book.getVolumeAt(105, Side::Sell), 8);
    ASSERT_EQ(book.getVolumeAt(105), 13);
}

void testBookDestructionCleansOrders(TestFramework::TestSuite& suite) {
    // Populate book and let it exit scope to ensure destructor runs cleanly
    {
        Book tempBook;
        for (int i = 1; i <= 20; ++i) {
            Order* b = new Order(i, 100 + (i % 5), 10 * i, 1000LL + i, Side::Buy);
            Order* s = new Order(100 + i, 200 + (i % 5), 10 * i, 2000LL + i, Side::Sell);
            tempBook.insertOrder(b);
            tempBook.insertOrder(s);
        }
        ASSERT_NOT_NULL(tempBook.getBestBid());
        ASSERT_NOT_NULL(tempBook.getBestAsk());
    }
    // If destructor had double-free or memory corruption, this test would crash.
    ASSERT_TRUE(true);
}

bool runAll(TestFramework::TestSuite& suite) {
    suite.runTest("Book Combined: Empty Book State", [&]() { testEmptyBook(suite); });
    suite.runTest("Book Combined: Order Validation", [&]() { testOrderValidation(suite); });
    suite.runTest("Book Combined: Insert and Price-Time Priority", [&]() { testInsertAndPriceTimePriority(suite); });
    suite.runTest("Book Combined: Cancel Order and Level Pruning", [&]() { testCancelOrderAndLevelPruning(suite); });
    suite.runTest("Book Combined: Modify Order (Priority Loss & Relocation)", [&]() { testModifyOrder(suite); });
    suite.runTest("Book Combined: popHeadOrder Matching Simulation", [&]() { testPopHeadOrderMatchingSimulation(suite); });
    suite.runTest("Book Combined: Crossed Book and Equal-Price Sides", [&]() { testCrossedBookSupport(suite); });
    suite.runTest("Book Combined: Book Destructor Cleanup", [&]() { testBookDestructionCleansOrders(suite); });
    return suite.allPassed();
}

} // namespace TestBookCombined

#ifdef STANDALONE_TEST_BOOK_COMBINED
int main() {
    TestFramework::TestSuite suite("Book Combined Integration Tests");
    TestBookCombined::runAll(suite);
    suite.printSummary();
    return suite.allPassed() ? 0 : 1;
}
#endif

