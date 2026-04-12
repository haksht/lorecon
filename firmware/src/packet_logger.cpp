/**
 * Packet Logger Implementation
 * 
 * Simple SD card logging for field data collection.
 * Focus: Capture data efficiently, analyze on PC later.
 */

#include "packet_logger.h"
#include "pcap_logger.h"
#include "config.h"
#include "logger.h"
#include "utils/sd_utils.h"
#include "utils/format_utils.h"
#include <SPI.h>
#include <time.h>

/**
 * Open a CSV file for appending, creating it with a header line if new.
 * Returns an open File handle on success, or a falsy File on failure.
 */
static File openCSVForAppend(const char* path, const char* headerLine) {
    if (!SD.exists(path)) {
        File f = SD.open(path, FILE_WRITE);
        if (f) {
            f.println(headerLine);
            f.close();
        }
    }
    return SD.open(path, FILE_APPEND);
}

// Global instance
PacketLogger packetLogger;

PacketLogger::PacketLogger()
    : sdAvailable(false)
    , currentSessionFile("")
    , currentSessionId("")
    , sessionStartTime(0)
    , packetsLogged(0)
    , pcapSession()
    , lowSpaceWarning(false)
    , lastFreeSpaceMB(0)
{
}

bool PacketLogger::initialize() {
    Serial.println("[SD] Initializing SD card logger...");
    
    // Use shared SD initialization (board-specific CS pin from config.h)
    if (!SDUtils::initialize()) {
        Serial.println("[SD] [!]  No SD card detected");
        Serial.println("[SD]   Insert SD card for data logging capability");
        Serial.println("[SD]   Device will continue without logging");
        sdAvailable = false;
        return false;
    }
    
    Serial.printf("[SD] [OK] SD card detected: %s\n", SDUtils::getCardTypeString());
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
        Serial.printf("[SD] [FAIL] Failed to create session file: %s\n", fullPath.c_str());
        return false;
    }
    
    sessionStartTime = millis();
    packetsLogged = 0;
    
    // Write CSV header
    writeCSVHeader();
    
    Serial.printf("[SD] [OK] Session started: %s\n", fullPath.c_str());
    
    // Start PCAP capture session
    if (Config::Logging::PCAP_EXPORT_ENABLED) {
        String pcapFilename = "/logs/" + currentSessionId + ".pcap";
        if (pcapSession.startSession(pcapFilename.c_str())) {
            Serial.printf("[PCAP] [OK] PCAP capture started: %s\n", pcapFilename.c_str());
        } else {
            Serial.println("[PCAP] [!]  PCAP capture failed to start");
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

    // Fixed fields: timestamp, session, nodeId, protocol, freq, config, rssi, snr, len, type, encrypted, psk_result
    sessionFile.printf("%llu,%s,%s,%s,%.3f,%u,%.1f,%.1f,%u,%s,%d,%s,",
        record.timestampMs,
        currentSessionId.c_str(),
        record.nodeId ? FormatUtils::formatNodeIdPadded(record.nodeId).c_str() : "",
        record.protocol ? record.protocol : "Unknown",
        record.frequencyMHz,
        record.configIndex,
        record.rssiDbm,
        record.snrDb,
        (unsigned)record.lengthBytes,
        record.packetType ? record.packetType : "unknown",
        record.encrypted ? 1 : 0,
        record.pskResult ? record.pskResult : "none");

    // psk_id (optional)
    if (record.pskId && record.pskId[0] != '\0') {
        sessionFile.print(record.pskId);
    }
    sessionFile.print(',');

    // lat, lon, alt (3 columns  -  empty when no position)
    if (record.hasPosition) {
        sessionFile.printf("%.6f,%.6f,%.1f,", record.latitudeDeg, record.longitudeDeg, record.altitudeM);
    } else {
        sessionFile.print(",,,");
    }

    // hop_count (optional), is_router, power_class (optional)
    if (record.hopCount >= 0) sessionFile.print(record.hopCount);
    sessionFile.printf(",%d,", record.isRouter ? 1 : 0);
    if (record.powerClass >= 0) sessionFile.print(record.powerClass);
    sessionFile.print(',');

    // packet_id, dest_id_hex, channel, relay_byte (new columns for relay tracking)
    if (record.packetId != 0) {
        sessionFile.printf("%08X", record.packetId);
    }
    sessionFile.print(',');
    if (record.destId != 0 && record.destId != 0xFFFFFFFF) {
        sessionFile.print(FormatUtils::formatNodeIdPadded(record.destId).c_str());
    } else if (record.destId == 0xFFFFFFFF) {
        sessionFile.print("FFFFFFFF");
    }
    sessionFile.printf(",%u,", record.channel);
    if (record.relayByte != 0) {
        sessionFile.printf("%02X", record.relayByte);
    }
    sessionFile.print(',');

    // Raw hex  -  write directly to avoid String allocation
    size_t safeLen = (length <= Config::PacketProcessing::MAX_PACKET_SIZE)
                     ? length : Config::PacketProcessing::MAX_PACKET_SIZE;
    for (size_t i = 0; i < safeLen; i++) {
        sessionFile.printf("%02X", data[i]);
    }
    sessionFile.println();
    
    packetsLogged++;

    // Flush every 10 packets to prevent data loss
    if (packetsLogged % Config::System::SD_FLUSH_INTERVAL == 0) {
        sessionFile.flush();
    }

    // Check free space every SD_SPACE_CHECK_INTERVAL packets.
    // SD.usedBytes() scans the FAT — too slow to call every packet.
    if (packetsLogged % Config::Logging::SD_SPACE_CHECK_INTERVAL == 0) {
        lastFreeSpaceMB = SDUtils::getFreeMB();
        if (lastFreeSpaceMB <= Config::Logging::SD_STOP_THRESHOLD_MB) {
            // Graceful stop: flush, close, disable — no more silent write failures
            sessionFile.flush();
            endSession();
            sdAvailable = false;
            LOG_ERROR("[SD] Card full (< %lu MB free) — logging stopped to prevent data corruption",
                      Config::Logging::SD_STOP_THRESHOLD_MB);
        } else if (lastFreeSpaceMB <= Config::Logging::SD_WARN_THRESHOLD_MB) {
            lowSpaceWarning = true;
            LOG_WARN("[SD] Low space: %llu MB free", lastFreeSpaceMB);
        } else {
            lowSpaceWarning = false;  // Cleared if space freed (e.g., user deleted files)
        }
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

    File f = openCSVForAppend("/logs/devices_summary.csv",
                              "timestamp,nodeId,deviceType,protocol,avgRSSI,bestRSSI,configIndex");
    if (!f) {
        LOG_ERROR("Failed to open devices summary for append");
        return false;
    }

    f.printf("%lu,%s,%s,%s,%.1f,%.1f,%u\n",
             (unsigned long)millis(),
             FormatUtils::formatNodeIdPadded(nodeId).c_str(),
             deviceType, protocol, avgRSSI, bestRSSI, configIndex);
    f.close();
    return true;
}

bool PacketLogger::logGPSPosition(uint32_t nodeId, double latitude,
                                 double longitude, uint16_t altitude,
                                 uint8_t satsInView) {
    if (!sdAvailable) {
        return false;
    }

    File f = openCSVForAppend("/logs/gps_tracks.csv",
                              "timestamp,nodeId,latitude,longitude,altitude,satsInView");
    if (!f) {
        LOG_ERROR("Failed to open GPS tracks file for append");
        return false;
    }

    f.printf("%lu,%s,%.6f,%.6f,%u,%u\n",
             (unsigned long)millis(),
             FormatUtils::formatNodeIdPadded(nodeId).c_str(),
             latitude, longitude, altitude, satsInView);
    f.close();
    return true;
}


uint32_t PacketLogger::getSessionDuration() const {
    if (sessionStartTime == 0) {
        return 0;
    }
    return (millis() - sessionStartTime) / 1000;
}

void PacketLogger::printStatus() {
    Serial.println("\n+========================================+");
    Serial.println("|      PACKET LOGGER STATUS              |");
    Serial.println("+========================================+");
    
    if (!sdAvailable) {
        Serial.println("Status: [FAIL] SD card not available");
        Serial.println("\nInsert SD card and restart for logging capability.");
        return;
    }
    
    Serial.println("Status: [OK] SD card ready");
    
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
    
    Serial.println("========================================\n");
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
    
    sessionFile.println("timestamp_ms,session_id,node_id_hex,protocol,frequency_mhz,config_index,rssi_dbm,snr_db,length_bytes,packet_type,encrypted,psk_result,psk_id,lat_deg,lon_deg,alt_m,hop_count,is_router,power_class,packet_id,dest_id_hex,channel,relay_byte,raw_hex");
    sessionFile.flush();
    return true;
}

void PacketLogger::flush() {
    if (sessionFile) sessionFile.flush();
    if (Config::Logging::PCAP_EXPORT_ENABLED && pcapSession.isActive()) pcapSession.flush();
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
