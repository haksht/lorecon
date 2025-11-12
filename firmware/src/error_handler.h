/**
 * Production-Grade Error Handler
 * 
 * Centralized error logging, recovery, and reporting system.
 * Provides structured error codes, recovery strategies, and debugging support.
 */

#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <Arduino.h>

// Error severity levels
enum ErrorSeverity {
    SEVERITY_INFO,      // Informational, no action needed
    SEVERITY_WARNING,   // Warning, operation continues
    SEVERITY_ERROR,     // Error, operation failed but system continues
    SEVERITY_CRITICAL,  // Critical error, system may be unstable
    SEVERITY_FATAL      // Fatal error, system halt required
};

// Error categories
enum ErrorCategory {
    ERR_RADIO,          // Radio hardware/configuration errors
    ERR_MEMORY,         // Memory allocation/corruption errors
    ERR_STATE,          // State management errors
    ERR_CONFIG,         // Configuration errors
    ERR_INTERRUPT,      // Interrupt handling errors
    ERR_STORAGE,        // File system/storage errors
    ERR_PROTOCOL,       // Protocol parsing errors
    ERR_USER_TIMEOUT,   // User timeout errors (renamed to avoid system conflict)
    ERR_SYSTEM          // System-level errors
};

// Structured error information
struct ErrorInfo {
    ErrorCategory category;
    ErrorSeverity severity;
    uint16_t code;
    const char* message;
    const char* function;
    uint16_t line;
    uint32_t timestamp;
    bool recovered;
};

/**
 * Error Handler - Production-grade error management
 * 
 * Features:
 * - Structured error logging with severity levels
 * - Automatic recovery attempt for recoverable errors
 * - Error history tracking (last 20 errors)
 * - Diagnostic reporting for debugging
 * - Integration with watchdog for critical errors
 */
class ErrorHandler {
public:
    static void initialize();
    
    // Main error logging interface
    static void logError(ErrorCategory category, ErrorSeverity severity, 
                        uint16_t code, const char* message,
                        const char* function = nullptr, uint16_t line = 0);
    
    // Convenience macros for common error types
    static void radioError(uint16_t code, const char* message, 
                          const char* function = nullptr, uint16_t line = 0);
    static void memoryError(uint16_t code, const char* message,
                           const char* function = nullptr, uint16_t line = 0);
    static void stateError(uint16_t code, const char* message,
                          const char* function = nullptr, uint16_t line = 0);
    
    // Recovery strategies
    static bool attemptRecovery(const ErrorInfo& error);
    static bool resetRadio();
    static bool resetState();
    
    // Error history and reporting
    static void printErrorHistory();
    static void printLastError();
    static uint16_t getErrorCount(ErrorCategory category = ERR_SYSTEM);
    static bool hasErrors();
    static void clearErrors();
    
    // System health check
    static bool systemHealthy();
    static void printHealthReport();
    
private:
    static const uint8_t MAX_ERROR_HISTORY = 20;
    static ErrorInfo errorHistory[MAX_ERROR_HISTORY];
    static uint8_t errorIndex;
    static uint8_t errorCount;
    
    static uint16_t categoryCounts[9];  // Count per category
    static uint32_t lastErrorTime;
    static bool initialized;
    
    // Recovery functions
    static bool recoverRadioError(const ErrorInfo& error);
    static bool recoverMemoryError(const ErrorInfo& error);
    static bool recoverStateError(const ErrorInfo& error);
    
    // Logging helpers
    static const char* severityToString(ErrorSeverity severity);
    static const char* categoryToString(ErrorCategory category);
    static void logToSerial(const ErrorInfo& error);
};

// Helper macros for easy error reporting with file/line info
// Note: Renamed to avoid conflict with logger.h LOG_ERROR macro
#define REPORT_ERROR(cat, sev, code, msg) \
    ErrorHandler::logError(cat, sev, code, msg, __FUNCTION__, __LINE__)

#define REPORT_RADIO_ERROR(code, msg) \
    ErrorHandler::radioError(code, msg, __FUNCTION__, __LINE__)

#define REPORT_MEMORY_ERROR(code, msg) \
    ErrorHandler::memoryError(code, msg, __FUNCTION__, __LINE__)

#define REPORT_STATE_ERROR(code, msg) \
    ErrorHandler::stateError(code, msg, __FUNCTION__, __LINE__)

// Error code definitions
namespace ErrorCodes {
    // Radio errors (100-199)
    const uint16_t RADIO_INIT_FAILED = 100;
    const uint16_t RADIO_CONFIG_FAILED = 101;
    const uint16_t RADIO_TX_FAILED = 102;
    const uint16_t RADIO_RX_FAILED = 103;
    const uint16_t RADIO_TIMEOUT = 104;
    const uint16_t RADIO_CRC_ERROR = 105;
    
    // Memory errors (200-299)
    const uint16_t MEMORY_ALLOC_FAILED = 200;
    const uint16_t MEMORY_OVERFLOW = 201;
    const uint16_t MEMORY_CORRUPTION = 202;
    const uint16_t HEAP_FRAGMENTATION = 203;
    
    // State errors (300-399)
    const uint16_t INVALID_STATE = 300;
    const uint16_t STATE_TRANSITION_FAILED = 301;
    const uint16_t CONFIG_OUT_OF_RANGE = 302;
    const uint16_t DEVICE_INDEX_INVALID = 303;
    
    // Storage errors (400-499)
    const uint16_t STORAGE_INIT_FAILED = 400;
    const uint16_t STORAGE_WRITE_FAILED = 401;
    const uint16_t STORAGE_READ_FAILED = 402;
    const uint16_t STORAGE_FULL = 403;
    
    // Protocol errors (500-599)
    const uint16_t PROTOCOL_PARSE_ERROR = 500;
    const uint16_t INVALID_PACKET_LENGTH = 501;
    const uint16_t CHECKSUM_MISMATCH = 502;
    
    // System errors (900-999)
    const uint16_t WATCHDOG_TIMEOUT = 900;
    const uint16_t INTERRUPT_FAILURE = 901;
    const uint16_t SYSTEM_OVERLOAD = 902;
}

#endif // ERROR_HANDLER_H
