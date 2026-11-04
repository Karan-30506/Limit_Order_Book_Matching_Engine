#include "test_framework.hpp"
#include "Book.hpp"
#include "Order.hpp"

#include <limits>
#include <stdexcept>

namespace {

void testQuantityOverflowIsRejected(TestFramework::TestSuite& suite) {
    Book book;
    const int maxQuantity = std::numeric_limits<int>::max();

    Order* first = new Order(1, 100, maxQuantity - 4, 1000LL, Side::Buy);
    Order* second = new Order(2, 100, 4, 1001LL, Side::Buy);
    ASSERT_TRUE(book.insertOrder(first));
    ASSERT_TRUE(book.insertOrder(second));
    ASSERT_EQ(book.getVolumeAt(100, Side::Buy), maxQuantity);

    Order* overflowing = new Order(3, 100, 1, 1002LL, Side::Buy);
    ASSERT_FALSE(book.insertOrder(overflowing));
    delete overflowing;
    ASSERT_NULL(book.getOrder(3));
    ASSERT_EQ(book.getVolumeAt(100, Side::Buy), maxQuantity);

    ASSERT_FALSE(book.modifyOrder(1, 100, maxQuantity - 3));
    ASSERT_EQ(book.getOrder(1)->quantity, maxQuantity - 4);
    ASSERT_EQ(book.getVolumeAt(100, Side::Buy), maxQuantity);

    Order* oppositeSide = new Order(4, 100, 1, 1003LL, Side::Sell);
    ASSERT_TRUE(book.insertOrder(oppositeSide));
    ASSERT_THROWS(book.getVolumeAt(100), std::overflow_error);
}

} // namespace

int main() {
    TestFramework::TestSuite suite("Regression Tests");
    suite.runTest("Quantity overflow is rejected", [&]() {
        testQuantityOverflowIsRejected(suite);
    });
    suite.printSummary();
    return suite.allPassed() ? 0 : 1;
}
