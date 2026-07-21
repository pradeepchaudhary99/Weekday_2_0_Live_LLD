#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

template <typename T>
class BoundedBuffer {
public:
    explicit BoundedBuffer(size_t capacity) : capacity_(capacity) {}

    void put(const T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return queue_.size() < capacity_; });
        queue_.push(item);
        cond_.notify_all();
    }

    T take() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return !queue_.empty(); });
        T item = queue_.front();
        queue_.pop();
        cond_.notify_all();
        return item;
    }

private:
    std::queue<T> queue_;
    size_t capacity_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

void producer(BoundedBuffer<int>& buffer) {
    for (int i = 0; i < 10; ++i) {
        buffer.put(i);
        std::cout << "Produced: " << i << "\n";
    }
}

void consumer(BoundedBuffer<int>& buffer) {
    for (int i = 0; i < 10; ++i) {
        int item = buffer.take();
        std::cout << "Consumed: " << item << "\n";
    }
}

int main() {
    BoundedBuffer<int> buffer(5);

    std::thread t1(producer, std::ref(buffer));
    std::thread t2(consumer, std::ref(buffer));
    t1.join();
    t2.join();
    return 0;
}
