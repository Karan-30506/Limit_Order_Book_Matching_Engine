#ifndef HASH_MAP_TPP
#define HASH_MAP_TPP
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>

template <typename K, typename V>
HashNode<K, V>::HashNode(const K& k, const V& v) : key(k), value(v), next(nullptr) {
}

template <typename K, typename V>
HashMap<K, V>::HashMap(int initialCapacity)
    : buckets(nullptr), capacity(initialCapacity), count(0) {
    if (capacity <= 0) {
        throw std::invalid_argument("hash capacity must be positive");
    }
    buckets = new HashNode<K, V>* [static_cast<std::size_t>(capacity)] {};
}

template <typename K, typename V>
HashMap<K, V>::~HashMap() {
    // Delete only chain entries; pointer-valued objects belong to their external owners.
    for (int i = 0; i < capacity; ++i) {
        auto* node = buckets[i];
        while (node) {
            auto* next = node->next;
            delete node;
            node = next;
        }
    }
    delete[] buckets;
}

template <typename K, typename V>
unsigned int HashMap<K, V>::hashKey(const K& key) {
    // Unsigned conversion defines negative-key behavior; all operations wrap safely.
    std::uint64_t mixedKey = static_cast<std::uint64_t>(key);
    mixedKey ^= mixedKey >> 30;
    mixedKey *= UINT64_C(0xbf58476d1ce4e5b9);
    mixedKey ^= mixedKey >> 27;
    mixedKey *= UINT64_C(0x94d049bb133111eb);
    mixedKey ^= mixedKey >> 31;
    return static_cast<unsigned int>(mixedKey % static_cast<std::uint64_t>(capacity));
}

template <typename K, typename V>
void HashMap<K, V>::insert(const K& key, const V& value) {
    auto index = hashKey(key);
    for (auto* node = buckets[index]; node; node = node->next) {
        if (node->key == key) {
            node->value = value;
            return;
        }
    }
    // Keep the new entry locally owned if allocating a larger bucket array fails.
    auto node = std::make_unique<HashNode<K, V>>(key, value);
    if ((static_cast<double>(count) + 1) / capacity > LOAD_FACTOR_THRESHOLD) {
        resize();
    }
    // Resizing changes the modulus; recompute the destination bucket afterward.
    index = hashKey(key);
    node->next = buckets[index];
    buckets[index] = node.release();
    ++count;
}

template <typename K, typename V>
V HashMap<K, V>::get(const K& key) {
    for (auto* node = buckets[hashKey(key)]; node; node = node->next) {
        if (node->key == key) {
            return node->value;
        }
    }
    return V{};
}

template <typename K, typename V>
bool HashMap<K, V>::contains(const K& key) {
    for (auto* node = buckets[hashKey(key)]; node; node = node->next) {
        if (node->key == key) {
            return true;
        }
    }
    return false;
}

template <typename K, typename V>
void HashMap<K, V>::remove(const K& key) {
    // Point at the incoming link so removing a chain head and a middle node is identical.
    auto** chainLink = &buckets[hashKey(key)];
    while (*chainLink) {
        if ((*chainLink)->key == key) {
            auto* removedNode = *chainLink;
            *chainLink = removedNode->next;
            delete removedNode;
            --count;
            return;
        }
        chainLink = &(*chainLink)->next;
    }
}

template <typename K, typename V>
int HashMap<K, V>::getSize() {
    return count;
}

template <typename K, typename V>
int HashMap<K, V>::getCapacity() {
    return capacity;
}

template <typename K, typename V>
double HashMap<K, V>::loadFactor() {
    return static_cast<double>(count) / capacity;
}

template <typename K, typename V>
void HashMap<K, V>::resize() {
    if (capacity > std::numeric_limits<int>::max() / 2) {
        throw std::length_error("hash capacity overflow");
    }
    const int oldCapacity = capacity;
    const int newCapacity = capacity * 2;
    // Allocate before changing table state; subsequent relinking does not allocate.
    auto** newBuckets = new HashNode<K, V>* [static_cast<std::size_t>(newCapacity)] {};
    capacity = newCapacity;
    for (int i = 0; i < oldCapacity; ++i) {
        auto* node = buckets[i];
        while (node) {
            // Save the old successor before rewiring next into the new bucket chain.
            auto* next = node->next;
            const auto index = hashKey(node->key);
            node->next = newBuckets[index];
            newBuckets[index] = node;
            node = next;
        }
    }
    delete[] buckets;
    buckets = newBuckets;
}
#endif
