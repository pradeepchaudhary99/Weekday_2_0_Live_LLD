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
    1. Thread-safety (moot for Node's single-threaded event loop, but the
       design would carry over to a worker-thread model).
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

const fs = require("fs");
const os = require("os");
const path = require("path");

const LogLevel = Object.freeze({ DEBUG: 0, INFO: 1, WARN: 2, ERROR: 3, FATAL: 4 });

function levelName(level) {
    return Object.keys(LogLevel).find((key) => LogLevel[key] === level);
}

class LogMessage {
    constructor(level, loggerName, message) {
        this.level = level;
        this.loggerName = loggerName;
        this.message = message;
        this.timestamp = new Date();
    }
}

class LogFormatter {
    format(_message) {
        throw new Error("format must be implemented by subclasses");
    }
}

class SimpleLogFormatter extends LogFormatter {
    format(message) {
        const timestamp = message.timestamp.toISOString().replace("T", " ").replace("Z", "");
        return `[${timestamp}] [${levelName(message.level)}] [${message.loggerName}] ${message.message}`;
    }
}

class LogAppender {
    append(_formattedMessage) {
        throw new Error("append must be implemented by subclasses");
    }
}

class ConsoleAppender extends LogAppender {
    append(formattedMessage) {
        console.log(formattedMessage);
    }
}

class FileAppender extends LogAppender {
    constructor(filePath) {
        super();
        this._filePath = filePath;
        this._fd = fs.openSync(filePath, "w");
    }

    append(formattedMessage) {
        fs.writeSync(this._fd, formattedMessage + "\n");
    }

    close() {
        fs.closeSync(this._fd);
    }
}

class Logger {
    constructor(name, minLevel, formatter) {
        this.name = name;
        this.minLevel = minLevel;
        this._formatter = formatter;
        this._appenders = [];
    }

    addAppender(appender) {
        this._appenders.push(appender);
    }

    setMinLevel(minLevel) {
        this.minLevel = minLevel;
    }

    debug(message) { this._log(LogLevel.DEBUG, message); }
    info(message) { this._log(LogLevel.INFO, message); }
    warn(message) { this._log(LogLevel.WARN, message); }
    error(message) { this._log(LogLevel.ERROR, message); }
    fatal(message) { this._log(LogLevel.FATAL, message); }

    _log(level, message) {
        if (level < this.minLevel) {
            return;
        }
        const logMessage = new LogMessage(level, this.name, message);
        const formatted = this._formatter.format(logMessage);
        for (const appender of this._appenders) {
            appender.append(formatted);
        }
    }
}

class LogManager {
    static _instance = null;

    constructor() {
        this._loggers = new Map();
    }

    static getInstance() {
        if (!LogManager._instance) {
            LogManager._instance = new LogManager();
        }
        return LogManager._instance;
    }

    getLogger(name) {
        if (!this._loggers.has(name)) {
            this._loggers.set(name, new Logger(name, LogLevel.INFO, new SimpleLogFormatter()));
        }
        return this._loggers.get(name);
    }
}

function main() {
    const logFilePath = path.join(os.tmpdir(), "lld_demo_app.log");

    const appLogger = LogManager.getInstance().getLogger("AppLogger");
    appLogger.addAppender(new ConsoleAppender());
    const fileAppender = new FileAppender(logFilePath);
    appLogger.addAppender(fileAppender);
    appLogger.setMinLevel(LogLevel.INFO);

    console.log(`Logging at INFO threshold (log file: ${logFilePath}):`);
    appLogger.debug("This debug message should be filtered out");
    appLogger.info("Application started");
    appLogger.warn("Cache is nearing capacity");
    appLogger.error("Failed to connect to downstream service");

    fileAppender.close();

    console.log("\nSame Logger instance retrieved from LogManager again (singleton registry):");
    const sameLogger = LogManager.getInstance().getLogger("AppLogger");
    console.log(`  same instance? ${sameLogger === appLogger}`);

    console.log("\nReading back the file appender's output to prove it was written:");
    const contents = fs.readFileSync(logFilePath, "utf8");
    for (const line of contents.trim().split("\n")) {
        console.log(`  ${line}`);
    }
}

main();
