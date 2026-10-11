#ifndef HASH_MAP_HPP
#define HASH_MAP_HPP

template <typename K, typename V>
struct HashNode {
    K key;
    V value;
    HashNode<K, V>* next;

    HashNode(const K& k, const V& v);
};

template <typename K, typename V>
class HashMap {
public:
    explicit HashMap(int initialCapacity = 16);
    ~HashMap();

    void insert(const K& key, const V& value);   // O(1) amortized, overwrites value if key already present
    V get(const K& key);                         // O(1) expected, returns nullptr if not found
    bool contains(const K& key);                 // O(1) expected, check if value is present or not
    void remove(const K& key);                   // O(1) expected; no-op if key not present

    int getSize();                               // O(1) — number of entries currently stored
    int getCapacity();                           // O(1) — number of buckets
    double loadFactor();                         

private:
    HashNode<K, V>** buckets;
    int capacity;
    int count;

    static constexpr double LOAD_FACTOR_THRESHOLD = 0.75;

    unsigned int hashKey(const K& key);    // integer-mixing hash, then % capacity
    void resize();                         // doubles capacity and rehashes every entry
};

#endif
