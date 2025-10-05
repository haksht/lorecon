/**
 * UI Display Components Implementation
 * 
 * Extracted display functions for easier web app transition.
 * These can be converted to JSON generators for REST API.
 */

#include "ui_components.h"
#include "geo_intelligence.h"

// Display list of discovered devices
void displayDeviceList() {
    if (reconState.numTargetableDevices == 0) {
        Serial.println("No targetable devices discovered yet.\n");
        return;
    }
    
    Serial.println("DISCOVERED DEVICES:");
    Serial.println("ID | Device Type          | Node ID    | Protocol     | RSSI  | Pkts | Router | FW");
    Serial.println("---|----------------------|------------|--------------|-------|------|--------|-------");
    
    for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
        displayDeviceSummary(reconState.getTargetableDevice(i), i);
    }
    Serial.println();
}

// Display single device summary
void displayDeviceSummary(const TargetableDevice& dev, uint8_t index) {
    Serial.printf("%2d | %-20s | 0x%08X | %-12s | %5.1f | %4d | %-6s | %s\n",
                  index + 1,
                  dev.deviceType,
                  (unsigned int)dev.nodeId,
                  dev.protocol,
                  dev.bestRSSI,
                  (int)dev.packetCount,
                  dev.isRouter ? "YES" : "NO",
                  dev.firmwareVersion);
}

// Display RF activity table
void displayRFActivityTable() {
    Serial.println("RF ACTIVITY BY FREQUENCY:");
    Serial.println("Configuration            | Frequency | SF | BW  | Activity | Count | Avg RSSI");
    Serial.println("-------------------------|-----------|----|----|----------|-------|----------");
    
    bool anyActivity = false;
    for (uint8_t i = 0; i < reconState.getNumConfigs(); i++) {
        const RFActivity& activity = reconState.getRFActivity(i);
        if (activity.activityCount > 0) {
            anyActivity = true;
            const ScanConfig& cfg = reconState.getScanConfig(i);
            
            Serial.printf("%-24s | %7.3f   | %2d | %3.0f | %-8s | %5d | %8.1f\n",
                          cfg.protocol,
                          cfg.frequency,
                          cfg.spreadingFactor,
                          cfg.bandwidth,
                          activity.activityLevel,
                          activity.activityCount,
                          activity.avgRSSI);
        }
    }
    
    if (!anyActivity) {
        Serial.println("(No RF activity detected)");
    }
    Serial.println();
}

// Display activity summary
void displayActivitySummary() {
    uint32_t reconTime = reconState.getReconDuration();
    
    Serial.println("RECONNAISSANCE SUMMARY:");
    Serial.printf("  Duration: %s\n", formatDuration(reconTime).c_str());
    Serial.printf("  Total Packets: %d\n", reconState.scanState.totalPackets);
    Serial.printf("  Devices Found: %d\n", reconState.numTargetableDevices);
    Serial.printf("  Tracked Nodes: %d\n", reconState.nodeCount);
    Serial.println();
}

// Display reconnaissance statistics
void displayReconStatistics() {
    displayActivitySummary();
    displayDeviceList();
    displayRFActivityTable();
}

// Display security vulnerability scores
void displaySecurityScores() {
    Serial.println("🛡️ SECURITY VULNERABILITY ASSESSMENT");
    Serial.println("====================================\n");
    
    if (reconState.numTargetableDevices == 0) {
        Serial.println("No devices to assess.\n");
        return;
    }
    
    Serial.println("Device                  | Score | Risk Level | Vulnerabilities");
    Serial.println("------------------------|-------|------------|------------------");
    
    uint16_t totalScore = 0;
    
    for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
        const TargetableDevice& dev = reconState.getTargetableDevice(i);
        
        // Calculate security score (100 = perfect, 0 = critical)
        uint8_t score = 100;
        String vulnerabilities = "";
        
        // Physical security risk (too strong signal)
        if (dev.bestRSSI > -50) {
            score -= 15;
            vulnerabilities += "Physical ";
        }
        
        // Router = higher attack surface
        if (dev.isRouter) {
            score -= 10;
            vulnerabilities += "Router ";
        }
        
        // High packet count = chatty/info leakage
        if (dev.packetCount > 100) {
            score -= 15;
            vulnerabilities += "Chatty ";
        }
        
        // Old firmware
        if (strstr(dev.firmwareVersion, "v1.x") || strstr(dev.firmwareVersion, "v2.0")) {
            score -= 20;
            vulnerabilities += "OldFW ";
        }
        
        if (vulnerabilities.length() == 0) {
            vulnerabilities = "None";
        }
        
        const char* riskLevel = score >= 80 ? "✅ LOW   " : 
                                score >= 60 ? "⚠️ MEDIUM" : "🚨 HIGH  ";
        
        Serial.printf("0x%08X (%8s) | %3d   | %s | %s\n",
                      dev.nodeId, dev.deviceType, score, riskLevel,
                      vulnerabilities.c_str());
        
        totalScore += score;
    }
    
    uint8_t avgScore = totalScore / reconState.numTargetableDevices;
    Serial.printf("\n📊 Network Security Score: %d/100\n\n", avgScore);
}

// Display geographic intelligence
void displayGeoIntelligence() {
    geoIntel.printSummary();
}

// Display device map (future: integrate with mapping library)
void displayDeviceMap() {
    Serial.println("📍 DEVICE GEOGRAPHIC MAP");
    Serial.println("========================\n");
    
    if (geoIntel.getPointCount() == 0) {
        Serial.println("No GPS data available. Capture position packets first.\n");
        return;
    }
    
    Serial.println("Use 'g' command to export KML/GeoJSON for mapping tools.\n");
    displayGeoIntelligence();
}

// Format RSSI with quality indicator
String formatRSSI(float rssi) {
    String result = String(rssi, 1) + " dBm ";
    result += getSignalQuality(rssi);
    return result;
}

// Format duration in human-readable form
String formatDuration(uint32_t seconds) {
    if (seconds < 60) {
        return String(seconds) + "s";
    } else if (seconds < 3600) {
        return String(seconds / 60) + "m " + String(seconds % 60) + "s";
    } else {
        uint32_t hours = seconds / 3600;
        uint32_t mins = (seconds % 3600) / 60;
        return String(hours) + "h " + String(mins) + "m";
    }
}

// Get signal quality description
String getSignalQuality(float rssi) {
    if (rssi > -60) return "Excellent";
    if (rssi > -80) return "Good";
    if (rssi > -100) return "Fair";
    return "Weak";
}
