"""

FR:
    Schedule a task for a future time
    Execute task at a scheduled time
    Support task priority
    Support recurring task
    cancel a scheduled task
    Thread safe scheduling
    Task should be executed async

"""

from abc import ABC, abstractmethod
import heapq
import threading


class Task(ABC):
    @abstractmethod
    def execute(self) -> None:
        pass


class EmailTask(Task):
    def __init__(self, email: str = None):
        self.email = email

    def execute(self) -> None:
        pass


class PaymentTask(Task):
    def execute(self) -> None:
        pass


class ScheduledTask:
    def __init__(self, task_id: str, task: Task, execution_time: int, priority: int):
        self.id = task_id
        self.task = task
        self.execution_time = execution_time
        self.priority = priority

    def __lt__(self, other: "ScheduledTask") -> bool:
        return (self.execution_time, self.priority) < (other.execution_time, other.priority)


class TaskQueue:
    def __init__(self):
        self.queue = []
        self.lock = threading.Lock()

    def add(self, task: ScheduledTask) -> None:
        with self.lock:
            heapq.heappush(self.queue, task)

    def peek(self) -> ScheduledTask:
        with self.lock:
            return self.queue[0] if self.queue else None


class TaskScheduler:
    def __init__(self, task_queue: TaskQueue):
        self.task_queue = task_queue

    def schedule(self, task: ScheduledTask) -> None:
        self.task_queue.add(task)


class Dispatcher:
    def __init__(self, task_queue: TaskQueue):
        self.task_queue = task_queue
        self.is_running = True
        self.worker_pool = None

    def run(self) -> None:
        while self.is_running:
            task = self.task_queue.peek()

            # what is the current time
            # delay =

            pass


class TaskSchedularDemo:
    pass


if __name__ == "__main__":
    pass
