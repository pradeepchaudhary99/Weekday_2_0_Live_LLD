'use strict';

/*
FR:
    Schedule a task for a future time
    Execute task at a scheduled time
    Support task priority
    Support recurring task
    Cancel a scheduled task
    Thread safe scheduling (Node.js is single-threaded, so queue operations are
    inherently serialized; "async" below models JS's event-loop concurrency rather
    than real OS threads)
    Task should be executed async

Design notes:
    - ScheduledTask entries live in a plain array acting as a delay queue, kept sorted
      by executionTime (ties broken by priority, HIGH before LOW).
    - Dispatcher is an async loop that awaits the earliest task's delay and then hands
      the actual task execution off to a small WorkerPool of concurrent async workers
      -> execution is async and the dispatcher loop is never blocked doing task work.
    - Cancellation is a soft-delete: a `cancelled` flag is set on the ScheduledTask; the
      dispatcher double-checks the flag before running.
    - Recurring tasks are re-inserted into the queue with a new executionTime after
      each run, so they perpetually flow back through the same pipeline.
*/

class Task {
    execute() {
        throw new Error('execute() must be implemented by subclasses');
    }
}

class EmailTask extends Task {
    constructor(email) {
        super();
        this.email = email;
    }

    execute() {
        console.log(`worker -> sending email to ${this.email}`);
    }
}

class PaymentTask extends Task {
    constructor(orderId) {
        super();
        this.orderId = orderId;
    }

    execute() {
        console.log(`worker -> processing payment for order ${this.orderId}`);
    }
}

const Priority = Object.freeze({ LOW: 0, MEDIUM: 1, HIGH: 2 });

class ScheduledTask {
    constructor(id, task, executionTime, priority, recurringIntervalMs) {
        this.id = id;
        this.task = task;
        this.executionTime = executionTime; // epoch ms
        this.priority = priority;
        this.recurringIntervalMs = recurringIntervalMs; // 0 => one-shot
        this.cancelled = false;
    }

    isRecurring() {
        return this.recurringIntervalMs > 0;
    }

    cancel() {
        this.cancelled = true;
    }

    scheduleNextRun() {
        this.executionTime = Date.now() + this.recurringIntervalMs;
    }
}

function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

class TaskQueue {
    constructor() {
        this.entries = [];
        this.index = new Map();
        this.waiters = [];
        this.stopped = false;
    }

    add(scheduledTask) {
        this.index.set(scheduledTask.id, scheduledTask);
        this.entries.push(scheduledTask);
        this.entries.sort((a, b) =>
            a.executionTime !== b.executionTime
                ? a.executionTime - b.executionTime
                : b.priority - a.priority, // higher priority wins ties
        );
        this._wake();
    }

    async take() {
        while (!this.stopped) {
            while (this.entries.length === 0 && !this.stopped) {
                await new Promise((resolve) => this.waiters.push(resolve));
            }
            if (this.stopped) {
                return null;
            }
            const delay = this.entries[0].executionTime - Date.now();
            if (delay <= 0) {
                return this.entries.shift();
            }
            await Promise.race([sleep(delay), new Promise((resolve) => this.waiters.push(resolve))]);
        }
        return null;
    }

    forget(id) {
        this.index.delete(id);
    }

    cancel(id) {
        const task = this.index.get(id);
        if (!task) {
            return false;
        }
        this.index.delete(id);
        task.cancel(); // best-effort; the dispatcher skips it when dequeued
        return true;
    }

    stop() {
        this.stopped = true;
        this._wake();
    }

    _wake() {
        const waiters = this.waiters;
        this.waiters = [];
        waiters.forEach((resolve) => resolve());
    }
}

class WorkerPool {
    constructor(numWorkers) {
        this.taskQueue = [];
        this.isShutdown = false;
        this.waiters = [];
        this.workers = [];
        for (let i = 0; i < numWorkers; i++) {
            this.workers.push(this._work(`worker-${i}`));
        }
    }

    submit(task) {
        this.taskQueue.push(task);
        this._wake();
    }

    shutdown() {
        this.isShutdown = true;
        this._wake();
    }

    async awaitTermination() {
        await Promise.all(this.workers);
    }

    _wake() {
        const waiters = this.waiters;
        this.waiters = [];
        waiters.forEach((resolve) => resolve());
    }

    async _work(name) {
        while (true) {
            while (this.taskQueue.length === 0 && !this.isShutdown) {
                await new Promise((resolve) => this.waiters.push(resolve));
            }
            if (this.taskQueue.length === 0 && this.isShutdown) {
                return; // no more work, pool is shutting down
            }
            const task = this.taskQueue.shift();
            try {
                await task(name);
            } catch (e) {
                console.log(`${name} task threw exception: ${e.message}`);
            }
        }
    }
}

class Dispatcher {
    constructor(taskQueue, workerPool) {
        this.taskQueue = taskQueue;
        this.workerPool = workerPool;
        this.running = true;
    }

    stop() {
        this.running = false;
    }

    async run() {
        while (this.running) {
            const scheduledTask = await this.taskQueue.take();
            if (!scheduledTask) {
                return; // queue was stopped
            }

            if (scheduledTask.cancelled) {
                this.taskQueue.forget(scheduledTask.id);
                continue;
            }

            this.workerPool.submit(async () => {
                try {
                    scheduledTask.task.execute();
                } catch (e) {
                    console.log(`Task ${scheduledTask.id} failed: ${e.message}`);
                }
            });

            if (scheduledTask.isRecurring() && !scheduledTask.cancelled) {
                scheduledTask.scheduleNextRun();
                this.taskQueue.add(scheduledTask);
            } else {
                this.taskQueue.forget(scheduledTask.id);
            }
        }
    }
}

class TaskScheduler {
    constructor(workerPoolSize) {
        this.taskQueue = new TaskQueue();
        this.workerPool = new WorkerPool(workerPoolSize);
        this.dispatcher = new Dispatcher(this.taskQueue, this.workerPool);
        this.dispatcherPromise = this.dispatcher.run();
        this.idCounter = 0;
    }

    schedule(task, delayMs, priority) {
        return this._scheduleInternal(task, delayMs, priority, 0);
    }

    scheduleRecurring(task, initialDelayMs, intervalMs, priority) {
        if (intervalMs <= 0) {
            throw new Error('intervalMs must be > 0 for a recurring task');
        }
        return this._scheduleInternal(task, initialDelayMs, priority, intervalMs);
    }

    _scheduleInternal(task, delayMs, priority, intervalMs) {
        const id = `task-${++this.idCounter}`;
        const executionTime = Date.now() + delayMs;
        this.taskQueue.add(new ScheduledTask(id, task, executionTime, priority, intervalMs));
        return id;
    }

    cancel(taskId) {
        return this.taskQueue.cancel(taskId);
    }

    async shutdown() {
        this.dispatcher.stop();
        this.taskQueue.stop();
        await this.dispatcherPromise;
        this.workerPool.shutdown();
        await this.workerPool.awaitTermination();
    }
}

async function main() {
    const scheduler = new TaskScheduler(4);

    scheduler.schedule(new EmailTask('user@example.com'), 2000, Priority.HIGH);
    scheduler.schedule(new PaymentTask('ORDER-123'), 1000, Priority.MEDIUM);
    const recurringId = scheduler.scheduleRecurring(new EmailTask('digest@example.com'), 500, 1500, Priority.LOW);

    await sleep(3000);
    const cancelled = scheduler.cancel(recurringId);
    console.log(`Cancelled recurring task ${recurringId}: ${cancelled}`);

    await sleep(2000);
    await scheduler.shutdown();
}

main();
