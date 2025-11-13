#ifndef PROTOCOL_ANALYZER_H
#define PROTOCOL_ANALYZER_H

#include <Arduino.h>

// Result structure containing all protocol analysis information
struct PacketInfo {
    const char* protocol;       // "Meshtastic", "LoRaWAN", "Beacon", "Unknown"
    const char* deviceType;     // Detailed device classification
    uint32_t nodeId;            // Extracted node/device ID (0 if not applicable)
    uint8_t powerClass;         // 0=Low, 1=Medium, 2=High power
    bool isRouter;              // true if device appears to be routing traffic
    const char* firmwareVersion; // Estimated firmware version
    const char* message;        // Decrypted message content (if any)
    
    PacketInfo() 
        : protocol("Unknown")
        , deviceType("Unknown Device")
        , nodeId(0)
        , powerClass(0)
        , isRouter(false)
        , firmwareVersion("Unknown")
        , message(nullptr)
    {}
};

// Protocol analysis class - extracts intelligence from raw LoRa packets
class ProtocolAnalyzer {
public:
    // Analyze a packet and return comprehensive information
    PacketInfo analyze(const uint8_t* data, size_t length, float rssi);
    
    // Individual analysis functions (public for flexibility)
    const char* identifyProtocol(const uint8_t* data, size_t length);
    uint32_t extractNodeId(const uint8_t* data, size_t length, const char* protocol);
    const char* identifyDeviceType(const uint8_t* data, size_t length, const char* protocol, float rssi);
    uint8_t estimatePowerClass(float rssi);
    bool isRoutingDevice(const uint8_t* data, size_t length, const char* protocol);
    const char* estimateFirmwareVersion(const uint8_t* data, size_t length, const char* protocol);
};

#endif // PROTOCOL_ANALYZER_H
