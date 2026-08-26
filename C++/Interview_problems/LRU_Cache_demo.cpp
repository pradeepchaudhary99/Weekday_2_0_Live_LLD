/*
================================================================================
LLD: LRU Cache
================================================================================

Functional Requirements:
    1. get(key): return the value for a key if present, in O(1); accessing a
       key marks it as most-recently-used. Return std::nullopt if the key is
       absent.
    2. put(key, value): insert or update a key's value in O(1); the key
       becomes most-recently-used.
    3. The cache has a fixed capacity. When a new key is inserted while the
       cache is full, the least-recently-used entry is evicted to make room.
    4. Support arbitrary key/value types (templated cache).

Non-Functional Requirements:
    1. O(1) average time for both get() and put().
    2. Thread-safety.
    3. Maintainability: recency bookkeeping (the linked list) stays cleanly
       separated from key lookup (the hash map). RAII cleans up every node
       the cache still owns when it is destroyed.

Design:
    Node<K, V> is a doubly linked list node holding one key/value pair plus
    raw prev/next pointers. The cache is the sole owner of every Node it
    allocates; the destructor walks the list and deletes them, so no node
    ever leaks even though the list itself is threaded with raw pointers.

    LRUCache<K, V> combines an unordered_map<K, Node<K, V>*> for O(1) key
    lookup with a doubly linked list, threaded through two sentinel nodes
    (head/tail), that tracks recency: the node right after head is the
    most-recently-used entry, the node right before tail is the
    least-recently-used entry.

    get(key) looks the node up in the map, unlinks it, and re-links it right
    after head (moveToFront). put(key, value) does the same for an existing
    key; for a new key, if the map is already at capacity it first evicts
    the node right before tail (the LRU entry) from both the list and the
    map (deleting it), then links the new node in at the front. All public
    operations run under a single std::mutex (via std::lock_guard) so the
    map and the list never observe each other mid-update.

Core Entities:
    Node<K, V>
    LRUCache<K, V>
================================================================================
*/

#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

template <typename K, typename V>
struct Node {
    K key;
    V value;
    Node* prev = nullptr;
    Node* next = nullptr;

    Node(K key, V value) : key(std::move(key)), value(std::move(value)) {}
};

template <typename K, typename V>
class LRUCache {
public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {
        if (capacity_ == 0) {
            throw std::invalid_argument("capacity must be positive");
        }
        head_ = new Node<K, V>(K{}, V{});
        tail_ = new Node<K, V>(K{}, V{});
        head_->next = tail_;
        tail_->prev = head_;
    }

    ~LRUCache() {
        Node<K, V>* current = head_;
        while (current != nullptr) {
            Node<K, V>* next = current->next;
            delete current;
            current = next;
        }
    }

    LRUCache(const LRUCache&) = delete;
    LRUCache& operator=(const LRUCache&) = delete;

    std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it == map_.end()) {
            return std::nullopt;
        }
        moveToFront(it->second);
        return it->second->value;
    }

    void put(const K& key, const V& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->value = value;
            moveToFront(it->second);
            return;
        }
        if (map_.size() == capacity_) {
            Node<K, V>* lru = tail_->prev;
            unlink(lru);
            map_.erase(lru->key);
            delete lru;
        }
        Node<K, V>* node = new Node<K, V>(key, value);
        linkToFront(node);
        map_[key] = node;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return map_.size();
    }

    // Debug/demo helper: keys from most- to least-recently-used.
    std::vector<K> keysMruToLru() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<K> keys;
        Node<K, V>* current = head_->next;
        while (current != tail_) {
            keys.push_back(current->key);
            current = current->next;
        }
        return keys;
    }

private:
    void unlink(Node<K, V>* node) {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    void linkToFront(Node<K, V>* node) {
        node->next = head_->next;
        node->prev = head_;
        head_->next->prev = node;
        head_->next = node;
    }

    void moveToFront(Node<K, V>* node) {
        unlink(node);
        linkToFront(node);
    }

    size_t capacity_;
    std::unordered_map<K, Node<K, V>*> map_;
    Node<K, V>* head_; // sentinel, head_->next = MRU
    Node<K, V>* tail_; // sentinel, tail_->prev = LRU
    mutable std::mutex mutex_;
};

std::string joinKeys(const std::vector<int>& keys) {
    std::string result;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i > 0) result += ", ";
        result += std::to_string(keys[i]);
    }
    return result;
}

int main() {
    std::cout << "Building an LRU cache with capacity 3\n";
    LRUCache<int, std::string> cache(3);

    cache.put(1, "A");
    std::cout << "  put(1, A) -> order (MRU..LRU): [" << joinKeys(cache.keysMruToLru()) << "]\n";
    cache.put(2, "B");
    std::cout << "  put(2, B) -> order (MRU..LRU): [" << joinKeys(cache.keysMruToLru()) << "]\n";
    cache.put(3, "C");
    std::cout << "  put(3, C) -> order (MRU..LRU): [" << joinKeys(cache.keysMruToLru()) << "]\n";

    std::cout << "\nAccessing key 1 marks it most-recently-used:\n";
    auto v1 = cache.get(1);
    std::cout << "  get(1) -> " << (v1 ? *v1 : "NOT FOUND") << ", order: [" << joinKeys(cache.keysMruToLru()) << "]\n";

    std::cout << "\nInserting key 4 while at capacity evicts the least-recently-used key (2):\n";
    cache.put(4, "D");
    std::cout << "  put(4, D) -> order (MRU..LRU): [" << joinKeys(cache.keysMruToLru()) << "]\n";

    std::cout << "\nLooking up the evicted key 2:\n";
    auto v2 = cache.get(2);
    std::cout << "  get(2) -> " << (v2 ? *v2 : "NOT FOUND") << "\n";

    std::cout << "\nUpdating existing key 3 refreshes its value and moves it to the front:\n";
    cache.put(3, "C-updated");
    std::cout << "  put(3, C-updated) -> order (MRU..LRU): [" << joinKeys(cache.keysMruToLru()) << "]\n";

    std::cout << "\nFinal cache size: " << cache.size() << "\n";

    return 0;
}
