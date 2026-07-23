package main

import "sync"

type ReadWriteLockDemo struct {
	mu             sync.Mutex
	cond           *sync.Cond
	activeReaders  int
	activeWriter   bool // if only 1 writer is allowed, other int
	waitingWriters int
}

func NewReadWriteLockDemo() *ReadWriteLockDemo {
	d := &ReadWriteLockDemo{}
	d.cond = sync.NewCond(&d.mu)
	return d
}

func (d *ReadWriteLockDemo) LockRead() {
	d.mu.Lock()
	defer d.mu.Unlock()
	// BLOCK ANY read locks if there is already a write lock
	for d.activeWriter || d.waitingWriters > 0 {
		d.cond.Wait()
	}
	d.activeReaders++
}

func (d *ReadWriteLockDemo) UnlockRead() {
	d.mu.Lock()
	defer d.mu.Unlock()
	d.activeReaders--
	if d.activeReaders == 0 {
		d.cond.Broadcast()
	}
}

func (d *ReadWriteLockDemo) LockWrite() {
	d.mu.Lock()
	defer d.mu.Unlock()
	d.waitingWriters++ // 500
	for d.activeWriter || d.activeReaders > 0 {
		d.cond.Wait()
	}
	d.waitingWriters--
	d.activeWriter = true
}

func (d *ReadWriteLockDemo) UnlockWrite() {
	d.mu.Lock()
	defer d.mu.Unlock()
	d.activeWriter = false
	d.cond.Broadcast()
}

func main() {
	lock := NewReadWriteLockDemo()
	lock.LockRead()
	lock.UnlockRead()
	lock.LockWrite()
	lock.UnlockWrite()
}
