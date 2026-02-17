/**
 * Packet Logger Implementation
 * 
 * Simple SD card logging for field data collection.
 * Focus: Capture data efficiently, analyze on PC later.
 */

#include "packet_logger.h"
#include "pcap_logger.h"
#include "config.h"
#include "utils/sd_utils.h"
#include <SPI.h>
#include <time.h>

// Global instance
PacketLogger packetLogger;

PacketLogger::PacketLogger() 
    : sdAvailable(false)
    , currentSessionFile("")
    , currentSessionId("")
    , sessionStartTime(0)
    , packetsLogged(0)
    , pcapSession()
{
}

bool PacketLogger::initialize() {
    Serial.println("[SD] Initializing SD card logger...");
    
    // Use shared SD initialization (board-specific CS pin from config.h)
    if (!SDUtils::initialize()) {
        Serial.println("[SD] ⚠️  No SD card detected");
        Serial.println("[SD]   Insert SD card for data logging capability");
        Serial.println("[SD]   Device will continue without logging");
        sdAvailable = false;
        return false;
    }
    
    Serial.printf("[SD] ✅ SD card detected: %s\n", SDUtils::getCardTypeString());
    Serial.printf("[SD] Card size: %llu MB\n", SDUtils::getCardSizeMB());
    
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
    currentSessionId = currentSessionFile;
    if (currentSessionId.endsWith(".csv")) {
        currentSessionId.remove(currentSessionId.length() - 4);
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
    
    // Start PCAP capture session
    if (Config::Logging::PCAP_EXPORT_ENABLED) {
        String pcapFilename = "/logs/" + currentSessionId + ".pcap";
        if (pcapSession.startSession(pcapFilename.c_str())) {
            Serial.printf("[PCAP] ✅ PCAP capture started: %s\n", pcapFilename.c_str());
        } else {
            Serial.println("[PCAP] ⚠️  PCAP capture failed to start");
        }
    }
    
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
    currentSessionId = "";
    sessionStartTime = 0;
}

bool PacketLogger::logPacket(const PacketLogRecord& record, const uint8_t* data, size_t length) {
    if (!sessionFile) {
        return false;
    }
    
    sessionFile.print(record.timestampMs);
    sessionFile.print(",");
    sessionFile.print(currentSessionId);
    sessionFile.print(",");
    if (record.nodeId != 0) {
        char nodeStr[9];
        snprintf(nodeStr, sizeof(nodeStr), "%08X", record.nodeId);
        sessionFile.print(nodeStr);
    }
    sessionFile.print(",");
    sessionFile.print(record.protocol ? record.protocol : "Unknown");
    sessionFile.print(",");
    sessionFile.print(record.frequencyMHz, 3);
    sessionFile.print(",");
    sessionFile.print(record.configIndex);
    sessionFile.print(",");
    sessionFile.print(record.rssiDbm, 1);
    sessionFile.print(",");
    sessionFile.print(record.snrDb, 1);
    sessionFile.print(",");
    sessionFile.print(record.lengthBytes);
    sessionFile.print(",");
    sessionFile.print(record.packetType ? record.packetType : "unknown");
    sessionFile.print(",");
    sessionFile.print(record.encrypted ? 1 : 0);
    sessionFile.print(",");
    sessionFile.print(record.pskResult ? record.pskResult : "none");
    sessionFile.print(",");
    if (record.pskId && record.pskId[0] != '\0') {
        sessionFile.print(record.pskId);
    }
    sessionFile.print(",");
    if (record.hasPosition) {
        sessionFile.print(record.latitudeDeg, 6);
        sessionFile.print(",");
        sessionFile.print(record.longitudeDeg, 6);
        sessionFile.print(",");
        sessionFile.print(record.altitudeM, 1);
    } else {
        sessionFile.print(",,");
    }
    sessionFile.print(",");
    if (record.hopCount >= 0) {
        sessionFile.print(record.hopCount);
    }
    sessionFile.print(",");
    sessionFile.print(record.isRouter ? 1 : 0);
    sessionFile.print(",");
    if (record.powerClass >= 0) {
        sessionFile.print(record.powerClass);
    }
    sessionFile.print(",");
    // Write hex directly to file — avoids String allocation for up to 256-byte packets
    {
        char hexBuf[3];
        for (size_t i = 0; i < length; i++) {
            snprintf(hexBuf, sizeof(hexBuf), "%02X", data[i]);
            sessionFile.print(hexBuf);
        }
    }
    sessionFile.println();
    
    packetsLogged++;
    
    // Flush every 10 packets to prevent data loss
    if (packetsLogged % 10 == 0) {
        sessionFile.flush();
    }
    
    // Also log to PCAP if enabled
    if (Config::Logging::PCAP_EXPORT_ENABLED && pcapSession.isActive()) {
        pcapSession.logPacket(data, length, record.rssiDbm, record.snrDb, record.frequencyMHz);
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
    // Format: snf_<millis>.csv (millis since boot keeps filenames unique)
    uint32_t nowMs = millis();
    char filename[32];
    snprintf(filename, sizeof(filename), "snf_%lu.csv", static_cast<unsigned long>(nowMs));
    return String(filename);
}

bool PacketLogger::writeCSVHeader() {
    if (!sessionFile) {
        return false;
    }
    
    sessionFile.println("timestamp_ms,session_id,node_id_hex,protocol,frequency_mhz,config_index,rssi_dbm,snr_db,length_bytes,packet_type,encrypted,psk_result,psk_id,lat_deg,lon_deg,alt_m,hop_count,is_router,power_class,raw_hex");
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
