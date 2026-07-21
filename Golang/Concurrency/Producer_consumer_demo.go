package main

import (
	"fmt"
	"sync"
)

type BoundedBuffer struct {
	mu       sync.Mutex
	cond     *sync.Cond
	items    []int
	capacity int
}

func NewBoundedBuffer(capacity int) *BoundedBuffer {
	b := &BoundedBuffer{capacity: capacity}
	b.cond = sync.NewCond(&b.mu)
	return b
}

func (b *BoundedBuffer) Put(item int) {
	b.mu.Lock()
	defer b.mu.Unlock()
	for len(b.items) == b.capacity {
		b.cond.Wait()
	}
	b.items = append(b.items, item)
	b.cond.Broadcast()
}

func (b *BoundedBuffer) Take() int {
	b.mu.Lock()
	defer b.mu.Unlock()
	for len(b.items) == 0 {
		b.cond.Wait()
	}
	item := b.items[0]
	b.items = b.items[1:]
	b.cond.Broadcast()
	return item
}

func producer(wg *sync.WaitGroup, buffer *BoundedBuffer) {
	defer wg.Done()
	for i := 0; i < 10; i++ {
		buffer.Put(i)
		fmt.Printf("Produced: %d\n", i)
	}
}

func consumer(wg *sync.WaitGroup, buffer *BoundedBuffer) {
	defer wg.Done()
	for i := 0; i < 10; i++ {
		item := buffer.Take()
		fmt.Printf("Consumed: %d\n", item)
	}
}

func main() {
	buffer := NewBoundedBuffer(5)

	var wg sync.WaitGroup
	wg.Add(2)
	go producer(&wg, buffer)
	go consumer(&wg, buffer)
	wg.Wait()
}
