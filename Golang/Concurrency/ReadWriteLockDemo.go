package main

import (
	"fmt"
	"sync"
	"time"
)

type SharedCounter struct {
	value int
	mu    sync.RWMutex
}

func (c *SharedCounter) Read(name string) int {
	c.mu.RLock()
	defer c.mu.RUnlock()
	fmt.Printf("%s reading: %d\n", name, c.value)
	time.Sleep(100 * time.Millisecond) // simulate read taking some time
	return c.value
}

func (c *SharedCounter) Write(name string, newValue int) {
	c.mu.Lock()
	defer c.mu.Unlock()
	fmt.Printf("%s WRITING: %d\n", name, newValue)
	time.Sleep(100 * time.Millisecond)
	c.value = newValue
}

func main() {
	counter := &SharedCounter{}
	var wg sync.WaitGroup

	// 4 readers — watch their timestamps overlap
	for i := 0; i < 4; i++ {
		wg.Add(1)
		name := fmt.Sprintf("reader-%d", i+1)
		go func() {
			defer wg.Done()
			counter.Read(name)
		}()
	}

	time.Sleep(20 * time.Millisecond) // let readers start first

	// 1 writer — watch it wait for ALL readers to finish, then block everyone else
	wg.Add(1)
	go func() {
		defer wg.Done()
		counter.Write("writer-1", 99)
	}()

	wg.Wait()
}
