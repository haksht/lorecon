/**
 * Packet Logger Implementation
 * 
 * Simple SD card logging for field data collection.
 * Focus: Capture data efficiently, analyze on PC later.
 */

#include "packet_logger.h"
#include <SPI.h>
#include <time.h>

// Global instance
PacketLogger packetLogger;

PacketLogger::PacketLogger() 
    : sdAvailable(false)
    , sessionStartTime(0)
    , packetsLogged(0)
{
}

bool PacketLogger::initialize() {
    Serial.println("[SD] Initializing SD card logger...");
    
    // Try to initialize SD card
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("[SD] ⚠️  No SD card detected");
        Serial.println("[SD]   Insert SD card for data logging capability");
        Serial.println("[SD]   Device will continue without logging");
        sdAvailable = false;
        return false;
    }
    
    // Check card type
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[SD] ❌ No SD card attached");
        sdAvailable = false;
        return false;
    }
    
    // Print card info
    Serial.print("[SD] ✅ SD card detected: ");
    if (cardType == CARD_MMC) {
        Serial.println("MMC");
    } else if (cardType == CARD_SD) {
        Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("[SD] Card size: %llu MB\n", cardSize);
    
    sdAvailable = true;
    return true;
}

void PacketLogger::shutdown() {
    if (sessionFile) {
        endSession();
    }
    
    if (sdAvailable) {
        SD.end();
        sdAvailable = false;
        Serial.println("[SD] Shut down safely");
    }
}

bool PacketLogger::startSession(const char* sessionName) {
    if (!sdAvailable) {
        return false;
    }
    
    // Close previous session if open
    if (sessionFile) {
        endSession();
    }
    
    // Generate filename
    if (sessionName == nullptr) {
        currentSessionFile = generateSessionFilename();
    } else {
        currentSessionFile = String(sessionName) + ".csv";
    }
    
    // Create directory if needed
    if (!SD.exists("/logs")) {
        SD.mkdir("/logs");
    }
    
    String fullPath = "/logs/" + currentSessionFile;
    
    // Open file for writing
    sessionFile = SD.open(fullPath.c_str(), FILE_WRITE);
    if (!sessionFile) {
        Serial.printf("[SD] ❌ Failed to create session file: %s\n", fullPath.c_str());
        return false;
    }
    
    sessionStartTime = millis();
    packetsLogged = 0;
    
    // Write CSV header
    writeCSVHeader();
    
    Serial.printf("[SD] ✅ Session started: %s\n", fullPath.c_str());
    return true;
}

void PacketLogger::endSession() {
    if (!sessionFile) {
        return;
    }
    
    sessionFile.flush();
    sessionFile.close();
    
    uint32_t duration = (millis() - sessionStartTime) / 1000;
    Serial.printf("[SD] Session ended: %u packets logged in %u seconds\n", 
                  packetsLogged, duration);
    
    currentSessionFile = "";
    sessionStartTime = 0;
}

bool PacketLogger::logPacket(const uint8_t* data, size_t length, float rssi, 
                             float snr, const char* protocol, uint32_t nodeId) {
    if (!sessionFile) {
        return false;
    }
    
    // CSV format: timestamp,nodeId,protocol,rssi,snr,length,hex_data
    sessionFile.print(millis());
    sessionFile.print(",");
    sessionFile.printf("0x%08X", nodeId);
    sessionFile.print(",");
    sessionFile.print(protocol);
    sessionFile.print(",");
    sessionFile.print(rssi, 1);
    sessionFile.print(",");
    sessionFile.print(snr, 1);
    sessionFile.print(",");
    sessionFile.print(length);
    sessionFile.print(",");
    sessionFile.println(bytesToHex(data, length));
    
    packetsLogged++;
    
    // Flush every 10 packets to prevent data loss
    if (packetsLogged % 10 == 0) {
        sessionFile.flush();
    }
    
    return true;
}

bool PacketLogger::logDevice(uint32_t nodeId, const char* deviceType, 
                             const char* protocol, float avgRSSI, 
                             float bestRSSI, uint8_t configIndex) {
    if (!sdAvailable) {
        return false;
    }
    
    // Append to devices summary file
    String devicesFile = "/logs/devices_summary.csv";
    
    // Create file with header if it doesn't exist
    if (!SD.exists(devicesFile.c_str())) {
        File f = SD.open(devicesFile.c_str(), FILE_WRITE);
        if (f) {
            f.println("timestamp,nodeId,deviceType,protocol,avgRSSI,bestRSSI,configIndex");
            f.close();
        }
    }
    
    File f = SD.open(devicesFile.c_str(), FILE_APPEND);
    if (!f) {
        return false;
    }
    
    f.print(millis());
    f.print(",");
    f.printf("0x%08X", nodeId);
    f.print(",");
    f.print(deviceType);
    f.print(",");
    f.print(protocol);
    f.print(",");
    f.print(avgRSSI, 1);
    f.print(",");
    f.print(bestRSSI, 1);
    f.print(",");
    f.println(configIndex);
    
    f.close();
    return true;
}

bool PacketLogger::logGPSPosition(uint32_t nodeId, double latitude, 
                                 double longitude, uint16_t altitude, 
                                 uint8_t satsInView) {
    if (!sdAvailable) {
        return false;
    }
    
    // Append to GPS tracks file
    String gpsFile = "/logs/gps_tracks.csv";
    
    // Create file with header if it doesn't exist
    if (!SD.exists(gpsFile.c_str())) {
        File f = SD.open(gpsFile.c_str(), FILE_WRITE);
        if (f) {
            f.println("timestamp,nodeId,latitude,longitude,altitude,satsInView");
            f.close();
        }
    }
    
    File f = SD.open(gpsFile.c_str(), FILE_APPEND);
    if (!f) {
        return false;
    }
    
    f.print(millis());
    f.print(",");
    f.printf("0x%08X", nodeId);
    f.print(",");
    f.print(latitude, 6);
    f.print(",");
    f.print(longitude, 6);
    f.print(",");
    f.print(altitude);
    f.print(",");
    f.println(satsInView);
    
    f.close();
    return true;
}

bool PacketLogger::exportDeviceSummaryJSON() {
    // NOTE: JSON/KML export is handled by PC analysis tools
    // See tools/pc_analyzer.py for conversion of CSV to JSON/KML formats
    // Keeping CSV format allows easy Excel/pandas import and is more flexible
    Serial.println("[SD] JSON export: Use PC tools (pc_analyzer.py) for format conversion");
    return true;
}

bool PacketLogger::exportGPSTracksKML() {
    // NOTE: KML export is handled by PC analysis tools
    // See tools/pc_analyzer.py for conversion of CSV to KML/GeoJSON formats
    // ESP32 CSV logging is simpler, more reliable, and doesn't require XML parsing
    Serial.println("[SD] KML export: Use PC tools (pc_analyzer.py) for format conversion");
    return true;
}

uint32_t PacketLogger::getSessionDuration() const {
    if (sessionStartTime == 0) {
        return 0;
    }
    return (millis() - sessionStartTime) / 1000;
}

void PacketLogger::printStatus() {
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║      PACKET LOGGER STATUS              ║");
    Serial.println("╚════════════════════════════════════════╝");
    
    if (!sdAvailable) {
        Serial.println("Status: ❌ SD card not available");
        Serial.println("\nInsert SD card and restart for logging capability.");
        return;
    }
    
    Serial.println("Status: ✅ SD card ready");
    
    if (sessionFile) {
        Serial.printf("Session: %s\n", currentSessionFile.c_str());
        Serial.printf("Packets logged: %u\n", packetsLogged);
        Serial.printf("Duration: %u seconds\n", getSessionDuration());
    } else {
        Serial.println("Session: No active session");
        Serial.println("\nTip: Logger automatically starts on first packet capture");
    }
    
    // Show available space
    uint64_t totalBytes = SD.totalBytes();
    uint64_t usedBytes = SD.usedBytes();
    Serial.printf("Storage: %llu / %llu MB used\n", 
                  usedBytes / (1024 * 1024), 
                  totalBytes / (1024 * 1024));
    
    Serial.println("════════════════════════════════════════\n");
}

// Private helper methods

String PacketLogger::generateSessionFilename() {
    // Format: recon_HHMMSS.csv
    uint32_t seconds = millis() / 1000;
    uint32_t hours = (seconds / 3600) % 24;
    uint32_t minutes = (seconds / 60) % 60;
    uint32_t secs = seconds % 60;
    
    char filename[32];
    snprintf(filename, sizeof(filename), "recon_%02u%02u%02u.csv", 
             hours, minutes, secs);
    
    return String(filename);
}

bool PacketLogger::writeCSVHeader() {
    if (!sessionFile) {
        return false;
    }
    
    sessionFile.println("timestamp,nodeId,protocol,rssi,snr,length,hex_data");
    sessionFile.flush();
    return true;
}

bool PacketLogger::flushBuffer() {
    if (!sessionFile) {
        return false;
    }
    
    sessionFile.flush();
    return true;
}

String PacketLogger::escapeCSV(const String& str) {
    // Simple CSV escaping - wrap in quotes if contains comma
    if (str.indexOf(',') >= 0) {
        return "\"" + str + "\"";
    }
    return str;
}

String PacketLogger::bytesToHex(const uint8_t* data, size_t length) {
    String hex = "";
    hex.reserve(length * 2);
    
    for (size_t i = 0; i < length; i++) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02X", data[i]);
        hex += buf;
    }
    
    return hex;
}
