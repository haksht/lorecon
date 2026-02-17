/**
 * Logger stub for native unit tests.
 * Provides no-op implementations of LOG_* macros.
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <cstdint>

enum class LogLevel : uint8_t {
    DEBUG = 0, INFO = 1, WARNING = 2, ERROR = 3, NONE = 4
};

// No-op logger — test output uses Unity assertions, not serial logs
#define LOG_DEBUG(...) ((void)0)
#define LOG_INFO(...)  ((void)0)
#define LOG_WARN(...)  ((void)0)
#define LOG_ERROR(...) ((void)0)

#endif // LOGGER_H
