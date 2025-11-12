/**
 * Logger - Abstraction for logging output
 * 
 * Provides clean separation between business logic and output destination.
 * Enables testing without Serial spam, and allows redirecting logs to SD card,
 * network, or disabling entirely for production builds.
 * 
 * Usage:
 *   Logger* log = Logger::getInstance();
 *   log->info("System initialized");
 *   log->debug("Packet received: %d bytes", length);
 *   log->error("Radio init failed");
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

/**
 * Log levels
 */
enum class LogLevel : uint8_t {
    DEBUG = 0,    // Detailed diagnostic information
    INFO = 1,     // General informational messages
    WARNING = 2,  // Warning messages (recoverable errors)
    ERROR = 3,    // Error messages (failures)
    NONE = 4      // Disable logging
};

/**
 * Logger interface - abstract base class
 * 
 * Implement this interface to create custom loggers
 * (SerialLogger, SDLogger, NetworkLogger, etc.)
 */
class Logger {
public:
    virtual ~Logger() = default;
    
    // Formatted logging methods
    virtual void debug(const char* format, ...) = 0;
    virtual void info(const char* format, ...) = 0;
    virtual void warning(const char* format, ...) = 0;
    virtual void error(const char* format, ...) = 0;
    
    // Set minimum log level (filter out lower priority messages)
    virtual void setLevel(LogLevel level) = 0;
    virtual LogLevel getLevel() const = 0;
    
    // Global singleton access (set once at startup)
    static void setInstance(Logger* logger);
    static Logger* getInstance();
    
private:
    static Logger* instance;
};

/**
 * SerialLogger - Logs to Arduino Serial console
 * 
 * Default implementation for development/debugging.
 * Formats messages with timestamps and log level indicators.
 */
class SerialLogger : public Logger {
public:
    SerialLogger(LogLevel minLevel = LogLevel::INFO);
    
    void debug(const char* format, ...) override;
    void info(const char* format, ...) override;
    void warning(const char* format, ...) override;
    void error(const char* format, ...) override;
    
    void setLevel(LogLevel level) override { minLevel = level; }
    LogLevel getLevel() const override { return minLevel; }
    
private:
    LogLevel minLevel;
    
    // Helper to format and print with level prefix
    void log(LogLevel level, const char* prefix, const char* format, va_list args);
};

/**
 * NullLogger - Discards all log messages
 * 
 * Use for production builds where logging overhead is unwanted.
 */
class NullLogger : public Logger {
public:
    void debug(const char* format, ...) override {}
    void info(const char* format, ...) override {}
    void warning(const char* format, ...) override {}
    void error(const char* format, ...) override {}
    
    void setLevel(LogLevel level) override {}
    LogLevel getLevel() const override { return LogLevel::NONE; }
};

// Convenience macros for common logging patterns
#define LOG_DEBUG(...) Logger::getInstance()->debug(__VA_ARGS__)
#define LOG_INFO(...) Logger::getInstance()->info(__VA_ARGS__)
#define LOG_WARN(...) Logger::getInstance()->warning(__VA_ARGS__)
#define LOG_ERROR(...) Logger::getInstance()->error(__VA_ARGS__)

#endif // LOGGER_H
