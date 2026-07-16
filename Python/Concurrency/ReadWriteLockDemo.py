import threading
import time
from concurrent.futures import ThreadPoolExecutor


class SharedCounter:
    def __init__(self) -> None:
        self._value = 0
        self._write_lock = threading.Lock()
        self._readers_lock = threading.Lock()
        self._readers = 0

    def read(self) -> int:
        with self._readers_lock:
            self._readers += 1
            if self._readers == 1:
                self._write_lock.acquire()
        try:
            print(f"{threading.current_thread().name} reading: {self._value}")
            self._sleep(0.1)  # simulate read taking some time
            return self._value
        finally:
            with self._readers_lock:
                self._readers -= 1
                if self._readers == 0:
                    self._write_lock.release()

    def write(self, new_value: int) -> None:
        with self._write_lock:
            print(f"{threading.current_thread().name} WRITING: {new_value}")
            self._sleep(0.1)
            self._value = new_value

    @staticmethod
    def _sleep(seconds: float) -> None:
        time.sleep(seconds)


def main() -> None:
    counter = SharedCounter()

    with ThreadPoolExecutor(max_workers=5) as pool:
        # 4 readers — watch their timestamps overlap
        for _ in range(4):
            pool.submit(counter.read)

        time.sleep(0.02)  # let readers start first

        # 1 writer — watch it wait for ALL readers to finish, then block everyone else
        pool.submit(counter.write, 99)


if __name__ == "__main__":
    main()
