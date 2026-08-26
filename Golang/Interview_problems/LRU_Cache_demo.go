/*
================================================================================
LLD: LRU Cache
================================================================================

Functional Requirements:
    1. Get(key): return the value for a key if present, in O(1); accessing a
       key marks it as most-recently-used. Return (zero value, false) if the
       key is absent.
    2. Put(key, value): insert or update a key's value in O(1); the key
       becomes most-recently-used.
    3. The cache has a fixed capacity. When a new key is inserted while the
       cache is full, the least-recently-used entry is evicted to make room.
    4. Support arbitrary key/value types (a generic cache).

Non-Functional Requirements:
    1. O(1) average time for both Get() and Put().
    2. Thread-safety.
    3. Maintainability: recency bookkeeping (the linked list) stays cleanly
       separated from key lookup (the map).

Design:
    Node[K, V] is a doubly linked list node holding one key/value pair plus
    prev/next pointers.

    LRUCache[K, V] combines a map[K]*Node[K, V] for O(1) key lookup with a
    doubly linked list, threaded through two sentinel nodes (head/tail),
    that tracks recency: the node right after head is the most-recently-used
    entry, the node right before tail is the least-recently-used entry.

    Get(key) looks the node up in the map, unlinks it, and re-links it right
    after head (moveToFront). Put(key, value) does the same for an existing
    key; for a new key, if the map is already at capacity it first evicts
    the node right before tail (the LRU entry) from both the list and the
    map, then links the new node in at the front. All public methods take
    the cache's sync.Mutex so the map and the list never observe each other
    mid-update.

Core Entities:
    Node[K, V]
    LRUCache[K, V]
================================================================================
*/

package main

import (
	"fmt"
	"sync"
)

type Node[K comparable, V any] struct {
	key   K
	value V
	prev  *Node[K, V]
	next  *Node[K, V]
}

type LRUCache[K comparable, V any] struct {
	mu       sync.Mutex
	capacity int
	items    map[K]*Node[K, V]
	head     *Node[K, V] // sentinel, head.next = MRU
	tail     *Node[K, V] // sentinel, tail.prev = LRU
}

func NewLRUCache[K comparable, V any](capacity int) *LRUCache[K, V] {
	if capacity <= 0 {
		panic("capacity must be positive")
	}
	head := &Node[K, V]{}
	tail := &Node[K, V]{}
	head.next = tail
	tail.prev = head
	return &LRUCache[K, V]{
		capacity: capacity,
		items:    make(map[K]*Node[K, V]),
		head:     head,
		tail:     tail,
	}
}

func (c *LRUCache[K, V]) unlink(node *Node[K, V]) {
	node.prev.next = node.next
	node.next.prev = node.prev
}

func (c *LRUCache[K, V]) linkToFront(node *Node[K, V]) {
	node.next = c.head.next
	node.prev = c.head
	c.head.next.prev = node
	c.head.next = node
}

func (c *LRUCache[K, V]) moveToFront(node *Node[K, V]) {
	c.unlink(node)
	c.linkToFront(node)
}

func (c *LRUCache[K, V]) Get(key K) (V, bool) {
	c.mu.Lock()
	defer c.mu.Unlock()
	node, ok := c.items[key]
	if !ok {
		var zero V
		return zero, false
	}
	c.moveToFront(node)
	return node.value, true
}

func (c *LRUCache[K, V]) Put(key K, value V) {
	c.mu.Lock()
	defer c.mu.Unlock()
	if existing, ok := c.items[key]; ok {
		existing.value = value
		c.moveToFront(existing)
		return
	}
	if len(c.items) == c.capacity {
		lru := c.tail.prev
		c.unlink(lru)
		delete(c.items, lru.key)
	}
	node := &Node[K, V]{key: key, value: value}
	c.linkToFront(node)
	c.items[key] = node
}

func (c *LRUCache[K, V]) Size() int {
	c.mu.Lock()
	defer c.mu.Unlock()
	return len(c.items)
}

// KeysMruToLru is a debug/demo helper: keys from most- to least-recently-used.
func (c *LRUCache[K, V]) KeysMruToLru() []K {
	c.mu.Lock()
	defer c.mu.Unlock()
	keys := make([]K, 0, len(c.items))
	for current := c.head.next; current != c.tail; current = current.next {
		keys = append(keys, current.key)
	}
	return keys
}

func main() {
	fmt.Println("Building an LRU cache with capacity 3")
	cache := NewLRUCache[int, string](3)

	cache.Put(1, "A")
	fmt.Printf("  Put(1, A) -> order (MRU..LRU): %v\n", cache.KeysMruToLru())
	cache.Put(2, "B")
	fmt.Printf("  Put(2, B) -> order (MRU..LRU): %v\n", cache.KeysMruToLru())
	cache.Put(3, "C")
	fmt.Printf("  Put(3, C) -> order (MRU..LRU): %v\n", cache.KeysMruToLru())

	fmt.Println("\nAccessing key 1 marks it most-recently-used:")
	v1, ok1 := cache.Get(1)
	if !ok1 {
		v1 = "NOT FOUND"
	}
	fmt.Printf("  Get(1) -> %v, order: %v\n", v1, cache.KeysMruToLru())

	fmt.Println("\nInserting key 4 while at capacity evicts the least-recently-used key (2):")
	cache.Put(4, "D")
	fmt.Printf("  Put(4, D) -> order (MRU..LRU): %v\n", cache.KeysMruToLru())

	fmt.Println("\nLooking up the evicted key 2:")
	v2, ok2 := cache.Get(2)
	if !ok2 {
		v2 = "NOT FOUND"
	}
	fmt.Printf("  Get(2) -> %v\n", v2)

	fmt.Println("\nUpdating existing key 3 refreshes its value and moves it to the front:")
	cache.Put(3, "C-updated")
	fmt.Printf("  Put(3, C-updated) -> order (MRU..LRU): %v\n", cache.KeysMruToLru())

	fmt.Printf("\nFinal cache size: %d\n", cache.Size())
}
