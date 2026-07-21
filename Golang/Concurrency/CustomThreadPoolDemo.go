package main

import (
	"fmt"
	"sync"
	"time"
)

type CustomThreadPool struct {
	mu         sync.Mutex
	cond       *sync.Cond
	taskQueue  []func()
	isShutdown bool
	wg         sync.WaitGroup
}

func NewCustomThreadPool(numWorkers int) *CustomThreadPool {
	p := &CustomThreadPool{}
	p.cond = sync.NewCond(&p.mu)
	p.wg.Add(numWorkers)
	for i := 0; i < numWorkers; i++ {
		go p.work(fmt.Sprintf("Worker-%d", i))
	}
	return p
}

func (p *CustomThreadPool) Submit(task func()) error {
	p.mu.Lock()
	defer p.mu.Unlock()
	if p.isShutdown {
		return fmt.Errorf("ThreadPool is shut down, cannot accept new tasks")
	}
	p.taskQueue = append(p.taskQueue, task)
	p.cond.Signal()
	return nil
}

func (p *CustomThreadPool) Shutdown() {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.isShutdown = true
	p.cond.Broadcast()
}

func (p *CustomThreadPool) AwaitTermination() {
	p.wg.Wait()
}

func (p *CustomThreadPool) work(name string) {
	defer p.wg.Done()
	for {
		p.mu.Lock()
		for len(p.taskQueue) == 0 && !p.isShutdown {
			p.cond.Wait()
		}
		if len(p.taskQueue) == 0 && p.isShutdown {
			p.mu.Unlock()
			return // no more work, pool is shutting down
		}

		task := p.taskQueue[0]
		p.taskQueue = p.taskQueue[1:]
		p.mu.Unlock()

		func() {
			defer func() {
				if r := recover(); r != nil {
					fmt.Printf("%s task threw exception: %v\n", name, r)
				}
			}()
			task()
		}()
	}
}

func main() {
	pool := NewCustomThreadPool(3)

	for i := 0; i < 10; i++ {
		taskID := i
		_ = pool.Submit(func() {
			fmt.Printf("executing task %d\n", taskID)
			time.Sleep(200 * time.Millisecond)
		})
	}

	pool.Shutdown()
	pool.AwaitTermination()
	fmt.Println("All tasks completed, pool terminated.")
}
