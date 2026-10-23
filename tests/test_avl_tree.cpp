#include "test_framework.hpp"
#include "AVLTree.hpp"
#include "LimitNode.hpp"
#include "Order.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
#include <stdexcept>

namespace TestAVLTree {

// Helper to verify AVL tree invariants:
// 1. BST property: left->price < node->price < right->price
// 2. Parent pointers: child->parent == node
// 3. Height property: node->height == 1 + max(h(left), h(right))
// 4. AVL balance: |h(left) - h(right)| <= 1
bool verifyAVLInvariants(LimitNode* node, LimitNode* parent, int& calculatedHeight, std::string& error) {
    if (!node) {
        calculatedHeight = 0;
        return true;
    }

    if (node->parent != parent) {
        error = "Parent pointer mismatch at price " + std::to_string(node->price);
        return false;
    }

    int leftHeight = 0;
    int rightHeight = 0;

    if (node->left) {
        if (node->left->price >= node->price) {
            error = "BST violation: left child price >= node price at " + std::to_string(node->price);
            return false;
        }
        if (!verifyAVLInvariants(node->left, node, leftHeight, error)) {
            return false;
        }
    }

    if (node->right) {
        if (node->right->price <= node->price) {
            error = "BST violation: right child price <= node price at " + std::to_string(node->price);
            return false;
        }
        if (!verifyAVLInvariants(node->right, node, rightHeight, error)) {
            return false;
        }
    }

    calculatedHeight = 1 + std::max(leftHeight, rightHeight);
    if (node->height != calculatedHeight) {
        error = "Height mismatch at price " + std::to_string(node->price) +
                ": node has " + std::to_string(node->height) +
                ", expected " + std::to_string(calculatedHeight);
        return false;
    }

    int balanceFactor = leftHeight - rightHeight;
    if (std::abs(balanceFactor) > 1) {
        error = "AVL balance factor violated at price " + std::to_string(node->price) +
                ": factor=" + std::to_string(balanceFactor);
        return false;
    }

    return true;
}

bool checkTree(AVLTree& tree, std::string& error) {
    int h = 0;
    if (!verifyAVLInvariants(tree.root, nullptr, h, error)) {
        return false;
    }
    if (tree.getHeightOfTree() != h) {
        error = "tree.getHeightOfTree() != calculated root height";
        return false;
    }
    return true;
}

void testEmptyTree(TestFramework::TestSuite& suite) {
    AVLTree tree;
    ASSERT_NULL(tree.root);
    ASSERT_TRUE(tree.isEmpty());
    ASSERT_EQ(tree.getHeightOfTree(), 0);
    ASSERT_NULL(tree.successor(nullptr));
    ASSERT_NULL(tree.predecessor(nullptr));
}

void testDuplicateInsert(TestFramework::TestSuite& suite) {
    AVLTree tree;
    LimitNode* node1 = tree.insert(100);
    ASSERT_NOT_NULL(node1);
    ASSERT_EQ(node1->price, 100);
    ASSERT_EQ(tree.getHeightOfTree(), 1);

    // Duplicate insert should return existing node
    LimitNode* node2 = tree.insert(100);
    ASSERT_EQ(node1, node2);
    ASSERT_EQ(tree.getHeightOfTree(), 1);
    ASSERT_FALSE(tree.isEmpty());

    std::string err;
    ASSERT_TRUE_MSG(checkTree(tree, err), err);
}

void testRotations(TestFramework::TestSuite& suite) {
    // 1. Right rotation (Left-Left case): 30, 20, 10
    {
        AVLTree tree;
        tree.insert(30);
        tree.insert(20);
        tree.insert(10);
        ASSERT_NOT_NULL(tree.root);
        ASSERT_EQ(tree.root->price, 20);
        ASSERT_NOT_NULL(tree.root->left);
        ASSERT_EQ(tree.root->left->price, 10);
        ASSERT_NOT_NULL(tree.root->right);
        ASSERT_EQ(tree.root->right->price, 30);
        ASSERT_EQ(tree.getHeightOfTree(), 2);

        std::string err;
        ASSERT_TRUE_MSG(checkTree(tree, err), err);
    }

    // 2. Left rotation (Right-Right case): 10, 20, 30
    {
        AVLTree tree;
        tree.insert(10);
        tree.insert(20);
        tree.insert(30);
        ASSERT_NOT_NULL(tree.root);
        ASSERT_EQ(tree.root->price, 20);
        ASSERT_NOT_NULL(tree.root->left);
        ASSERT_EQ(tree.root->left->price, 10);
        ASSERT_NOT_NULL(tree.root->right);
        ASSERT_EQ(tree.root->right->price, 30);
        ASSERT_EQ(tree.getHeightOfTree(), 2);

        std::string err;
        ASSERT_TRUE_MSG(checkTree(tree, err), err);
    }

    // 3. Left-Right rotation: 30, 10, 20
    {
        AVLTree tree;
        tree.insert(30);
        tree.insert(10);
        tree.insert(20);
        ASSERT_NOT_NULL(tree.root);
        ASSERT_EQ(tree.root->price, 20);
        ASSERT_NOT_NULL(tree.root->left);
        ASSERT_EQ(tree.root->left->price, 10);
        ASSERT_NOT_NULL(tree.root->right);
        ASSERT_EQ(tree.root->right->price, 30);
        ASSERT_EQ(tree.getHeightOfTree(), 2);

        std::string err;
        ASSERT_TRUE_MSG(checkTree(tree, err), err);
    }

    // 4. Right-Left rotation: 10, 30, 20
    {
        AVLTree tree;
        tree.insert(10);
        tree.insert(30);
        tree.insert(20);
        ASSERT_NOT_NULL(tree.root);
        ASSERT_EQ(tree.root->price, 20);
        ASSERT_NOT_NULL(tree.root->left);
        ASSERT_EQ(tree.root->left->price, 10);
        ASSERT_NOT_NULL(tree.root->right);
        ASSERT_EQ(tree.root->right->price, 30);
        ASSERT_EQ(tree.getHeightOfTree(), 2);

        std::string err;
        ASSERT_TRUE_MSG(checkTree(tree, err), err);
    }
}

void testSuccessorAndPredecessor(TestFramework::TestSuite& suite) {
    AVLTree tree;
    // Insert values: 10, 20, 30, 40, 50, 60, 70
    std::vector<int> prices = {40, 20, 60, 10, 30, 50, 70};
    for (int p : prices) {
        tree.insert(p);
    }

    std::string err;
    ASSERT_TRUE_MSG(checkTree(tree, err), err);

    // Find min node (10)
    LimitNode* curr = tree.root;
    while (curr->left) curr = curr->left;
    ASSERT_EQ(curr->price, 10);
    ASSERT_NULL(tree.predecessor(curr));

    // Follow successors
    std::vector<int> expectedInOrder = {10, 20, 30, 40, 50, 60, 70};
    for (size_t i = 0; i < expectedInOrder.size(); ++i) {
        ASSERT_EQ(curr->price, expectedInOrder[i]);
        if (i + 1 < expectedInOrder.size()) {
            LimitNode* succ = tree.successor(curr);
            ASSERT_NOT_NULL(succ);
            ASSERT_EQ(tree.predecessor(succ), curr);
            curr = succ;
        } else {
            ASSERT_NULL(tree.successor(curr));
        }
    }

    // Foreign node returns nullptr
    LimitNode foreignNode(999);
    ASSERT_NULL(tree.successor(&foreignNode));
    ASSERT_NULL(tree.predecessor(&foreignNode));
}

void testRemove(TestFramework::TestSuite& suite) {
    AVLTree tree;
    LimitNode* n20 = tree.insert(20);
    LimitNode* n10 = tree.insert(10);
    LimitNode* n30 = tree.insert(30);
    LimitNode* n25 = tree.insert(25);
    LimitNode* n40 = tree.insert(40);

    // 1. Cannot remove a non-empty price level
    Order dummyOrder(1, 10, 100, 1000LL, Side::Buy);
    n10->queue.pushBack(&dummyOrder, n10);
    ASSERT_THROWS(tree.remove(n10), std::logic_error);

    // Empty it again
    n10->queue.popFront();

    // 2. Remove leaf node (25)
    tree.remove(n25);
    std::string err;
    ASSERT_TRUE_MSG(checkTree(tree, err), err);
    ASSERT_NULL(tree.successor(n20)->left);

    // 3. Remove node with single child (30 has child 40)
    tree.remove(n30);
    ASSERT_TRUE_MSG(checkTree(tree, err), err);
    ASSERT_EQ(tree.successor(n20), n40);

    // 4. Remove node with two children (root 20 has children 10 and 40)
    tree.remove(n20);
    ASSERT_TRUE_MSG(checkTree(tree, err), err);
    ASSERT_EQ(tree.root->price, 40);

    // 5. Remove remaining nodes until tree is empty
    tree.remove(n10);
    ASSERT_TRUE_MSG(checkTree(tree, err), err);
    tree.remove(n40);
    ASSERT_TRUE(tree.isEmpty());
    ASSERT_NULL(tree.root);
    ASSERT_EQ(tree.getHeightOfTree(), 0);

    // 6. Removing foreign node is safe no-op
    LimitNode foreignNode(999);
    ASSERT_NO_THROW(tree.remove(&foreignNode));
    ASSERT_NO_THROW(tree.remove(nullptr));
}

void testStressBalance(TestFramework::TestSuite& suite) {
    AVLTree tree;
    std::vector<int> values;
    for (int i = 1; i <= 63; ++i) {
        values.push_back(i);
    }
    // Deterministic shuffle
    for (size_t i = 0; i < values.size(); ++i) {
        size_t j = (i * 37 + 13) % values.size();
        std::swap(values[i], values[j]);
    }

    std::vector<LimitNode*> insertedNodes;
    for (int v : values) {
        LimitNode* node = tree.insert(v);
        insertedNodes.push_back(node);
        std::string err;
        ASSERT_TRUE_MSG(checkTree(tree, err), err);
    }

    // Remove in another order
    for (size_t i = 0; i < insertedNodes.size(); ++i) {
        size_t idx = (i * 17 + 5) % insertedNodes.size();
        if (insertedNodes[idx]) {
            tree.remove(insertedNodes[idx]);
            insertedNodes[idx] = nullptr;
            std::string err;
            ASSERT_TRUE_MSG(checkTree(tree, err), err);
        }
    }
    for (LimitNode* node : insertedNodes) {
        if (node) {
            tree.remove(node);
            std::string err;
            ASSERT_TRUE_MSG(checkTree(tree, err), err);
        }
    }

    ASSERT_TRUE(tree.isEmpty());
}

bool runAll(TestFramework::TestSuite& suite) {
    suite.runTest("AVLTree: Empty Tree State", [&]() { testEmptyTree(suite); });
    suite.runTest("AVLTree: Duplicate Insert", [&]() { testDuplicateInsert(suite); });
    suite.runTest("AVLTree: Rotations (LL, RR, LR, RL)", [&]() { testRotations(suite); });
    suite.runTest("AVLTree: Successor and Predecessor", [&]() { testSuccessorAndPredecessor(suite); });
    suite.runTest("AVLTree: Node Removal Variations", [&]() { testRemove(suite); });
    suite.runTest("AVLTree: Stress Balance and Invariants", [&]() { testStressBalance(suite); });
    return suite.allPassed();
}

} // namespace TestAVLTree

#ifdef STANDALONE_TEST_AVL_TREE
int main() {
    TestFramework::TestSuite suite("AVLTree Unit Tests");
    TestAVLTree::runAll(suite);
    suite.printSummary();
    return suite.allPassed() ? 0 : 1;
}
#endif

