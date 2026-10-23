#include "test_framework.hpp"
#include "LimitNode.hpp"
#include "Order.hpp"

namespace TestLimitNode {

void testInitialState(TestFramework::TestSuite& suite) {
    LimitNode node(150);
    ASSERT_EQ(node.price, 150);
    ASSERT_EQ(node.height, 1);
    ASSERT_NULL(node.parent);
    ASSERT_NULL(node.left);
    ASSERT_NULL(node.right);
    ASSERT_TRUE(node.isEmpty());
    ASSERT_EQ(node.getOrderCount(), 0);
    ASSERT_EQ(node.getTotalVolume(), 0);
}

void testQueueDelegationAndVolume(TestFramework::TestSuite& suite) {
    LimitNode node(200);

    Order o1(1, 200, 100, 1000LL, Side::Buy);
    Order o2(2, 200, 250, 1001LL, Side::Buy);

    node.queue.pushBack(&o1, &node);
    ASSERT_FALSE(node.isEmpty());
    ASSERT_EQ(node.getOrderCount(), 1);
    ASSERT_EQ(node.getTotalVolume(), 100);

    node.queue.pushBack(&o2, &node);
    ASSERT_FALSE(node.isEmpty());
    ASSERT_EQ(node.getOrderCount(), 2);
    ASSERT_EQ(node.getTotalVolume(), 350);

    // Partially reduce head order
    node.queue.reduceQuantity(50);
    ASSERT_EQ(node.getOrderCount(), 2);
    ASSERT_EQ(node.getTotalVolume(), 300);

    // Pop head order
    Order* popped = node.queue.popFront();
    ASSERT_EQ(popped, &o1);
    ASSERT_FALSE(node.isEmpty());
    ASSERT_EQ(node.getOrderCount(), 1);
    ASSERT_EQ(node.getTotalVolume(), 250);

    // Remove remaining order
    node.queue.remove(&o2);
    ASSERT_TRUE(node.isEmpty());
    ASSERT_EQ(node.getOrderCount(), 0);
    ASSERT_EQ(node.getTotalVolume(), 0);
}

bool runAll(TestFramework::TestSuite& suite) {
    suite.runTest("LimitNode: Initial State", [&]() { testInitialState(suite); });
    suite.runTest("LimitNode: Queue Delegation and Volume Aggregation", [&]() { testQueueDelegationAndVolume(suite); });
    return suite.allPassed();
}

} // namespace TestLimitNode

#ifdef STANDALONE_TEST_LIMIT_NODE
int main() {
    TestFramework::TestSuite suite("LimitNode Unit Tests");
    TestLimitNode::runAll(suite);
    suite.printSummary();
    return suite.allPassed() ? 0 : 1;
}
#endif

