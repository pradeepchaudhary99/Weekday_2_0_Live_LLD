import threading
from collections import deque
from typing import Deque, Generic, TypeVar

T = TypeVar("T")


class BoundedBuffer(Generic[T]):
    def __init__(self, capacity: int) -> None:
        self._queue: Deque[T] = deque()
        self._capacity = capacity
        self._lock = threading.Lock()
        self._producer_cond = threading.Condition(self._lock)
        self._consumer_cond = threading.Condition(self._lock)

    def put(self, item: T) -> None:
        with self._producer_cond:
            while len(self._queue) == self._capacity:
                self._producer_cond.wait()
            self._queue.append(item)
            self._consumer_cond.notify()

    def take(self) -> T:
        with self._consumer_cond:
            while not self._queue:
                self._consumer_cond.wait()
            item = self._queue.popleft()
            self._producer_cond.notify()
            return item


def produce(buffer: "BoundedBuffer[int]") -> None:
    for i in range(10):
        buffer.put(i)
        print(f"Produced: {i}")


def consume(buffer: "BoundedBuffer[int]") -> None:
    for _ in range(10):
        item = buffer.take()
        print(f"Consumed: {item}")


def main() -> None:
    buffer: BoundedBuffer[int] = BoundedBuffer(5)

    t1 = threading.Thread(target=produce, args=(buffer,))
    t2 = threading.Thread(target=consume, args=(buffer,))
    t1.start()
    t2.start()
    t1.join()
    t2.join()


if __name__ == "__main__":
    main()
