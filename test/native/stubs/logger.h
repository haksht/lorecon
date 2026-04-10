/**
 * Logger stub for native unit tests.
 * Provides no-op macros AND a minimal Logger class so that firmware
 * source files compiled via relative-path includes (e.g. "../logger.h"
 * from repositories/*.cpp) get a complete, linkable implementation.
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <cstdint>
#include <cstdarg>

enum class LogLevel : uint8_t {
    DEBUG = 0, INFO = 1, WARNING = 2, ERROR = 3, NONE = 4
};

// Forward-declare Stream so logger.h compiles when Arduino.h is already included
class Stream;

// Minimal Logger class matching the interface expected by firmware source files
class ILogger {
public:
    virtual ~ILogger() {}
    virtual void debug(const char*, ...) {}
    virtual void info(const char*, ...) {}
    virtual void warning(const char*, ...) {}
    virtual void error(const char*, ...) {}
    virtual void setLevel(LogLevel) {}
    virtual LogLevel getLevel() const { return LogLevel::NONE; }
};

class Logger : public ILogger {
public:
    static Logger* getInstance() {
        static Logger instance;
        return &instance;
    }
    void setStream(void*) {}
private:
    Logger() {}
};

// No-op logger macros — test output uses Unity assertions, not serial logs
#define LOG_DEBUG(...) ((void)0)
#define LOG_INFO(...)  ((void)0)
#define LOG_WARN(...)  ((void)0)
#define LOG_ERROR(...) ((void)0)

#endif // LOGGER_H
