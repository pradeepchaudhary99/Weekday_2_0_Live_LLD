/*
================================================================================
LLD: LRU Cache
================================================================================

Functional Requirements:
    1. get(key): return the value for a key if present, in O(1); accessing a
       key marks it as most-recently-used. Return an absent/empty result if
       the key is not in the cache.
    2. put(key, value): insert or update a key's value in O(1); the key
       becomes most-recently-used.
    3. The cache has a fixed capacity. When a new key is inserted while the
       cache is full, the least-recently-used entry is evicted to make room.
    4. Support arbitrary key/value types (generic cache).

Non-Functional Requirements:
    1. O(1) average time for both get() and put().
    2. Thread-safety.
    3. Maintainability: recency bookkeeping (the linked list) stays cleanly
       separated from key lookup (the hash map).

Design:
    Node<K, V> is a doubly linked list node holding one key/value pair plus
    prev/next pointers.

    LRUCache<K, V> combines a HashMap<K, Node<K, V>> for O(1) key lookup with
    a doubly linked list, threaded through two sentinel nodes (head/tail),
    that tracks recency: the node right after head is the most-recently-used
    entry, the node right before tail is the least-recently-used entry.

    get(key) looks the node up in the map, unlinks it, and re-links it right
    after head (moveToFront). put(key, value) does the same for an existing
    key; for a new key, if the map is already at capacity it first evicts the
    node right before tail (the LRU entry) from both the list and the map,
    then links the new node in at the front. All public operations run under
    a single lock object so the map and the list never observe each other
    mid-update.

Core Entities:
    Node<K, V>
    LRUCache<K, V>
================================================================================
*/

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;

class Node<K, V> {
    K key;
    V value;
    Node<K, V> prev;
    Node<K, V> next;

    Node(K key, V value) {
        this.key = key;
        this.value = value;
    }
}

class LRUCache<K, V> {
    private final int capacity;
    private final Map<K, Node<K, V>> map = new HashMap<>();
    private final Node<K, V> head = new Node<>(null, null); // sentinel, head.next = MRU
    private final Node<K, V> tail = new Node<>(null, null); // sentinel, tail.prev = LRU
    private final Object lock = new Object();

    LRUCache(int capacity) {
        if (capacity <= 0) {
            throw new IllegalArgumentException("capacity must be positive");
        }
        this.capacity = capacity;
        head.next = tail;
        tail.prev = head;
    }

    private void unlink(Node<K, V> node) {
        node.prev.next = node.next;
        node.next.prev = node.prev;
    }

    private void linkToFront(Node<K, V> node) {
        node.next = head.next;
        node.prev = head;
        head.next.prev = node;
        head.next = node;
    }

    private void moveToFront(Node<K, V> node) {
        unlink(node);
        linkToFront(node);
    }

    Optional<V> get(K key) {
        synchronized (lock) {
            Node<K, V> node = map.get(key);
            if (node == null) {
                return Optional.empty();
            }
            moveToFront(node);
            return Optional.of(node.value);
        }
    }

    void put(K key, V value) {
        synchronized (lock) {
            Node<K, V> existing = map.get(key);
            if (existing != null) {
                existing.value = value;
                moveToFront(existing);
                return;
            }
            if (map.size() == capacity) {
                Node<K, V> lru = tail.prev;
                unlink(lru);
                map.remove(lru.key);
            }
            Node<K, V> node = new Node<>(key, value);
            linkToFront(node);
            map.put(key, node);
        }
    }

    int size() {
        synchronized (lock) {
            return map.size();
        }
    }

    /** Debug/demo helper: keys from most- to least-recently-used. */
    List<K> keysMruToLru() {
        synchronized (lock) {
            List<K> keys = new ArrayList<>();
            Node<K, V> current = head.next;
            while (current != tail) {
                keys.add(current.key);
                current = current.next;
            }
            return keys;
        }
    }
}

public class LRU_Cache_demo {
    public static void main(String[] args) {
        System.out.println("Building an LRU cache with capacity 3");
        LRUCache<Integer, String> cache = new LRUCache<>(3);

        cache.put(1, "A");
        System.out.println("  put(1, A) -> order (MRU..LRU): " + cache.keysMruToLru());
        cache.put(2, "B");
        System.out.println("  put(2, B) -> order (MRU..LRU): " + cache.keysMruToLru());
        cache.put(3, "C");
        System.out.println("  put(3, C) -> order (MRU..LRU): " + cache.keysMruToLru());

        System.out.println("\nAccessing key 1 marks it most-recently-used:");
        Optional<String> v1 = cache.get(1);
        System.out.println("  get(1) -> " + v1.orElse("NOT FOUND") + ", order: " + cache.keysMruToLru());

        System.out.println("\nInserting key 4 while at capacity evicts the least-recently-used key (2):");
        cache.put(4, "D");
        System.out.println("  put(4, D) -> order (MRU..LRU): " + cache.keysMruToLru());

        System.out.println("\nLooking up the evicted key 2:");
        Optional<String> v2 = cache.get(2);
        System.out.println("  get(2) -> " + v2.orElse("NOT FOUND"));

        System.out.println("\nUpdating existing key 3 refreshes its value and moves it to the front:");
        cache.put(3, "C-updated");
        System.out.println("  put(3, C-updated) -> order (MRU..LRU): " + cache.keysMruToLru());

        System.out.println("\nFinal cache size: " + cache.size());
    }
}
