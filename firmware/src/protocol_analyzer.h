#ifndef PROTOCOL_ANALYZER_H
#define PROTOCOL_ANALYZER_H

#include <Arduino.h>

// Result structure containing all protocol analysis information
struct PacketInfo {
    const char* protocol;       // "Meshtastic", "LoRaWAN", "Beacon", "Unknown"
    const char* deviceType;     // Detailed device classification
    uint32_t nodeId;            // Source/From node ID (0 if not applicable)
    uint32_t destId;            // Destination node ID (0xFFFFFFFF = broadcast)
    uint32_t packetId;          // Packet ID for deduplication (0 if not applicable)
    uint8_t hopCount;           // Hop count from flags (lower 3 bits)
    uint8_t channel;            // Channel index (byte 13)
    bool wantAck;               // Sender wants acknowledgment (flags bit 3)
    bool viaMqtt;               // Packet came via MQTT gateway (flags bit 4)
    uint8_t priority;           // Packet priority 0-3 (flags bits 5-6)
    uint8_t powerClass;         // 0=Low, 1=Medium, 2=High power
    bool isRouter;              // true if device appears to be routing traffic
    const char* firmwareVersion; // Estimated firmware version
    char message[256];          // Decrypted message content storage
    bool hasMessage;            // True if message field contains valid data
    
    PacketInfo() 
        : protocol("Unknown")
        , deviceType("Unknown Device")
        , nodeId(0)
        , destId(0)
        , packetId(0)
        , hopCount(0)
        , channel(0)
        , wantAck(false)
        , viaMqtt(false)
        , priority(0)
        , powerClass(0)
        , isRouter(false)
        , firmwareVersion("Unknown")
        , hasMessage(false)
    {
        message[0] = '\0';
    }
};

// Protocol analysis class - extracts intelligence from raw LoRa packets
class ProtocolAnalyzer {
public:
    // Analyze a packet and return comprehensive information
    PacketInfo analyze(const uint8_t* data, size_t length, float rssi);
    
    // Individual analysis functions (public for flexibility)
    const char* identifyProtocol(const uint8_t* data, size_t length);
    uint32_t extractNodeId(const uint8_t* data, size_t length, const char* protocol);
    uint32_t extractDestId(const uint8_t* data, size_t length, const char* protocol);
    uint32_t extractPacketId(const uint8_t* data, size_t length, const char* protocol);
    uint8_t extractHopCount(const uint8_t* data, size_t length, const char* protocol);
    uint8_t extractChannel(const uint8_t* data, size_t length, const char* protocol);
    void extractFlags(const uint8_t* data, size_t length, const char* protocol,
                      bool& wantAck, bool& viaMqtt, uint8_t& priority);
    const char* identifyDeviceType(const uint8_t* data, size_t length, const char* protocol, float rssi);
    // estimatePowerClass moved to FormatUtils::estimatePowerClass()
    bool isRoutingDevice(const uint8_t* data, size_t length, const char* protocol);
    const char* estimateFirmwareVersion(const uint8_t* data, size_t length, const char* protocol);
};

#endif // PROTOCOL_ANALYZER_H
