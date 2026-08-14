package main

import (
	"container/heap"
	"fmt"
	"sync"
	"sync/atomic"
	"time"
)

/*
FR:
	Schedule a task for a future time
	Execute task at a scheduled time
	Support task priority
	Support recurring task
	Cancel a scheduled task
	Thread safe scheduling
	Task should be executed async

Design notes:
	- scheduledTask entries live in a container/heap min-heap ordered by execution
	  time (ties broken by priority), guarded by a sync.Cond -> a thread-safe blocking
	  delay queue, similar in spirit to java.util.concurrent.DelayQueue.
	- The dispatcher runs on its own goroutine, blocks until the earliest task's
	  execution time has arrived, and hands the actual task execution off to a worker
	  pool of goroutines -> execution is async and the dispatcher goroutine is never
	  blocked doing task work.
	- Cancellation is a soft-delete: an atomic cancelled flag is set on the
	  scheduledTask; the dispatcher double-checks the flag before running.
	- Recurring tasks are re-inserted into the heap with a new execution time after
	  each run, so they perpetually flow back through the same pipeline.
*/

type Task interface {
	Execute()
}

type EmailTask struct {
	Email string
}

func (t *EmailTask) Execute() {
	fmt.Printf("worker -> sending email to %s\n", t.Email)
}

type PaymentTask struct {
	OrderID string
}

func (t *PaymentTask) Execute() {
	fmt.Printf("worker -> processing payment for order %s\n", t.OrderID)
}

type Priority int

const (
	Low Priority = iota
	Medium
	High
)

type scheduledTask struct {
	id                string
	task              Task
	executionTime     time.Time
	priority          Priority
	recurringInterval time.Duration // 0 => one-shot
	cancelled         int32         // 0 = false, 1 = true; access via sync/atomic
	index             int           // heap index, maintained by container/heap
}

func (t *scheduledTask) isRecurring() bool {
	return t.recurringInterval > 0
}

func (t *scheduledTask) isCancelled() bool {
	return atomic.LoadInt32(&t.cancelled) == 1
}

func (t *scheduledTask) cancel() {
	atomic.StoreInt32(&t.cancelled, 1)
}

func (t *scheduledTask) scheduleNextRun() {
	t.executionTime = time.Now().Add(t.recurringInterval)
}

// taskHeap is a min-heap by executionTime; ties are broken by priority (High first).
type taskHeap []*scheduledTask

func (h taskHeap) Len() int { return len(h) }

func (h taskHeap) Less(i, j int) bool {
	if !h[i].executionTime.Equal(h[j].executionTime) {
		return h[i].executionTime.Before(h[j].executionTime)
	}
	return h[i].priority > h[j].priority
}

func (h taskHeap) Swap(i, j int) {
	h[i], h[j] = h[j], h[i]
	h[i].index, h[j].index = i, j
}

func (h *taskHeap) Push(x interface{}) {
	task := x.(*scheduledTask)
	task.index = len(*h)
	*h = append(*h, task)
}

func (h *taskHeap) Pop() interface{} {
	old := *h
	n := len(old)
	task := old[n-1]
	old[n-1] = nil
	*h = old[:n-1]
	return task
}

type taskQueue struct {
	mu      sync.Mutex
	cond    *sync.Cond
	heap    taskHeap
	index   map[string]*scheduledTask
	stopped bool
}

func newTaskQueue() *taskQueue {
	q := &taskQueue{index: make(map[string]*scheduledTask)}
	q.cond = sync.NewCond(&q.mu)
	return q
}

func (q *taskQueue) add(task *scheduledTask) {
	q.mu.Lock()
	defer q.mu.Unlock()
	q.index[task.id] = task
	heap.Push(&q.heap, task)
	q.cond.Broadcast()
}

// take blocks until the earliest task's execution time has arrived, or the queue is
// stopped (in which case it returns nil).
func (q *taskQueue) take() *scheduledTask {
	q.mu.Lock()
	defer q.mu.Unlock()
	for {
		if q.stopped {
			return nil
		}
		for len(q.heap) == 0 && !q.stopped {
			q.cond.Wait()
		}
		if q.stopped {
			return nil
		}
		delay := time.Until(q.heap[0].executionTime)
		if delay <= 0 {
			return heap.Pop(&q.heap).(*scheduledTask)
		}
		// wake ourselves once the earliest task is due, or sooner if add()/stop() broadcasts
		timer := time.AfterFunc(delay, func() {
			q.mu.Lock()
			q.cond.Broadcast()
			q.mu.Unlock()
		})
		q.cond.Wait()
		timer.Stop()
	}
}

func (q *taskQueue) forget(id string) {
	q.mu.Lock()
	defer q.mu.Unlock()
	delete(q.index, id)
}

func (q *taskQueue) cancel(id string) bool {
	q.mu.Lock()
	defer q.mu.Unlock()
	task, ok := q.index[id]
	if !ok {
		return false
	}
	delete(q.index, id)
	task.cancel() // best-effort; stale heap entry is skipped when popped
	return true
}

func (q *taskQueue) stop() {
	q.mu.Lock()
	q.stopped = true
	q.mu.Unlock()
	q.cond.Broadcast()
}

type workerPool struct {
	tasks chan func()
	wg    sync.WaitGroup
}

func newWorkerPool(numWorkers int) *workerPool {
	p := &workerPool{tasks: make(chan func(), 64)}
	p.wg.Add(numWorkers)
	for i := 0; i < numWorkers; i++ {
		go p.work()
	}
	return p
}

func (p *workerPool) work() {
	defer p.wg.Done()
	for task := range p.tasks {
		task()
	}
}

func (p *workerPool) submit(task func()) {
	p.tasks <- task
}

func (p *workerPool) shutdown() {
	close(p.tasks)
	p.wg.Wait()
}

type dispatcher struct {
	taskQueue  *taskQueue
	workerPool *workerPool
}

func (d *dispatcher) run() {
	for {
		task := d.taskQueue.take()
		if task == nil {
			return // queue was stopped
		}

		if task.isCancelled() {
			d.taskQueue.forget(task.id)
			continue
		}

		d.workerPool.submit(func() {
			task.task.Execute()
		})

		if task.isRecurring() && !task.isCancelled() {
			task.scheduleNextRun()
			d.taskQueue.add(task)
		} else {
			d.taskQueue.forget(task.id)
		}
	}
}

type TaskScheduler struct {
	queue      *taskQueue
	workerPool *workerPool
	dispatcher *dispatcher
	done       chan struct{}
	idCounter  int64
}

func NewTaskScheduler(workerPoolSize int) *TaskScheduler {
	s := &TaskScheduler{
		queue:      newTaskQueue(),
		workerPool: newWorkerPool(workerPoolSize),
		done:       make(chan struct{}),
	}
	s.dispatcher = &dispatcher{taskQueue: s.queue, workerPool: s.workerPool}
	go func() {
		s.dispatcher.run()
		close(s.done)
	}()
	return s
}

func (s *TaskScheduler) Schedule(task Task, delay time.Duration, priority Priority) string {
	return s.scheduleInternal(task, delay, priority, 0)
}

func (s *TaskScheduler) ScheduleRecurring(task Task, initialDelay, interval time.Duration, priority Priority) string {
	if interval <= 0 {
		panic("interval must be > 0 for a recurring task")
	}
	return s.scheduleInternal(task, initialDelay, priority, interval)
}

func (s *TaskScheduler) scheduleInternal(task Task, delay time.Duration, priority Priority, interval time.Duration) string {
	id := fmt.Sprintf("task-%d", atomic.AddInt64(&s.idCounter, 1))
	st := &scheduledTask{
		id:                id,
		task:              task,
		executionTime:     time.Now().Add(delay),
		priority:          priority,
		recurringInterval: interval,
	}
	s.queue.add(st)
	return id
}

func (s *TaskScheduler) Cancel(taskID string) bool {
	return s.queue.cancel(taskID)
}

func (s *TaskScheduler) Shutdown() {
	s.queue.stop()
	<-s.done
	s.workerPool.shutdown()
}

func main() {
	scheduler := NewTaskScheduler(4)

	scheduler.Schedule(&EmailTask{Email: "user@example.com"}, 2*time.Second, High)
	scheduler.Schedule(&PaymentTask{OrderID: "ORDER-123"}, 1*time.Second, Medium)
	recurringID := scheduler.ScheduleRecurring(&EmailTask{Email: "digest@example.com"}, 500*time.Millisecond, 1500*time.Millisecond, Low)

	time.Sleep(3 * time.Second)
	cancelled := scheduler.Cancel(recurringID)
	fmt.Printf("Cancelled recurring task %s: %t\n", recurringID, cancelled)

	time.Sleep(2 * time.Second)
	scheduler.Shutdown()
}
