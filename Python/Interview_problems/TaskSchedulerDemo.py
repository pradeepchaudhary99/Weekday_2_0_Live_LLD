import heapq
import itertools
import threading
import time
from abc import ABC, abstractmethod
from concurrent.futures import ThreadPoolExecutor
from enum import IntEnum

"""
FR:
    Schedule a task for a future time
    Execute task at a scheduled time
    Support task priority
    Support recurring task
    Cancel a scheduled task
    Thread safe scheduling
    Task should be executed async

Design notes:
    - ScheduledTask entries live in a heapq-backed min-heap keyed by (execution_time,
      -priority), guarded by a Condition -> a thread-safe blocking DelayQueue
      equivalent to java.util.concurrent.DelayQueue.
    - Ties (same execution_time) are broken by priority (HIGH before LOW).
    - Dispatcher is a single background thread that blocks until the earliest task's
      execution time has arrived and hands the actual task execution off to a worker
      ThreadPoolExecutor -> execution is async and the dispatcher thread is never
      blocked doing task work.
    - Cancellation is a soft-delete: a `cancelled` flag is set on the ScheduledTask;
      the dispatcher double-checks the flag before running.
    - Recurring tasks are re-inserted into the heap with a new execution_time after
      each run, so they perpetually flow back through the same pipeline.
"""


class Task(ABC):
    @abstractmethod
    def execute(self) -> None:
        raise NotImplementedError


class EmailTask(Task):
    def __init__(self, email: str):
        self.email = email

    def execute(self) -> None:
        print(f"{threading.current_thread().name} -> sending email to {self.email}")


class PaymentTask(Task):
    def __init__(self, order_id: str):
        self.order_id = order_id

    def execute(self) -> None:
        print(f"{threading.current_thread().name} -> processing payment for order {self.order_id}")


class Priority(IntEnum):
    LOW = 0
    MEDIUM = 1
    HIGH = 2


class ScheduledTask:
    def __init__(self, task_id: str, task: Task, execution_time: float, priority: Priority,
                 recurring_interval: float):
        self.id = task_id
        self.task = task
        self.execution_time = execution_time
        self.priority = priority
        self.recurring_interval = recurring_interval  # 0 => one-shot
        self.cancelled = False

    def is_recurring(self) -> bool:
        return self.recurring_interval > 0

    def cancel(self) -> None:
        self.cancelled = True

    def schedule_next_run(self) -> None:
        self.execution_time = time.time() + self.recurring_interval

    def _sort_key(self):
        # higher priority wins ties -> encode as negative priority value
        return (self.execution_time, -self.priority)

    def __lt__(self, other: "ScheduledTask") -> bool:
        return self._sort_key() < other._sort_key()


class TaskQueue:
    def __init__(self):
        self._heap: list[ScheduledTask] = []
        self._index: dict[str, ScheduledTask] = {}
        self._lock = threading.Lock()
        self._cond = threading.Condition(self._lock)

    def add(self, task: ScheduledTask) -> None:
        with self._cond:
            self._index[task.id] = task
            heapq.heappush(self._heap, task)
            self._cond.notify_all()

    def take(self) -> ScheduledTask:
        with self._cond:
            while True:
                while not self._heap:
                    self._cond.wait()
                delay = self._heap[0].execution_time - time.time()
                if delay <= 0:
                    return heapq.heappop(self._heap)
                self._cond.wait(timeout=delay)

    def forget(self, task_id: str) -> None:
        with self._cond:
            self._index.pop(task_id, None)

    def cancel(self, task_id: str) -> bool:
        with self._cond:
            task = self._index.pop(task_id, None)
            if task is None:
                return False
            task.cancel()  # best-effort; stale heap entry is skipped when popped
            return True


class Dispatcher:
    def __init__(self, task_queue: TaskQueue, worker_pool: ThreadPoolExecutor):
        self.task_queue = task_queue
        self.worker_pool = worker_pool
        self._running = True

    def stop(self) -> None:
        self._running = False

    def run(self) -> None:
        while self._running:
            scheduled_task = self.task_queue.take()

            if scheduled_task.cancelled:
                self.task_queue.forget(scheduled_task.id)
                continue

            def run_task(scheduled_task: ScheduledTask = scheduled_task) -> None:
                try:
                    scheduled_task.task.execute()
                except Exception as e:
                    print(f"Task {scheduled_task.id} failed: {e}")

            self.worker_pool.submit(run_task)

            if scheduled_task.is_recurring() and not scheduled_task.cancelled:
                scheduled_task.schedule_next_run()
                self.task_queue.add(scheduled_task)
            else:
                self.task_queue.forget(scheduled_task.id)


class TaskScheduler:
    def __init__(self, worker_pool_size: int):
        self._task_queue = TaskQueue()
        self._worker_pool = ThreadPoolExecutor(max_workers=worker_pool_size)
        self._dispatcher = Dispatcher(self._task_queue, self._worker_pool)
        self._dispatcher_thread = threading.Thread(target=self._dispatcher.run, daemon=True)
        self._dispatcher_thread.start()
        self._id_counter = itertools.count(1)
        self._id_lock = threading.Lock()

    def schedule(self, task: Task, delay_seconds: float, priority: Priority) -> str:
        return self._schedule_internal(task, delay_seconds, priority, 0)

    def schedule_recurring(self, task: Task, initial_delay_seconds: float,
                            interval_seconds: float, priority: Priority) -> str:
        if interval_seconds <= 0:
            raise ValueError("interval_seconds must be > 0 for a recurring task")
        return self._schedule_internal(task, initial_delay_seconds, priority, interval_seconds)

    def _schedule_internal(self, task: Task, delay_seconds: float, priority: Priority,
                            interval_seconds: float) -> str:
        with self._id_lock:
            task_id = f"task-{next(self._id_counter)}"
        execution_time = time.time() + delay_seconds
        self._task_queue.add(ScheduledTask(task_id, task, execution_time, priority, interval_seconds))
        return task_id

    def cancel(self, task_id: str) -> bool:
        return self._task_queue.cancel(task_id)

    def shutdown(self) -> None:
        self._dispatcher.stop()
        self._worker_pool.shutdown(wait=False)


def main() -> None:
    scheduler = TaskScheduler(4)

    scheduler.schedule(EmailTask("user@example.com"), 2.0, Priority.HIGH)
    scheduler.schedule(PaymentTask("ORDER-123"), 1.0, Priority.MEDIUM)
    recurring_id = scheduler.schedule_recurring(
        EmailTask("digest@example.com"), 0.5, 1.5, Priority.LOW)

    time.sleep(3)
    cancelled = scheduler.cancel(recurring_id)
    print(f"Cancelled recurring task {recurring_id}: {cancelled}")

    time.sleep(2)
    scheduler.shutdown()


if __name__ == "__main__":
    main()
