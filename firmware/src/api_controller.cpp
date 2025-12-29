/**
 * API Controller Implementation
 * 
 * Thin orchestration layer between HTTP endpoints and service layer.
 * Uses JsonUtils for standardized response formatting.
 * JSON building delegated to JsonBuilders namespace.
 */

#include "api_controller.h"
#include "config.h"
#include "logger.h"
#include "mode_manager.h"
#include "recon_service.h"
#include "recon_state.h"
#include "geo_intelligence.h"
#include "json_builders.h"
#include "radio_controller.h"
#include "lora_recon_tool.h"  // For g_reconTool global
#include "utils/json_utils.h"
#include <SD.h>

/**
 * Set reconnaissance tool reference
 */
void APIController::setReconTool(IReconTool* tool) {
    ReconService::initialize(tool);
    LOG_INFO("API Controller initialized");
}

// ============================================================================
// Simple pass-through methods to JsonBuilders
// These exist to provide a stable API layer if internals change
// ============================================================================

String APIController::getDevices() { return JsonBuilders::buildDevicesJson(reconState); }

String APIController::getDevice(uint32_t nodeId) { 
    // Find device index by nodeId
    for (uint8_t i = 0; i < reconState.getNumTargetableDevices(); i++) {
        if (reconState.getTargetableDevice(i).nodeId == nodeId) {
            return JsonBuilders::buildDeviceJson(reconState, i);
        }
    }
    return JsonUtils::error("Device not found");
}

String APIController::getPositions() { return JsonBuilders::buildPositionsJson(geoIntel); }
String APIController::exportGeoJSON() { return JsonBuilders::buildGeoJson(geoIntel); }
String APIController::exportKML() { return JsonBuilders::buildKml(geoIntel); }
String APIController::getStatus() { return JsonBuilders::buildStatusJson(reconState); }
String APIController::getStatistics() { return JsonBuilders::buildStatisticsJson(reconState); }
String APIController::getRFActivity() { return JsonBuilders::buildActivityJson(reconState); }
String APIController::getReconSummary() { return JsonBuilders::buildReconSummaryJson(reconState, geoIntel); }
String APIController::getDeviceTypeSummary() { return JsonBuilders::buildDeviceTypeSummaryJson(reconState); }
String APIController::getSecurityAssessment() { return JsonBuilders::buildSecurityAssessmentJson(reconState); }
String APIController::getReplaySlots() { return JsonBuilders::buildReplaySlotsJson(reconState); }
String APIController::getDiagnostics() { return JsonBuilders::buildDiagnosticsJson(); }

// ============================================================================
// Action methods with error handling
// ============================================================================

String APIController::startTargetedCapture(uint32_t nodeId) {
    String message;
    if (!ReconService::startTargetedCaptureByNodeId(nodeId, message)) {
        return JsonUtils::error(message);
    }
    return JsonUtils::success(message);
}

String APIController::stopCapture() {
    String message;
    ReconService::stopCapture(message);
    return JsonUtils::success(message);
}

String APIController::clearReplaySlots() {
    String message;
    if (!ReconService::clearReplaySlots(message)) {
        return JsonUtils::error(message);
    }
    return JsonUtils::success(message);
}

String APIController::clearDevices() {
    String message;
    if (!ReconService::clearDevices(message)) {
        return JsonUtils::error(message);
    }
    return JsonUtils::success(message);
}

String APIController::replayPacket(uint8_t slotIndex, uint8_t repeatCount, uint16_t delayMs) {
    String message;
    if (!ReconService::replayPacket(slotIndex, repeatCount, delayMs, message)) {
        return JsonUtils::error(message);
    }
    return JsonUtils::success(message);
}

String APIController::startFrequencyTargeting(uint8_t configIndex) {
    String message;
    if (!ReconService::startFrequencyTargeting(configIndex, message)) {
        return JsonUtils::error(message);
    }
    return JsonUtils::success(message);
}

String APIController::setVerboseMode(bool enableVerbose) {
    String message;
    if (!ReconService::setVerboseMode(enableVerbose, message)) {
        return JsonUtils::error(message);
    }
    return JsonUtils::success(message);
}

// ============================================================================
// Methods with custom JSON building
// ============================================================================

String APIController::getPackets(int limit, int offset) {
    return JsonUtils::successWithData([&](JsonDocument& doc) {
        doc["limit"] = limit;
        doc["offset"] = offset;
        doc["totalPackets"] = reconState.scanState.totalPackets;
        doc["packets"] = JsonArray();
        doc["message"] = "Packet history requires SD card logging";
    });
}

String APIController::getConfig() {
    return JsonUtils::successWithData([](JsonDocument& doc) {
        JsonObject config = doc["config"].to<JsonObject>();
        config["scanDwellTime"] = Config::Scanning::DWELL_TIME_MS;
        config["queueSize"] = Config::PacketProcessing::QUEUE_SIZE;
        config["maxDevices"] = Config::Tracking::MAX_DEVICES;
        config["deviceTimeout"] = Config::Tracking::DEVICE_TIMEOUT_MS;
    });
}

/**
 * POST /api/scan/start
 * 
 * Start or resume reconnaissance scan.
 * Will switch from any mode (including targeted capture) to reconnaissance.
 * This is an explicit user action, so it's allowed.
 */
String APIController::startScan() {
    // Log mode transition for debugging
    const char* prevMode = (reconState.scanState.mode == MODE_TARGETED_CAPTURE) ? "targeted_capture" :
                           (reconState.scanState.mode == MODE_INTERACTIVE_MENU) ? "menu" :
                           (reconState.scanState.mode == MODE_PACKET_REPLAY) ? "replay" : "reconnaissance";
    LOG_INFO("API: startScan called (previous mode: %s)", prevMode);
    
    // Clear persisted targeting mode from NVS
    ModeManager modeManager;
    modeManager.clearPersistedMode("API:startScan");
    
    // Resume reconnaissance WITHOUT clearing discovered devices/data
    LOG_INFO("API: Resuming reconnaissance scan");
    
    // Log mode transition and reset scan state
    modeManager.logModeTransition(reconState.scanState.mode, MODE_RECONNAISSANCE, "API:startScan");
    reconState.scanState.mode = MODE_RECONNAISSANCE;
    reconState.scanState.currentConfig = 0;
    reconState.scanState.lastScanSwitch = millis();
    reconState.scanState.packetPending = false;
    reconState.scanState.waitingForUserInput = false;
    
    // Clear any menu timeout
    if (g_reconTool) {
        g_reconTool->clearMenuTimeout();
    }
    
    // Apply first config and start receiving via ReconService
    IReconTool* tool = ReconService::getReconTool();
    if (tool) {
        const ScanConfig& cfg = reconState.getScanConfig(0);
        tool->getRadioController()->applyConfig(cfg);
        tool->getRadioController()->startReceive();
        LOG_INFO("Radio reconfigured for reconnaissance");
    }
    
    return JsonUtils::success("Reconnaissance scan started");
}

/**
 * POST /api/scan/stop
 * 
 * Stop current scan and enter menu mode.
 * Works from any mode - this is an explicit user action.
 */
String APIController::stopScan() {
    // Log mode transition for debugging
    const char* prevMode = (reconState.scanState.mode == MODE_TARGETED_CAPTURE) ? "targeted_capture" :
                           (reconState.scanState.mode == MODE_INTERACTIVE_MENU) ? "menu" :
                           (reconState.scanState.mode == MODE_PACKET_REPLAY) ? "replay" : "reconnaissance";
    LOG_INFO("API: stopScan called (previous mode: %s)", prevMode);
    
    ModeManager modeManager;
    modeManager.logModeTransition(reconState.scanState.mode, MODE_INTERACTIVE_MENU, "API:stopScan");
    reconState.scanState.mode = MODE_INTERACTIVE_MENU;
    if (g_reconTool) {
        g_reconTool->setMenuModeEntered();
    }
    return JsonUtils::success("Scan stopped");
}

/**
 * GET /api/dashboard
 * 
 * Combined data for initial page load
 */
String APIController::getDashboard() {
    JsonDocument doc;
    doc["status"] = "success";
    
    // Status data
    JsonDocument statusDoc;
    deserializeJson(statusDoc, JsonBuilders::buildStatusJson(reconState));
    if (statusDoc["status"].is<const char*>() && statusDoc["status"] == "success") {
        doc["systemStatus"] = statusDoc;
    }
    
    // Devices list
    JsonDocument devicesDoc;
    deserializeJson(devicesDoc, JsonBuilders::buildDevicesJson(reconState));
    if (devicesDoc["devices"].is<JsonArray>()) {
        doc["devices"] = devicesDoc["devices"];
        doc["deviceCount"] = devicesDoc["devices"].size();
    } else {
        doc["devices"] = JsonArray();
        doc["deviceCount"] = 0;
    }
    
    // RF Activity
    JsonDocument activityDoc;
    deserializeJson(activityDoc, JsonBuilders::buildActivityJson(reconState));
    if (activityDoc["activities"].is<JsonArray>()) {
        doc["activities"] = activityDoc["activities"];
    }
    
    // Stats
    doc["replaySlots"] = reconState.getNumCapturedPackets();
    doc["maxReplaySlots"] = Config::Replay::MAX_SLOTS;
    doc["totalPackets"] = reconState.scanState.totalPackets;
    doc["uptime"] = millis() / 1000;
    doc["mode"] = reconState.scanState.mode;
    doc["currentConfig"] = reconState.scanState.currentConfig;
    
    return JsonUtils::serialize(doc);
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
    tracking["maxGeoPoints"] = Config::Tracking::MAX_GEO_POINTS;
    tracking["deviceTimeoutMs"] = Config::Tracking::DEVICE_TIMEOUT_MS;
    
    // Current usage
    JsonObject usage = doc["usage"].to<JsonObject>();
    usage["devices"] = reconState.getNumTargetableDevices();
    usage["replaySlots"] = reconState.getNumCapturedPackets();
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
    
    // Handle circular buffer correctly
    uint8_t maxIndex = std::min((uint16_t)reconState.numAnomalies, (uint16_t)Config::Anomaly::MAX_ANOMALIES);
    for (uint8_t i = 0; i < maxIndex; i++) {
        const AnomalyRecord& anomaly = reconState.anomalies[i];
        
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
    doc["totalDetected"] = reconState.numAnomalies;  // Total anomalies detected (may exceed buffer)
    doc["unacknowledged"] = reconState.getUnacknowledgedAnomalies();
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
    // Bounds check for circular buffer
    uint8_t maxIndex = std::min((uint16_t)reconState.numAnomalies, (uint16_t)Config::Anomaly::MAX_ANOMALIES);
    if (index >= maxIndex) {
        return JsonUtils::error("Invalid anomaly index");
    }
    
    reconState.anomalies[index].acknowledged = true;
    return JsonUtils::success("Anomaly acknowledged");
}

/**
 * GET /api/temporal
 * 
 * Returns temporal analysis data: traffic histogram and beacon devices
 */
String APIController::getTemporalData() {
    JsonDocument doc;
    
    // Traffic histogram (24-hour buckets)
    JsonArray histogram = doc["histogram"].to<JsonArray>();
    for (uint8_t i = 0; i < 24; i++) {
        JsonObject hour = histogram.add<JsonObject>();
        hour["hour"] = i;
        hour["packets"] = reconState.trafficHist.hourlyPackets[i];
    }
    
    // Beacon devices (periodic transmitters)
    JsonArray beacons = doc["beacons"].to<JsonArray>();
    uint8_t beaconCount = 0;
    for (uint8_t i = 0; i < reconState.getNumTargetableDevices(); i++) {
        const TargetableDevice& device = reconState.getTargetableDevice(i);
        
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
/**
 * GET /api/psk/stats
 * 
 * Returns PSK decryption statistics for attack dashboard
 */
String APIController::getPSKStats() {
    JsonDocument doc;
    
    // Overall stats
    doc["attempts"] = reconState.pskStats.attempts;
    doc["successes"] = reconState.pskStats.successes;
    doc["successRate"] = reconState.pskStats.attempts > 0 
        ? (float)reconState.pskStats.successes / reconState.pskStats.attempts * 100.0f 
        : 0.0f;
    doc["totalKeys"] = Config::PSK::NUM_DEFAULT_KEYS;
    
    // Per-key hit counts
    JsonArray keys = doc["keys"].to<JsonArray>();
    for (uint8_t i = 0; i < Config::PSK::NUM_DEFAULT_KEYS; i++) {
        JsonObject key = keys.add<JsonObject>();
        key["index"] = i;
        key["hits"] = reconState.pskStats.hitCount[i];
    }
    
    // Count keys with at least one hit (networks cracked)
    uint8_t networksCracked = 0;
    for (uint8_t i = 0; i < Config::PSK::NUM_DEFAULT_KEYS; i++) {
        if (reconState.pskStats.hitCount[i] > 0) {
            networksCracked++;
        }
    }
    doc["networksCracked"] = networksCracked;
    
    doc["status"] = "success";
    
    String response;
    serializeJson(doc, response);
    return response;
}