#include <chrono>
#include <iostream>
#include <shared_mutex>
#include <sstream>
#include <thread>
#include <vector>

std::string currentThreadName() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return "thread-" + oss.str();
}

class SharedCounter {
public:
    int read() {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::cout << currentThreadName() << " reading: " << value_ << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));  // simulate read taking some time
        return value_;
    }

    void write(int newValue) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        std::cout << currentThreadName() << " WRITING: " << newValue << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        value_ = newValue;
    }

private:
    int value_ = 0;
    std::shared_mutex mutex_;
};

int main() {
    SharedCounter counter;
    std::vector<std::thread> pool;

    // 4 readers — watch their timestamps overlap
    for (int i = 0; i < 4; ++i) {
        pool.emplace_back([&counter] { counter.read(); });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));  // let readers start first

    // 1 writer — watch it wait for ALL readers to finish, then block everyone else
    pool.emplace_back([&counter] { counter.write(99); });

    for (auto& t : pool) t.join();
    return 0;
}
