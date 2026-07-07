#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

class Logger {
public:
    static Logger& getInstance() {
        std::cout << "Thread\n";
        std::call_once(initFlag, []() {
            instance.reset(new Logger("filePath"));
        });
        return *instance;
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    explicit Logger(const std::string& filePath) : filePath(filePath) {
        std::cout << "object Created\n";
    }

    std::string filePath;
    static std::unique_ptr<Logger> instance;
    static std::once_flag initFlag;
};

std::unique_ptr<Logger> Logger::instance = nullptr;
std::once_flag Logger::initFlag;

int main() {
    Logger& logger = Logger::getInstance();
    (void)logger;

    auto task = []() {
        Logger::getInstance();
    };

    std::thread thread1(task);
    std::thread thread2(task);
    std::thread thread3(task);

    thread1.join();
    thread2.join();
    thread3.join();

    return 0;
}
