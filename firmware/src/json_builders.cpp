/**
 * @file json_builders.cpp
 * @brief JSON response builders for API endpoints
 * 
 * Extracted from ReconService (~650 lines of JSON building code).
 */

#include "json_builders.h"
#include <ArduinoJson.h>
#include "recon_state.h"
#include "geo_intelligence.h"
#include "data_structures.h"
#include "text_packet_diagnostic.h"
#include "utils/format_utils.h"
#include "utils/security_scorer.h"
#include "config.h"
#include "web_server.h"  // For g_webServer->getClientCount()

namespace JsonBuilders {

namespace Internal {

const char* modeToString(uint8_t mode) {
    switch (mode) {
        case MODE_RECONNAISSANCE: return "reconnaissance";
        case MODE_TARGETED_CAPTURE: return "targeted_capture";
        case MODE_INTERACTIVE_MENU: return "interactive_menu";
        case MODE_PACKET_REPLAY: return "packet_replay";
        default: return "unknown";
    }
}

void fillDevice(ArduinoJson::JsonObject& obj, const TargetableDevice& dev, uint8_t index) {
    obj["index"] = index;
    obj["nodeId"] = FormatUtils::formatNodeIdJson(dev.nodeId);
    obj["nodeIdDecimal"] = dev.nodeId;
    obj["protocol"] = dev.protocol;
    obj["deviceType"] = dev.deviceType;
    obj["firmwareVersion"] = dev.firmwareVersion;
    obj["packetCount"] = dev.packetCount;
    obj["avgRSSI"] = dev.avgRSSI;
    obj["bestRSSI"] = dev.bestRSSI;
    obj["firstSeen"] = dev.firstSeen;
    obj["lastSeen"] = dev.lastSeen;
    obj["isRouter"] = dev.isRouter;
    obj["configIndex"] = dev.configIndex;
    obj["powerClass"] = dev.powerClass;
    
    // Derive power descriptor from powerClass
    const char* descriptor = "Low";
    if (dev.powerClass >= 2) {
        descriptor = "High";
    } else if (dev.powerClass == 1) {
        descriptor = "Medium";
    }
    obj["powerDescriptor"] = descriptor;
}

} // namespace Internal

String buildDevicesJson(ReconState& reconState) {
    JsonDocument doc;
    doc["status"] = "success";
    doc["count"] = reconState.getNumTargetableDevices();
    
    JsonArray devices = doc["devices"].to<JsonArray>();
    for (uint8_t i = 0; i < reconState.getNumTargetableDevices(); i++) {
        JsonObject deviceObj = devices.add<JsonObject>();
        const TargetableDevice& dev = reconState.getTargetableDevice(i);
        Internal::fillDevice(deviceObj, dev, i);
    }

    String response;
    serializeJson(doc, response);
    return response;
}

String buildDeviceJson(ReconState& reconState, uint8_t deviceIndex) {
    if (deviceIndex >= reconState.getNumTargetableDevices()) {
        return "";
    }
    
    JsonDocument doc;
    doc["status"] = "success";
    
    JsonObject deviceObj = doc["device"].to<JsonObject>();
    const TargetableDevice& dev = reconState.getTargetableDevice(deviceIndex);
    Internal::fillDevice(deviceObj, dev, deviceIndex);

    String response;
    serializeJson(doc, response);
    return response;
}

String buildStatusJson(ReconState& reconState) {
    JsonDocument doc;
    doc["status"] = "success";
    doc["mode"] = Internal::modeToString(reconState.scanState.mode);
    doc["uptime"] = millis() / 1000;
    doc["devices"] = reconState.getNumTargetableDevices();
    doc["totalPackets"] = reconState.scanState.totalPackets;
    doc["droppedPackets"] = reconState.scanState.droppedPackets;
    doc["peakQueueSize"] = reconState.scanState.peakQueueSize;
    doc["capturedPackets"] = reconState.getNumCapturedPackets();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["heapSize"] = ESP.getHeapSize();

    // Battery voltage reading (Heltec V3: GPIO 37 control, GPIO 1 ADC)
    pinMode(Config::Hardware::VBAT_CTRL_PIN, OUTPUT);
    digitalWrite(Config::Hardware::VBAT_CTRL_PIN, HIGH);
    analogReadResolution(12);
    float batteryVoltage = (analogReadMilliVolts(Config::Hardware::VBAT_ADC_PIN) * Config::Hardware::VBAT_SCALE) / 1000.0f;
    // Map 3.2V (empty) to 4.2V (full) -> 0-100%
    int batteryPercent = constrain((int)((batteryVoltage - 3.2f) / (4.2f - 3.2f) * 100.0f), 0, 100);
    doc["batteryVoltage"] = serialized(String(batteryVoltage, 2));
    doc["batteryPercent"] = batteryPercent;

    if (g_webServer) {
        doc["clientCount"] = g_webServer->getClientCount();
    }

    // Scan info when in reconnaissance mode
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
        for (uint8_t i = 0; i < reconState.getNumTargetableDevices(); i++) {
            const TargetableDevice& device = reconState.getTargetableDevice(i);
            if (device.configIndex == reconState.scanState.targetConfig) {
                target["nodeId"] = FormatUtils::formatNodeIdJson(device.nodeId);
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

String buildStatisticsJson(ReconState& reconState) {
    JsonDocument doc;
    doc["status"] = "success";

    JsonObject stats = doc["statistics"].to<JsonObject>();
    stats["totalPackets"] = reconState.scanState.totalPackets;
    stats["totalDevices"] = reconState.getNumTargetableDevices();

    JsonObject protocols = stats["protocolDistribution"].to<JsonObject>();
    JsonObject freqDist = stats["frequencyDistribution"].to<JsonObject>();

    int meshtastic = 0, lorawan = 0, helium = 0, generic = 0;
    for (uint8_t i = 0; i < reconState.getNumTargetableDevices(); i++) {
        const TargetableDevice& device = reconState.getTargetableDevice(i);
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

String buildActivityJson(ReconState& reconState) {
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

String buildPositionsJson(GeoIntelligence& geoIntel) {
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
        pos["nodeId"] = FormatUtils::formatNodeIdJson(point.nodeId);
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

String buildGeoJson(GeoIntelligence& geoIntel) {
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
        properties["nodeId"] = FormatUtils::formatNodeIdJson(point.nodeId);
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

String buildKml(GeoIntelligence& geoIntel) {
    uint8_t pointCount = geoIntel.getPointCount();
    
    // Pre-allocate string to prevent fragmentation (header ~300 bytes + ~200 bytes per point)
    String kml;
    kml.reserve(400 + pointCount * 250);
    
    kml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    kml += "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n";
    kml += "  <Document>\n";
    kml += "    <name>ESP32 LoRa Sniffer - Device Positions</name>\n";
    kml += "    <description>Discovered device GPS positions</description>\n";

    for (uint8_t i = 0; i < pointCount; i++) {
        const GeoPoint& point = geoIntel.getPoint(i);
        if (!point.valid) {
            continue;
        }

        kml += "    <Placemark>\n";
        kml += "      <name>Device " + FormatUtils::formatNodeIdPadded(point.nodeId) + "</name>\n";
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

String buildReconSummaryJson(ReconState& reconState, GeoIntelligence& geoIntel) {
    JsonDocument doc;
    doc["status"] = "success";

    JsonObject summary = doc["summary"].to<JsonObject>();
    summary["mode"] = Internal::modeToString(reconState.scanState.mode);
    summary["uptimeSeconds"] = millis() / 1000;
    summary["reconDurationSeconds"] = reconState.getReconDuration();
    summary["totalPackets"] = reconState.scanState.totalPackets;
    summary["totalDetections"] = reconState.scanState.totalDetections;
    summary["targetableDevices"] = reconState.getNumTargetableDevices();
    summary["nodesTracked"] = reconState.getNodeCount();
    summary["capturedPackets"] = reconState.getNumCapturedPackets();
    summary["verboseDiagnostics"] = TextPacketDiagnostic::isVerbose();
    
    // Network intelligence summary
    JsonObject intel = summary["networkIntel"].to<JsonObject>();
    intel["beaconDevices"] = reconState.networkIntel.beaconDevices;
    intel["activeTransmitters"] = reconState.networkIntel.activeTransmitters;
    intel["relayNodes"] = reconState.networkIntel.relayOnlyNodes;
    intel["mixedNodes"] = reconState.networkIntel.mixedNodes;
    intel["encryptedNetworks"] = reconState.networkIntel.encryptedNetworks;
    intel["anomalyCount"] = reconState.networkIntel.anomalyCount;

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
        uint32_t now = millis();
        obj["lastActivitySecondsAgo"] = (rf.lastActivity > 0 && rf.lastActivity <= now) ? (now - rf.lastActivity) / 1000 : 0;
    }

    JsonArray devices = doc["devices"].to<JsonArray>();
    uint32_t now = millis();
    for (uint8_t i = 0; i < reconState.getNumTargetableDevices(); i++) {
        JsonObject deviceObj = devices.add<JsonObject>();
        const TargetableDevice& dev = reconState.getTargetableDevice(i);
        Internal::fillDevice(deviceObj, dev, i);
        uint32_t lastSeenAge = (dev.lastSeen > 0 && dev.lastSeen <= now) ?
            (now - dev.lastSeen) : 0;
        uint32_t firstSeenAge = (dev.firstSeen > 0 && dev.firstSeen <= now) ?
            (now - dev.firstSeen) : 0;
        deviceObj["lastSeenSecondsAgo"] = lastSeenAge / 1000;
        deviceObj["firstSeenSecondsAgo"] = firstSeenAge / 1000;
    }

    doc["rfActivityCount"] = activity.size();

    String response;
    serializeJson(doc, response);
    return response;
}

String buildDeviceTypeSummaryJson(ReconState& reconState) {
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

    for (uint8_t i = 0; i < reconState.getNumTargetableDevices(); i++) {
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
    summary["totalDevices"] = reconState.getNumTargetableDevices();
    summary["routersDetected"] = totalRouters;
    summary["hasDevices"] = reconState.getNumTargetableDevices() > 0;

    String response;
    serializeJson(doc, response);
    return response;
}

String buildSecurityAssessmentJson(ReconState& reconState) {
    JsonDocument doc;
    doc["status"] = "success";

    JsonArray devices = doc["devices"].to<JsonArray>();
    uint8_t vulnerableCount = 0;
    uint8_t moderateCount = 0;

    for (uint8_t i = 0; i < reconState.getNumTargetableDevices(); i++) {
        const TargetableDevice& dev = reconState.getTargetableDevice(i);
        
        // Use shared security scorer for consistent assessment
        SecurityScorer::Assessment assessment = SecurityScorer::assess(dev);
        
        JsonObject deviceObj = devices.add<JsonObject>();
        deviceObj["nodeId"] = FormatUtils::formatNodeIdJson(dev.nodeId);
        deviceObj["nodeIdDecimal"] = dev.nodeId;
        deviceObj["deviceType"] = dev.deviceType;
        deviceObj["protocol"] = dev.protocol;
        deviceObj["firmwareVersion"] = dev.firmwareVersion;
        deviceObj["bestRSSI"] = dev.bestRSSI;
        deviceObj["avgRSSI"] = dev.avgRSSI;
        deviceObj["packetCount"] = dev.packetCount;
        deviceObj["isRouter"] = dev.isRouter;
        uint32_t now = millis();
        deviceObj["lastSeenSecondsAgo"] = (dev.lastSeen > 0 && dev.lastSeen <= now) ? (now - dev.lastSeen) / 1000 : 0;

        JsonArray findings = deviceObj["findings"].to<JsonArray>();

        // Add findings based on assessment
        if (assessment.physicalProximity) {
            findings.add("High signal strength (physical proximity risk)");
        }
        if (assessment.possibleUnencrypted) {
            findings.add("Heavy traffic without confirmed encryption");
        }
        if (assessment.isRouter) {
            findings.add("Router device - elevated attack surface");
        }
        if (assessment.chatty) {
            findings.add("Chatty device leaking metadata");
        }
        if (assessment.intermittent) {
            findings.add("Intermittent transmissions (weak power or sporadic)");
        }
        if (assessment.outdatedFirmware) {
            findings.add("Outdated firmware signature detected");
        }

        // Track counts
        if (strcmp(assessment.rating, "vulnerable") == 0) {
            vulnerableCount++;
        } else if (strcmp(assessment.rating, "moderate") == 0) {
            moderateCount++;
        }

        deviceObj["score"] = assessment.score;
        deviceObj["riskLevel"] = assessment.rating;

        if (findings.size() == 0) {
            findings.add("No obvious vulnerabilities detected");
        }
    }

    JsonObject summary = doc["summary"].to<JsonObject>();
    summary["totalDevices"] = reconState.getNumTargetableDevices();
    summary["vulnerable"] = vulnerableCount;
    summary["moderate"] = moderateCount;
    uint8_t secureCount = reconState.getNumTargetableDevices() > vulnerableCount + moderateCount ?
        reconState.getNumTargetableDevices() - vulnerableCount - moderateCount : 0;
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
    } else if (reconState.getNumTargetableDevices() == 0) {
        recommendations.add("No targetable devices detected during this session");
    } else {
        recommendations.add("Maintain watch for newly joining devices");
    }

    String response;
    serializeJson(doc, response);
    return response;
}

String buildReplaySlotsJson(ReconState& reconState) {
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
        uint32_t now = millis();
        slot["capturedSecondsAgo"] = (packet.captureTime > 0 && packet.captureTime <= now) ? (now - packet.captureTime) / 1000 : 0;
        
        // Include node ID if available
        if (packet.nodeId != 0) {
            slot["nodeId"] = FormatUtils::formatNodeIdJson(packet.nodeId);
        }
        
        // Include packet ID if available
        if (packet.packetId != 0) {
            char packetIdHex[9];
            snprintf(packetIdHex, sizeof(packetIdHex), "%08X", packet.packetId);
            slot["packetId"] = packetIdHex;
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

String buildDiagnosticsJson() {
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

} // namespace JsonBuilders
