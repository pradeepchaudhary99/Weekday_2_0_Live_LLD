#include <iostream>
#include <sstream>
#include <thread>

std::string currentThreadName() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return "thread-" + oss.str();
}

class Pradeep {
public:
    void run() {
        while (true) {
            std::cout << "Pradeep is continously working\n";
            std::cout << "Pradeep Running this in a new Thread: " << currentThreadName() << "\n";
        }
    }
};

int main() {
    Pradeep p1;
    Pradeep p2;

    std::thread t1(&Pradeep::run, &p1);
    std::thread t2(&Pradeep::run, &p2);

    // Alternative approaches considered:
    //
    // A fixed-size thread pool executing a single task:
    //   auto task1 = [] { for (int i = 0; i < 20; ++i) std::cout << "pradeep\n"; };
    //
    // Three tasks each run on their own thread, then joined:
    //   auto task1 = [] { for (int i = 0; i < 20; ++i) std::cout << "pradeep\n"; };
    //   auto task2 = [] { for (int i = 0; i < 20; ++i) std::cout << "ishita\n"; };
    //   auto task3 = [] { for (int i = 0; i < 20; ++i) std::cout << "Sahil\n"; };
    //   std::vector<std::thread> threads;
    //   for (auto& task : {task1, task2, task3}) threads.emplace_back(task);
    //   for (auto& t : threads) t.join();
    //   std::cout << "main thread\n";

    t1.join();
    t2.join();
    return 0;
}
