#ifndef AVL_TREE_HPP
#define AVL_TREE_HPP

#include "LimitNode.hpp"

class AVLTree {
public:
    LimitNode* root;

    // constructors and destructors
    AVLTree();
    ~AVLTree();

    // methods
    LimitNode* insert(int price);   // insert a new price level if not exists
    void remove(LimitNode* node);   // lazy delete a price level after a while

    bool isEmpty();         // --> return root == nullptr;
    int getHeightOfTree();  // --> return root.height;
    
private:
    LimitNode* successor(LimitNode* node);      
    LimitNode* predecessor(LimitNode* node);

    // Rebalancing functions for self balancing
    LimitNode* rotateLeft(LimitNode* node);
    LimitNode* rotateRight(LimitNode* node);
    LimitNode* rebalance(LimitNode* node);

    int getHeight(LimitNode* node);
    int getBalanceFactor(LimitNode* node);
};

#endif