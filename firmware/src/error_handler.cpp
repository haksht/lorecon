/**
 * Error Handler Implementation
 */

#include "error_handler.h"
#include "lora_recon_tool.h"
#include <esp_task_wdt.h>

// Static member initialization
ErrorInfo ErrorHandler::errorHistory[MAX_ERROR_HISTORY] = {};
uint8_t ErrorHandler::errorIndex = 0;
uint8_t ErrorHandler::errorCount = 0;
uint16_t ErrorHandler::categoryCounts[9] = {};
uint32_t ErrorHandler::lastErrorTime = 0;
bool ErrorHandler::initialized = false;

void ErrorHandler::initialize() {
    memset(errorHistory, 0, sizeof(errorHistory));
    memset(categoryCounts, 0, sizeof(categoryCounts));
    errorIndex = 0;
    errorCount = 0;
    lastErrorTime = 0;
    initialized = true;
    
    Serial.println("[ERROR_HANDLER] Initialized - production-grade error recovery enabled");
}

void ErrorHandler::logError(ErrorCategory category, ErrorSeverity severity, 
                            uint16_t code, const char* message,
                            const char* function, uint16_t line) {
    if (!initialized) initialize();
    
    // Create error record
    ErrorInfo error;
    error.category = category;
    error.severity = severity;
    error.code = code;
    error.message = message;
    error.function = function;
    error.line = line;
    error.timestamp = millis();
    error.recovered = false;
    
    // Log to serial
    logToSerial(error);
    
    // Store in history (circular buffer)
    errorHistory[errorIndex] = error;
    errorIndex = (errorIndex + 1) % MAX_ERROR_HISTORY;
    if (errorCount < MAX_ERROR_HISTORY) errorCount++;
    
    // Update category counters
    if (category < 9) {
        categoryCounts[category]++;
    }
    
    lastErrorTime = millis();
    
    // Attempt recovery for recoverable errors
    if (severity <= SEVERITY_ERROR) {
        bool recovered = attemptRecovery(error);
        if (recovered) {
            // Update recovery status in history
            uint8_t lastIdx = (errorIndex == 0) ? (MAX_ERROR_HISTORY - 1) : (errorIndex - 1);
            errorHistory[lastIdx].recovered = true;
            Serial.println("[ERROR_HANDLER] ✅ Automatic recovery successful");
        } else {
            Serial.println("[ERROR_HANDLER] ⚠️ Recovery failed - manual intervention may be needed");
        }
    }
    
    // Critical and fatal errors may require system halt
    if (severity == SEVERITY_CRITICAL) {
        Serial.println("[ERROR_HANDLER] 🚨 CRITICAL ERROR - System stability compromised");
        printHealthReport();
    } else if (severity == SEVERITY_FATAL) {
        Serial.println("[ERROR_HANDLER] ☠️ FATAL ERROR - System halt required");
        printHealthReport();
        printErrorHistory();
        while(1) {
            esp_task_wdt_reset();  // Keep watchdog happy but halt
            delay(1000);
        }
    }
}

void ErrorHandler::radioError(uint16_t code, const char* message, 
                              const char* function, uint16_t line) {
    logError(ERR_RADIO, SEVERITY_ERROR, code, message, function, line);
}

void ErrorHandler::memoryError(uint16_t code, const char* message,
                               const char* function, uint16_t line) {
    logError(ERR_MEMORY, SEVERITY_CRITICAL, code, message, function, line);
}

void ErrorHandler::stateError(uint16_t code, const char* message,
                              const char* function, uint16_t line) {
    logError(ERR_STATE, SEVERITY_WARNING, code, message, function, line);
}

bool ErrorHandler::attemptRecovery(const ErrorInfo& error) {
    Serial.printf("[ERROR_HANDLER] Attempting recovery for error %d...\n", error.code);
    
    switch (error.category) {
        case ERR_RADIO:
            return recoverRadioError(error);
            
        case ERR_MEMORY:
            return recoverMemoryError(error);
            
        case ERR_STATE:
            return recoverStateError(error);
            
        default:
            Serial.println("[ERROR_HANDLER] No recovery strategy available");
            return false;
    }
}

bool ErrorHandler::recoverRadioError(const ErrorInfo& error) {
    Serial.println("[ERROR_HANDLER] Attempting radio recovery...");
    
    if (g_reconTool) {
        delay(100);  // Brief pause
        
        // Try to re-initialize radio
        SX1262& radio = g_reconTool->getRadio();
        
        // Reset radio
        int state = radio.reset();
        if (state != RADIOLIB_ERR_NONE) {
            Serial.println("[ERROR_HANDLER] Radio reset failed");
            return false;
        }
        
        delay(100);
        
        // Re-initialize with known good configuration
        state = radio.begin();
        if (state != RADIOLIB_ERR_NONE) {
            Serial.println("[ERROR_HANDLER] Radio re-initialization failed");
            return false;
        }
        
        // Apply current configuration
        g_reconTool->applyConfigPublic(reconState.scanState.currentConfig);
        g_reconTool->startReceiving();
        
        Serial.println("[ERROR_HANDLER] Radio recovery successful");
        return true;
    }
    
    return false;
}

bool ErrorHandler::recoverMemoryError(const ErrorInfo& error) {
    Serial.println("[ERROR_HANDLER] Attempting memory recovery...");
    
    // Check heap status
    size_t freeHeap = ESP.getFreeHeap();
    size_t minFreeHeap = ESP.getMinFreeHeap();
    
    Serial.printf("[ERROR_HANDLER] Free heap: %d bytes, Min: %d bytes\n", 
                  freeHeap, minFreeHeap);
    
    if (freeHeap < 10000) {
        Serial.println("[ERROR_HANDLER] ⚠️ Low memory condition detected");
        // Could trigger cleanup here if we had object pools
        return false;
    }
    
    return true;
}

bool ErrorHandler::recoverStateError(const ErrorInfo& error) {
    Serial.println("[ERROR_HANDLER] Attempting state recovery...");
    
    // State errors are usually non-critical, just log and continue
    return true;
}

bool ErrorHandler::resetRadio() {
    Serial.println("[ERROR_HANDLER] Manual radio reset requested");
    ErrorInfo dummy = {ERR_RADIO, SEVERITY_ERROR, 0, "Manual reset", nullptr, 0, millis(), false};
    return recoverRadioError(dummy);
}

bool ErrorHandler::resetState() {
    Serial.println("[ERROR_HANDLER] Manual state reset requested");
    reconState.reset();
    return true;
}

void ErrorHandler::printErrorHistory() {
    if (!initialized || errorCount == 0) {
        Serial.println("[ERROR_HANDLER] No errors logged");
        return;
    }
    
    Serial.println("\n╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║              ERROR HISTORY (Last 20 Errors)                  ║");
    Serial.println("╠══════════════════════════════════════════════════════════════╣");
    
    uint8_t start = (errorCount < MAX_ERROR_HISTORY) ? 0 : errorIndex;
    uint8_t count = min(errorCount, MAX_ERROR_HISTORY);
    
    for (uint8_t i = 0; i < count; i++) {
        uint8_t idx = (start + i) % MAX_ERROR_HISTORY;
        const ErrorInfo& err = errorHistory[idx];
        
        Serial.printf("║ [%3d] %s/%s (Code: %d)\n", 
                      i + 1,
                      categoryToString(err.category),
                      severityToString(err.severity),
                      err.code);
        Serial.printf("║       %s\n", err.message);
        
        if (err.function) {
            Serial.printf("║       at %s:%d\n", err.function, err.line);
        }
        
        Serial.printf("║       Time: +%u ms, Recovered: %s\n",
                      (unsigned int)err.timestamp,
                      err.recovered ? "YES" : "NO");
        Serial.println("║");
    }
    
    Serial.println("╚══════════════════════════════════════════════════════════════╝\n");
}

void ErrorHandler::printLastError() {
    if (errorCount == 0) {
        Serial.println("[ERROR_HANDLER] No errors logged");
        return;
    }
    
    uint8_t lastIdx = (errorIndex == 0) ? (MAX_ERROR_HISTORY - 1) : (errorIndex - 1);
    const ErrorInfo& err = errorHistory[lastIdx];
    
    Serial.println("\n=== LAST ERROR ===");
    Serial.printf("Category: %s\n", categoryToString(err.category));
    Serial.printf("Severity: %s\n", severityToString(err.severity));
    Serial.printf("Code: %d\n", err.code);
    Serial.printf("Message: %s\n", err.message);
    if (err.function) {
        Serial.printf("Location: %s:%d\n", err.function, err.line);
    }
    Serial.printf("Time: +%u ms\n", (unsigned int)err.timestamp);
    Serial.printf("Recovered: %s\n", err.recovered ? "YES" : "NO");
    Serial.println();
}

uint16_t ErrorHandler::getErrorCount(ErrorCategory category) {
    if (category == ERR_SYSTEM) {
        return errorCount;
    }
    return (category < 9) ? categoryCounts[category] : 0;
}

bool ErrorHandler::hasErrors() {
    return errorCount > 0;
}

void ErrorHandler::clearErrors() {
    memset(errorHistory, 0, sizeof(errorHistory));
    memset(categoryCounts, 0, sizeof(categoryCounts));
    errorIndex = 0;
    errorCount = 0;
    Serial.println("[ERROR_HANDLER] Error history cleared");
}

bool ErrorHandler::systemHealthy() {
    // System is healthy if:
    // 1. No critical errors in last 60 seconds
    // 2. Less than 10 errors total
    // 3. Memory is adequate
    
    uint32_t now = millis();
    bool noCriticalRecent = (now - lastErrorTime) > 60000 || errorCount == 0;
    bool fewErrors = errorCount < 10;
    bool memoryOk = ESP.getFreeHeap() > 50000;
    
    return noCriticalRecent && fewErrors && memoryOk;
}

void ErrorHandler::printHealthReport() {
    Serial.println("\n╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║                    SYSTEM HEALTH REPORT                      ║");
    Serial.println("╠══════════════════════════════════════════════════════════════╣");
    
    // Overall status
    bool healthy = systemHealthy();
    Serial.printf("║ Overall Status: %s                                        ║\n",
                  healthy ? "✅ HEALTHY " : "⚠️ DEGRADED");
    Serial.println("║                                                              ║");
    
    // Error counts by category
    Serial.println("║ Error Counts by Category:                                    ║");
    const char* categories[] = {"Radio", "Memory", "State", "Config", "Interrupt", 
                                "Storage", "Protocol", "Timeout", "System"};
    for (uint8_t i = 0; i < 9; i++) {
        if (categoryCounts[i] > 0) {
            Serial.printf("║   %-10s: %3d errors                                   ║\n",
                          categories[i], categoryCounts[i]);
        }
    }
    Serial.println("║                                                              ║");
    
    // Memory status
    size_t freeHeap = ESP.getFreeHeap();
    size_t minFreeHeap = ESP.getMinFreeHeap();
    size_t heapSize = ESP.getHeapSize();
    
    Serial.println("║ Memory Status:                                               ║");
    Serial.printf("║   Free Heap:  %6d / %6d bytes (%3d%%)                  ║\n",
                  freeHeap, heapSize, (freeHeap * 100) / heapSize);
    Serial.printf("║   Min Free:   %6d bytes                                  ║\n",
                  minFreeHeap);
    
    if (freeHeap < 50000) {
        Serial.println("║   ⚠️ WARNING: Low memory condition                           ║");
    }
    
    Serial.println("║                                                              ║");
    
    // Uptime
    uint32_t uptime = millis() / 1000;
    Serial.printf("║ Uptime: %u seconds (%u min)                              ║\n",
                  (unsigned int)uptime, (unsigned int)(uptime / 60));
    
    Serial.println("╚══════════════════════════════════════════════════════════════╝\n");
}

void ErrorHandler::logToSerial(const ErrorInfo& error) {
    // Severity indicator
    const char* indicator;
    switch (error.severity) {
        case SEVERITY_INFO:     indicator = "ℹ️"; break;
        case SEVERITY_WARNING:  indicator = "⚠️"; break;
        case SEVERITY_ERROR:    indicator = "❌"; break;
        case SEVERITY_CRITICAL: indicator = "🚨"; break;
        case SEVERITY_FATAL:    indicator = "☠️"; break;
        default:                indicator = "?"; break;
    }
    
    Serial.printf("[ERROR] %s %s/%s (Code: %d): %s",
                  indicator,
                  categoryToString(error.category),
                  severityToString(error.severity),
                  error.code,
                  error.message);
    
    if (error.function) {
        Serial.printf(" at %s:%d", error.function, error.line);
    }
    
    Serial.println();
}

const char* ErrorHandler::severityToString(ErrorSeverity severity) {
    switch (severity) {
        case SEVERITY_INFO:     return "INFO";
        case SEVERITY_WARNING:  return "WARNING";
        case SEVERITY_ERROR:    return "ERROR";
        case SEVERITY_CRITICAL: return "CRITICAL";
        case SEVERITY_FATAL:    return "FATAL";
        default:                return "UNKNOWN";
    }
}

const char* ErrorHandler::categoryToString(ErrorCategory category) {
    switch (category) {
        case ERR_RADIO:     return "RADIO";
        case ERR_MEMORY:    return "MEMORY";
        case ERR_STATE:     return "STATE";
        case ERR_CONFIG:    return "CONFIG";
        case ERR_INTERRUPT: return "INTERRUPT";
        case ERR_STORAGE:   return "STORAGE";
        case ERR_PROTOCOL:  return "PROTOCOL";
        case ERR_USER_TIMEOUT: return "TIMEOUT";
        case ERR_SYSTEM:    return "SYSTEM";
        default:            return "UNKNOWN";
    }
}
