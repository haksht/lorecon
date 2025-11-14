#include "recon_service.h"
#include "config.h"
#include "logger.h"

IReconTool* ReconService::reconTool = nullptr;

void ReconService::initialize(IReconTool* tool) {
    reconTool = tool;
}

bool ReconService::isInitialized() {
    return reconTool != nullptr;
}

TargetableDevice* ReconService::findTargetableDevice(uint32_t nodeId) {
    for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
        TargetableDevice& device = reconState.targetableDevices[i];
        if (device.nodeId == nodeId) {
            return &device;
        }
    }
    return nullptr;
}

uint8_t ReconService::findDeviceIndex(uint32_t nodeId) {
    for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
        if (reconState.targetableDevices[i].nodeId == nodeId) {
            return i;
        }
    }
    return UINT8_MAX;
}

bool ReconService::startTargetedCaptureByNodeId(uint32_t nodeId, String& outMessage) {
    if (!isInitialized()) {
        outMessage = "Recon tool not initialized";
        return false;
    }

    uint8_t index = findDeviceIndex(nodeId);
    if (index == UINT8_MAX) {
        outMessage = "Device not found";
        return false;
    }

    return startTargetedCaptureByIndex(index, outMessage);
}

bool ReconService::startTargetedCaptureByIndex(uint8_t deviceIndex, String& outMessage) {
    if (!isInitialized()) {
        outMessage = "Recon tool not initialized";
        return false;
    }

    if (deviceIndex >= reconState.numTargetableDevices) {
        outMessage = "Invalid device index";
        return false;
    }

    reconTool->startTargetedCapture(deviceIndex);
    const TargetableDevice& device = reconState.getTargetableDevice(deviceIndex);
    outMessage = "Targeted capture started on device 0x" + String(device.nodeId, HEX);
    return true;
}

bool ReconService::stopCapture(String& outMessage) {
    reconState.scanState.mode = MODE_RECONNAISSANCE;
    outMessage = "Capture stopped, resumed reconnaissance";
    return true;
}

void ReconService::fillDevice(JsonObject& deviceObj, const TargetableDevice& device, uint8_t index) {
    deviceObj["index"] = index + 1;
    deviceObj["nodeId"] = String(device.nodeId, HEX);
    deviceObj["nodeIdDecimal"] = device.nodeId;
    const ScanConfig& cfg = reconState.getScanConfig(device.configIndex);
    deviceObj["frequency"] = cfg.frequency;
    deviceObj["protocol"] = device.protocol;
    deviceObj["deviceType"] = device.deviceType;
    deviceObj["firmwareVersion"] = device.firmwareVersion;
    deviceObj["rssi"] = device.bestRSSI;
    deviceObj["avgRSSI"] = device.avgRSSI;
    deviceObj["packetCount"] = device.packetCount;
    deviceObj["lastSeen"] = device.lastSeen;
    deviceObj["powerClass"] = device.powerClass;
    deviceObj["isRouter"] = device.isRouter;
}

String ReconService::buildDevicesJson() {
    JsonDocument doc;
    doc["status"] = "success";

    JsonArray devices = doc["devices"].to<JsonArray>();
    for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
        JsonObject deviceObj = devices.add<JsonObject>();
        fillDevice(deviceObj, reconState.targetableDevices[i], i);
    }

    doc["totalDevices"] = reconState.numTargetableDevices;

    String response;
    serializeJson(doc, response);
    return response;
}

String ReconService::buildDeviceJson(uint32_t nodeId) {
    JsonDocument doc;
    TargetableDevice* device = findTargetableDevice(nodeId);

    if (!device) {
        doc["status"] = "error";
        doc["error"] = "Device not found";
    } else {
        doc["status"] = "success";
        JsonObject deviceObj = doc["device"].to<JsonObject>();
        fillDevice(deviceObj, *device, 0);
        deviceObj.remove("index");
    }

    String response;
    serializeJson(doc, response);
    return response;
}

static const char* modeToString(OperationMode mode) {
    switch (mode) {
        case MODE_RECONNAISSANCE: return "reconnaissance";
        case MODE_TARGETED_CAPTURE: return "targeted";
        case MODE_INTERACTIVE_MENU: return "menu";
        case MODE_PACKET_REPLAY: return "replay";
        default: return "unknown";
    }
}

String ReconService::buildStatusJson() {
    JsonDocument doc;
    doc["status"] = "success";
    doc["mode"] = modeToString(reconState.scanState.mode);
    doc["uptime"] = millis() / 1000;
    doc["devices"] = reconState.numTargetableDevices;
    doc["totalPackets"] = reconState.scanState.totalPackets;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["heapSize"] = ESP.getHeapSize();

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

String ReconService::buildStatisticsJson() {
    JsonDocument doc;
    doc["status"] = "success";

    JsonObject stats = doc["statistics"].to<JsonObject>();
    stats["totalPackets"] = reconState.scanState.totalPackets;
    stats["totalDevices"] = reconState.numTargetableDevices;

    JsonObject protocols = stats["protocolDistribution"].to<JsonObject>();
    JsonObject freqDist = stats["frequencyDistribution"].to<JsonObject>();

    int meshtastic = 0, lorawan = 0, helium = 0, generic = 0;
    for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
        const TargetableDevice& device = reconState.targetableDevices[i];
        if (strcmp(device.protocol, "Meshtastic") == 0) meshtastic++;
        else if (strcmp(device.protocol, "LoRaWAN") == 0) lorawan++;
        else if (strcmp(device.protocol, "Helium") == 0) helium++;
        else generic++;

        const ScanConfig& cfg = reconState.getScanConfig(device.configIndex);
        String freqKey = String(cfg.frequency, 3);
        int current = freqDist[freqKey] | 0;
        freqDist[freqKey] = current + device.packetCount;
    }

    protocols["Meshtastic"] = meshtastic;
    protocols["LoRaWAN"] = lorawan;
    protocols["Helium"] = helium;
    protocols["Other"] = generic;

    JsonObject capture = stats["captureRate"].to<JsonObject>();
    capture["total"] = reconState.scanState.totalPackets;

    String response;
    serializeJson(doc, response);
    return response;
}

String ReconService::buildActivityJson() {
    JsonDocument doc;
    doc["status"] = "success";

    JsonArray activities = doc["activities"].to<JsonArray>();
    for (uint8_t i = 0; i < reconState.getNumConfigs(); i++) {
        const RFActivity& activity = reconState.getRFActivity(i);
        if (activity.activityCount == 0) {
            continue;
        }

        JsonObject obj = activities.add<JsonObject>();
        const ScanConfig& cfg = reconState.getScanConfig(i);
        obj["frequency"] = cfg.frequency;
        obj["protocol"] = cfg.protocol;
        obj["packets"] = activity.activityCount;
        obj["peakRSSI"] = activity.peakRSSI;
        obj["avgRSSI"] = activity.avgRSSI;
        obj["lastActivity"] = activity.lastActivity;
        obj["activityLevel"] = activity.activityLevel ? activity.activityLevel : "UNKNOWN";
    }

    doc["totalActivities"] = activities.size();

    String response;
    serializeJson(doc, response);
    return response;
}

String ReconService::buildPositionsJson() {
    JsonDocument doc;
    doc["status"] = "success";

    JsonArray positions = doc["positions"].to<JsonArray>();
    uint8_t pointCount = geoIntel.getPointCount();
    for (uint8_t i = 0; i < pointCount; i++) {
        const GeoPoint& point = geoIntel.getPoint(i);
        if (!point.valid) {
            continue;
        }
        JsonObject pos = positions.add<JsonObject>();
        pos["nodeId"] = String(point.nodeId, HEX);
        pos["lat"] = point.latitude;
        pos["lon"] = point.longitude;
        pos["alt"] = point.altitude;
        pos["timestamp"] = point.timestamp;
        pos["precision"] = point.precision;
    }

    doc["totalPositions"] = positions.size();

    String response;
    serializeJson(doc, response);
    return response;
}

String ReconService::buildGeoJson() {
    JsonDocument doc;
    doc["type"] = "FeatureCollection";

    JsonArray features = doc["features"].to<JsonArray>();
    uint8_t pointCount = geoIntel.getPointCount();
    for (uint8_t i = 0; i < pointCount; i++) {
        const GeoPoint& point = geoIntel.getPoint(i);
        if (!point.valid) {
            continue;
        }

        JsonObject feature = features.add<JsonObject>();
        feature["type"] = "Feature";

        JsonObject properties = feature["properties"].to<JsonObject>();
        properties["nodeId"] = String(point.nodeId, HEX);
        properties["altitude"] = point.altitude;
        properties["timestamp"] = point.timestamp;
        properties["precision"] = point.precision;

        JsonObject geometry = feature["geometry"].to<JsonObject>();
        geometry["type"] = "Point";
        JsonArray coordinates = geometry["coordinates"].to<JsonArray>();
        coordinates.add(point.longitude);
        coordinates.add(point.latitude);
        coordinates.add(point.altitude);
    }

    String response;
    serializeJson(doc, response);
    return response;
}

String ReconService::buildKml() {
    String kml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    kml += "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n";
    kml += "  <Document>\n";
    kml += "    <name>ESP32 LoRa Sniffer - Device Positions</name>\n";
    kml += "    <description>Discovered device GPS positions</description>\n";

    uint8_t pointCount = geoIntel.getPointCount();
    for (uint8_t i = 0; i < pointCount; i++) {
        const GeoPoint& point = geoIntel.getPoint(i);
        if (!point.valid) {
            continue;
        }

        kml += "    <Placemark>\n";
        kml += "      <name>Device 0x" + String(point.nodeId, HEX) + "</name>\n";
        kml += "      <description>Altitude: " + String(point.altitude) + "m</description>\n";
        kml += "      <Point>\n";
        kml += "        <coordinates>" + String(point.longitude, 6) + "," + String(point.latitude, 6) + "," + String(point.altitude) + "</coordinates>\n";
        kml += "      </Point>\n";
        kml += "    </Placemark>\n";
    }

    kml += "  </Document>\n";
    kml += "</kml>\n";
    return kml;
}

bool ReconService::toggleQuietMode(bool enableVerbose, String& outMessage) {
    bool current = TextPacketDiagnostic::isVerbose();
    if (current == enableVerbose) {
        outMessage = enableVerbose ? "Verbose mode already enabled" : "Quiet mode already enabled";
        return false;
    }

    TextPacketDiagnostic::enableVerbose(enableVerbose);
    outMessage = enableVerbose ? "Verbose mode enabled" : "Quiet mode enabled";
    return true;
}
