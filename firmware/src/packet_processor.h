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
#include "data_structures.h"
#include "protocol_analyzer.h"
#include "geo_intelligence.h"

// Forward declarations
class OLEDDisplay;

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
    
    // Queue management
    bool queuePacket(const uint8_t* data, size_t length, float rssi, float snr);
    void processQueue(OLEDDisplay* display = nullptr);
    size_t getQueueSize() const { return packetQueue.size(); }
    bool isQueueFull() const { return packetQueue.size() >= MAX_QUEUE_SIZE; }
    
    // Access to last packet (for replay capture)
    const uint8_t* getLastPacket() const { return lastPacketData; }
    size_t getLastPacketLength() const { return lastPacketLength; }
    
    // Access to protocol analyzer (for manual packet analysis)
    ProtocolAnalyzer& getProtocolAnalyzer() { return protocolAnalyzer; }
    
private:
    // Packet queue
    std::queue<QueuedPacket> packetQueue;
    static const size_t MAX_QUEUE_SIZE = 10;
    
    // Last packet storage (for potential replay)
    uint8_t lastPacketData[MAX_PACKET_SIZE];
    size_t lastPacketLength;
    
    // Analysis modules
    ProtocolAnalyzer protocolAnalyzer;
    GeoIntelligence geoIntel;
    
    // Processing helpers
    void processSinglePacket(const QueuedPacket& qp, OLEDDisplay* display);
    void handleReconPacket(const PacketInfo& info, const uint8_t* data, size_t length, 
                          float rssi, float snr, OLEDDisplay* display);
    void handleTargetedPacket(const PacketInfo& info, const uint8_t* data, size_t length, 
                             float rssi, float snr, OLEDDisplay* display);
};

#endif // PACKET_PROCESSOR_H
