/**
 * Logger Implementation
 */

#include "logger.h"
#include <stdarg.h>

// Global instance and null-safe fallback
Logger* Logger::instance = nullptr;
static NullLogger nullLogger;  // Static null logger for pre-init safety

void Logger::setInstance(Logger* logger) {
    instance = logger;
}

Logger* Logger::getInstance() {
    // Return null logger if instance not yet set (static init safety)
    return instance ? instance : &nullLogger;
}

// SerialLogger implementation
SerialLogger::SerialLogger(LogLevel minLevel) : minLevel(minLevel) {
}

void SerialLogger::debug(const char* format, ...) {
    if (minLevel <= LogLevel::DEBUG) {
        va_list args;
        va_start(args, format);
        log(LogLevel::DEBUG, "[DEBUG]", format, args);
        va_end(args);
    }
}

void SerialLogger::info(const char* format, ...) {
    if (minLevel <= LogLevel::INFO) {
        va_list args;
        va_start(args, format);
        log(LogLevel::INFO, "[INFO]", format, args);
        va_end(args);
    }
}

void SerialLogger::warning(const char* format, ...) {
    if (minLevel <= LogLevel::WARNING) {
        va_list args;
        va_start(args, format);
        log(LogLevel::WARNING, "[WARN]", format, args);
        va_end(args);
    }
}

void SerialLogger::error(const char* format, ...) {
    if (minLevel <= LogLevel::ERROR) {
        va_list args;
        va_start(args, format);
        log(LogLevel::ERROR, "[ERROR]", format, args);
        va_end(args);
    }
}

void SerialLogger::log(LogLevel level, const char* prefix, const char* format, va_list args) {
    // Print prefix
    Serial.print(prefix);
    Serial.print(" ");
    
    // Format and print message
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    Serial.println(buffer);
}
