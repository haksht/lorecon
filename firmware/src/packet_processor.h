/**
 * Packet Processor - Queue Management and Packet Analysis
 * 
 * Handles packet queue processing, protocol analysis, and tracking.
 * Separates packet processing logic from radio and UI concerns.
 */

#ifndef PACKET_PROCESSOR_H
#define PACKET_PROCESSOR_H

#include <Arduino.h>
#include <queue>
#include <vector>
#include "data_structures.h"
#include "protocol_analyzer.h"
#include "geo_intelligence.h"
#include "config.h"

// Forward declarations
class OLEDDisplay;
class WebServer;

/**
 * PacketProcessor - Manages packet queue and analysis
 * 
 * Responsibilities:
 * - Queue management for interrupt-received packets
 * - Protocol analysis and classification
 * - Node and RF activity tracking (via ReconState)
 * - PSK decryption coordination
 * - Mode-specific packet handling
 */
class PacketProcessor {
public:
    PacketProcessor();
    
    // Set web server for live packet broadcasting
    void setWebServer(WebServer* ws) { webServer = ws; }
    
    // Queue management
    bool queuePacket(const uint8_t* data, size_t length, float rssi, float snr);
    void processQueue(OLEDDisplay* display = nullptr);
    size_t getQueueSize() const { return packetQueue.size(); }
    bool isQueueFull() const { return packetQueue.size() >= Config::PacketProcessing::QUEUE_SIZE; }
    
    // Access to last packet (for replay capture)
    const std::vector<uint8_t>& getLastPacket() const { return lastPacketData; }
    size_t getLastPacketLength() const { return lastPacketData.size(); }
    
    // Access to protocol analyzer (for manual packet analysis)
    ProtocolAnalyzer& getProtocolAnalyzer() { return protocolAnalyzer; }
    
private:
    // Packet queue
    std::queue<QueuedPacket> packetQueue;
    
    // Last packet storage (for potential replay) - using vector for safety
    std::vector<uint8_t> lastPacketData;
    
    // Analysis modules
    ProtocolAnalyzer protocolAnalyzer;
    GeoIntelligence geoIntel;
    
    // Web server for live updates (optional)
    WebServer* webServer = nullptr;
    
    // Processing helpers
    void processSinglePacket(const QueuedPacket& qp, OLEDDisplay* display);
    void handleReconPacket(const PacketInfo& info, const uint8_t* data, size_t length, 
                          float rssi, float snr, OLEDDisplay* display);
    void handleTargetedPacket(const PacketInfo& info, const uint8_t* data, size_t length, 
                             float rssi, float snr, OLEDDisplay* display);
};

#endif // PACKET_PROCESSOR_H
