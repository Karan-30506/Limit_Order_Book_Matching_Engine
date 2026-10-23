#include "test_framework.hpp"
#include <iostream>

namespace TestOrder { bool runAll(TestFramework::TestSuite& suite); }
namespace TestOrderQueue { bool runAll(TestFramework::TestSuite& suite); }
namespace TestLimitNode { bool runAll(TestFramework::TestSuite& suite); }
namespace TestAVLTree { bool runAll(TestFramework::TestSuite& suite); }
namespace TestHashMap { bool runAll(TestFramework::TestSuite& suite); }
namespace TestBookCombined { bool runAll(TestFramework::TestSuite& suite); }

int main() {
    std::cout << "========================================================\n";
    std::cout << "       LIMIT ORDER BOOK ENGINE - COMPREHENSIVE TESTS     \n";
    std::cout << "========================================================\n\n";

    int totalPassed = 0;
    int totalRun = 0;
    int suitesFailed = 0;

    auto runSuite = [&](const std::string& name, auto testRunnerFunc) {
        std::cout << ">>> Running " << name << "...\n";
        TestFramework::TestSuite suite(name);
        testRunnerFunc(suite);
        suite.printSummary();
        std::cout << "\n";
        totalRun += suite.testsRun;
        totalPassed += suite.testsPassed;
        if (!suite.allPassed()) {
            suitesFailed++;
        }
    };

    // 1. Data Structure: Order
    runSuite("Order Unit Tests", TestOrder::runAll);

    // 2. Data Structure: OrderQueue
    runSuite("OrderQueue Unit Tests", TestOrderQueue::runAll);

    // 3. Data Structure: LimitNode
    runSuite("LimitNode Unit Tests", TestLimitNode::runAll);

    // 4. Data Structure: AVLTree
    runSuite("AVLTree Unit Tests", TestAVLTree::runAll);

    // 5. Data Structure: HashMap
    runSuite("HashMap Unit Tests", TestHashMap::runAll);

    // 6. Combined Integration: Limit Order Book
    runSuite("Combined Limit Order Book Tests", TestBookCombined::runAll);

    std::cout << "========================================================\n";
    std::cout << "                    OVERALL SUMMARY                     \n";
    std::cout << "========================================================\n";
    std::cout << "Total Test Cases Run   : " << totalRun << "\n";
    std::cout << "Total Test Cases Passed: " << totalPassed << "\n";
    std::cout << "Total Test Cases Failed: " << (totalRun - totalPassed) << "\n";
    std::cout << "Test Suites Failed     : " << suitesFailed << "\n";
    if (suitesFailed == 0 && totalRun == totalPassed) {
        std::cout << "RESULT: ALL TESTS PASSED SUCCESSFULLY!\n";
    } else {
        std::cout << "RESULT: SOME TESTS FAILED.\n";
    }
    std::cout << "========================================================\n";

    return (suitesFailed == 0) ? 0 : 1;
}

