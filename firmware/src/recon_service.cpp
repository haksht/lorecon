#include "recon_service.h"
#include "radio_controller.h"
#include "config.h"
#include "logger.h"
#include "web_server.h"

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
    
    // Use non-static buffer and explicit String construction to ensure copy
    char msgBuffer[128];
    snprintf(msgBuffer, sizeof(msgBuffer), "Targeted capture started on device 0x%08X", device.nodeId);
    outMessage = String(msgBuffer);
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
    // Calculate seconds since last seen for frontend display
    deviceObj["lastSeen"] = device.lastSeen > 0 ? (millis() - device.lastSeen) : 0;
    deviceObj["lastSeenSecondsAgo"] = device.lastSeen > 0 ? (millis() - device.lastSeen) / 1000 : 0;
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

const char* ReconService::modeToString(OperationMode mode) {
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
    doc["droppedPackets"] = reconState.scanState.droppedPackets;
    doc["peakQueueSize"] = reconState.scanState.peakQueueSize;
    doc["capturedPackets"] = reconState.getNumCapturedPackets();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["heapSize"] = ESP.getHeapSize();

    if (g_webServer) {
        doc["clientCount"] = g_webServer->getClientCount();
    }

    if (reconState.scanState.mode == MODE_RECONNAISSANCE) {
        JsonObject scan = doc["scan"].to<JsonObject>();
        scan["currentConfig"] = reconState.scanState.currentConfig;
        scan["totalConfigs"] = reconState.getNumConfigs();
        uint32_t elapsed = millis() - reconState.scanState.reconStartTime;
        scan["cyclesCompleted"] = elapsed / (reconState.getNumConfigs() * Config::Scanning::DWELL_TIME_MS);
    } else if (reconState.scanState.mode == MODE_TARGETED_CAPTURE) {
        // Add target information when in targeted mode
        JsonObject target = doc["target"].to<JsonObject>();
        target["targetedByDevice"] = reconState.scanState.targetedByDevice;
        
        // Add config information
        const ScanConfig& cfg = reconState.getScanConfig(reconState.scanState.targetConfig);
        target["configIndex"] = reconState.scanState.targetConfig;
        target["frequency"] = cfg.frequency;
        target["protocol"] = cfg.protocol;
        target["bandwidth"] = cfg.bandwidth;
        target["spreadingFactor"] = cfg.spreadingFactor;
        
        // Try to find the targetable device to get node ID
        for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
            const TargetableDevice& device = reconState.targetableDevices[i];
            if (device.configIndex == reconState.scanState.targetConfig) {
                target["nodeId"] = String(device.nodeId, HEX);
                target["deviceType"] = device.deviceType;
                target["rssi"] = device.bestRSSI;
                target["packetCount"] = device.packetCount;
                break;
            }
        }
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
        const ScanConfig& cfg = reconState.getScanConfig(i);
        
        JsonObject obj = activities.add<JsonObject>();
        obj["configIndex"] = i;
        obj["frequencyMHz"] = cfg.frequency;
        obj["spreadingFactor"] = cfg.spreadingFactor;
        obj["bandwidthKHz"] = cfg.bandwidth;
        obj["syncWord"] = cfg.syncWord;
        obj["protocol"] = cfg.protocol;
        
        if (activity.activityCount > 0) {
            obj["packets"] = activity.activityCount;
            obj["peakRSSI"] = activity.peakRSSI;
            obj["avgRSSI"] = activity.avgRSSI;
            obj["lastActivity"] = activity.lastActivity;
            obj["activityLevel"] = activity.activityLevel ? activity.activityLevel : "UNKNOWN";
        } else {
            obj["packets"] = 0;
            obj["avgRSSI"] = 0;
            obj["activityLevel"] = "NONE";
        }
    }

    doc["totalConfigs"] = activities.size();

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

String ReconService::buildReconSummaryJson() {
    JsonDocument doc;
    doc["status"] = "success";

    JsonObject summary = doc["summary"].to<JsonObject>();
    summary["mode"] = modeToString(reconState.scanState.mode);
    summary["uptimeSeconds"] = millis() / 1000;
    summary["reconDurationSeconds"] = reconState.getReconDuration();
    summary["totalPackets"] = reconState.scanState.totalPackets;
    summary["totalDetections"] = reconState.scanState.totalDetections;
    summary["targetableDevices"] = reconState.numTargetableDevices;
    summary["nodesTracked"] = reconState.nodeCount;
    summary["capturedPackets"] = reconState.getNumCapturedPackets();
    summary["verboseDiagnostics"] = TextPacketDiagnostic::isVerbose();

    if (reconState.scanState.mode == MODE_TARGETED_CAPTURE ||
        reconState.scanState.mode == MODE_PACKET_REPLAY) {
        JsonObject targeted = doc["targetedCapture"].to<JsonObject>();
        targeted["configIndex"] = reconState.scanState.targetConfig;
        const ScanConfig& cfg = reconState.getScanConfig(reconState.scanState.targetConfig);
        targeted["protocol"] = cfg.protocol;
        targeted["frequencyMHz"] = cfg.frequency;
        targeted["spreadingFactor"] = cfg.spreadingFactor;
        targeted["bandwidthKHz"] = cfg.bandwidth;
        targeted["replaySlotsUsed"] = reconState.getNumCapturedPackets();
    }

    JsonArray activity = doc["rfActivity"].to<JsonArray>();
    for (uint8_t i = 0; i < reconState.getNumConfigs(); i++) {
        const RFActivity& rf = reconState.getRFActivity(i);
        if (rf.activityCount == 0) {
            continue;
        }

        JsonObject obj = activity.add<JsonObject>();
        const ScanConfig& cfg = reconState.getScanConfig(i);
        obj["configIndex"] = i;
        obj["protocol"] = cfg.protocol;
        obj["frequencyMHz"] = cfg.frequency;
        obj["packets"] = rf.activityCount;
        obj["avgRSSI"] = rf.avgRSSI;
        obj["peakRSSI"] = rf.peakRSSI;
        obj["activityLevel"] = rf.activityLevel ? rf.activityLevel : "UNKNOWN";
        obj["lastActivitySecondsAgo"] = rf.lastActivity > 0 ? (millis() - rf.lastActivity) / 1000 : 0;
    }

    JsonArray devices = doc["devices"].to<JsonArray>();
    for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
        JsonObject deviceObj = devices.add<JsonObject>();
        fillDevice(deviceObj, reconState.targetableDevices[i], i);
        deviceObj["lastSeenSecondsAgo"] = reconState.targetableDevices[i].lastSeen > 0 ?
            (millis() - reconState.targetableDevices[i].lastSeen) / 1000 : 0;
        deviceObj["firstSeenSecondsAgo"] = reconState.targetableDevices[i].firstSeen > 0 ?
            (millis() - reconState.targetableDevices[i].firstSeen) / 1000 : 0;
    }

    doc["rfActivityCount"] = activity.size();

    String response;
    serializeJson(doc, response);
    return response;
}

String ReconService::buildDeviceTypeSummaryJson() {
    JsonDocument doc;
    doc["status"] = "success";

    struct DeviceTypeStats {
        char type[24];
        uint8_t count;
        float totalRSSI;
        uint8_t routers;
        uint8_t powerClassSum;
    };

    DeviceTypeStats stats[Config::Tracking::MAX_DEVICES];
    memset(stats, 0, sizeof(stats));
    uint8_t typeCount = 0;
    uint16_t totalRouters = 0;

    for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
        const TargetableDevice& dev = reconState.getTargetableDevice(i);

        DeviceTypeStats* statPtr = nullptr;
        for (uint8_t j = 0; j < typeCount; j++) {
            if (strcmp(stats[j].type, dev.deviceType) == 0) {
                statPtr = &stats[j];
                break;
            }
        }

        if (!statPtr && typeCount < Config::Tracking::MAX_DEVICES) {
            statPtr = &stats[typeCount++];
            strncpy(statPtr->type, dev.deviceType, sizeof(statPtr->type) - 1);
            statPtr->type[sizeof(statPtr->type) - 1] = '\0';
            statPtr->count = 0;
            statPtr->totalRSSI = 0.0f;
            statPtr->routers = 0;
            statPtr->powerClassSum = 0;
        }

        if (!statPtr) {
            continue;
        }

        statPtr->count++;
        statPtr->totalRSSI += dev.avgRSSI;
        statPtr->routers += dev.isRouter ? 1 : 0;
        statPtr->powerClassSum += dev.powerClass;
    }

    JsonArray types = doc["deviceTypes"].to<JsonArray>();
    for (uint8_t i = 0; i < typeCount; i++) {
        const DeviceTypeStats& stat = stats[i];
        JsonObject obj = types.add<JsonObject>();
        obj["type"] = stat.type;
        obj["count"] = stat.count;
        obj["avgRSSI"] = stat.count > 0 ? stat.totalRSSI / stat.count : 0;
        obj["routers"] = stat.routers;
        totalRouters += stat.routers;
        float avgPowerClass = stat.count > 0 ? static_cast<float>(stat.powerClassSum) / stat.count : 0.0f;
        obj["avgPowerClass"] = avgPowerClass;
        const char* descriptor = "Low";
        if (avgPowerClass > 1.5f) {
            descriptor = "High";
        } else if (avgPowerClass > 0.5f) {
            descriptor = "Medium";
        }
        obj["powerDescriptor"] = descriptor;
    }

    JsonObject summary = doc["summary"].to<JsonObject>();
    summary["totalDeviceTypes"] = typeCount;
    summary["totalDevices"] = reconState.numTargetableDevices;
    summary["routersDetected"] = totalRouters;
    summary["hasDevices"] = reconState.numTargetableDevices > 0;

    String response;
    serializeJson(doc, response);
    return response;
}

String ReconService::buildSecurityAssessmentJson() {
    JsonDocument doc;
    doc["status"] = "success";

    JsonArray devices = doc["devices"].to<JsonArray>();
    uint8_t vulnerableCount = 0;
    uint8_t moderateCount = 0;

    for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
        const TargetableDevice& dev = reconState.getTargetableDevice(i);
        uint8_t score = 100;
        JsonObject deviceObj = devices.add<JsonObject>();
        deviceObj["nodeId"] = String(dev.nodeId, HEX);
        deviceObj["nodeIdDecimal"] = dev.nodeId;
        deviceObj["deviceType"] = dev.deviceType;
        deviceObj["protocol"] = dev.protocol;
        deviceObj["firmwareVersion"] = dev.firmwareVersion;
        deviceObj["bestRSSI"] = dev.bestRSSI;
        deviceObj["avgRSSI"] = dev.avgRSSI;
        deviceObj["packetCount"] = dev.packetCount;
        deviceObj["isRouter"] = dev.isRouter;
        deviceObj["lastSeenSecondsAgo"] = dev.lastSeen > 0 ? (millis() - dev.lastSeen) / 1000 : 0;

        JsonArray findings = deviceObj["findings"].to<JsonArray>();

        if (dev.bestRSSI > -50) {
            score = score > 15 ? score - 15 : 0;
            findings.add("High signal strength (physical proximity risk)");
        }

        bool maybeUnencrypted = (dev.packetCount > 10 && strcmp(dev.protocol, "Meshtastic") == 0);
        if (maybeUnencrypted) {
            findings.add("Heavy traffic without confirmed encryption");
        }

        if (dev.isRouter) {
            score = score > 10 ? score - 10 : 0;
            findings.add("Router device - elevated attack surface");
        }

        if (dev.packetCount > 100) {
            score = score > 15 ? score - 15 : 0;
            findings.add("Chatty device leaking metadata");
        }

        if (dev.packetCount < 5) {
            score = score > 5 ? score - 5 : 0;
            findings.add("Intermittent transmissions (weak power or sporadic)");
        }

        if (strstr(dev.firmwareVersion, "v1.x") != nullptr || strstr(dev.firmwareVersion, "v2.0") != nullptr) {
            score = score > 20 ? score - 20 : 0;
            findings.add("Outdated firmware signature detected");
        }

        const char* rating;
        if (score >= 80) {
            rating = "secure";
        } else if (score >= 60) {
            rating = "moderate";
            moderateCount++;
        } else {
            rating = "vulnerable";
            vulnerableCount++;
        }

        deviceObj["score"] = score;
        deviceObj["riskLevel"] = rating;

        if (findings.size() == 0) {
            findings.add("No obvious vulnerabilities detected");
        }
    }

    JsonObject summary = doc["summary"].to<JsonObject>();
    summary["totalDevices"] = reconState.numTargetableDevices;
    summary["vulnerable"] = vulnerableCount;
    summary["moderate"] = moderateCount;
    uint8_t secureCount = reconState.numTargetableDevices > vulnerableCount + moderateCount ?
        reconState.numTargetableDevices - vulnerableCount - moderateCount : 0;
    summary["secure"] = secureCount;

    const char* statusMessage = "All devices appear secure";
    if (vulnerableCount > 0) {
        statusMessage = "High-priority targets identified";
    } else if (moderateCount > 0) {
        statusMessage = "Some devices warrant investigation";
    }
    summary["status"] = statusMessage;

    JsonArray recommendations = doc["recommendations"].to<JsonArray>();
    if (vulnerableCount > 0) {
        recommendations.add("Consider enabling encryption and rotating PSKs");
        recommendations.add("Update vulnerable nodes to the latest firmware");
    } else if (moderateCount > 0) {
        recommendations.add("Monitor moderate-risk nodes for changes in behavior");
    } else if (reconState.numTargetableDevices == 0) {
        recommendations.add("No targetable devices detected during this session");
    } else {
        recommendations.add("Maintain watch for newly joining devices");
    }

    String response;
    serializeJson(doc, response);
    return response;
}

String ReconService::buildReplaySlotsJson() {
    JsonDocument doc;
    doc["status"] = "success";
    doc["capacity"] = Config::Replay::MAX_SLOTS;
    doc["count"] = reconState.getNumCapturedPackets();
    doc["available"] = Config::Replay::MAX_SLOTS - reconState.getNumCapturedPackets();

    JsonArray slots = doc["slots"].to<JsonArray>();
    for (uint8_t i = 0; i < reconState.getNumCapturedPackets(); i++) {
        const CapturedPacket& packet = reconState.getReplayPacket(i);
        if (!packet.valid) {
            continue;
        }

        JsonObject slot = slots.add<JsonObject>();
        slot["index"] = i + 1;
        slot["length"] = packet.length;
        slot["protocol"] = packet.protocol;
        slot["configIndex"] = packet.configIndex;
        slot["frequencyMHz"] = reconState.getScanConfig(packet.configIndex).frequency;
        slot["rssi"] = packet.originalRSSI;
        slot["capturedSecondsAgo"] = packet.captureTime > 0 ? (millis() - packet.captureTime) / 1000 : 0;
        
        // Include node ID if available
        if (packet.nodeId != 0) {
            char nodeIdHex[9];
            snprintf(nodeIdHex, sizeof(nodeIdHex), "%08X", packet.nodeId);
            slot["nodeId"] = nodeIdHex;
        }
        
        // Include decrypted text if available
        if (packet.decryptedText[0] != '\0') {
            slot["decryptedText"] = packet.decryptedText;
        }
    }

    String response;
    serializeJson(doc, response);
    return response;
}

bool ReconService::clearReplaySlots(String& outMessage) {
    if (reconState.getNumCapturedPackets() == 0) {
        outMessage = "No replay slots to clear";
        return false;
    }

    reconState.clearReplaySlots();
    outMessage = "Replay slots cleared";
    return true;
}

bool ReconService::replayPacket(uint8_t slotIndex, uint8_t repeatCount, uint16_t delayMs, String& outMessage) {
    if (!isInitialized()) {
        outMessage = "Recon tool not initialized";
        return false;
    }

    // Validate slot index
    if (slotIndex >= reconState.getNumCapturedPackets()) {
        outMessage = "Invalid packet slot";
        return false;
    }

    const CapturedPacket& pkt = reconState.getReplayPacket(slotIndex);
    if (!pkt.valid) {
        outMessage = "Packet slot is empty";
        return false;
    }

    // Validate parameters
    if (repeatCount == 0 || repeatCount > 100) {
        outMessage = "Repeat count must be 1-100";
        return false;
    }

    if (delayMs < 100 || delayMs > 10000) {
        outMessage = "Delay must be 100-10000 ms";
        return false;
    }

    // Get radio controller
    RadioController* radioController = reconTool->getRadioController();
    if (!radioController) {
        outMessage = "Radio controller not available";
        return false;
    }

    // Apply the correct radio configuration for replay
    const ScanConfig& replayCfg = reconState.getScanConfig(pkt.configIndex);
    if (!radioController->applyConfig(replayCfg)) {
        outMessage = "Failed to apply radio configuration";
        return false;
    }

    // Create mutable copy for transmission
    uint8_t txBuffer[256];
    memcpy(txBuffer, pkt.data, pkt.length);

    // Set transmission power
    radioController->getRadio().setOutputPower(10);

    // Transmit the packet(s)
    int successCount = 0;
    int failCount = 0;

    for (int i = 0; i < repeatCount; i++) {
        int state = radioController->getRadio().transmit(txBuffer, pkt.length);
        
        if (state == RADIOLIB_ERR_NONE) {
            successCount++;
        } else {
            failCount++;
        }

        // Delay between transmissions (except after last one)
        if (i < repeatCount - 1) {
            delay(delayMs);
        }
    }

    // Build result message
    char msgBuffer[128];
    snprintf(msgBuffer, sizeof(msgBuffer), "Replay complete: %d/%d successful", 
             successCount, repeatCount);
    outMessage = String(msgBuffer);
    
    return successCount > 0;
}

bool ReconService::startFrequencyTargeting(uint8_t configIndex, String& outMessage) {
    if (!isInitialized()) {
        outMessage = "Recon tool not initialized";
        return false;
    }

    if (!reconState.isValidConfigIndex(configIndex)) {
        outMessage = "Invalid frequency configuration";
        return false;
    }

    reconTool->startFrequencyTargeting(configIndex);
    const ScanConfig& cfg = reconState.getScanConfig(configIndex);
    
    // Use non-static buffer and explicit String construction to ensure copy
    char msgBuffer[128];
    snprintf(msgBuffer, sizeof(msgBuffer), "Frequency targeting started on %s (%.3f MHz)",
             cfg.protocol, cfg.frequency);
    outMessage = String(msgBuffer);
    return true;
}

String ReconService::buildDiagnosticsJson() {
    JsonDocument doc;
    doc["status"] = "success";

    TextPacketDiagnostic::DiagnosticStats stats;
    TextPacketDiagnostic::getStats(stats);

    JsonObject diag = doc["diagnostics"].to<JsonObject>();
    diag["verboseMode"] = stats.verboseMode;
    diag["gapCount"] = stats.gapCount;
    diag["maxGapMs"] = stats.maxGapMs;
    diag["largeGapCount"] = stats.largeGapCount;

    JsonObject encryption = diag["encryption"].to<JsonObject>();
    encryption["encryptedPackets"] = stats.encryptedPacketCount;
    encryption["plaintextPackets"] = stats.plaintextPacketCount;
    encryption["unknownPackets"] = stats.unknownPacketCount;

    auto fillCategory = [](JsonObject obj, const TextPacketDiagnostic::PacketCategoryStats& cat) {
        obj["count"] = cat.count;
        obj["minSize"] = cat.minSize;
        obj["maxSize"] = cat.maxSize;
        obj["averageSize"] = cat.averageSize;
    };

    JsonObject categories = diag["packetTypes"].to<JsonObject>();
    fillCategory(categories["routing"].to<JsonObject>(), stats.routing);
    fillCategory(categories["position"].to<JsonObject>(), stats.position);
    fillCategory(categories["text"].to<JsonObject>(), stats.text);
    fillCategory(categories["other"].to<JsonObject>(), stats.other);

    JsonArray recommendations = doc["recommendations"].to<JsonArray>();
    if (stats.largeGapCount > 0) {
        recommendations.add("Large timing gaps detected; consider reducing verbose output or buffering packets");
    }
    if (stats.plaintextPacketCount > 0) {
        recommendations.add("Plaintext packets observed; inspect for unencrypted traffic");
    }
    if (stats.text.count == 0) {
        recommendations.add("No text messages captured; verify PSK alignment and frequency settings");
    }
    if (recommendations.size() == 0) {
        recommendations.add("Diagnostics nominal; continue monitoring");
    }

    String response;
    serializeJson(doc, response);
    return response;
}

bool ReconService::setVerboseMode(bool enableVerbose, String& outMessage) {
    bool current = TextPacketDiagnostic::isVerbose();
    if (current == enableVerbose) {
        outMessage = enableVerbose ? "Verbose mode already enabled" : "Quiet mode already enabled";
        return false;
    }

    TextPacketDiagnostic::enableVerbose(enableVerbose);
    outMessage = enableVerbose ? "Verbose diagnostics enabled" : "Quiet mode enabled";
    return true;
}
