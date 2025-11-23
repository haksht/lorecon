/**
 * API Controller Implementation
 */

#include "api_controller.h"
#include "config.h"
#include "logger.h"
#include "recon_service.h"
#include "radio_controller.h"

/**
 * Set reconnaissance tool reference
 */
void APIController::setReconTool(IReconTool* tool) {
    ReconService::initialize(tool);
    LOG_INFO("API Controller initialized");
}

/**
 * Create error response JSON
 */
String APIController::createErrorResponse(const String& error) {
    JsonDocument doc;
    doc["status"] = "error";
    doc["error"] = error;
    
    String response;
    serializeJson(doc, response);
    return response;
}

/**
 * Create success response JSON with message
 */
String APIController::createSuccessResponse(const String& message) {
    JsonDocument doc;
    doc["status"] = "success";
    doc["message"] = message;
    
    String response;
    serializeJson(doc, response);
    return response;
}

/**
 * GET /api/devices
 * 
 * Returns list of all discovered devices with their details
 */
String APIController::getDevices() {
    return ReconService::buildDevicesJson();
}

/**
 * GET /api/device/:nodeId
 * 
 * Returns detailed information about specific device
 */
String APIController::getDevice(uint32_t nodeId) {
    return ReconService::buildDeviceJson(nodeId);
}

/**
 * POST /api/capture/start
 * 
 * Start targeted capture on specific device
 */
String APIController::startTargetedCapture(uint32_t nodeId) {
    String message;
    if (!ReconService::startTargetedCaptureByNodeId(nodeId, message)) {
        return createErrorResponse(message);
    }
    return createSuccessResponse(message);
}

/**
 * POST /api/capture/stop
 * 
 * Stop current capture and resume reconnaissance
 */
String APIController::stopCapture() {
    String message;
    ReconService::stopCapture(message);
    return createSuccessResponse(message);
}

/**
 * GET /api/packets
 * 
 * Returns recent packets (limited and paginated)
 */
String APIController::getPackets(int limit, int offset) {
    JsonDocument doc;
    doc["status"] = "success";
    doc["limit"] = limit;
    doc["offset"] = offset;
    doc["totalPackets"] = reconState.scanState.totalPackets;
    doc["packets"] = JsonArray();
    doc["message"] = "Packet history requires SD card logging";

    String response;
    serializeJson(doc, response);
    return response;
}

/**
 * GET /api/positions
 * 
 * Returns all GPS positions from discovered devices
 */
String APIController::getPositions() {
    return ReconService::buildPositionsJson();
}

/**
 * GET /api/export/geojson
 * 
 * Export geographic data as GeoJSON
 */
String APIController::exportGeoJSON() {
    return ReconService::buildGeoJson();
}

/**
 * GET /api/export/kml
 * 
 * Export geographic data as KML (Google Earth format)
 */
String APIController::exportKML() {
    return ReconService::buildKml();
}

/**
 * GET /api/status
 * 
 * Returns current system status
 */
String APIController::getStatus() {
    return ReconService::buildStatusJson();
}

/**
 * GET /api/dashboard
 * 
 * Returns combined data for initial dashboard load
 * Reduces number of API calls on page load
 */
String APIController::getDashboard() {
    JsonDocument doc;
    doc["status"] = "success";
    
    // Status data
    JsonDocument statusDoc;
    deserializeJson(statusDoc, ReconService::buildStatusJson());
    if (statusDoc.containsKey("status") && statusDoc["status"] == "success") {
        doc["systemStatus"] = statusDoc;
    }
    
    // Devices list
    JsonDocument devicesDoc;
    deserializeJson(devicesDoc, ReconService::buildDevicesJson());
    if (devicesDoc.containsKey("devices")) {
        doc["devices"] = devicesDoc["devices"];
        doc["deviceCount"] = devicesDoc["devices"].size();
    } else {
        doc["devices"] = JsonArray();
        doc["deviceCount"] = 0;
    }
    
    // RF Activity
    JsonDocument activityDoc;
    deserializeJson(activityDoc, ReconService::buildActivityJson());
    if (activityDoc.containsKey("activities")) {
        doc["activities"] = activityDoc["activities"];
    }
    
    // Replay slots
    doc["replaySlots"] = reconState.numCapturedPackets;
    doc["maxReplaySlots"] = Config::Replay::MAX_SLOTS;
    
    // Basic stats
    doc["totalPackets"] = reconState.scanState.totalPackets;
    doc["uptime"] = millis() / 1000;
    doc["mode"] = reconState.scanState.mode;
    doc["currentConfig"] = reconState.scanState.currentConfig;
    
    String response;
    serializeJson(doc, response);
    return response;
}

/**
 * GET /api/statistics
 * 
 * Returns comprehensive statistics
 */
String APIController::getStatistics() {
    return ReconService::buildStatisticsJson();
}

/**
 * GET /api/activity
 * 
 * Returns RF activity analysis
 */
String APIController::getRFActivity() {
    return ReconService::buildActivityJson();
}

/**
 * GET /api/config
 * 
 * Returns current configuration
 */
String APIController::getConfig() {
    JsonDocument doc;
    
    doc["status"] = "success";
    
    JsonObject config = doc["config"].to<JsonObject>();
    config["scanDwellTime"] = Config::Scanning::DWELL_TIME_MS;
    config["queueSize"] = Config::PacketProcessing::QUEUE_SIZE;
    config["maxDevices"] = Config::Tracking::MAX_DEVICES;
    config["deviceTimeout"] = Config::Tracking::DEVICE_TIMEOUT_MS;
    
    String response;
    serializeJson(doc, response);
    return response;
}

/**
 * POST /api/scan/start
 * 
 * Start or resume reconnaissance scan
 */
String APIController::startScan() {
    // Resume reconnaissance WITHOUT clearing discovered devices/data
    LOG_INFO("API: Resuming reconnaissance scan");
    
    // Reset scan state to restart the cycle
    reconState.scanState.mode = MODE_RECONNAISSANCE;
    reconState.scanState.currentConfig = 0;
    reconState.scanState.lastScanSwitch = millis();
    reconState.scanState.packetPending = false;
    reconState.scanState.waitingForUserInput = false;
    
    // Apply first config and start receiving via ReconService
    IReconTool* tool = ReconService::getReconTool();
    if (tool) {
        const ScanConfig& cfg = reconState.getScanConfig(0);
        tool->getRadioController()->applyConfig(cfg);
        tool->getRadioController()->startReceive();
        LOG_INFO("Radio reconfigured for reconnaissance");
    }
    
    return createSuccessResponse("Reconnaissance scan started");
}

/**
 * POST /api/scan/stop
 * 
 * Stop current scan
 */
String APIController::stopScan() {
    reconState.scanState.mode = MODE_TARGETED_CAPTURE;  // Use as idle/paused state
    return createSuccessResponse("Scan stopped");
}

String APIController::getReconSummary() {
    return ReconService::buildReconSummaryJson();
}

String APIController::getDeviceTypeSummary() {
    return ReconService::buildDeviceTypeSummaryJson();
}

String APIController::getSecurityAssessment() {
    return ReconService::buildSecurityAssessmentJson();
}

String APIController::getReplaySlots() {
    return ReconService::buildReplaySlotsJson();
}

String APIController::clearReplaySlots() {
    String message;
    if (!ReconService::clearReplaySlots(message)) {
        return createErrorResponse(message);
    }
    return createSuccessResponse(message);
}

String APIController::replayPacket(uint8_t slotIndex, uint8_t repeatCount, uint16_t delayMs) {
    String message;
    if (!ReconService::replayPacket(slotIndex, repeatCount, delayMs, message)) {
        return createErrorResponse(message);
    }
    return createSuccessResponse(message);
}

String APIController::startFrequencyTargeting(uint8_t configIndex) {
    String message;
    if (!ReconService::startFrequencyTargeting(configIndex, message)) {
        return createErrorResponse(message);
    }
    return createSuccessResponse(message);
}

String APIController::getDiagnostics() {
    return ReconService::buildDiagnosticsJson();
}

String APIController::setVerboseMode(bool enableVerbose) {
    String message;
    if (!ReconService::setVerboseMode(enableVerbose, message)) {
        return createErrorResponse(message);
    }
    return createSuccessResponse(message);
}
