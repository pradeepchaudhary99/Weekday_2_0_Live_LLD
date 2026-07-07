import threading


class Logger:
    _instance = None
    _lock = threading.Lock()

    def __init__(self, file_path: str):
        print("object Created")
        self.file_path = file_path

    @classmethod
    def get_instance(cls) -> "Logger":
        print("Thread")
        with cls._lock:
            if cls._instance is None:
                cls._instance = Logger("filePath")
        return cls._instance


if __name__ == "__main__":
    logger = Logger.get_instance()

    def task() -> None:
        Logger.get_instance()

    thread1 = threading.Thread(target=task)
    thread2 = threading.Thread(target=task)
    thread3 = threading.Thread(target=task)

    thread1.start()
    thread2.start()
    thread3.start()

    thread1.join()
    thread2.join()
    thread3.join()
