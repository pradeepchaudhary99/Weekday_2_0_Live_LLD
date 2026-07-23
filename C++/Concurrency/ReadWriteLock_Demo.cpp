#include <condition_variable>
#include <mutex>

class ReadWriteLockDemo {
public:
    void lockRead() {
        std::unique_lock<std::mutex> lock(mutex_);
        // BLOCK ANY read locks if there is already a write lock
        condition_.wait(lock, [this] { return !activeWriter_ && waitingWriters_ == 0; });
        ++activeReaders_;
    }

    void unlockRead() {
        std::unique_lock<std::mutex> lock(mutex_);
        --activeReaders_;
        if (activeReaders_ == 0) {
            condition_.notify_all();
        }
    }

    void lockWrite() {
        std::unique_lock<std::mutex> lock(mutex_);
        ++waitingWriters_;  // 500
        condition_.wait(lock, [this] { return !activeWriter_ && activeReaders_ == 0; });
        --waitingWriters_;
        activeWriter_ = true;
    }

    void unlockWrite() {
        std::unique_lock<std::mutex> lock(mutex_);
        activeWriter_ = false;
        condition_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    int activeReaders_ = 0;
    bool activeWriter_ = false;  // if only 1 writer is allowed, other int
    int waitingWriters_ = 0;
};

int main() {
    ReadWriteLockDemo lock;
    lock.lockRead();
    lock.unlockRead();
    lock.lockWrite();
    lock.unlockWrite();
    return 0;
}
