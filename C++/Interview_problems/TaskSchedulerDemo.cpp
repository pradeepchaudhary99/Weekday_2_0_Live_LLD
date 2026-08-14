#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

/*
FR:
    Schedule a task for a future time
    Execute task at a scheduled time
    Support task priority
    Support recurring task
    Cancel a scheduled task
    Thread safe scheduling
    Task should be executed async

Design notes:
    - ScheduledTask entries live in a std::priority_queue (min-heap by execution time,
      ties broken by priority) guarded by a mutex/condition_variable pair, giving a
      thread-safe blocking DelayQueue equivalent.
    - Dispatcher runs on its own std::thread, blocks until the earliest task's
      execution time has arrived, and hands the actual task execution off to a worker
      WorkerPool -> execution is async and the dispatcher thread is never blocked doing
      task work. take() polls with a bounded wait so it can also observe shutdown
      without needing a Java-style thread interrupt.
    - Cancellation is a soft-delete: an atomic `cancelled` flag is set on the
      ScheduledTask; the dispatcher double-checks the flag before running (a cancelled
      entry left in the heap is simply skipped when it is popped).
    - Recurring tasks are re-inserted into the queue with a new execution time after
      each run, so they perpetually flow back through the same pipeline.
*/

std::string currentThreadName() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return "thread-" + oss.str();
}

struct Task {
    virtual ~Task() = default;
    virtual void execute() const = 0;
};

class EmailTask : public Task {
public:
    explicit EmailTask(std::string email) : email_(std::move(email)) {}
    void execute() const override {
        std::cout << currentThreadName() << " -> sending email to " << email_ << "\n";
    }

private:
    std::string email_;
};

class PaymentTask : public Task {
public:
    explicit PaymentTask(std::string orderId) : orderId_(std::move(orderId)) {}
    void execute() const override {
        std::cout << currentThreadName() << " -> processing payment for order " << orderId_ << "\n";
    }

private:
    std::string orderId_;
};

enum class Priority { LOW = 0, MEDIUM = 1, HIGH = 2 };

class ScheduledTask {
public:
    ScheduledTask(std::string id, std::shared_ptr<Task> task,
                  std::chrono::steady_clock::time_point executionTime, Priority priority,
                  std::chrono::milliseconds recurringInterval)
        : id_(std::move(id)),
          task_(std::move(task)),
          executionTime_(executionTime),
          priority_(priority),
          recurringInterval_(recurringInterval) {}

    const std::string& id() const { return id_; }
    const std::shared_ptr<Task>& task() const { return task_; }
    std::chrono::steady_clock::time_point executionTime() const { return executionTime_; }
    Priority priority() const { return priority_; }
    bool isRecurring() const { return recurringInterval_.count() > 0; }
    bool isCancelled() const { return cancelled_.load(); }
    void cancel() { cancelled_.store(true); }

    void scheduleNextRun() {
        executionTime_ = std::chrono::steady_clock::now() + recurringInterval_;
    }

private:
    std::string id_;
    std::shared_ptr<Task> task_;
    std::chrono::steady_clock::time_point executionTime_;
    Priority priority_;
    std::chrono::milliseconds recurringInterval_;
    std::atomic<bool> cancelled_{false};
};

struct ScheduledTaskCompare {
    // std::priority_queue is a max-heap; invert the order so the earliest execution
    // time (and among ties, the highest priority) comes out first.
    bool operator()(const std::shared_ptr<ScheduledTask>& a, const std::shared_ptr<ScheduledTask>& b) const {
        if (a->executionTime() != b->executionTime()) {
            return a->executionTime() > b->executionTime();
        }
        return a->priority() < b->priority();
    }
};

class TaskQueue {
public:
    void add(std::shared_ptr<ScheduledTask> task) {
        std::lock_guard<std::mutex> lock(mutex_);
        index_[task->id()] = task;
        heap_.push(std::move(task));
        cond_.notify_all();
    }

    // Blocks until the earliest task's execution time has arrived, or `running`
    // becomes false, in which case nullptr is returned.
    std::shared_ptr<ScheduledTask> take(std::atomic<bool>& running) {
        std::unique_lock<std::mutex> lock(mutex_);
        while (running.load()) {
            while (heap_.empty() && running.load()) {
                cond_.wait_for(lock, std::chrono::milliseconds(100));
            }
            if (!running.load()) {
                return nullptr;
            }
            auto now = std::chrono::steady_clock::now();
            auto top = heap_.top();
            if (top->executionTime() <= now) {
                heap_.pop();
                return top;
            }
            cond_.wait_for(lock, top->executionTime() - now);
        }
        return nullptr;
    }

    void forget(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        index_.erase(id);
    }

    bool cancel(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = index_.find(id);
        if (it == index_.end()) {
            return false;
        }
        it->second->cancel();  // best-effort; stale heap entry is skipped when popped
        index_.erase(it);
        return true;
    }

private:
    std::priority_queue<std::shared_ptr<ScheduledTask>, std::vector<std::shared_ptr<ScheduledTask>>,
                         ScheduledTaskCompare>
        heap_;
    std::unordered_map<std::string, std::shared_ptr<ScheduledTask>> index_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

class WorkerPool {
public:
    explicit WorkerPool(int numThreads) {
        for (int i = 0; i < numThreads; ++i) {
            workers_.emplace_back(&WorkerPool::work, this);
        }
    }

    ~WorkerPool() {
        shutdown();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void submit(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
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
            task();
        }
    }

    std::queue<std::function<void()>> taskQueue_;
    std::vector<std::thread> workers_;
    std::mutex mutex_;
    std::condition_variable cond_;
    bool isShutdown_ = false;
};

class Dispatcher {
public:
    Dispatcher(TaskQueue& taskQueue, WorkerPool& workerPool) : taskQueue_(taskQueue), workerPool_(workerPool) {}

    void stop() { running_.store(false); }

    void run() {
        while (running_.load()) {
            auto scheduledTask = taskQueue_.take(running_);
            if (!scheduledTask) {
                return;  // stop() was called while waiting
            }

            if (scheduledTask->isCancelled()) {
                taskQueue_.forget(scheduledTask->id());
                continue;
            }

            workerPool_.submit([scheduledTask] {
                try {
                    scheduledTask->task()->execute();
                } catch (const std::exception& e) {
                    std::cout << "Task " << scheduledTask->id() << " failed: " << e.what() << "\n";
                }
            });

            if (scheduledTask->isRecurring() && !scheduledTask->isCancelled()) {
                scheduledTask->scheduleNextRun();
                taskQueue_.add(scheduledTask);
            } else {
                taskQueue_.forget(scheduledTask->id());
            }
        }
    }

private:
    TaskQueue& taskQueue_;
    WorkerPool& workerPool_;
    std::atomic<bool> running_{true};
};

class TaskScheduler {
public:
    explicit TaskScheduler(int workerPoolSize) : workerPool_(workerPoolSize), dispatcher_(taskQueue_, workerPool_) {
        dispatcherThread_ = std::thread([this] { dispatcher_.run(); });
    }

    std::string schedule(std::shared_ptr<Task> task, std::chrono::milliseconds delay, Priority priority) {
        return scheduleInternal(std::move(task), delay, priority, std::chrono::milliseconds(0));
    }

    std::string scheduleRecurring(std::shared_ptr<Task> task, std::chrono::milliseconds initialDelay,
                                   std::chrono::milliseconds interval, Priority priority) {
        if (interval.count() <= 0) {
            throw std::invalid_argument("interval must be > 0 for a recurring task");
        }
        return scheduleInternal(std::move(task), initialDelay, priority, interval);
    }

    bool cancel(const std::string& taskId) { return taskQueue_.cancel(taskId); }

    void shutdown() {
        dispatcher_.stop();
        if (dispatcherThread_.joinable()) {
            dispatcherThread_.join();
        }
        workerPool_.shutdown();
    }

private:
    std::string scheduleInternal(std::shared_ptr<Task> task, std::chrono::milliseconds delay, Priority priority,
                                  std::chrono::milliseconds interval) {
        std::string id = "task-" + std::to_string(idGenerator_.fetch_add(1) + 1);
        auto executionTime = std::chrono::steady_clock::now() + delay;
        taskQueue_.add(std::make_shared<ScheduledTask>(id, std::move(task), executionTime, priority, interval));
        return id;
    }

    TaskQueue taskQueue_;
    WorkerPool workerPool_;
    Dispatcher dispatcher_;
    std::thread dispatcherThread_;
    std::atomic<long> idGenerator_{0};
};

int main() {
    TaskScheduler scheduler(4);

    scheduler.schedule(std::make_shared<EmailTask>("user@example.com"), std::chrono::milliseconds(2000),
                        Priority::HIGH);
    scheduler.schedule(std::make_shared<PaymentTask>("ORDER-123"), std::chrono::milliseconds(1000),
                        Priority::MEDIUM);
    std::string recurringId = scheduler.scheduleRecurring(std::make_shared<EmailTask>("digest@example.com"),
                                                            std::chrono::milliseconds(500),
                                                            std::chrono::milliseconds(1500), Priority::LOW);

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    bool cancelled = scheduler.cancel(recurringId);
    std::cout << "Cancelled recurring task " << recurringId << ": " << (cancelled ? "true" : "false") << "\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    scheduler.shutdown();
    return 0;
}
