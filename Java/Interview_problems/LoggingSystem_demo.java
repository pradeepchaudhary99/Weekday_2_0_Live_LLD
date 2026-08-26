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

import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

enum LogLevel {
    DEBUG(0),
    INFO(1),
    WARN(2),
    ERROR(3),
    FATAL(4);

    final int severity;

    LogLevel(int severity) {
        this.severity = severity;
    }
}

class LogMessage {
    final LogLevel level;
    final String loggerName;
    final String message;
    final LocalDateTime timestamp;

    LogMessage(LogLevel level, String loggerName, String message) {
        this.level = level;
        this.loggerName = loggerName;
        this.message = message;
        this.timestamp = LocalDateTime.now();
    }
}

interface LogFormatter {
    String format(LogMessage message);
}

class SimpleLogFormatter implements LogFormatter {
    private static final DateTimeFormatter TIMESTAMP_FORMAT =
            DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss.SSS");

    @Override
    public String format(LogMessage message) {
        return "[" + message.timestamp.format(TIMESTAMP_FORMAT) + "] [" + message.level + "] ["
                + message.loggerName + "] " + message.message;
    }
}

interface LogAppender {
    void append(String formattedMessage);
}

class ConsoleAppender implements LogAppender {
    @Override
    public void append(String formattedMessage) {
        System.out.println(formattedMessage);
    }
}

class FileAppender implements LogAppender {
    private final PrintWriter writer;

    FileAppender(String filePath) {
        try {
            this.writer = new PrintWriter(new FileWriter(filePath));
        } catch (IOException e) {
            throw new RuntimeException("Unable to open log file " + filePath, e);
        }
    }

    @Override
    public void append(String formattedMessage) {
        writer.println(formattedMessage);
        writer.flush();
    }

    void close() {
        writer.close();
    }
}

class Logger {
    private final String name;
    private volatile LogLevel minLevel;
    private final List<LogAppender> appenders = new ArrayList<>();
    private final LogFormatter formatter;
    private final Object lock = new Object();

    Logger(String name, LogLevel minLevel, LogFormatter formatter) {
        this.name = name;
        this.minLevel = minLevel;
        this.formatter = formatter;
    }

    void addAppender(LogAppender appender) {
        synchronized (lock) {
            appenders.add(appender);
        }
    }

    void setMinLevel(LogLevel minLevel) {
        this.minLevel = minLevel;
    }

    void debug(String message) { log(LogLevel.DEBUG, message); }
    void info(String message) { log(LogLevel.INFO, message); }
    void warn(String message) { log(LogLevel.WARN, message); }
    void error(String message) { log(LogLevel.ERROR, message); }
    void fatal(String message) { log(LogLevel.FATAL, message); }

    private void log(LogLevel level, String message) {
        if (level.severity < minLevel.severity) {
            return;
        }
        LogMessage logMessage = new LogMessage(level, name, message);
        String formatted = formatter.format(logMessage);
        synchronized (lock) {
            for (LogAppender appender : appenders) {
                appender.append(formatted);
            }
        }
    }
}

class LogManager {
    private static LogManager instance;

    private final Map<String, Logger> loggers = new LinkedHashMap<>();
    private final Object lock = new Object();

    private LogManager() {
    }

    static LogManager getInstance() {
        synchronized (LogManager.class) {
            if (instance == null) {
                instance = new LogManager();
            }
            return instance;
        }
    }

    Logger getLogger(String name) {
        synchronized (lock) {
            return loggers.computeIfAbsent(name, n -> new Logger(n, LogLevel.INFO, new SimpleLogFormatter()));
        }
    }
}

public class LoggingSystem_demo {
    public static void main(String[] args) throws IOException {
        String logFilePath = System.getProperty("java.io.tmpdir") + "/lld_demo_app.log";

        Logger appLogger = LogManager.getInstance().getLogger("AppLogger");
        appLogger.addAppender(new ConsoleAppender());
        FileAppender fileAppender = new FileAppender(logFilePath);
        appLogger.addAppender(fileAppender);
        appLogger.setMinLevel(LogLevel.INFO);

        System.out.println("Logging at INFO threshold (log file: " + logFilePath + "):");
        appLogger.debug("This debug message should be filtered out");
        appLogger.info("Application started");
        appLogger.warn("Cache is nearing capacity");
        appLogger.error("Failed to connect to downstream service");

        fileAppender.close();

        System.out.println("\nSame Logger instance retrieved from LogManager again (singleton registry):");
        Logger sameLogger = LogManager.getInstance().getLogger("AppLogger");
        System.out.println("  same instance? " + (sameLogger == appLogger));

        System.out.println("\nReading back the file appender's output to prove it was written:");
        List<String> lines = Files.readAllLines(Paths.get(logFilePath));
        for (String line : lines) {
            System.out.println("  " + line);
        }
    }
}
