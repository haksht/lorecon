/**
 * Packet Logger - SD Card Data Collection for PC Analysis
 * 
 * Simple, focused module for logging captured packets to SD card.
 * Designed for field deployment with post-analysis on PC.
 * 
 * Philosophy:
 * - ESP32 captures and logs raw data efficiently
 * - PC does heavy analysis, mapping, and visualization
 * - Simple CSV/JSON format for easy PC import
 */

#ifndef PACKET_LOGGER_H
#define PACKET_LOGGER_H

#include <Arduino.h>
#include <SD.h>
#include "pcap_logger.h"
#include "data_structures.h"

// SD card chip select — use board-specific config, not a hardcoded pin
// NOTE: On T3-S3, the old hardcoded GPIO 5 conflicted with LoRa SPI_SCK!

struct PacketLogRecord {
    uint64_t timestampMs;
    uint32_t nodeId;
    const char* protocol;
    float frequencyMHz;
    uint8_t configIndex;
    float rssiDbm;
    float snrDb;
    size_t lengthBytes;
    const char* packetType;
    bool encrypted;  // True if packet is still encrypted (could not decrypt), false if cleartext/decrypted
    const char* pskResult;
    const char* pskId;
    bool hasPosition;
    double latitudeDeg;
    double longitudeDeg;
    double altitudeM;
    int hopCount;
    bool isRouter;
    int powerClass;

    PacketLogRecord()
        : timestampMs(0)
        , nodeId(0)
        , protocol("Unknown")
        , frequencyMHz(0.0f)
        , configIndex(0)
        , rssiDbm(0.0f)
        , snrDb(0.0f)
        , lengthBytes(0)
        , packetType("unknown")
        , encrypted(false)
        , pskResult("none")
        , pskId(nullptr)
        , hasPosition(false)
        , latitudeDeg(0.0)
        , longitudeDeg(0.0)
        , altitudeM(0.0)
        , hopCount(-1)
        , isRouter(false)
        , powerClass(-1) {}
};
/**
 * PacketLogger - Logs reconnaissance data to SD card
 * 
 * Features:
 * - Automatic session management (timestamped files)
 * - CSV format for easy Excel/Python import
 * - Optional JSON export for structured analysis
 * - Minimal memory footprint (streaming writes)
 * - Safe shutdown support (flush on demand)
 */
class PacketLogger {
public:
    PacketLogger();
    
    // Lifecycle
    bool initialize();
    bool isAvailable() const { return sdAvailable; }
    void shutdown();
    
    // Session management
    bool startSession(const char* sessionName = nullptr);
    void endSession();
    String getCurrentSessionFile() const { return currentSessionFile; }
    
    // Logging methods
    bool logPacket(const PacketLogRecord& record, const uint8_t* data, size_t length);
    bool logDevice(uint32_t nodeId, const char* deviceType, const char* protocol,
                   float avgRSSI, float bestRSSI, uint8_t configIndex);
    bool logGPSPosition(uint32_t nodeId, double latitude, double longitude, 
                        uint16_t altitude, uint8_t satsInView);
    
    // Export methods (for end of session)
    bool exportDeviceSummaryJSON();
    bool exportGPSTracksKML();
    
    // Statistics
    uint32_t getPacketCount() const { return packetsLogged; }
    uint32_t getSessionDuration() const;
    void printStatus();
    
private:
    bool sdAvailable;
    File sessionFile;
    String currentSessionFile;
        String currentSessionId;
    uint32_t sessionStartTime;
    uint32_t packetsLogged;
    PCAPSession pcapSession;
    
    // Helper methods
    String generateSessionFilename();
    bool writeCSVHeader();
    bool flushBuffer();
    String escapeCSV(const String& str);
    String bytesToHex(const uint8_t* data, size_t length);
};

// Global logger instance
extern PacketLogger packetLogger;

#endif // PACKET_LOGGER_H
