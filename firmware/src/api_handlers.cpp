/**
 * API Handlers Implementation
 * 
 * REST API request handlers extracted from web_server.cpp.
 * All handlers are free functions in the APIHandlers namespace.
 */

#include "api_handlers.h"
#include "api_controller.h"
#include "api_security.h"
#include "recon_state.h"
#include "packet_logger.h"
#include "logger.h"
#include "config.h"
#include "utils/json_utils.h"
#include <SD.h>
#include <ArduinoJson.h>

namespace APIHandlers {

// =============================================================================
// DEVICE MANAGEMENT
// =============================================================================

void handleGetDevices(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::getDevices());
}

void handleGetDevice(AsyncWebServerRequest* request) {
    if (!request->hasParam("nodeId")) {
        request->send(400, "application/json", JsonUtils::error("Missing nodeId parameter"));
        return;
    }
    
    String nodeIdStr = request->getParam("nodeId")->value();
    char* endPtr = nullptr;
    uint32_t nodeId = strtoul(nodeIdStr.c_str(), &endPtr, 16);
    
    // Validate parse succeeded (endPtr moved and no trailing garbage)
    if (endPtr == nodeIdStr.c_str() || (*endPtr != '\0' && !isspace(*endPtr))) {
        request->send(400, "application/json", JsonUtils::error("Invalid nodeId format"));
        return;
    }
    
    request->send(200, "application/json", APIController::getDevice(nodeId));
}

void handleStartCapture(AsyncWebServerRequest* request) {
    if (!request->hasParam("nodeId", true)) {
        request->send(400, "application/json", JsonUtils::error("Missing nodeId in body"));
        return;
    }
    
    String nodeIdStr = request->getParam("nodeId", true)->value();
    char* endPtr = nullptr;
    uint32_t nodeId = strtoul(nodeIdStr.c_str(), &endPtr, 16);
    
    // Validate parse succeeded
    if (endPtr == nodeIdStr.c_str() || (*endPtr != '\0' && !isspace(*endPtr))) {
        request->send(400, "application/json", JsonUtils::error("Invalid nodeId format"));
        return;
    }
    
    request->send(200, "application/json", APIController::startTargetedCapture(nodeId));
}

void handleStopCapture(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::stopCapture());
}

// =============================================================================
// GEOGRAPHIC DATA
// =============================================================================

void handleGetPositions(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::getPositions());
}

void handleExportGeoJSON(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::exportGeoJSON());
}

void handleExportKML(AsyncWebServerRequest* request) {
    request->send(200, "application/vnd.google-earth.kml+xml", APIController::exportKML());
}

void handleExportPCAP(AsyncWebServerRequest* request) {
    #ifdef ENABLE_PCAP_EXPORT
    // Check if SD card is available
    if (!packetLogger.isAvailable()) {
        request->send(404, "application/json", 
            JsonUtils::error("SD card not available. Insert SD card and restart device to enable PCAP capture."));
        LOG_WARN("PCAP download requested but SD card not available");
        return;
    }
    
    String sessionFile = packetLogger.getCurrentSessionFile();
    
    // Check if a session is active
    if (sessionFile.isEmpty()) {
        request->send(404, "application/json", 
            JsonUtils::error("No active logging session. Wait for packet capture to start."));
        LOG_WARN("PCAP download requested but no active session");
        return;
    }
    
    // Build PCAP file path: /logs/<session>.pcap
    String pcapFile = "/logs/";
    if (sessionFile.endsWith(".csv")) {
        pcapFile += sessionFile.substring(0, sessionFile.length() - 4) + ".pcap";
    } else {
        pcapFile += sessionFile + ".pcap";
    }
    
    // Check if PCAP file exists
    if (SD.exists(pcapFile.c_str())) {
        // Send file with proper MIME type and force download
        request->send(SD, pcapFile, "application/vnd.tcpdump.pcap", true);
        LOG_INFO("PCAP file downloaded: %s", pcapFile.c_str());
    } else {
        // PCAP file doesn't exist - provide helpful error message
        String csvPath = "/logs/" + sessionFile;
        bool csvExists = SD.exists(csvPath.c_str());
        
        if (csvExists) {
            request->send(404, "application/json", 
                JsonUtils::error("PCAP file not found. CSV logging is active but PCAP generation may have failed."));
        } else {
            request->send(404, "application/json", 
                JsonUtils::error("No packet capture file available. Ensure SD card is inserted and packets have been captured."));
        }
        
        LOG_WARN("PCAP download requested but file not found: %s (CSV exists: %s)", 
                 pcapFile.c_str(), csvExists ? "yes" : "no");
    }
    #else
    // PCAP export disabled
    request->send(501, "application/json", 
        JsonUtils::error("PCAP export is disabled. Enable ENABLE_PCAP_EXPORT in config.h and recompile."));
    #endif
}

// =============================================================================
// STATUS & CONFIGURATION
// =============================================================================

void handleGetStatus(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::getStatus());
}

void handleGetDashboard(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::getDashboard());
}

void handleGetStatistics(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::getStatistics());
}

void handleGetActivity(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::getRFActivity());
}

void handleGetConfig(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::getConfig());
}

void handleGetSystemConfig(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::getSystemConfig());
}

// =============================================================================
// SCAN CONTROL
// =============================================================================

void handleStartScan(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::startScan());
}

void handleStopScan(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::stopScan());
}

// =============================================================================
// RECONNAISSANCE DATA
// =============================================================================

void handleGetReconSummary(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::getReconSummary());
}

void handleGetDeviceTypeSummary(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::getDeviceTypeSummary());
}

void handleGetSecurityAssessment(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::getSecurityAssessment());
}

// =============================================================================
// ANOMALY & TEMPORAL ANALYSIS
// =============================================================================

void handleGetAnomalies(AsyncWebServerRequest* request) {
    bool unacknowledgedOnly = false;
    if (request->hasParam("unacknowledged")) {
        String val = request->getParam("unacknowledged")->value();
        unacknowledgedOnly = (val == "true" || val == "1");
    }
    
    request->send(200, "application/json", APIController::getAnomalies(unacknowledgedOnly));
}

void handleAcknowledgeAnomaly(AsyncWebServerRequest* request) {
    if (!request->hasParam("index", true)) {
        request->send(400, "application/json", JsonUtils::error("Missing index in body"));
        return;
    }
    
    String indexStr = request->getParam("index", true)->value();
    
    // Safe parsing with bounds checking
    char* endPtr = nullptr;
    unsigned long indexVal = strtoul(indexStr.c_str(), &endPtr, 10);
    
    // Validate: must be a valid number and within uint8_t range
    if (endPtr == indexStr.c_str() || *endPtr != '\0' || indexVal > 255) {
        request->send(400, "application/json", JsonUtils::error("Invalid index (0-255)"));
        return;
    }
    
    request->send(200, "application/json", APIController::acknowledgeAnomaly(static_cast<uint8_t>(indexVal)));
}

void handleGetTemporalData(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::getTemporalData());
}

// =============================================================================
// PSK STATISTICS
// =============================================================================

void handleGetPSKStats(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::getPSKStats());
}

// =============================================================================
// PACKET REPLAY
// =============================================================================

void handleGetReplaySlots(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::getReplaySlots());
}

void handleClearReplaySlots(AsyncWebServerRequest* request) {
    // Protected endpoint - requires authentication and rate limiting
    if (!APISecurity::checkRateLimit(request)) {
        APISecurity::sendRateLimited(request);
        return;
    }
    if (!APISecurity::isAuthenticated(request)) {
        APISecurity::sendUnauthorized(request);
        return;
    }
    
    request->send(200, "application/json", APIController::clearReplaySlots());
}

void handleReplayPacket(AsyncWebServerRequest* request) {
    // Protected endpoint - requires authentication and rate limiting (transmits RF)
    if (!APISecurity::checkRateLimit(request)) {
        APISecurity::sendRateLimited(request);
        return;
    }
    if (!APISecurity::isAuthenticated(request)) {
        APISecurity::sendUnauthorized(request);
        return;
    }
    
    // Get parameters from POST body
    AsyncWebParameter* slotParam = nullptr;
    AsyncWebParameter* repeatParam = nullptr;
    AsyncWebParameter* delayParam = nullptr;

    if (request->hasParam("slotIndex", true)) {
        slotParam = request->getParam("slotIndex", true);
    } else if (request->hasParam("slotIndex")) {
        slotParam = request->getParam("slotIndex");
    }

    if (request->hasParam("repeatCount", true)) {
        repeatParam = request->getParam("repeatCount", true);
    } else if (request->hasParam("repeatCount")) {
        repeatParam = request->getParam("repeatCount");
    }

    if (request->hasParam("delayMs", true)) {
        delayParam = request->getParam("delayMs", true);
    } else if (request->hasParam("delayMs")) {
        delayParam = request->getParam("delayMs");
    }

    if (!slotParam) {
        request->send(400, "application/json", JsonUtils::error("Missing slotIndex"));
        return;
    }

    uint8_t rawSlotIndex = strtoul(slotParam->value().c_str(), nullptr, 0);
    uint8_t rawRepeatCount = repeatParam ? strtoul(repeatParam->value().c_str(), nullptr, 0) : 1;
    uint16_t rawDelayMs = delayParam ? strtoul(delayParam->value().c_str(), nullptr, 0) : 1000;
    
    // Apply security bounds to prevent abuse
    uint8_t repeatCount;
    uint16_t delayMs;
    APISecurity::boundReplayParams(rawRepeatCount, rawDelayMs, repeatCount, delayMs);

    request->send(200, "application/json", APIController::replayPacket(rawSlotIndex, repeatCount, delayMs));
}

// =============================================================================
// DEVICE MANAGEMENT ACTIONS
// =============================================================================

void handleClearDevices(AsyncWebServerRequest* request) {
    // Protected endpoint - requires authentication and rate limiting
    if (!APISecurity::checkRateLimit(request)) {
        APISecurity::sendRateLimited(request);
        return;
    }
    if (!APISecurity::isAuthenticated(request)) {
        APISecurity::sendUnauthorized(request);
        return;
    }
    
    request->send(200, "application/json", APIController::clearDevices());
}

void handleStartFrequencyTargeting(AsyncWebServerRequest* request) {
    AsyncWebParameter* param = nullptr;
    if (request->hasParam("configIndex", true)) {
        param = request->getParam("configIndex", true);
    } else if (request->hasParam("configIndex")) {
        param = request->getParam("configIndex");
    }

    if (!param) {
        request->send(400, "application/json", JsonUtils::error("Missing configIndex"));
        return;
    }

    String indexStr = param->value();
    uint32_t rawIndex = strtoul(indexStr.c_str(), nullptr, 0);

    if (rawIndex > UINT8_MAX) {
        request->send(400, "application/json", JsonUtils::error("configIndex out of range"));
        return;
    }

    uint8_t configIndex = static_cast<uint8_t>(rawIndex);
    if (!reconState.isValidConfigIndex(configIndex) && rawIndex > 0) {
        uint32_t adjusted = rawIndex - 1;
        if (adjusted <= UINT8_MAX && reconState.isValidConfigIndex(static_cast<uint8_t>(adjusted))) {
            configIndex = static_cast<uint8_t>(adjusted);
        }
    }

    request->send(200, "application/json", APIController::startFrequencyTargeting(configIndex));
}

// =============================================================================
// DIAGNOSTICS
// =============================================================================

void handleGetDiagnostics(AsyncWebServerRequest* request) {
    request->send(200, "application/json", APIController::getDiagnostics());
}

void handleSetVerboseMode(AsyncWebServerRequest* request) {
    AsyncWebParameter* param = nullptr;
    if (request->hasParam("enable", true)) {
        param = request->getParam("enable", true);
    } else if (request->hasParam("enable")) {
        param = request->getParam("enable");
    }

    if (!param) {
        request->send(400, "application/json", JsonUtils::error("Missing enable parameter"));
        return;
    }

    String enableStr = param->value();
    enableStr.trim();
    enableStr.toLowerCase();

    bool enableVerbose;
    if (enableStr == "true" || enableStr == "1" || enableStr == "yes" || enableStr == "on" || enableStr == "verbose") {
        enableVerbose = true;
    } else if (enableStr == "false" || enableStr == "0" || enableStr == "no" || enableStr == "off" || enableStr == "quiet") {
        enableVerbose = false;
    } else {
        request->send(400, "application/json", JsonUtils::error("Invalid enable value"));
        return;
    }

    request->send(200, "application/json", APIController::setVerboseMode(enableVerbose));
}

// =============================================================================
// COMMANDS
// =============================================================================

void handleCommand(AsyncWebServerRequest* request) {
    // Authentication and rate limiting required - command can reboot device
    if (!APISecurity::checkRateLimit(request)) {
        APISecurity::sendRateLimited(request);
        return;
    }
    if (!APISecurity::isAuthenticated(request)) {
        APISecurity::sendUnauthorized(request);
        return;
    }
    
    AsyncWebParameter* param = nullptr;
    if (request->hasParam("command", true)) {
        param = request->getParam("command", true);
    } else if (request->hasParam("command")) {
        param = request->getParam("command");
    }

    if (!param) {
        request->send(400, "application/json", JsonUtils::error("Missing command"));
        return;
    }

    String cmd = param->value();
    
    // Handle reboot command
    if (cmd == "b") {
        AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", 
            JsonUtils::success("Rebooting device..."));
        resp->addHeader("Connection", "close");
        request->send(resp);
        
        LOG_INFO("Reboot command received via web API");
        delay(1000);
        ESP.restart();
        return;
    }
    
    // Unknown command
    request->send(400, "application/json", JsonUtils::error("Unknown command"));
}

}  // namespace APIHandlers
