#include "AVLTree.hpp"
#include <algorithm>
#include <stdexcept>

AVLTree::AVLTree() : root(nullptr) {
}

AVLTree::~AVLTree() {
    destroy(root);
}

void AVLTree::destroy(LimitNode *node) {
    if (!node) {
        return;
    }
    // Postorder traversal frees children before their parent; queues only detach orders.
    destroy(node->left);
    destroy(node->right);
    delete node;
}

bool AVLTree::isEmpty() {
    return root == nullptr;
}

int AVLTree::getHeight(LimitNode *node) {
    return node ? node->height : 0;
}

int AVLTree::getHeightOfTree() {
    return getHeight(root);
}

int AVLTree::getBalanceFactor(LimitNode *node) {
    return node ? getHeight(node->left) - getHeight(node->right) : 0;
}

bool AVLTree::owns(LimitNode *node) {
    if (!node) {
        return false;
    }
    LimitNode *current = root;
    while (current && current->price != node->price) {
        current = node->price < current->price ? current->left : current->right;
    }
    // A foreign live node may share the price, so key equality alone is insufficient.
    return current == node;
}

void AVLTree::replace(LimitNode *oldNode, LimitNode *newNode) {
    // Reconnect the subtree's incoming edge, including the special root case.
    if (!oldNode->parent) {
        root = newNode;
    } else if (oldNode == oldNode->parent->left) {
        oldNode->parent->left = newNode;
    } else {
        oldNode->parent->right = newNode;
    }
    if (newNode) {
        newNode->parent = oldNode->parent;
    }
}

LimitNode *AVLTree::rotateLeft(LimitNode *node) {
    LimitNode *rightChild = node->right;
    replace(node, rightChild);
    // The promoted child's inner subtree becomes the demoted node's outer subtree.
    node->right = rightChild->left;
    if (node->right) {
        node->right->parent = node;
    }
    rightChild->left = node;
    node->parent = rightChild;
    // Recompute bottom-up: the promoted node depends on the demoted node's height.
    node->height = 1 + std::max(getHeight(node->left), getHeight(node->right));
    rightChild->height = 1 + std::max(getHeight(rightChild->left), getHeight(rightChild->right));
    return rightChild;
}

LimitNode *AVLTree::rotateRight(LimitNode *node) {
    // Mirror of rotateLeft; all node objects and their queues retain their identity.
    LimitNode *leftChild = node->left;
    replace(node, leftChild);
    node->left = leftChild->right;
    if (node->left) {
        node->left->parent = node;
    }
    leftChild->right = node;
    node->parent = leftChild;
    node->height = 1 + std::max(getHeight(node->left), getHeight(node->right));
    leftChild->height = 1 + std::max(getHeight(leftChild->left), getHeight(leftChild->right));
    return leftChild;
}

LimitNode *AVLTree::rebalance(LimitNode *node) {
    node->height = 1 + std::max(getHeight(node->left), getHeight(node->right));
    if (getBalanceFactor(node) > 1) {
        // Left-right case first becomes left-left, then a right rotation repairs it.
        if (getBalanceFactor(node->left) < 0) {
            rotateLeft(node->left);
        }
        return rotateRight(node);
    }
    if (getBalanceFactor(node) < -1) {
        // Right-left case first becomes right-right, then a left rotation repairs it.
        if (getBalanceFactor(node->right) > 0) {
            rotateRight(node->right);
        }
        return rotateLeft(node);
    }
    return node;
}

void AVLTree::rebalanceUp(LimitNode *node) {
    // A rotation can change the local root, so continue from its returned parent.
    while (node) {
        node = rebalance(node)->parent;
    }
}

LimitNode *AVLTree::insert(int price) {
    LimitNode *parent = nullptr;
    LimitNode *node = root;
    while (node) {
        if (price == node->price) {
            return node;
        }
        parent = node;
        node = price < node->price ? node->left : node->right;
    }
    node = new LimitNode(price);
    node->parent = parent;
    if (!parent) {
        root = node;
    } else if (price < parent->price) {
        parent->left = node;
    } else {
        parent->right = node;
    }
    rebalanceUp(parent);
    return node;
}

LimitNode *AVLTree::successor(LimitNode *node) {
    if (!owns(node)) {
        return nullptr;
    }
    if (node->right) {
        node = node->right;
        while (node->left) {
            node = node->left;
        }
        return node;
    }
    // Without a right subtree, find the first ancestor reached from its left child.
    while (node->parent && node == node->parent->right) {
        node = node->parent;
    }
    return node->parent;
}

LimitNode *AVLTree::predecessor(LimitNode *node) {
    if (!owns(node)) {
        return nullptr;
    }
    if (node->left) {
        node = node->left;
        while (node->right) {
            node = node->right;
        }
        return node;
    }
    // Symmetrically, a predecessor is the first ancestor reached from its right child.
    while (node->parent && node == node->parent->left) {
        node = node->parent;
    }
    return node->parent;
}

void AVLTree::remove(LimitNode *node) {
    if (!owns(node)) {
        return;
    }
    if (!node->isEmpty()) {
        throw std::logic_error("cannot remove a nonempty price level");
    }
    LimitNode *start = node->parent;
    if (!node->left || !node->right) {
        // Zero or one child can directly replace the removed node.
        replace(node, node->left ? node->left : node->right);
    } else {
        // Transplant the successor object: copying its price/queue would break aliases.
        LimitNode *successorNode = node->right;
        while (successorNode->left) {
            successorNode = successorNode->left;
        }
        if (successorNode->parent != node) {
            // Close the successor's old position before attaching the target's subtrees.
            start = successorNode->parent;
            replace(successorNode, successorNode->right);
            successorNode->right = node->right;
            successorNode->right->parent = successorNode;
        } else {
            start = successorNode;
        }
        replace(node, successorNode);
        successorNode->left = node->left;
        successorNode->left->parent = successorNode;
        // Rebalancing from the successor's old parent reaches the transplanted node.
    }
    delete node;
    rebalanceUp(start);
}
