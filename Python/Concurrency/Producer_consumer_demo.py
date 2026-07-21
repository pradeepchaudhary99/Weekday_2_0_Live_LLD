import threading
from collections import deque
from typing import Deque, Generic, TypeVar

T = TypeVar("T")


class BoundedBuffer(Generic[T]):
    def __init__(self, capacity: int) -> None:
        self._queue: Deque[T] = deque()
        self._capacity = capacity
        self._condition = threading.Condition()

    def put(self, item: T) -> None:
        with self._condition:
            while len(self._queue) == self._capacity:
                self._condition.wait()
            self._queue.append(item)
            self._condition.notify_all()

    def take(self) -> T:
        with self._condition:
            while not self._queue:
                self._condition.wait()
            item = self._queue.popleft()
            self._condition.notify_all()
            return item


def producer(buffer: "BoundedBuffer[int]") -> None:
    for i in range(10):
        buffer.put(i)
        print(f"Produced: {i}")


def consumer(buffer: "BoundedBuffer[int]") -> None:
    for _ in range(10):
        item = buffer.take()
        print(f"Consumed: {item}")


def main() -> None:
    buffer: BoundedBuffer[int] = BoundedBuffer(5)

    t1 = threading.Thread(target=producer, args=(buffer,))
    t2 = threading.Thread(target=consumer, args=(buffer,))
    t1.start()
    t2.start()
    t1.join()
    t2.join()


if __name__ == "__main__":
    main()
