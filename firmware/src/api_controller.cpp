/**
 * API Controller Implementation
 */

#include "api_controller.h"
#include "logger.h"
#include "geo_intelligence.h"

// Static member initialization
IReconTool* APIController::reconTool = nullptr;

/**
 * Set reconnaissance tool reference
 */
void APIController::setReconTool(IReconTool* tool) {
    reconTool = tool;
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
 * Create success response JSON with data
 */
String APIController::createSuccessResponse(const String& message, const JsonDocument& data) {
    JsonDocument doc;
    doc["status"] = "success";
    doc["message"] = message;
    doc["data"] = data;
    
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
    if (!reconTool) {
        return createErrorResponse("Recon tool not initialized");
    }
    
    JsonDocument doc;
    JsonArray devices = doc["devices"].to<JsonArray>();
    
    // Get devices from ReconState
    for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
        const TargetableDevice* device = &reconState.targetableDevices[i];
        if (device && device->nodeId != 0) {
            JsonObject deviceObj = devices.add<JsonObject>();
            deviceObj["index"] = i + 1;
            deviceObj["nodeId"] = String(device->nodeId, HEX);
            deviceObj["nodeIdDecimal"] = device->nodeId;
            const ScanConfig& cfg = reconState.getScanConfig(device->configIndex);
            deviceObj["frequency"] = cfg.frequency;
            deviceObj["protocol"] = device->protocol;
            deviceObj["rssi"] = device->bestRSSI;
            deviceObj["snr"] = 0.0;  // SNR not stored in TargetableDevice
            deviceObj["packetCount"] = device->packetCount;
            deviceObj["lastSeen"] = device->lastSeen;
            deviceObj["powerClass"] = device->powerClass;
            deviceObj["isRouter"] = device->isRouter;
        }
    }
    
    doc["totalDevices"] = reconState.numTargetableDevices;
    doc["status"] = "success";
    
    String response;
    serializeJson(doc, response);
    return response;
}

/**
 * GET /api/device/:nodeId
 * 
 * Returns detailed information about specific device
 */
String APIController::getDevice(uint32_t nodeId) {
    JsonDocument doc;
    
    // Find device by nodeId
    const TargetableDevice* device = nullptr;
    for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
        const TargetableDevice* d = &reconState.targetableDevices[i];
        if (d && d->nodeId == nodeId) {
            device = d;
            break;
        }
    }
    
    if (!device) {
        return createErrorResponse("Device not found");
    }
    
    JsonObject deviceObj = doc["device"].to<JsonObject>();
    deviceObj["nodeId"] = String(device->nodeId, HEX);
    deviceObj["nodeIdDecimal"] = device->nodeId;
    const ScanConfig& cfg = reconState.getScanConfig(device->configIndex);
    deviceObj["frequency"] = cfg.frequency;
    deviceObj["protocol"] = device->protocol;
    deviceObj["rssi"] = device->bestRSSI;
    deviceObj["snr"] = 0.0;  // SNR not stored in TargetableDevice
    deviceObj["packetCount"] = device->packetCount;
    deviceObj["lastSeen"] = device->lastSeen;
    deviceObj["powerClass"] = device->powerClass;
    deviceObj["isRouter"] = device->isRouter;
    
    doc["status"] = "success";
    
    String response;
    serializeJson(doc, response);
    return response;
}

/**
 * POST /api/capture/start
 * 
 * Start targeted capture on specific device
 */
String APIController::startTargetedCapture(uint32_t nodeId) {
    if (!reconTool) {
        return createErrorResponse("Recon tool not initialized");
    }
    
    // Find device index
    uint8_t deviceIndex = 255;
    for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
        const TargetableDevice* d = &reconState.targetableDevices[i];
        if (d && d->nodeId == nodeId) {
            deviceIndex = i;
            break;
        }
    }
    
    if (deviceIndex == 255) {
        return createErrorResponse("Device not found");
    }
    
    // Start targeted capture
    reconTool->startTargetedCapture(deviceIndex);
    
    return createSuccessResponse("Targeted capture started on device 0x" + String(nodeId, HEX));
}

/**
 * POST /api/capture/stop
 * 
 * Stop current capture and resume reconnaissance
 */
String APIController::stopCapture() {
    if (!reconTool) {
        return createErrorResponse("Recon tool not initialized");
    }
    
    // Switch back to reconnaissance mode
    reconState.scanState.mode = MODE_RECONNAISSANCE;
    
    return createSuccessResponse("Capture stopped, resumed reconnaissance");
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
    
    // Note: Current ReconState doesn't store packet history
    // This is a placeholder for future packet logging
    doc["packets"] = JsonArray();  // Empty array for now
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
    JsonDocument doc;
    JsonArray positions = doc["positions"].to<JsonArray>();
    
    // Get positions from GeoIntelligence
    uint8_t pointCount = geoIntel.getPointCount();
    
    for (uint8_t i = 0; i < pointCount; i++) {
        const GeoPoint& point = geoIntel.getPoint(i);
        JsonObject posObj = positions.add<JsonObject>();
        posObj["nodeId"] = String(point.nodeId, HEX);
        posObj["lat"] = point.latitude;
        posObj["lon"] = point.longitude;
        posObj["alt"] = point.altitude;
        posObj["timestamp"] = point.timestamp;
    }
    
    doc["totalPositions"] = pointCount;
    doc["status"] = "success";
    
    String response;
    serializeJson(doc, response);
    return response;
}

/**
 * GET /api/export/geojson
 * 
 * Export geographic data as GeoJSON
 */
String APIController::exportGeoJSON() {
    JsonDocument doc;
    doc["type"] = "FeatureCollection";
    
    JsonArray features = doc["features"].to<JsonArray>();
    uint8_t pointCount = geoIntel.getPointCount();
    
    for (uint8_t i = 0; i < pointCount; i++) {
        const GeoPoint& point = geoIntel.getPoint(i);
        JsonObject feature = features.add<JsonObject>();
        feature["type"] = "Feature";
        
        JsonObject properties = feature["properties"].to<JsonObject>();
        properties["nodeId"] = String(point.nodeId, HEX);
        properties["altitude"] = point.altitude;
        properties["timestamp"] = point.timestamp;
        
        JsonObject geometry = feature["geometry"].to<JsonObject>();
        geometry["type"] = "Point";
        JsonArray coordinates = geometry["coordinates"].to<JsonArray>();
        coordinates.add(point.longitude);
        coordinates.add(point.latitude);
    }
    
    String response;
    serializeJson(doc, response);
    return response;
}

/**
 * GET /api/export/kml
 * 
 * Export geographic data as KML (Google Earth format)
 */
String APIController::exportKML() {
    String kml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    kml += "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n";
    kml += "  <Document>\n";
    kml += "    <name>ESP32 LoRa Sniffer - Device Positions</name>\n";
    kml += "    <description>Discovered device GPS positions</description>\n";
    
    uint8_t pointCount = geoIntel.getPointCount();
    
    for (uint8_t i = 0; i < pointCount; i++) {
        const GeoPoint& point = geoIntel.getPoint(i);
        kml += "    <Placemark>\n";
        kml += "      <name>Device 0x" + String(point.nodeId, HEX) + "</name>\n";
        kml += "      <description>Altitude: " + String(point.altitude) + "m</description>\n";
        kml += "      <Point>\n";
        kml += "        <coordinates>" + String(point.longitude, 6) + "," + 
               String(point.latitude, 6) + "," + String(point.altitude) + "</coordinates>\n";
        kml += "      </Point>\n";
        kml += "    </Placemark>\n";
    }
    
    kml += "  </Document>\n";
    kml += "</kml>\n";
    
    return kml;
}

/**
 * GET /api/status
 * 
 * Returns current system status
 */
String APIController::getStatus() {
    JsonDocument doc;
    
    doc["status"] = "success";
    doc["mode"] = reconState.scanState.mode == MODE_RECONNAISSANCE ? "reconnaissance" : "targeted";
    doc["uptime"] = millis() / 1000;
    doc["devices"] = reconState.numTargetableDevices;
    doc["totalPackets"] = reconState.scanState.totalPackets;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["heapSize"] = ESP.getHeapSize();
    
    // Add current scan config if in reconnaissance
    if (reconState.scanState.mode == MODE_RECONNAISSANCE) {
        JsonObject scan = doc["scan"].to<JsonObject>();
        scan["currentConfig"] = reconState.scanState.currentConfig;
        scan["totalConfigs"] = reconState.getNumConfigs();
        uint32_t elapsed = millis() - reconState.scanState.reconStartTime;
        scan["cyclesCompleted"] = elapsed / (reconState.getNumConfigs() * Config::Scanning::DWELL_TIME_MS);
    }
    
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
    JsonDocument doc;
    
    doc["status"] = "success";
    doc["totalPackets"] = reconState.scanState.totalPackets;
    doc["totalDevices"] = reconState.numTargetableDevices;
    
    // Protocol distribution
    JsonObject protocols = doc["protocols"].to<JsonObject>();
    int meshtastic = 0, lorawan = 0, helium = 0, generic = 0;
    
    for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
        const TargetableDevice* device = &reconState.targetableDevices[i];
        if (device && device->nodeId != 0) {
            if (strcmp(device->protocol, "Meshtastic") == 0) meshtastic++;
            else if (strcmp(device->protocol, "LoRaWAN") == 0) lorawan++;
            else if (strcmp(device->protocol, "Helium") == 0) helium++;
            else generic++;
        }
    }
    
    protocols["meshtastic"] = meshtastic;
    protocols["lorawan"] = lorawan;
    protocols["helium"] = helium;
    protocols["generic"] = generic;
    
    String response;
    serializeJson(doc, response);
    return response;
}

/**
 * GET /api/activity
 * 
 * Returns RF activity analysis
 */
String APIController::getRFActivity() {
    JsonDocument doc;
    JsonArray activities = doc["activities"].to<JsonArray>();
    
    // Iterate through RF activity tracking
    for (uint8_t i = 0; i < reconState.getNumConfigs(); i++) {
        const RFActivity& activity = reconState.getRFActivity(i);
        if (activity.activityCount > 0) {
            JsonObject actObj = activities.add<JsonObject>();
            const ScanConfig& cfg = reconState.getScanConfig(i);
            actObj["frequency"] = cfg.frequency;
            actObj["packets"] = activity.activityCount;
            actObj["peakRSSI"] = activity.peakRSSI;
            actObj["avgRSSI"] = activity.avgRSSI;
            actObj["lastActivity"] = activity.lastActivity;
        }
    }
    
    doc["totalActivities"] = activities.size();
    doc["status"] = "success";
    
    String response;
    serializeJson(doc, response);
    return response;
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
    reconState.scanState.mode = MODE_RECONNAISSANCE;
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
