"""
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

    LogManager (Singleton) is the facade/registry: get_instance() returns
    the single manager, and get_logger(name) creates-or-returns the named
    Logger, the same "one place wires the system together" role
    AmazonLockerManager plays in the locker LLD.

Core Entities:
    LogLevel (enum)
    LogMessage
    LogAppender / ConsoleAppender / FileAppender
    LogFormatter / SimpleLogFormatter
    Logger
    LogManager (Singleton)
================================================================================
"""

import os
import tempfile
import threading
from abc import ABC, abstractmethod
from datetime import datetime
from enum import Enum
from typing import Dict, List, Optional


class LogLevel(Enum):
    DEBUG = 0
    INFO = 1
    WARN = 2
    ERROR = 3
    FATAL = 4


class LogMessage:
    def __init__(self, level: LogLevel, logger_name: str, message: str):
        self.level = level
        self.logger_name = logger_name
        self.message = message
        self.timestamp = datetime.now()


class LogFormatter(ABC):
    @abstractmethod
    def format(self, message: LogMessage) -> str:
        raise NotImplementedError


class SimpleLogFormatter(LogFormatter):
    def format(self, message: LogMessage) -> str:
        timestamp = message.timestamp.strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        return f"[{timestamp}] [{message.level.name}] [{message.logger_name}] {message.message}"


class LogAppender(ABC):
    @abstractmethod
    def append(self, formatted_message: str) -> None:
        raise NotImplementedError


class ConsoleAppender(LogAppender):
    def append(self, formatted_message: str) -> None:
        print(formatted_message)


class FileAppender(LogAppender):
    def __init__(self, file_path: str):
        self._file_path = file_path
        self._file = open(file_path, "w")

    def append(self, formatted_message: str) -> None:
        self._file.write(formatted_message + "\n")
        self._file.flush()

    def close(self) -> None:
        self._file.close()


class Logger:
    def __init__(self, name: str, min_level: LogLevel, formatter: LogFormatter):
        self.name = name
        self.min_level = min_level
        self._formatter = formatter
        self._appenders: List[LogAppender] = []
        self._lock = threading.Lock()

    def add_appender(self, appender: LogAppender) -> None:
        with self._lock:
            self._appenders.append(appender)

    def set_min_level(self, min_level: LogLevel) -> None:
        self.min_level = min_level

    def debug(self, message: str) -> None:
        self._log(LogLevel.DEBUG, message)

    def info(self, message: str) -> None:
        self._log(LogLevel.INFO, message)

    def warn(self, message: str) -> None:
        self._log(LogLevel.WARN, message)

    def error(self, message: str) -> None:
        self._log(LogLevel.ERROR, message)

    def fatal(self, message: str) -> None:
        self._log(LogLevel.FATAL, message)

    def _log(self, level: LogLevel, message: str) -> None:
        if level.value < self.min_level.value:
            return
        log_message = LogMessage(level, self.name, message)
        formatted = self._formatter.format(log_message)
        with self._lock:
            for appender in self._appenders:
                appender.append(formatted)


class LogManager:
    _instance: Optional["LogManager"] = None
    _instance_lock = threading.Lock()

    def __init__(self):
        self._loggers: Dict[str, Logger] = {}
        self._lock = threading.Lock()

    @classmethod
    def get_instance(cls) -> "LogManager":
        with cls._instance_lock:
            if cls._instance is None:
                cls._instance = LogManager()
            return cls._instance

    def get_logger(self, name: str) -> Logger:
        with self._lock:
            if name not in self._loggers:
                self._loggers[name] = Logger(name, LogLevel.INFO, SimpleLogFormatter())
            return self._loggers[name]


def main() -> None:
    log_file_path = os.path.join(tempfile.gettempdir(), "lld_demo_app.log")

    app_logger = LogManager.get_instance().get_logger("AppLogger")
    app_logger.add_appender(ConsoleAppender())
    file_appender = FileAppender(log_file_path)
    app_logger.add_appender(file_appender)
    app_logger.set_min_level(LogLevel.INFO)

    print(f"Logging at INFO threshold (log file: {log_file_path}):")
    app_logger.debug("This debug message should be filtered out")
    app_logger.info("Application started")
    app_logger.warn("Cache is nearing capacity")
    app_logger.error("Failed to connect to downstream service")

    file_appender.close()

    print("\nSame Logger instance retrieved from LogManager again (singleton registry):")
    same_logger = LogManager.get_instance().get_logger("AppLogger")
    print(f"  same instance? {same_logger is app_logger}")

    print("\nReading back the file appender's output to prove it was written:")
    with open(log_file_path) as f:
        for line in f:
            print(f"  {line.rstrip()}")


if __name__ == "__main__":
    main()
