/*

FR:
    Schedule a task for a future time
    Execute task at a scheduled time
    Support task priority
    Support recurring task
    cancel a scheduled task
    Thread safe scheduling
    Task should be executed async

*/

#include <string>
#include <queue>
#include <mutex>
#include <vector>
#include <memory>

struct Task {
    virtual void execute() = 0;
    virtual ~Task() = default;
};

class EmailTask : public Task {
public:
    std::string email;

    void execute() override {
    }
};

class PaymentTask : public Task {
public:
    void execute() override {
    }
};

class ScheduledTask {
public:
    std::string id;
    std::shared_ptr<Task> task;
    long executionTime;
    int priority;

    bool operator<(const ScheduledTask& other) const {
        if (executionTime != other.executionTime) {
            return executionTime > other.executionTime;
        }
        return priority > other.priority;
    }
};

class TaskQueue {
public:
    std::priority_queue<ScheduledTask> queue;
    std::mutex mtx;

    void add(const ScheduledTask& task) {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push(task);
    }
};

class TaskScheduler {
public:
    std::shared_ptr<TaskQueue> taskQueue;

    explicit TaskScheduler(std::shared_ptr<TaskQueue> taskQueue) : taskQueue(taskQueue) {}

    void schedule(const ScheduledTask& task) {
        taskQueue->add(task);
    }
};

class Dispatcher {
public:
    std::shared_ptr<TaskQueue> taskQueue;
    bool isRunning = true;

    void run() {
        while (isRunning) {
            // what is the current time
            // delay =
        }
    }
};

class TaskSchedularDemo {
};

int main() {
    return 0;
}
