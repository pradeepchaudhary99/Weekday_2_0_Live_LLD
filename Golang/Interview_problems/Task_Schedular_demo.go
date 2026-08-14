/*

FR:
    Schedule a task for a future time
    Execute task at a scheduled time
    Support task priority
    Support recurring task
    cancel a scheduled task
    Thread safe scheduling
    Task should be executed async

*/

package main

import (
	"container/heap"
	"sync"
)

type Task interface {
	Execute()
}

type EmailTask struct {
	Email string
}

func (e *EmailTask) Execute() {
}

type PaymentTask struct{}

func (p *PaymentTask) Execute() {
}

type ScheduledTask struct {
	Id            string
	Task          Task
	ExecutionTime int64
	Priority      int
}

type TaskQueue struct {
	mu    sync.Mutex
	tasks []*ScheduledTask
}

func (q *TaskQueue) Len() int { return len(q.tasks) }

func (q *TaskQueue) Less(i, j int) bool {
	if q.tasks[i].ExecutionTime != q.tasks[j].ExecutionTime {
		return q.tasks[i].ExecutionTime < q.tasks[j].ExecutionTime
	}
	return q.tasks[i].Priority < q.tasks[j].Priority
}

func (q *TaskQueue) Swap(i, j int) { q.tasks[i], q.tasks[j] = q.tasks[j], q.tasks[i] }

func (q *TaskQueue) Push(x interface{}) {
	q.tasks = append(q.tasks, x.(*ScheduledTask))
}

func (q *TaskQueue) Pop() interface{} {
	old := q.tasks
	n := len(old)
	item := old[n-1]
	q.tasks = old[:n-1]
	return item
}

func (q *TaskQueue) Add(task *ScheduledTask) {
	q.mu.Lock()
	defer q.mu.Unlock()
	heap.Push(q, task)
}

type TaskScheduler struct {
	TaskQueue *TaskQueue
}

func (s *TaskScheduler) Schedule(task *ScheduledTask) {
	s.TaskQueue.Add(task)
}

type Dispatcher struct {
	TaskQueue *TaskQueue
	IsRunning bool
}

func (d *Dispatcher) Run() {
	for d.IsRunning {
		// what is the current time
		// delay =
	}
}

func main() {
}
