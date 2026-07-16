import threading
import time
from collections import deque
from concurrent.futures import ThreadPoolExecutor
from typing import Deque, Generic, TypeVar

T = TypeVar("T")


class BoundedQueue(Generic[T]):
    def __init__(self, capacity: int) -> None:
        self._queue: Deque[T] = deque()
        self._capacity = capacity
        self._condition = threading.Condition()

    def put(self, item: T) -> None:
        with self._condition:
            while len(self._queue) == self._capacity:  # full — wait for a taker
                print(f"{threading.current_thread().name} sees FULL "
                      f"({len(self._queue)}/{self._capacity}), waiting...")
                self._condition.wait()
            self._queue.append(item)
            print(f"{threading.current_thread().name} put: {item} [size={len(self._queue)}]")
            self._condition.notify_all()  # wake possible waiting takers (and putters)

    def take(self) -> T:
        with self._condition:
            while not self._queue:  # empty — wait for a putter
                print(f"{threading.current_thread().name} sees EMPTY, waiting...")
                self._condition.wait()
            item = self._queue.popleft()
            print(f"{threading.current_thread().name} took: {item} [size={len(self._queue)}]")
            self._condition.notify_all()
            return item


def producer(queue: "BoundedQueue[str]") -> None:
    for i in range(5):
        queue.put(f"{threading.current_thread().name}-item{i}")
        time.sleep(0.03)


def consumer(queue: "BoundedQueue[str]") -> None:
    for _ in range(5):
        queue.take()
        time.sleep(0.06)


def main() -> None:
    queue: BoundedQueue[str] = BoundedQueue(3)  # capacity 3, not 1

    with ThreadPoolExecutor(max_workers=4) as pool:
        pool.submit(producer, queue)
        pool.submit(producer, queue)
        pool.submit(consumer, queue)
        pool.submit(consumer, queue)


if __name__ == "__main__":
    main()
