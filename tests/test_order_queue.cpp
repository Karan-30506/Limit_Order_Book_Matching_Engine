#include "test_framework.hpp"
#include "OrderQueue.hpp"
#include "LimitNode.hpp"
#include <limits>
#include <stdexcept>

namespace TestOrderQueue {

void testEmptyQueue(TestFramework::TestSuite& suite) {
    OrderQueue queue;
    ASSERT_NULL(queue.head);
    ASSERT_NULL(queue.tail);
    ASSERT_EQ(queue.totalOrders, 0);
    ASSERT_EQ(queue.totalQuantity, 0);

    // Popping empty queue returns nullptr
    ASSERT_NULL(queue.popFront());
    ASSERT_EQ(queue.totalOrders, 0);
    ASSERT_EQ(queue.totalQuantity, 0);

    // Reducing empty queue throws logic_error
    ASSERT_THROWS(queue.reduceQuantity(10), std::logic_error);
}

void testPushBackValidation(TestFramework::TestSuite& suite) {
    LimitNode node1(100);
    LimitNode node2(100);
    OrderQueue& q1 = node1.queue;

    Order validOrder(1, 100, 10, 1000LL, Side::Buy);

    // 1. Null order
    ASSERT_THROWS(q1.pushBack(nullptr, &node1), std::invalid_argument);

    // 2. Null owner
    ASSERT_THROWS(q1.pushBack(&validOrder, nullptr), std::invalid_argument);

    // 3. Mismatched owner (&owner->queue != this)
    ASSERT_THROWS(q1.pushBack(&validOrder, &node2), std::invalid_argument);

    // 4. Order with negative quantity
    Order negativeQtyOrder(2, 100, -5, 1000LL, Side::Buy);
    ASSERT_THROWS(q1.pushBack(&negativeQtyOrder, &node1), std::invalid_argument);

    // Push validOrder to attach it
    q1.pushBack(&validOrder, &node1);

    // 5. Already attached order
    ASSERT_THROWS(q1.pushBack(&validOrder, &node1), std::invalid_argument);

    // Clean up
    q1.popFront();
}

void testPushAndPopFIFO(TestFramework::TestSuite& suite) {
    LimitNode node(100);
    OrderQueue& q = node.queue;

    Order o1(1, 100, 10, 1001LL, Side::Buy);
    Order o2(2, 100, 20, 1002LL, Side::Buy);
    Order o3(3, 100, 30, 1003LL, Side::Buy);

    q.pushBack(&o1, &node);
    ASSERT_EQ(q.totalOrders, 1);
    ASSERT_EQ(q.totalQuantity, 10);
    ASSERT_EQ(q.head, &o1);
    ASSERT_EQ(q.tail, &o1);
    ASSERT_NULL(o1.prev);
    ASSERT_NULL(o1.next);
    ASSERT_EQ(o1.parentLimit, &node);
    ASSERT_TRUE(o1.isAttached());

    q.pushBack(&o2, &node);
    ASSERT_EQ(q.totalOrders, 2);
    ASSERT_EQ(q.totalQuantity, 30);
    ASSERT_EQ(q.head, &o1);
    ASSERT_EQ(q.tail, &o2);
    ASSERT_EQ(o1.next, &o2);
    ASSERT_EQ(o2.prev, &o1);
    ASSERT_NULL(o2.next);

    q.pushBack(&o3, &node);
    ASSERT_EQ(q.totalOrders, 3);
    ASSERT_EQ(q.totalQuantity, 60);
    ASSERT_EQ(q.head, &o1);
    ASSERT_EQ(q.tail, &o3);
    ASSERT_EQ(o2.next, &o3);
    ASSERT_EQ(o3.prev, &o2);

    // Pop o1
    Order* popped1 = q.popFront();
    ASSERT_EQ(popped1, &o1);
    ASSERT_FALSE(popped1->isAttached());
    ASSERT_NULL(popped1->prev);
    ASSERT_NULL(popped1->next);
    ASSERT_NULL(popped1->parentLimit);
    ASSERT_EQ(q.totalOrders, 2);
    ASSERT_EQ(q.totalQuantity, 50);
    ASSERT_EQ(q.head, &o2);
    ASSERT_NULL(o2.prev);

    // Pop o2
    Order* popped2 = q.popFront();
    ASSERT_EQ(popped2, &o2);
    ASSERT_FALSE(popped2->isAttached());
    ASSERT_EQ(q.totalOrders, 1);
    ASSERT_EQ(q.totalQuantity, 30);
    ASSERT_EQ(q.head, &o3);
    ASSERT_EQ(q.tail, &o3);

    // Pop o3
    Order* popped3 = q.popFront();
    ASSERT_EQ(popped3, &o3);
    ASSERT_FALSE(popped3->isAttached());
    ASSERT_EQ(q.totalOrders, 0);
    ASSERT_EQ(q.totalQuantity, 0);
    ASSERT_NULL(q.head);
    ASSERT_NULL(q.tail);

    // Pop from now empty queue
    ASSERT_NULL(q.popFront());
}

void testRemoveVariants(TestFramework::TestSuite& suite) {
    LimitNode node(100);
    OrderQueue& q = node.queue;

    Order o1(1, 100, 10, 1001LL, Side::Buy);
    Order o2(2, 100, 20, 1002LL, Side::Buy);
    Order o3(3, 100, 30, 1003LL, Side::Buy);
    Order o4(4, 100, 40, 1004LL, Side::Buy);

    q.pushBack(&o1, &node);
    q.pushBack(&o2, &node);
    q.pushBack(&o3, &node);
    q.pushBack(&o4, &node);

    // 1. Remove foreign / null / detached order: safe no-op
    Order foreignOrder(99, 100, 10, 1000LL, Side::Buy);
    q.remove(nullptr);
    ASSERT_EQ(q.totalOrders, 4);
    q.remove(&foreignOrder);
    ASSERT_EQ(q.totalOrders, 4);

    // 2. Remove middle element (o2)
    q.remove(&o2);
    ASSERT_FALSE(o2.isAttached());
    ASSERT_NULL(o2.prev);
    ASSERT_NULL(o2.next);
    ASSERT_NULL(o2.parentLimit);
    ASSERT_EQ(q.totalOrders, 3);
    ASSERT_EQ(q.totalQuantity, 80);
    ASSERT_EQ(o1.next, &o3);
    ASSERT_EQ(o3.prev, &o1);

    // 3. Remove head element (o1)
    q.remove(&o1);
    ASSERT_FALSE(o1.isAttached());
    ASSERT_EQ(q.totalOrders, 2);
    ASSERT_EQ(q.totalQuantity, 70);
    ASSERT_EQ(q.head, &o3);
    ASSERT_NULL(o3.prev);

    // 4. Remove tail element (o4)
    q.remove(&o4);
    ASSERT_FALSE(o4.isAttached());
    ASSERT_EQ(q.totalOrders, 1);
    ASSERT_EQ(q.totalQuantity, 30);
    ASSERT_EQ(q.head, &o3);
    ASSERT_EQ(q.tail, &o3);
    ASSERT_NULL(o3.next);

    // 5. Remove singleton element (o3)
    q.remove(&o3);
    ASSERT_FALSE(o3.isAttached());
    ASSERT_EQ(q.totalOrders, 0);
    ASSERT_EQ(q.totalQuantity, 0);
    ASSERT_NULL(q.head);
    ASSERT_NULL(q.tail);
}

void testReduceQuantity(TestFramework::TestSuite& suite) {
    LimitNode node(100);
    OrderQueue& q = node.queue;

    Order o1(1, 100, 50, 1001LL, Side::Buy);
    Order o2(2, 100, 30, 1002LL, Side::Buy);

    q.pushBack(&o1, &node);
    q.pushBack(&o2, &node);

    // Reduce head order by 20
    q.reduceQuantity(20);
    ASSERT_EQ(o1.quantity, 30);
    ASSERT_EQ(q.totalQuantity, 60);
    ASSERT_EQ(q.totalOrders, 2);

    // Reduce head order by remaining 30 -> head has 0 quantity, stays attached
    q.reduceQuantity(30);
    ASSERT_EQ(o1.quantity, 0);
    ASSERT_TRUE(o1.isFullyFilled());
    ASSERT_EQ(q.totalQuantity, 30);
    ASSERT_EQ(q.totalOrders, 2);
    ASSERT_EQ(q.head, &o1);

    // Pop head order manually
    Order* popped = q.popFront();
    ASSERT_EQ(popped, &o1);
    ASSERT_EQ(q.head, &o2);
    ASSERT_EQ(q.totalOrders, 1);
    ASSERT_EQ(q.totalQuantity, 30);

    q.popFront();
}

void testDestructorDetachesOrders(TestFramework::TestSuite& suite) {
    Order o1(1, 100, 10, 1001LL, Side::Buy);
    Order o2(2, 100, 20, 1002LL, Side::Buy);

    {
        LimitNode node(100);
        node.queue.pushBack(&o1, &node);
        node.queue.pushBack(&o2, &node);
        ASSERT_TRUE(o1.isAttached());
        ASSERT_TRUE(o2.isAttached());
    } // node and its queue go out of scope here

    // Invariant: Non-owning queue destructor detaches orders without deleting them!
    ASSERT_FALSE(o1.isAttached());
    ASSERT_FALSE(o2.isAttached());
    ASSERT_NULL(o1.prev);
    ASSERT_NULL(o1.next);
    ASSERT_NULL(o1.parentLimit);
    ASSERT_NULL(o2.prev);
    ASSERT_NULL(o2.next);
    ASSERT_NULL(o2.parentLimit);
}

void testOverflowProtection(TestFramework::TestSuite& suite) {
    LimitNode node(100);
    OrderQueue& q = node.queue;

    Order o1(1, 100, std::numeric_limits<int>::max() - 10, 1000LL, Side::Buy);
    q.pushBack(&o1, &node);

    Order o2(2, 100, 20, 1001LL, Side::Buy);
    ASSERT_THROWS(q.pushBack(&o2, &node), std::overflow_error);

    q.popFront();
}

bool runAll(TestFramework::TestSuite& suite) {
    suite.runTest("OrderQueue: Empty Queue State", [&]() { testEmptyQueue(suite); });
    suite.runTest("OrderQueue: pushBack Validations", [&]() { testPushBackValidation(suite); });
    suite.runTest("OrderQueue: FIFO Push and Pop", [&]() { testPushAndPopFIFO(suite); });
    suite.runTest("OrderQueue: Remove Variations (Head, Middle, Tail, Singleton)", [&]() { testRemoveVariants(suite); });
    suite.runTest("OrderQueue: reduceQuantity on Head", [&]() { testReduceQuantity(suite); });
    suite.runTest("OrderQueue: Non-owning Destructor Detachment", [&]() { testDestructorDetachesOrders(suite); });
    suite.runTest("OrderQueue: Overflow Protection", [&]() { testOverflowProtection(suite); });
    return suite.allPassed();
}

} // namespace TestOrderQueue

#ifdef STANDALONE_TEST_ORDER_QUEUE
int main() {
    TestFramework::TestSuite suite("OrderQueue Unit Tests");
    TestOrderQueue::runAll(suite);
    suite.printSummary();
    return suite.allPassed() ? 0 : 1;
}
#endif

