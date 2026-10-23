#include "test_framework.hpp"
#include "Hashmap.hpp"
#include "Order.hpp"
#include <string>
#include <vector>
#include <stdexcept>

namespace TestHashMap {

void testConstructionAndInvalidCapacity(TestFramework::TestSuite& suite) {
    HashMap<int, int> mapDefault;
    ASSERT_EQ(mapDefault.getSize(), 0);
    ASSERT_EQ(mapDefault.getCapacity(), 16);
    ASSERT_EQ(mapDefault.loadFactor(), 0.0);

    HashMap<int, int> mapCustom(32);
    ASSERT_EQ(mapCustom.getSize(), 0);
    ASSERT_EQ(mapCustom.getCapacity(), 32);

    ASSERT_THROWS((HashMap<int, int>(0)), std::invalid_argument);
    ASSERT_THROWS((HashMap<int, int>(-5)), std::invalid_argument);
}

void testInsertGetContains(TestFramework::TestSuite& suite) {
    HashMap<int, std::string> map;
    ASSERT_FALSE(map.contains(10));
    ASSERT_EQ(map.get(10), std::string{});

    map.insert(10, "Ten");
    map.insert(20, "Twenty");
    map.insert(30, "Thirty");

    ASSERT_EQ(map.getSize(), 3);
    ASSERT_TRUE(map.contains(10));
    ASSERT_TRUE(map.contains(20));
    ASSERT_TRUE(map.contains(30));
    ASSERT_FALSE(map.contains(40));

    ASSERT_EQ(map.get(10), "Ten");
    ASSERT_EQ(map.get(20), "Twenty");
    ASSERT_EQ(map.get(30), "Thirty");
    ASSERT_EQ(map.get(40), "");

    // Updating existing key
    map.insert(20, "Twenty-Updated");
    ASSERT_EQ(map.getSize(), 3); // size should not increase
    ASSERT_EQ(map.get(20), "Twenty-Updated");
}

void testPointerValues(TestFramework::TestSuite& suite) {
    HashMap<int, Order*> map;
    Order o1(101, 500, 10, 1000LL, Side::Buy);
    Order o2(102, 510, 20, 1001LL, Side::Sell);

    map.insert(o1.id, &o1);
    map.insert(o2.id, &o2);

    ASSERT_EQ(map.getSize(), 2);
    ASSERT_EQ(map.get(101), &o1);
    ASSERT_EQ(map.get(102), &o2);
    ASSERT_NULL(map.get(999));

    map.remove(101);
    ASSERT_EQ(map.getSize(), 1);
    ASSERT_NULL(map.get(101));
    ASSERT_FALSE(map.contains(101));

    // Order itself must not be deleted or modified
    ASSERT_EQ(o1.id, 101);
    ASSERT_EQ(o1.price, 500);
}

void testRemove(TestFramework::TestSuite& suite) {
    HashMap<int, int> map(8);

    // Insert elements
    map.insert(1, 100);
    map.insert(2, 200);
    map.insert(3, 300);

    // Remove non-existent key: safe no-op
    map.remove(99);
    ASSERT_EQ(map.getSize(), 3);

    // Remove existing keys
    map.remove(2);
    ASSERT_EQ(map.getSize(), 2);
    ASSERT_FALSE(map.contains(2));
    ASSERT_EQ(map.get(2), 0);
    ASSERT_TRUE(map.contains(1));
    ASSERT_TRUE(map.contains(3));

    map.remove(1);
    ASSERT_EQ(map.getSize(), 1);
    ASSERT_FALSE(map.contains(1));

    map.remove(3);
    ASSERT_EQ(map.getSize(), 0);
    ASSERT_FALSE(map.contains(3));
}

void testNegativeAnd64BitKeys(TestFramework::TestSuite& suite) {
    HashMap<long long, int> map;
    map.insert(-1LL, 10);
    map.insert(-42LL, 20);
    map.insert(-999999999999LL, 30);
    map.insert(999999999999LL, 40);

    ASSERT_EQ(map.getSize(), 4);
    ASSERT_TRUE(map.contains(-1LL));
    ASSERT_TRUE(map.contains(-42LL));
    ASSERT_TRUE(map.contains(-999999999999LL));
    ASSERT_TRUE(map.contains(999999999999LL));

    ASSERT_EQ(map.get(-1LL), 10);
    ASSERT_EQ(map.get(-42LL), 20);
    ASSERT_EQ(map.get(-999999999999LL), 30);
    ASSERT_EQ(map.get(999999999999LL), 40);

    map.remove(-42LL);
    ASSERT_FALSE(map.contains(-42LL));
    ASSERT_EQ(map.getSize(), 3);
}

void testResizingAndRehash(TestFramework::TestSuite& suite) {
    // Start with small capacity 4
    HashMap<int, int> map(4);
    ASSERT_EQ(map.getCapacity(), 4);

    // Insert 1: count 1, threshold check (1)/4 = 0.25 <= 0.75
    map.insert(1, 10);
    ASSERT_EQ(map.getCapacity(), 4);

    // Insert 2: count 2, threshold check (2)/4 = 0.50 <= 0.75
    map.insert(2, 20);
    ASSERT_EQ(map.getCapacity(), 4);

    // Insert 3: count 3, threshold check (3)/4 = 0.75 <= 0.75
    map.insert(3, 30);
    ASSERT_EQ(map.getCapacity(), 4);

    // Insert 4: next element (3 + 1)/4 = 1.0 > 0.75 -> trigger resize!
    // capacity should double from 4 to 8
    map.insert(4, 40);
    ASSERT_EQ(map.getCapacity(), 8);
    ASSERT_EQ(map.getSize(), 4);

    // Verify all keys 1..4 are still present with correct values
    for (int k = 1; k <= 4; ++k) {
        ASSERT_TRUE(map.contains(k));
        ASSERT_EQ(map.get(k), k * 10);
    }

    // Insert up to 50 elements to trigger multiple resizes (8 -> 16 -> 32 -> 64)
    for (int k = 5; k <= 50; ++k) {
        map.insert(k, k * 10);
    }
    ASSERT_EQ(map.getSize(), 50);
    ASSERT_TRUE(map.getCapacity() >= 64);

    // Verify all 50 elements
    for (int k = 1; k <= 50; ++k) {
        ASSERT_TRUE(map.contains(k));
        ASSERT_EQ(map.get(k), k * 10);
    }
}

bool runAll(TestFramework::TestSuite& suite) {
    suite.runTest("HashMap: Construction and Capacity Checks", [&]() { testConstructionAndInvalidCapacity(suite); });
    suite.runTest("HashMap: Insert, Get, Contains, and Update", [&]() { testInsertGetContains(suite); });
    suite.runTest("HashMap: Pointer Values Storage", [&]() { testPointerValues(suite); });
    suite.runTest("HashMap: Key Removal", [&]() { testRemove(suite); });
    suite.runTest("HashMap: Negative and 64-bit Keys", [&]() { testNegativeAnd64BitKeys(suite); });
    suite.runTest("HashMap: Resizing and Rehashing", [&]() { testResizingAndRehash(suite); });
    return suite.allPassed();
}

} // namespace TestHashMap

#ifdef STANDALONE_TEST_HASH_MAP
int main() {
    TestFramework::TestSuite suite("HashMap Unit Tests");
    TestHashMap::runAll(suite);
    suite.printSummary();
    return suite.allPassed() ? 0 : 1;
}
#endif

