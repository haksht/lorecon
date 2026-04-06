/**
 * Packet Processor - Queue Management and Packet Analysis
 * 
 * Handles packet queue processing, protocol analysis, and tracking.
 * Separates packet processing logic from radio and UI concerns.
 */

#ifndef PACKET_PROCESSOR_H
#define PACKET_PROCESSOR_H

#include <Arduino.h>
#include <functional>
#include "data_structures.h"
#include "protocol_analyzer.h"
#include "geo_intelligence.h"
#include "config.h"

// Forward declarations
class OLEDDisplay;

// Packet event structure for callbacks
struct PacketEvent {
    uint32_t nodeId;
    const char* protocol;
    float rssi;
    float snr;
    size_t length;
    const char* message;  // nullptr if no message
    uint32_t timestamp;
    // Sniffer GPS position at time of reception (T-Beam Supreme only).
    // 0.0/false on boards without GPS or when no fix is available.
    double lat;
    double lon;
    double alt;
    bool hasPosition;
};

/**
 * Static ring buffer for packet queue  -  zero heap allocations.
 * Replaces std::queue<QueuedPacket> which used std::deque internally,
 * causing heap fragmentation over days of continuous operation.
 */
template<typename T, size_t N>
class StaticRingBuffer {
public:
    bool push(const T& item) {
        if (count_ >= N) return false;
        buf_[tail_] = item;
        tail_ = (tail_ + 1) % N;
        count_++;
        return true;
    }

    const T& front() const { return buf_[head_]; }

    void pop() {
        if (count_ == 0) return;
        head_ = (head_ + 1) % N;
        count_--;
    }

    bool empty() const { return count_ == 0; }
    size_t size() const { return count_; }

private:
    T buf_[N];
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
};

/**
 * PacketProcessor - Manages packet queue and analysis
 *
 * Uses a static ring buffer instead of std::queue to avoid heap
 * fragmentation during long-running operation.
 */
class PacketProcessor {
public:
    PacketProcessor();

    // Set packet event callback for live updates (web server, loggers, etc.)
    void setPacketCallback(std::function<void(const PacketEvent&)> callback) {
        packetCallback = callback;
    }

    // Queue management
    bool queuePacket(const uint8_t* data, size_t length, float rssi, float snr,
                     uint8_t configIndex, float frequencyMHz);
    void processQueue(OLEDDisplay* display = nullptr);
    size_t getQueueSize() const { return packetQueue.size(); }
    bool isQueueFull() const { return packetQueue.size() >= Config::PacketProcessing::QUEUE_SIZE; }

    // Access to last packet (for replay capture)
    const uint8_t* getLastPacketData() const { return lastPacketData; }
    size_t getLastPacketLength() const { return lastPacketLength; }

    // Access to protocol analyzer (for manual packet analysis)
    ProtocolAnalyzer& getProtocolAnalyzer() { return protocolAnalyzer; }

private:
    // Static ring buffer  -  no heap allocations, no fragmentation
    StaticRingBuffer<QueuedPacket, Config::PacketProcessing::QUEUE_SIZE> packetQueue;

    // Last packet storage (static buffer instead of vector)
    uint8_t lastPacketData[Config::PacketProcessing::MAX_PACKET_SIZE];
    size_t lastPacketLength = 0;

    // Analysis modules
    ProtocolAnalyzer protocolAnalyzer;
    GeoIntelligence geoIntel;

    // Event callback for live updates (optional)
    std::function<void(const PacketEvent&)> packetCallback;

    // Meshtastic header fields extracted from a packet
    struct MeshtasticHeader {
        const uint8_t* payload;    // Pointer to start of Meshtastic frame (after search)
        size_t payloadLen;
        uint32_t nodeId;
        uint32_t destId;
        uint32_t packetId;
        uint8_t hopCount;
        uint8_t channel;
        bool wantAck;
        bool viaMqtt;
        uint8_t priority;
        bool found;                // True if Meshtastic header was located
    };

    // Find Meshtastic header in packet and extract all fields
    MeshtasticHeader findAndExtractMeshtasticHeader(const uint8_t* data, size_t length);

    // Common logic: attempt PSK decryption and auto-capture for replay
    void tryDecryptAndCapture(const uint8_t* data, size_t length, float rssi, float snr,
                              const char* protocol, const MeshtasticHeader& hdr);

    // Processing helpers
    void processSinglePacket(const QueuedPacket& qp, OLEDDisplay* display);
    void handlePacket(PacketInfo& info, const uint8_t* data, size_t length,
                     float rssi, float snr, OLEDDisplay* display);
};

#endif // PACKET_PROCESSOR_H
