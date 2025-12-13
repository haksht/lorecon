/**
 * API Controller Implementation
 */

#include "api_controller.h"
#include "config.h"
#include "logger.h"
#include "recon_service.h"
#include "radio_controller.h"
#include "lora_recon_tool.h"  // For g_reconTool global
#include <SD.h>

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
    if (statusDoc["status"].is<const char*>() && statusDoc["status"] == "success") {
        doc["systemStatus"] = statusDoc;
    }
    
    // Devices list
    JsonDocument devicesDoc;
    deserializeJson(devicesDoc, ReconService::buildDevicesJson());
    if (devicesDoc["devices"].is<JsonArray>()) {
        doc["devices"] = devicesDoc["devices"];
        doc["deviceCount"] = devicesDoc["devices"].size();
    } else {
        doc["devices"] = JsonArray();
        doc["deviceCount"] = 0;
    }
    
    // RF Activity
    JsonDocument activityDoc;
    deserializeJson(activityDoc, ReconService::buildActivityJson());
    if (activityDoc["activities"].is<JsonArray>()) {
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
    reconState.scanState.mode = MODE_INTERACTIVE_MENU;
    if (g_reconTool) {
        g_reconTool->setMenuModeEntered();
    }
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

/**
 * GET /api/config/system
 * 
 * Returns system configuration constants and runtime limits (read-only)
 */
String APIController::getSystemConfig() {
    JsonDocument doc;
    
    // Scanning configuration
    JsonObject scanning = doc["scanning"].to<JsonObject>();
    scanning["dwellTimeMs"] = Config::Scanning::DWELL_TIME_MS;
    scanning["numConfigs"] = Config::Scanning::NUM_CONFIGURATIONS;
    scanning["cycleTimeMinutes"] = (Config::Scanning::NUM_CONFIGURATIONS * Config::Scanning::DWELL_TIME_MS) / 60000;
    
    // Packet processing
    JsonObject processing = doc["processing"].to<JsonObject>();
    processing["queueSize"] = Config::PacketProcessing::QUEUE_SIZE;
    processing["maxPacketSize"] = Config::PacketProcessing::MAX_PACKET_SIZE;
    processing["metricCacheMs"] = Config::PacketProcessing::METRIC_CACHE_MS;
    
    // Device tracking
    JsonObject tracking = doc["tracking"].to<JsonObject>();
    tracking["maxDevices"] = Config::Tracking::MAX_DEVICES;
    tracking["maxNodes"] = Config::Tracking::MAX_HOT_NODES;
    tracking["maxGeoPoints"] = Config::Tracking::MAX_GEO_POINTS;
    tracking["deviceTimeoutMs"] = Config::Tracking::DEVICE_TIMEOUT_MS;
    
    // Current usage
    JsonObject usage = doc["usage"].to<JsonObject>();
    usage["devices"] = reconState.numTargetableDevices;
    usage["nodes"] = reconState.nodeCount;
    usage["replaySlots"] = reconState.numCapturedPackets;
    usage["droppedPackets"] = reconState.scanState.droppedPackets;
    usage["totalPackets"] = reconState.scanState.totalPackets;
    
    // Replay configuration
    JsonObject replay = doc["replay"].to<JsonObject>();
    replay["maxSlots"] = Config::Replay::MAX_SLOTS;
    
    // PSK configuration
    JsonObject psk = doc["psk"].to<JsonObject>();
    psk["numDefaultKeys"] = Config::PSK::NUM_DEFAULT_KEYS;
    psk["keySize"] = Config::PSK::KEY_SIZE;
    
    // UI configuration
    JsonObject ui = doc["ui"].to<JsonObject>();
    ui["menuTimeoutMs"] = Config::UI::MENU_TIMEOUT_MS;
    ui["buttonLongPressMs"] = Config::UI::BUTTON_LONG_PRESS_MS;
    
    // System configuration
    JsonObject system = doc["system"].to<JsonObject>();
    system["watchdogTimeoutSec"] = Config::System::WATCHDOG_TIMEOUT_SEC;
    system["freeHeap"] = ESP.getFreeHeap();
    system["minFreeHeap"] = ESP.getMinFreeHeap();
    system["uptimeMs"] = millis();
    
    // Hardware capabilities
    JsonObject hardware = doc["hardware"].to<JsonObject>();
    hardware["board"] = "HELTEC_V3";
    #ifdef HAS_OLED_DISPLAY
    hardware["hasOLED"] = true;
    #else
    hardware["hasOLED"] = false;
    #endif
    #ifdef HAS_SD_CARD
    hardware["hasSD"] = SD.cardSize() > 0;
    #else
    hardware["hasSD"] = false;
    #endif
    
    doc["status"] = "success";
    
    String response;
    serializeJson(doc, response);
    return response;
}

/**
 * GET /api/anomalies
 * 
 * Returns detected anomalies with filtering options
 */
String APIController::getAnomalies(bool unacknowledgedOnly) {
    JsonDocument doc;
    JsonArray anomaliesArray = doc["anomalies"].to<JsonArray>();
    
    extern ReconState reconState;
    ReconState& state = reconState;
    
    // Handle circular buffer correctly
    uint8_t maxIndex = std::min((uint16_t)state.numAnomalies, (uint16_t)Config::Anomaly::MAX_ANOMALIES);
    for (uint8_t i = 0; i < maxIndex; i++) {
        const AnomalyRecord& anomaly = state.anomalies[i];
        
        // Filter acknowledged if requested
        if (unacknowledgedOnly && anomaly.acknowledged) {
            continue;
        }
        
        JsonObject obj = anomaliesArray.add<JsonObject>();
        obj["index"] = i;
        obj["nodeId"] = anomaly.nodeId;
        
        // Convert anomaly type enum to string
        const char* typeStr = "unknown";
        switch (anomaly.type) {
            case AnomalyType::PACKET_SIZE_OUTLIER: typeStr = "packet_size"; break;
            case AnomalyType::EXCESSIVE_RELAY_HOPS: typeStr = "relay_hops"; break;
            case AnomalyType::RATE_VIOLATION: typeStr = "rate_limit"; break;
            case AnomalyType::RSSI_INCONSISTENCY: typeStr = "rssi_variance"; break;
            case AnomalyType::REPLAY_ATTACK: typeStr = "replay"; break;
            case AnomalyType::TIMING_ANOMALY: typeStr = "timing"; break;
        }
        obj["type"] = typeStr;
        
        obj["timestamp"] = anomaly.timestamp;
        obj["severity"] = anomaly.severity;
        obj["description"] = anomaly.description;
        obj["acknowledged"] = anomaly.acknowledged;
    }
    
    doc["total"] = maxIndex;  // Actual records in response (capped at MAX_ANOMALIES)
    doc["totalDetected"] = state.numAnomalies;  // Total anomalies detected (may exceed buffer)
    doc["unacknowledged"] = state.getUnacknowledgedAnomalies();
    doc["status"] = "success";
    
    String response;
    serializeJson(doc, response);
    return response;
}

/**
 * POST /api/anomaly/:index/acknowledge
 * 
 * Mark anomaly as acknowledged
 */
String APIController::acknowledgeAnomaly(uint8_t index) {
    extern ReconState reconState;
    ReconState& state = reconState;
    
    // Bounds check for circular buffer
    uint8_t maxIndex = std::min((uint16_t)state.numAnomalies, (uint16_t)Config::Anomaly::MAX_ANOMALIES);
    if (index >= maxIndex) {
        return createErrorResponse("Invalid anomaly index");
    }
    
    state.anomalies[index].acknowledged = true;
    return createSuccessResponse("Anomaly acknowledged");
}

/**
 * GET /api/temporal
 * 
 * Returns temporal analysis data: traffic histogram and beacon devices
 */
String APIController::getTemporalData() {
    JsonDocument doc;
    extern ReconState reconState;
    ReconState& state = reconState;
    
    // Traffic histogram (24-hour buckets)
    JsonArray histogram = doc["histogram"].to<JsonArray>();
    for (uint8_t i = 0; i < 24; i++) {
        JsonObject hour = histogram.add<JsonObject>();
        hour["hour"] = i;
        hour["packets"] = state.trafficHist.hourlyPackets[i];
    }
    
    // Beacon devices (periodic transmitters)
    JsonArray beacons = doc["beacons"].to<JsonArray>();
    uint8_t beaconCount = 0;
    for (uint8_t i = 0; i < state.numTargetableDevices; i++) {
        const TargetableDevice& device = state.targetableDevices[i];
        
        // Only include devices with high periodicity score
        if (device.periodicityScore >= Config::Anomaly::MIN_BEACON_CONFIDENCE) {
            JsonObject beacon = beacons.add<JsonObject>();
            beacon["nodeId"] = device.nodeId;
            beacon["avgInterval"] = device.avgPacketInterval;
            beacon["lastInterval"] = device.lastPacketInterval;
            beacon["periodicityScore"] = device.periodicityScore;
            beacon["packetCount"] = device.packetCount;
            beaconCount++;
        }
    }
    
    doc["beaconCount"] = beaconCount;
    doc["status"] = "success";
    
    String response;
    serializeJson(doc, response);
    return response;
}
