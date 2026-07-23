import threading


class ReadWriteLockDemo:
    def __init__(self) -> None:
        self._condition = threading.Condition()
        self._active_readers = 0
        self._active_writer = False  # if only 1 writer is allowed, other int
        self._waiting_writers = 0

    def lock_read(self) -> None:
        with self._condition:
            # BLOCK ANY read locks if there is already a write lock
            while self._active_writer or self._waiting_writers > 0:
                self._condition.wait()
            self._active_readers += 1

    def unlock_read(self) -> None:
        with self._condition:
            self._active_readers -= 1
            if self._active_readers == 0:
                self._condition.notify_all()

    def lock_write(self) -> None:
        with self._condition:
            self._waiting_writers += 1  # 500
            try:
                while self._active_writer or self._active_readers > 0:
                    self._condition.wait()
            finally:
                self._waiting_writers -= 1
            self._active_writer = True

    def unlock_write(self) -> None:
        with self._condition:
            self._active_writer = False
            self._condition.notify_all()


def main() -> None:
    lock = ReadWriteLockDemo()
    lock.lock_read()
    lock.unlock_read()
    lock.lock_write()
    lock.unlock_write()


if __name__ == "__main__":
    main()
