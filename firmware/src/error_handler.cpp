/**
 * Error Handler Implementation - Simplified
 * 
 * Minimal implementation - only initialize() and systemHealthy() are used.
 */

#include "error_handler.h"
#include "logger.h"

bool ErrorHandler::initialized = false;

void ErrorHandler::initialize() {
    if (initialized) return;
    
    initialized = true;
    LOG_INFO("Error handler initialized");
}

bool ErrorHandler::systemHealthy() {
    // System is healthy if we have adequate memory
    return ESP.getFreeHeap() > 50000;
}
