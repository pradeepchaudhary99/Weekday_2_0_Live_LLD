package main

import (
	"fmt"
	"sync"
	"sync/atomic"
)

func main() {
	var unsafeCounter int
	var safeCounter int64

	tasks := make(chan struct{}, 1000)
	for i := 0; i < 1000; i++ {
		tasks <- struct{}{}
	}
	close(tasks)

	var wg sync.WaitGroup
	for i := 0; i < 8; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for range tasks {
				unsafeCounter++                  // NOT atomic — data race
				atomic.AddInt64(&safeCounter, 1) // atomic
			}
		}()
	}
	wg.Wait()

	fmt.Println("unsafe:", unsafeCounter)                   // almost never 1000
	fmt.Println("safe:  ", atomic.LoadInt64(&safeCounter)) // always 1000
}
