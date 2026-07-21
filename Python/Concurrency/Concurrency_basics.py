import threading


class Pradeep:
    def run(self) -> None:
        while True:
            print("Pradeep is continously working")
            print(f"Pradeep Running this in a new Thread: {threading.current_thread().name}")


def main() -> None:
    p1 = Pradeep()
    p2 = Pradeep()

    t1 = threading.Thread(target=p1.run)
    t2 = threading.Thread(target=p2.run)

    t1.start()
    t2.start()

    # Alternative approaches considered:
    #
    # from concurrent.futures import ThreadPoolExecutor
    # def task1():
    #     for i in range(20):
    #         print("pradeep")
    # with ThreadPoolExecutor(max_workers=5) as pool:
    #     pool.submit(task1)
    #
    # def task1():
    #     for i in range(20):
    #         print("pradeep")
    # def task2():
    #     for i in range(20):
    #         print("ishita")
    # def task3():
    #     for i in range(20):
    #         print("Sahil")
    #
    # threads = []
    # for task in (task1, task2, task3):
    #     t = threading.Thread(target=task)
    #     threads.append(t)
    #     t.start()
    # for t in threads:
    #     t.join()
    # print("main thread")

    t1.join()
    t2.join()


if __name__ == "__main__":
    main()
