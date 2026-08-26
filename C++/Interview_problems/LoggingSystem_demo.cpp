/*
================================================================================
LLD: Logging System
================================================================================

Functional Requirements:
    1. Log messages at different severity levels (DEBUG, INFO, WARN, ERROR,
       FATAL).
    2. Route each log message to one or more pluggable output destinations
       (console, file, ...).
    3. Filter out messages below a logger's configured minimum severity
       threshold.
    4. Format a log message consistently before it is written to a
       destination.
    5. Look up/create named loggers through a single central registry.

Non-Functional Requirements:
    1. Thread-safety.
    2. Maintainability / extensibility to new appenders (e.g. network,
       database) and formatters.
    3. Low overhead: a filtered-out message should be dropped before it is
       ever formatted or written.

Design:
    LogAppender (Strategy) is the pluggable output sink for a formatted log
    line; ConsoleAppender writes to stdout, FileAppender writes lines to a
    local log file.

    LogFormatter (Strategy) turns a LogMessage into a line of text;
    SimpleLogFormatter produces "[timestamp] [LEVEL] [logger] message".

    Logger owns a name, a minimum LogLevel threshold, an ordered list of
    LogAppenders and a LogFormatter. Each convenience method (debug/info/
    warn/error/fatal) builds a LogMessage and, only if its level meets the
    threshold, formats it once and dispatches it to every appender.

    LogManager (Singleton) is the facade/registry: getInstance() returns the
    single manager, and getLogger(name) creates-or-returns the named Logger,
    the same "one place wires the system together" role AmazonLockerManager
    plays in the locker LLD.

Core Entities:
    LogLevel (enum)
    LogMessage
    LogAppender / ConsoleAppender / FileAppender
    LogFormatter / SimpleLogFormatter
    Logger
    LogManager (Singleton)
================================================================================
*/

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

enum class LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3, FATAL = 4 };

std::string levelName(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
    }
    return "UNKNOWN";
}

struct LogMessage {
    LogLevel level;
    std::string loggerName;
    std::string message;
    std::chrono::system_clock::time_point timestamp;

    LogMessage(LogLevel level, std::string loggerName, std::string message)
        : level(level), loggerName(std::move(loggerName)), message(std::move(message)),
          timestamp(std::chrono::system_clock::now()) {}
};

struct LogFormatter {
    virtual ~LogFormatter() = default;
    virtual std::string format(const LogMessage& message) const = 0;
};

class SimpleLogFormatter : public LogFormatter {
public:
    std::string format(const LogMessage& message) const override {
        std::time_t time = std::chrono::system_clock::to_time_t(message.timestamp);
        std::tm tm = *std::localtime(&time);
        std::ostringstream oss;
        oss << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] [" << levelName(message.level)
            << "] [" << message.loggerName << "] " << message.message;
        return oss.str();
    }
};

struct LogAppender {
    virtual ~LogAppender() = default;
    virtual void append(const std::string& formattedMessage) = 0;
};

class ConsoleAppender : public LogAppender {
public:
    void append(const std::string& formattedMessage) override {
        std::cout << formattedMessage << "\n";
    }
};

class FileAppender : public LogAppender {
public:
    explicit FileAppender(const std::string& filePath)
        : filePath_(filePath), file_(filePath, std::ios::out | std::ios::trunc) {
        if (!file_.is_open()) {
            throw std::runtime_error("Unable to open log file " + filePath);
        }
    }

    ~FileAppender() override {
        close();
    }

    void append(const std::string& formattedMessage) override {
        file_ << formattedMessage << "\n";
        file_.flush();
    }

    void close() {
        if (file_.is_open()) {
            file_.close();
        }
    }

private:
    std::string filePath_;
    std::ofstream file_;
};

class Logger {
public:
    Logger(std::string name, LogLevel minLevel, std::shared_ptr<LogFormatter> formatter)
        : name_(std::move(name)), minLevel_(minLevel), formatter_(std::move(formatter)) {}

    void addAppender(std::shared_ptr<LogAppender> appender) {
        std::lock_guard<std::mutex> lock(mutex_);
        appenders_.push_back(std::move(appender));
    }

    void setMinLevel(LogLevel minLevel) { minLevel_ = minLevel; }

    void debug(const std::string& message) { log(LogLevel::DEBUG, message); }
    void info(const std::string& message) { log(LogLevel::INFO, message); }
    void warn(const std::string& message) { log(LogLevel::WARN, message); }
    void error(const std::string& message) { log(LogLevel::ERROR, message); }
    void fatal(const std::string& message) { log(LogLevel::FATAL, message); }

private:
    void log(LogLevel level, const std::string& message) {
        if (static_cast<int>(level) < static_cast<int>(minLevel_)) {
            return;
        }
        LogMessage logMessage(level, name_, message);
        std::string formatted = formatter_->format(logMessage);
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& appender : appenders_) {
            appender->append(formatted);
        }
    }

    std::string name_;
    LogLevel minLevel_;
    std::shared_ptr<LogFormatter> formatter_;
    std::vector<std::shared_ptr<LogAppender>> appenders_;
    std::mutex mutex_;
};

class LogManager {
public:
    static LogManager& getInstance() {
        static LogManager instance;
        return instance;
    }

    std::shared_ptr<Logger> getLogger(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = loggers_.find(name);
        if (it != loggers_.end()) {
            return it->second;
        }
        auto logger = std::make_shared<Logger>(name, LogLevel::INFO, std::make_shared<SimpleLogFormatter>());
        loggers_[name] = logger;
        return logger;
    }

    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

private:
    LogManager() = default;
    std::map<std::string, std::shared_ptr<Logger>> loggers_;
    std::mutex mutex_;
};

int main() {
    std::filesystem::path logFilePath = std::filesystem::temp_directory_path() / "lld_demo_app.log";

    auto appLogger = LogManager::getInstance().getLogger("AppLogger");
    appLogger->addAppender(std::make_shared<ConsoleAppender>());
    auto fileAppender = std::make_shared<FileAppender>(logFilePath.string());
    appLogger->addAppender(fileAppender);
    appLogger->setMinLevel(LogLevel::INFO);

    std::cout << "Logging at INFO threshold (log file: " << logFilePath.string() << "):\n";
    appLogger->debug("This debug message should be filtered out");
    appLogger->info("Application started");
    appLogger->warn("Cache is nearing capacity");
    appLogger->error("Failed to connect to downstream service");

    fileAppender->close();

    std::cout << "\nSame Logger instance retrieved from LogManager again (singleton registry):\n";
    auto sameLogger = LogManager::getInstance().getLogger("AppLogger");
    std::cout << "  same instance? " << (sameLogger == appLogger ? "true" : "false") << "\n";

    std::cout << "\nReading back the file appender's output to prove it was written:\n";
    std::ifstream in(logFilePath);
    std::string line;
    while (std::getline(in, line)) {
        std::cout << "  " << line << "\n";
    }

    return 0;
}
