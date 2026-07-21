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
        producerCond_.wait(lock, [this] { return queue_.size() < capacity_; });
        queue_.push(item);
        consumerCond_.notify_one();
    }

    T take() {
        std::unique_lock<std::mutex> lock(mutex_);
        consumerCond_.wait(lock, [this] { return !queue_.empty(); });
        T item = queue_.front();
        queue_.pop();
        producerCond_.notify_one();
        return item;
    }

private:
    std::queue<T> queue_;
    size_t capacity_;
    std::mutex mutex_;
    std::condition_variable producerCond_;
    std::condition_variable consumerCond_;
};

void produce(BoundedBuffer<int>& buffer) {
    for (int i = 0; i < 10; ++i) {
        buffer.put(i);
        std::cout << "Produced: " << i << "\n";
    }
}

void consume(BoundedBuffer<int>& buffer) {
    for (int i = 0; i < 10; ++i) {
        int item = buffer.take();
        std::cout << "Consumed: " << item << "\n";
    }
}

int main() {
    BoundedBuffer<int> buffer(5);

    std::thread t1(produce, std::ref(buffer));
    std::thread t2(consume, std::ref(buffer));
    t1.join();
    t2.join();
    return 0;
}
