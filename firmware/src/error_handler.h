/**
 * Error Handler - System Health Monitoring
 * 
 * Minimal error infrastructure providing:
 * - Initialization hook for future expansion
 * - System health check based on heap memory
 * 
 * Logging is handled by logger.h (LOG_ERROR, LOG_WARN, etc.)
 */

#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <Arduino.h>

/**
 * ErrorHandler - System health monitoring
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
     * @return true if free heap > 50KB
     */
    static bool systemHealthy();

private:
    static bool initialized;
};

#endif // ERROR_HANDLER_H
