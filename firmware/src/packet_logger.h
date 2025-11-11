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
#include "data_structures.h"

// SD card configuration (adjust for your hardware)
#define SD_CS_PIN 21  // Chip select pin for SD card

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
    bool logPacket(const uint8_t* data, size_t length, float rssi, float snr, 
                   const char* protocol, uint32_t nodeId = 0);
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
    uint32_t sessionStartTime;
    uint32_t packetsLogged;
    
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
