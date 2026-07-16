#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

std::string currentThreadName() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return "thread-" + oss.str();
}

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t capacity) : capacity_(capacity) {}

    void put(const T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        while (queue_.size() == capacity_) {  // full — wait for a taker
            std::cout << currentThreadName() << " sees FULL (" << queue_.size()
                      << "/" << capacity_ << "), waiting...\n";
            cond_.wait(lock);
        }
        queue_.push(item);
        std::cout << currentThreadName() << " put: " << item << " [size=" << queue_.size() << "]\n";
        cond_.notify_all();  // wake possible waiting takers (and putters)
    }

    T take() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (queue_.empty()) {  // empty — wait for a putter
            std::cout << currentThreadName() << " sees EMPTY, waiting...\n";
            cond_.wait(lock);
        }
        T item = queue_.front();
        queue_.pop();
        std::cout << currentThreadName() << " took: " << item << " [size=" << queue_.size() << "]\n";
        cond_.notify_all();
        return item;
    }

private:
    std::queue<T> queue_;
    size_t capacity_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

void produce(BoundedQueue<std::string>& queue) {
    for (int i = 0; i < 5; ++i) {
        queue.put(currentThreadName() + "-item" + std::to_string(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

void consume(BoundedQueue<std::string>& queue) {
    for (int i = 0; i < 5; ++i) {
        queue.take();
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
    }
}

int main() {
    BoundedQueue<std::string> queue(3);  // capacity 3, not 1

    std::vector<std::thread> pool;
    pool.emplace_back(produce, std::ref(queue));
    pool.emplace_back(produce, std::ref(queue));
    pool.emplace_back(consume, std::ref(queue));
    pool.emplace_back(consume, std::ref(queue));

    for (auto& t : pool) t.join();
    return 0;
}
