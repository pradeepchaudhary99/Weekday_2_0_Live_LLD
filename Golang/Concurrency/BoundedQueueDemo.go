package main

import (
	"fmt"
	"sync"
	"time"
)

type BoundedQueue struct {
	mu       sync.Mutex
	cond     *sync.Cond
	items    []string
	capacity int
}

func NewBoundedQueue(capacity int) *BoundedQueue {
	q := &BoundedQueue{capacity: capacity}
	q.cond = sync.NewCond(&q.mu)
	return q
}

func (q *BoundedQueue) Put(name, item string) {
	q.mu.Lock()
	defer q.mu.Unlock()
	for len(q.items) == q.capacity { // full — wait for a taker
		fmt.Printf("%s sees FULL (%d/%d), waiting...\n", name, len(q.items), q.capacity)
		q.cond.Wait()
	}
	q.items = append(q.items, item)
	fmt.Printf("%s put: %s [size=%d]\n", name, item, len(q.items))
	q.cond.Broadcast() // wake possible waiting takers (and putters)
}

func (q *BoundedQueue) Take(name string) string {
	q.mu.Lock()
	defer q.mu.Unlock()
	for len(q.items) == 0 { // empty — wait for a putter
		fmt.Printf("%s sees EMPTY, waiting...\n", name)
		q.cond.Wait()
	}
	item := q.items[0]
	q.items = q.items[1:]
	fmt.Printf("%s took: %s [size=%d]\n", name, item, len(q.items))
	q.cond.Broadcast()
	return item
}

func producer(wg *sync.WaitGroup, queue *BoundedQueue, name string) {
	defer wg.Done()
	for i := 0; i < 5; i++ {
		queue.Put(name, fmt.Sprintf("%s-item%d", name, i))
		time.Sleep(30 * time.Millisecond)
	}
}

func consumer(wg *sync.WaitGroup, queue *BoundedQueue, name string) {
	defer wg.Done()
	for i := 0; i < 5; i++ {
		queue.Take(name)
		time.Sleep(60 * time.Millisecond)
	}
}

func main() {
	queue := NewBoundedQueue(3) // capacity 3, not 1

	var wg sync.WaitGroup
	wg.Add(4)
	go producer(&wg, queue, "producer-1")
	go producer(&wg, queue, "producer-2")
	go consumer(&wg, queue, "consumer-1")
	go consumer(&wg, queue, "consumer-2")
	wg.Wait()
}
