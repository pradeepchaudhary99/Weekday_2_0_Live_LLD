#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>
#include <vector>

std::string currentThreadName() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return "thread-" + oss.str();
}

class CustomThreadPool {
public:
    explicit CustomThreadPool(int numThreads) {
        for (int i = 0; i < numThreads; ++i) {
            workers_.emplace_back(&CustomThreadPool::work, this);
        }
    }

    void submit(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (isShutdown_) {
                throw std::runtime_error("ThreadPool is shut down, cannot accept new tasks");
            }
            taskQueue_.push(std::move(task));
        }
        cond_.notify_one();
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            isShutdown_ = true;
        }
        cond_.notify_all();
    }

    void awaitTermination() {
        for (auto& worker : workers_) {
            worker.join();
        }
    }

private:
    void work() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cond_.wait(lock, [this] { return !taskQueue_.empty() || isShutdown_; });

                if (taskQueue_.empty() && isShutdown_) {
                    return;  // no more work, pool is shutting down
                }

                task = std::move(taskQueue_.front());
                taskQueue_.pop();
            }
            try {
                task();
            } catch (const std::exception& e) {
                std::cout << currentThreadName() << " task threw exception: " << e.what() << "\n";
            }
        }
    }

    std::queue<std::function<void()>> taskQueue_;
    std::vector<std::thread> workers_;
    std::mutex mutex_;
    std::condition_variable cond_;
    bool isShutdown_ = false;
};

int main() {
    CustomThreadPool pool(3);

    for (int i = 0; i < 10; ++i) {
        pool.submit([i] {
            std::cout << currentThreadName() << " executing task " << i << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        });
    }

    pool.shutdown();
    pool.awaitTermination();
    std::cout << "All tasks completed, pool terminated.\n";
    return 0;
}
