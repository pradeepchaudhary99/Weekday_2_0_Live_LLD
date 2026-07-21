import threading
import time
from collections import deque
from typing import Callable, Deque


class CustomThreadPool:
    def __init__(self, num_threads: int) -> None:
        self._task_queue: Deque[Callable[[], None]] = deque()
        self._lock = threading.Lock()
        self._not_empty = threading.Condition(self._lock)
        self._is_shutdown = False
        self._workers = [
            threading.Thread(target=self._work, name=f"Worker-{i}")
            for i in range(num_threads)
        ]
        for worker in self._workers:
            worker.start()

    def submit(self, task: Callable[[], None]) -> None:
        with self._not_empty:
            if self._is_shutdown:
                raise RuntimeError("ThreadPool is shut down, cannot accept new tasks")
            self._task_queue.append(task)
            self._not_empty.notify()

    def shutdown(self) -> None:
        with self._not_empty:
            self._is_shutdown = True
            self._not_empty.notify_all()

    def await_termination(self) -> None:
        for worker in self._workers:
            worker.join()

    def _work(self) -> None:
        while True:
            with self._not_empty:
                while not self._task_queue and not self._is_shutdown:
                    self._not_empty.wait()
                if not self._task_queue and self._is_shutdown:
                    return  # no more work, pool is shutting down
                task = self._task_queue.popleft()

            try:
                task()
            except Exception as e:  # noqa: BLE001
                print(f"{threading.current_thread().name} task threw exception: {e}")


def main() -> None:
    pool = CustomThreadPool(3)

    def make_task(task_id: int) -> Callable[[], None]:
        def task() -> None:
            print(f"{threading.current_thread().name} executing task {task_id}")
            time.sleep(0.2)
        return task

    for i in range(10):
        pool.submit(make_task(i))

    pool.shutdown()
    pool.await_termination()
    print("All tasks completed, pool terminated.")


if __name__ == "__main__":
    main()
