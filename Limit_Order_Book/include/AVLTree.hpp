#ifndef AVL_TREE_HPP
#define AVL_TREE_HPP
#include "LimitNode.hpp"

// Owns price-level nodes. Rotations and deletion preserve surviving node addresses.
class AVLTree {
public:
    LimitNode* root;

    AVLTree();
    ~AVLTree();
    AVLTree(const AVLTree&) = delete;
    AVLTree& operator=(const AVLTree&) = delete;

    // Duplicate prices return the existing level without replacing its queue.
    LimitNode* insert(int price);
    void remove(LimitNode* node); // physical deletion, empty members only
    bool isEmpty();
    int getHeightOfTree();

    // Ordered neighbors; null or live foreign nodes return nullptr.
    LimitNode* successor(LimitNode* node);
    LimitNode* predecessor(LimitNode* node);

private:
    LimitNode* rotateLeft(LimitNode* node);
    LimitNode* rotateRight(LimitNode* node);
    LimitNode* rebalance(LimitNode* node);
    int getHeight(LimitNode* node);
    int getBalanceFactor(LimitNode* node);
    void rebalanceUp(LimitNode* node);
    void replace(LimitNode* oldNode, LimitNode* newNode);
    bool owns(LimitNode* node);
    static void destroy(LimitNode* node);
};
#endif
