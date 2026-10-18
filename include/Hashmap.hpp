#ifndef HASH_MAP_HPP
#define HASH_MAP_HPP
#include <type_traits>

// A collision-chain entry. Pointer-valued values remain owned by their caller.
template <typename K, typename V>
struct HashNode {
    K key;
    V value;
    HashNode<K, V>* next;

    HashNode(const K& k, const V& v);
};

// Separate chaining with integral keys and expected O(1) lookup.
template <typename K, typename V>
class HashMap {
    static_assert(std::is_integral<K>::value && !std::is_same<K, bool>::value,
                  "HashMap supports integral keys other than bool (up to 64 bits)");
    static_assert(sizeof(K) <= 8, "HashMap supports keys up to 64 bits");

public:
    explicit HashMap(int initialCapacity = 16);
    ~HashMap();
    HashMap(const HashMap&) = delete;
    HashMap& operator=(const HashMap&) = delete;

    void insert(const K& key, const V& value);
    V get(const K& key); // missing: V{}, nullptr for pointers
    bool contains(const K& key);
    void remove(const K& key);

    int getSize();
    int getCapacity();
    double loadFactor();

private:
    HashNode<K, V>** buckets;
    int capacity;
    int count;

    static constexpr double LOAD_FACTOR_THRESHOLD = 0.75;

    unsigned int hashKey(const K& key);
    void resize();
};

// Template definitions must be visible in every translation unit using HashMap.
#include "./Hashmap.tpp"
#endif
