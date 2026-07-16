#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cond_.notify_all();
        for (auto& worker : workers_) worker.join();
    }

    void submit(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            tasks_.push(std::move(task));
        }
        cond_.notify_one();
    }

private:
    void workerLoop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cond_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cond_;
    bool stop_ = false;
};

int unsafeCounter = 0;
std::atomic<int> safeCounter{0};

int main() {
    {
        ThreadPool pool(8);
        for (int i = 0; i < 1000; ++i) {
            pool.submit([] {
                unsafeCounter++;           // NOT atomic — data race
                safeCounter.fetch_add(1);  // atomic
            });
        }
    }  // pool destructor joins all workers before continuing

    std::cout << "unsafe: " << unsafeCounter << std::endl;      // almost never 1000
    std::cout << "safe:   " << safeCounter.load() << std::endl;  // always 1000
    return 0;
}
