"""
================================================================================
LLD: LRU Cache
================================================================================

Functional Requirements:
    1. get(key): return the value for a key if present, in O(1); accessing a
       key marks it as most-recently-used. Return None if the key is absent.
    2. put(key, value): insert or update a key's value in O(1); the key
       becomes most-recently-used.
    3. The cache has a fixed capacity. When a new key is inserted while the
       cache is full, the least-recently-used entry is evicted to make room.
    4. Support arbitrary key/value types (generic cache).

Non-Functional Requirements:
    1. O(1) average time for both get() and put().
    2. Thread-safety.
    3. Maintainability: recency bookkeeping (the linked list) stays cleanly
       separated from key lookup (the dict).

Design:
    Node is a doubly linked list node holding one key/value pair plus
    prev/next pointers.

    LRUCache combines a dict[key -> Node] for O(1) key lookup with a doubly
    linked list, threaded through two sentinel nodes (head/tail), that
    tracks recency: the node right after head is the most-recently-used
    entry, the node right before tail is the least-recently-used entry.

    get(key) looks the node up in the dict, unlinks it, and re-links it
    right after head (_move_to_front). put(key, value) does the same for an
    existing key; for a new key, if the dict is already at capacity it first
    evicts the node right before tail (the LRU entry) from both the list and
    the dict, then links the new node in at the front. All public operations
    run under a single threading.Lock so the dict and the list never
    observe each other mid-update.

Core Entities:
    Node
    LRUCache
================================================================================
"""

import threading
from typing import Dict, Generic, List, Optional, TypeVar

K = TypeVar("K")
V = TypeVar("V")


class Node(Generic[K, V]):
    def __init__(self, key: Optional[K], value: Optional[V]):
        self.key = key
        self.value = value
        self.prev: Optional["Node[K, V]"] = None
        self.next: Optional["Node[K, V]"] = None


class LRUCache(Generic[K, V]):
    def __init__(self, capacity: int):
        if capacity <= 0:
            raise ValueError("capacity must be positive")
        self._capacity = capacity
        self._map: Dict[K, Node[K, V]] = {}
        self._head: Node[K, V] = Node(None, None)  # sentinel, head.next = MRU
        self._tail: Node[K, V] = Node(None, None)  # sentinel, tail.prev = LRU
        self._head.next = self._tail
        self._tail.prev = self._head
        self._lock = threading.Lock()

    def _unlink(self, node: Node[K, V]) -> None:
        node.prev.next = node.next
        node.next.prev = node.prev

    def _link_to_front(self, node: Node[K, V]) -> None:
        node.next = self._head.next
        node.prev = self._head
        self._head.next.prev = node
        self._head.next = node

    def _move_to_front(self, node: Node[K, V]) -> None:
        self._unlink(node)
        self._link_to_front(node)

    def get(self, key: K) -> Optional[V]:
        with self._lock:
            node = self._map.get(key)
            if node is None:
                return None
            self._move_to_front(node)
            return node.value

    def put(self, key: K, value: V) -> None:
        with self._lock:
            existing = self._map.get(key)
            if existing is not None:
                existing.value = value
                self._move_to_front(existing)
                return
            if len(self._map) == self._capacity:
                lru = self._tail.prev
                self._unlink(lru)
                del self._map[lru.key]
            node = Node(key, value)
            self._link_to_front(node)
            self._map[key] = node

    def size(self) -> int:
        with self._lock:
            return len(self._map)

    def keys_mru_to_lru(self) -> List[K]:
        """Debug/demo helper: keys from most- to least-recently-used."""
        with self._lock:
            keys: List[K] = []
            current = self._head.next
            while current is not self._tail:
                keys.append(current.key)
                current = current.next
            return keys


def main() -> None:
    print("Building an LRU cache with capacity 3")
    cache: LRUCache[int, str] = LRUCache(3)

    cache.put(1, "A")
    print(f"  put(1, A) -> order (MRU..LRU): {cache.keys_mru_to_lru()}")
    cache.put(2, "B")
    print(f"  put(2, B) -> order (MRU..LRU): {cache.keys_mru_to_lru()}")
    cache.put(3, "C")
    print(f"  put(3, C) -> order (MRU..LRU): {cache.keys_mru_to_lru()}")

    print("\nAccessing key 1 marks it most-recently-used:")
    v1 = cache.get(1)
    print(f"  get(1) -> {v1 if v1 is not None else 'NOT FOUND'}, order: {cache.keys_mru_to_lru()}")

    print("\nInserting key 4 while at capacity evicts the least-recently-used key (2):")
    cache.put(4, "D")
    print(f"  put(4, D) -> order (MRU..LRU): {cache.keys_mru_to_lru()}")

    print("\nLooking up the evicted key 2:")
    v2 = cache.get(2)
    print(f"  get(2) -> {v2 if v2 is not None else 'NOT FOUND'}")

    print("\nUpdating existing key 3 refreshes its value and moves it to the front:")
    cache.put(3, "C-updated")
    print(f"  put(3, C-updated) -> order (MRU..LRU): {cache.keys_mru_to_lru()}")

    print(f"\nFinal cache size: {cache.size()}")


if __name__ == "__main__":
    main()
