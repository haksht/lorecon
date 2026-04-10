/**
 * Logger stub for native unit tests.
 *
 * Mirrors the full interface of firmware/src/logger.h so that firmware
 * source files compiled via relative path (#include "../logger.h") get
 * a complete, linkable implementation with no Arduino/Stream dependencies.
 *
 * All methods are no-ops. getInstance() returns a NullLogger singleton.
 * Static members are defined inline (C++17 inline variables) so no
 * separate logger.cpp compilation unit is needed.
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <cstdint>
#include <cstdarg>

// ── Forward declarations to satisfy SerialLogger without pulling in Arduino ──
class Stream;

// ── LogLevel (must match firmware enum) ──────────────────────────────────────
enum class LogLevel : uint8_t {
    DEBUG = 0, INFO = 1, WARNING = 2, ERROR = 3, NONE = 4
};

// ── Abstract Logger base ──────────────────────────────────────────────────────
class Logger {
public:
    virtual ~Logger() = default;
    virtual void debug(const char* format, ...) = 0;
    virtual void info(const char* format, ...) = 0;
    virtual void warning(const char* format, ...) = 0;
    virtual void error(const char* format, ...) = 0;
    virtual void setLevel(LogLevel level) = 0;
    virtual LogLevel getLevel() const = 0;

    static void setInstance(Logger* logger) { instance = logger; }
    static Logger* getInstance();   // defined below, after NullLogger

private:
    inline static Logger* instance = nullptr;
};

// ── NullLogger — discards everything ─────────────────────────────────────────
class NullLogger : public Logger {
public:
    void debug(const char*, ...)   override {}
    void info(const char*, ...)    override {}
    void warning(const char*, ...) override {}
    void error(const char*, ...)   override {}
    void setLevel(LogLevel)        override {}
    LogLevel getLevel() const      override { return LogLevel::NONE; }
};

// ── SerialLogger stub — no-op, satisfies the declared class name ──────────────
class SerialLogger : public NullLogger {
public:
    SerialLogger(LogLevel = LogLevel::INFO) {}
    void setStream(Stream*) {}
};

// ── Logger::getInstance() — returns NullLogger singleton ─────────────────────
inline Logger* Logger::getInstance() {
    if (instance) return instance;
    static NullLogger nullLogger;
    return &nullLogger;
}

// ── No-op macros ──────────────────────────────────────────────────────────────
#define LOG_DEBUG(...) ((void)0)
#define LOG_INFO(...)  ((void)0)
#define LOG_WARN(...)  ((void)0)
#define LOG_ERROR(...) ((void)0)

#endif // LOGGER_H
