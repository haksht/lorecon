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
    if (reconState.getNumTargetableDevices() == 0) {
        Serial.println("No targetable devices discovered yet.\n");
        return;
    }
    
    Serial.println("DISCOVERED DEVICES:");
    Serial.println("ID | Device Type          | Node ID    | Protocol     | RSSI  | Pkts | Router | FW");
    Serial.println("---|----------------------|------------|--------------|-------|------|--------|-------");
    
    for (uint8_t i = 0; i < reconState.getNumTargetableDevices(); i++) {
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
    Serial.printf("  Devices Found: %d\n", reconState.getNumTargetableDevices());
    Serial.printf("  Tracked Nodes: %d\n", reconState.getNodeCount());
    Serial.println();
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
