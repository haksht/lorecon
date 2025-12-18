/**
 * Error Handler - Simplified
 * 
 * Basic error infrastructure for potential future use.
 * Currently only initialize() is called by the application.
 * 
 * The error codes and enums are kept for future expansion.
 * Recovery and reporting functions removed (were never used).
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
    ERR_USER_TIMEOUT,   // User timeout errors
    ERR_SYSTEM          // System-level errors
};

/**
 * Error Handler - Minimal implementation
 * 
 * Provides basic infrastructure. Currently only initialize() is used.
 * Logging is handled by logger.h (LOG_ERROR, LOG_WARN, etc.)
 */
class ErrorHandler {
public:
    /**
     * Initialize error handler
     * Called once at startup from LoRaReconTool::initialize()
     */
    static void initialize();
    
    /**
     * Check if system is in healthy state
     * Based on free heap memory
     */
    static bool systemHealthy();

private:
    static bool initialized;
};

// Error code definitions (kept for future use)
namespace ErrorCodes {
    // Radio errors (100-199)
    constexpr uint16_t RADIO_INIT_FAILED = 100;
    constexpr uint16_t RADIO_CONFIG_FAILED = 101;
    constexpr uint16_t RADIO_TX_FAILED = 102;
    constexpr uint16_t RADIO_RX_FAILED = 103;
    constexpr uint16_t RADIO_TIMEOUT = 104;
    constexpr uint16_t RADIO_CRC_ERROR = 105;
    
    // Memory errors (200-299)
    constexpr uint16_t MEMORY_ALLOC_FAILED = 200;
    constexpr uint16_t MEMORY_OVERFLOW = 201;
    constexpr uint16_t MEMORY_CORRUPTION = 202;
    constexpr uint16_t HEAP_FRAGMENTATION = 203;
    
    // State errors (300-399)
    constexpr uint16_t INVALID_STATE = 300;
    constexpr uint16_t STATE_TRANSITION_FAILED = 301;
    constexpr uint16_t CONFIG_OUT_OF_RANGE = 302;
    constexpr uint16_t DEVICE_INDEX_INVALID = 303;
    
    // Storage errors (400-499)
    constexpr uint16_t STORAGE_INIT_FAILED = 400;
    constexpr uint16_t STORAGE_WRITE_FAILED = 401;
    constexpr uint16_t STORAGE_READ_FAILED = 402;
    constexpr uint16_t STORAGE_FULL = 403;
    
    // Protocol errors (500-599)
    constexpr uint16_t PROTOCOL_PARSE_ERROR = 500;
    constexpr uint16_t INVALID_PACKET_LENGTH = 501;
    constexpr uint16_t CHECKSUM_MISMATCH = 502;
    
    // System errors (900-999)
    constexpr uint16_t WATCHDOG_TIMEOUT = 900;
    constexpr uint16_t INTERRUPT_FAILURE = 901;
    constexpr uint16_t SYSTEM_OVERLOAD = 902;
}

#endif // ERROR_HANDLER_H
