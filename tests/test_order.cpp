#include "test_framework.hpp"
#include "Order.hpp"
#include "OrderQueue.hpp"
#include "LimitNode.hpp"
#include <stdexcept>

namespace TestOrder {

void testConstructors(TestFramework::TestSuite& suite) {
    Order order1(101, 1500, 50, 1620000000LL, Side::Sell);
    ASSERT_EQ(order1.id, 101);
    ASSERT_EQ(order1.price, 1500);
    ASSERT_EQ(order1.quantity, 50);
    ASSERT_EQ(order1.timestamp, 1620000000LL);
    ASSERT_TRUE(order1.side == Side::Sell);
    ASSERT_NULL(order1.prev);
    ASSERT_NULL(order1.next);
    ASSERT_NULL(order1.parentLimit);
    ASSERT_FALSE(order1.isAttached());
    ASSERT_FALSE(order1.isFullyFilled());

    // 4-argument constructor defaults to Side::Buy
    Order order2(102, 1450, 30, 1620000001LL);
    ASSERT_EQ(order2.id, 102);
    ASSERT_EQ(order2.price, 1450);
    ASSERT_EQ(order2.quantity, 30);
    ASSERT_EQ(order2.timestamp, 1620000001LL);
    ASSERT_TRUE(order2.side == Side::Buy);
    ASSERT_NULL(order2.prev);
    ASSERT_NULL(order2.next);
    ASSERT_NULL(order2.parentLimit);
    ASSERT_FALSE(order2.isAttached());
    ASSERT_FALSE(order2.isFullyFilled());
}

void testIsFullyFilled(TestFramework::TestSuite& suite) {
    Order order(1, 100, 10, 1000LL, Side::Buy);
    ASSERT_FALSE(order.isFullyFilled());

    order.reduceQuantity(5);
    ASSERT_FALSE(order.isFullyFilled());
    ASSERT_EQ(order.quantity, 5);

    order.reduceQuantity(5);
    ASSERT_TRUE(order.isFullyFilled());
    ASSERT_EQ(order.quantity, 0);
}

void testReduceQuantityDetached(TestFramework::TestSuite& suite) {
    Order order(1, 100, 25, 1000LL, Side::Buy);

    // Reduction of 0 shares is valid
    order.reduceQuantity(0);
    ASSERT_EQ(order.quantity, 25);

    // Partial reduction
    order.reduceQuantity(10);
    ASSERT_EQ(order.quantity, 15);

    // Negative reduction throws invalid_argument
    ASSERT_THROWS(order.reduceQuantity(-1), std::invalid_argument);
    ASSERT_EQ(order.quantity, 15);

    // Reduction greater than remaining quantity throws invalid_argument
    ASSERT_THROWS(order.reduceQuantity(16), std::invalid_argument);
    ASSERT_EQ(order.quantity, 15);

    // Exact remaining reduction
    order.reduceQuantity(15);
    ASSERT_EQ(order.quantity, 0);
    ASSERT_TRUE(order.isFullyFilled());

    // Reducing from 0 throws if > 0
    ASSERT_THROWS(order.reduceQuantity(1), std::invalid_argument);
}

void testReduceQuantityAttached(TestFramework::TestSuite& suite) {
    LimitNode limitNode(100);
    Order order(1, 100, 50, 1000LL, Side::Buy);

    limitNode.queue.pushBack(&order, &limitNode);
    ASSERT_TRUE(order.isAttached());
    ASSERT_EQ(limitNode.queue.totalQuantity, 50);
    ASSERT_EQ(limitNode.queue.totalOrders, 1);

    // Reducing attached order's quantity updates queue's totalQuantity
    order.reduceQuantity(20);
    ASSERT_EQ(order.quantity, 30);
    ASSERT_EQ(limitNode.queue.totalQuantity, 30);
    ASSERT_EQ(limitNode.queue.totalOrders, 1);
    ASSERT_TRUE(order.isAttached());

    // Reduce to 0: order stays alive and attached with remaining quantity 0
    order.reduceQuantity(30);
    ASSERT_EQ(order.quantity, 0);
    ASSERT_TRUE(order.isFullyFilled());
    ASSERT_TRUE(order.isAttached());
    ASSERT_EQ(limitNode.queue.totalQuantity, 0);
    ASSERT_EQ(limitNode.queue.totalOrders, 1);

    limitNode.queue.popFront();
    ASSERT_FALSE(order.isAttached());
}

bool runAll(TestFramework::TestSuite& suite) {
    suite.runTest("Order: Constructors and Initial State", [&]() { testConstructors(suite); });
    suite.runTest("Order: isFullyFilled Check", [&]() { testIsFullyFilled(suite); });
    suite.runTest("Order: reduceQuantity Detached", [&]() { testReduceQuantityDetached(suite); });
    suite.runTest("Order: reduceQuantity Attached to Queue", [&]() { testReduceQuantityAttached(suite); });
    return suite.allPassed();
}

} // namespace TestOrder

#ifdef STANDALONE_TEST_ORDER
int main() {
    TestFramework::TestSuite suite("Order Unit Tests");
    TestOrder::runAll(suite);
    suite.printSummary();
    return suite.allPassed() ? 0 : 1;
}
#endif

