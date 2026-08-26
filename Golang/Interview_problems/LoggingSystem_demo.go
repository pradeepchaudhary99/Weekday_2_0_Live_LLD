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
    LogAppenders and a LogFormatter. Each convenience method (Debug/Info/
    Warn/Error/Fatal) builds a LogMessage and, only if its level meets the
    threshold, formats it once and dispatches it to every appender.

    LogManager (Singleton, built with sync.Once) is the facade/registry:
    GetLogManager() returns the single manager, and GetLogger(name)
    creates-or-returns the named Logger, the same "one place wires the
    system together" role AmazonLockerManager plays in the locker LLD.

Core Entities:
    LogLevel (enum)
    LogMessage
    LogAppender / ConsoleAppender / FileAppender
    LogFormatter / SimpleLogFormatter
    Logger
    LogManager (Singleton)
================================================================================
*/

package main

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"sync"
	"time"
)

type LogLevel int

const (
	Debug LogLevel = iota
	Info
	Warn
	Error
	Fatal
)

func (l LogLevel) String() string {
	switch l {
	case Debug:
		return "DEBUG"
	case Info:
		return "INFO"
	case Warn:
		return "WARN"
	case Error:
		return "ERROR"
	case Fatal:
		return "FATAL"
	}
	return "UNKNOWN"
}

type LogMessage struct {
	Level      LogLevel
	LoggerName string
	Message    string
	Timestamp  time.Time
}

type LogFormatter interface {
	Format(message LogMessage) string
}

type SimpleLogFormatter struct{}

func (SimpleLogFormatter) Format(message LogMessage) string {
	return fmt.Sprintf("[%s] [%s] [%s] %s",
		message.Timestamp.Format("2006-01-02 15:04:05.000"), message.Level, message.LoggerName, message.Message)
}

type LogAppender interface {
	Append(formattedMessage string)
}

type ConsoleAppender struct{}

func (ConsoleAppender) Append(formattedMessage string) {
	fmt.Println(formattedMessage)
}

type FileAppender struct {
	file *os.File
}

func NewFileAppender(filePath string) (*FileAppender, error) {
	file, err := os.Create(filePath)
	if err != nil {
		return nil, fmt.Errorf("unable to open log file %s: %w", filePath, err)
	}
	return &FileAppender{file: file}, nil
}

func (a *FileAppender) Append(formattedMessage string) {
	fmt.Fprintln(a.file, formattedMessage)
	a.file.Sync()
}

func (a *FileAppender) Close() error {
	return a.file.Close()
}

type Logger struct {
	mu        sync.Mutex
	name      string
	minLevel  LogLevel
	appenders []LogAppender
	formatter LogFormatter
}

func NewLogger(name string, minLevel LogLevel, formatter LogFormatter) *Logger {
	return &Logger{name: name, minLevel: minLevel, formatter: formatter}
}

func (l *Logger) AddAppender(appender LogAppender) {
	l.mu.Lock()
	defer l.mu.Unlock()
	l.appenders = append(l.appenders, appender)
}

func (l *Logger) SetMinLevel(minLevel LogLevel) {
	l.mu.Lock()
	defer l.mu.Unlock()
	l.minLevel = minLevel
}

func (l *Logger) Debug(message string) { l.log(Debug, message) }
func (l *Logger) Info(message string)  { l.log(Info, message) }
func (l *Logger) Warn(message string)  { l.log(Warn, message) }
func (l *Logger) Error(message string) { l.log(Error, message) }
func (l *Logger) Fatal(message string) { l.log(Fatal, message) }

func (l *Logger) log(level LogLevel, message string) {
	l.mu.Lock()
	defer l.mu.Unlock()
	if level < l.minLevel {
		return
	}
	logMessage := LogMessage{Level: level, LoggerName: l.name, Message: message, Timestamp: time.Now()}
	formatted := l.formatter.Format(logMessage)
	for _, appender := range l.appenders {
		appender.Append(formatted)
	}
}

type LogManager struct {
	mu      sync.Mutex
	loggers map[string]*Logger
}

var (
	logManagerInstance *LogManager
	logManagerOnce     sync.Once
)

func GetLogManager() *LogManager {
	logManagerOnce.Do(func() {
		logManagerInstance = &LogManager{loggers: make(map[string]*Logger)}
	})
	return logManagerInstance
}

func (m *LogManager) GetLogger(name string) *Logger {
	m.mu.Lock()
	defer m.mu.Unlock()
	if logger, ok := m.loggers[name]; ok {
		return logger
	}
	logger := NewLogger(name, Info, SimpleLogFormatter{})
	m.loggers[name] = logger
	return logger
}

func main() {
	logFilePath := filepath.Join(os.TempDir(), "lld_demo_app.log")

	appLogger := GetLogManager().GetLogger("AppLogger")
	appLogger.AddAppender(ConsoleAppender{})
	fileAppender, err := NewFileAppender(logFilePath)
	if err != nil {
		panic(err)
	}
	appLogger.AddAppender(fileAppender)
	appLogger.SetMinLevel(Info)

	fmt.Printf("Logging at INFO threshold (log file: %s):\n", logFilePath)
	appLogger.Debug("This debug message should be filtered out")
	appLogger.Info("Application started")
	appLogger.Warn("Cache is nearing capacity")
	appLogger.Error("Failed to connect to downstream service")

	fileAppender.Close()

	fmt.Println("\nSame Logger instance retrieved from LogManager again (singleton registry):")
	sameLogger := GetLogManager().GetLogger("AppLogger")
	fmt.Printf("  same instance? %t\n", sameLogger == appLogger)

	fmt.Println("\nReading back the file appender's output to prove it was written:")
	file, err := os.Open(logFilePath)
	if err != nil {
		panic(err)
	}
	defer file.Close()
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		fmt.Printf("  %s\n", scanner.Text())
	}
}
