/*
================================================================================
LLD: LRU Cache
================================================================================

Functional Requirements:
    1. get(key): return the value for a key if present, in O(1); accessing a
       key marks it as most-recently-used. Return null if the key is absent.
    2. put(key, value): insert or update a key's value in O(1); the key
       becomes most-recently-used.
    3. The cache has a fixed capacity. When a new key is inserted while the
       cache is full, the least-recently-used entry is evicted to make room.
    4. Support arbitrary key/value types (generic cache).

Non-Functional Requirements:
    1. O(1) average time for both get() and put().
    2. Thread-safety (moot for Node's single-threaded event loop, but the
       design would carry over to a worker-thread model - no locking here).
    3. Maintainability: recency bookkeeping (the linked list) stays cleanly
       separated from key lookup (the Map).

Design:
    Node is a doubly linked list node holding one key/value pair plus
    prev/next pointers.

    LRUCache combines a Map<key, Node> for O(1) key lookup with a doubly
    linked list, threaded through two sentinel nodes (head/tail), that
    tracks recency: the node right after head is the most-recently-used
    entry, the node right before tail is the least-recently-used entry.

    get(key) looks the node up in the Map, unlinks it, and re-links it
    right after head (_moveToFront). put(key, value) does the same for an
    existing key; for a new key, if the Map is already at capacity it first
    evicts the node right before tail (the LRU entry) from both the list and
    the Map, then links the new node in at the front.

Core Entities:
    Node
    LRUCache
================================================================================
*/

class Node {
    constructor(key, value) {
        this.key = key;
        this.value = value;
        this.prev = null;
        this.next = null;
    }
}

class LRUCache {
    constructor(capacity) {
        if (capacity <= 0) {
            throw new Error("capacity must be positive");
        }
        this._capacity = capacity;
        this._map = new Map();
        this._head = new Node(null, null); // sentinel, head.next = MRU
        this._tail = new Node(null, null); // sentinel, tail.prev = LRU
        this._head.next = this._tail;
        this._tail.prev = this._head;
    }

    _unlink(node) {
        node.prev.next = node.next;
        node.next.prev = node.prev;
    }

    _linkToFront(node) {
        node.next = this._head.next;
        node.prev = this._head;
        this._head.next.prev = node;
        this._head.next = node;
    }

    _moveToFront(node) {
        this._unlink(node);
        this._linkToFront(node);
    }

    get(key) {
        const node = this._map.get(key);
        if (!node) {
            return null;
        }
        this._moveToFront(node);
        return node.value;
    }

    put(key, value) {
        const existing = this._map.get(key);
        if (existing) {
            existing.value = value;
            this._moveToFront(existing);
            return;
        }
        if (this._map.size === this._capacity) {
            const lru = this._tail.prev;
            this._unlink(lru);
            this._map.delete(lru.key);
        }
        const node = new Node(key, value);
        this._linkToFront(node);
        this._map.set(key, node);
    }

    size() {
        return this._map.size;
    }

    /** Debug/demo helper: keys from most- to least-recently-used. */
    keysMruToLru() {
        const keys = [];
        let current = this._head.next;
        while (current !== this._tail) {
            keys.push(current.key);
            current = current.next;
        }
        return keys;
    }
}

function main() {
    console.log("Building an LRU cache with capacity 3");
    const cache = new LRUCache(3);

    cache.put(1, "A");
    console.log(`  put(1, A) -> order (MRU..LRU): [${cache.keysMruToLru()}]`);
    cache.put(2, "B");
    console.log(`  put(2, B) -> order (MRU..LRU): [${cache.keysMruToLru()}]`);
    cache.put(3, "C");
    console.log(`  put(3, C) -> order (MRU..LRU): [${cache.keysMruToLru()}]`);

    console.log("\nAccessing key 1 marks it most-recently-used:");
    const v1 = cache.get(1);
    console.log(`  get(1) -> ${v1 ?? "NOT FOUND"}, order: [${cache.keysMruToLru()}]`);

    console.log("\nInserting key 4 while at capacity evicts the least-recently-used key (2):");
    cache.put(4, "D");
    console.log(`  put(4, D) -> order (MRU..LRU): [${cache.keysMruToLru()}]`);

    console.log("\nLooking up the evicted key 2:");
    const v2 = cache.get(2);
    console.log(`  get(2) -> ${v2 ?? "NOT FOUND"}`);

    console.log("\nUpdating existing key 3 refreshes its value and moves it to the front:");
    cache.put(3, "C-updated");
    console.log(`  put(3, C-updated) -> order (MRU..LRU): [${cache.keysMruToLru()}]`);

    console.log(`\nFinal cache size: ${cache.size()}`);
}

main();
