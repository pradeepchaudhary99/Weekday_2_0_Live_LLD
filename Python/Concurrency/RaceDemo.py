import threading
from concurrent.futures import ThreadPoolExecutor


class SafeCounter:
    def __init__(self) -> None:
        self._value = 0
        self._lock = threading.Lock()

    def increment(self) -> None:
        with self._lock:
            self._value += 1

    def get(self) -> int:
        return self._value


unsafe_counter = 0
safe_counter = SafeCounter()


def increment_both() -> None:
    global unsafe_counter
    unsafe_counter += 1  # NOT atomic — read-modify-write across threads
    safe_counter.increment()  # atomic (guarded by a lock)


def main() -> None:
    with ThreadPoolExecutor(max_workers=8) as pool:
        for _ in range(1000):
            pool.submit(increment_both)

    print(f"unsafe: {unsafe_counter}")  # almost never 1000
    print(f"safe:   {safe_counter.get()}")  # always 1000


if __name__ == "__main__":
    main()
