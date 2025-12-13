#include "protocol_analyzer.h"
#include <string.h>

// Comprehensive packet analysis - calls all analysis functions and returns consolidated info
PacketInfo ProtocolAnalyzer::analyze(const uint8_t* data, size_t length, float rssi) {
    PacketInfo info;
    
    // Step 1: Identify protocol
    info.protocol = identifyProtocol(data, length);
    
    // Step 2: Extract node ID (protocol-specific)
    info.nodeId = extractNodeId(data, length, info.protocol);
    
    // Step 2b: Extract packet ID (protocol-specific)
    info.packetId = extractPacketId(data, length, info.protocol);
    
    // Step 3: Classify device type
    info.deviceType = identifyDeviceType(data, length, info.protocol, rssi);
    
    // Step 4: Estimate power class
    info.powerClass = estimatePowerClass(rssi);
    
    // Step 5: Check if routing
    info.isRouter = isRoutingDevice(data, length, info.protocol);
    
    // Step 6: Estimate firmware version
    info.firmwareVersion = estimateFirmwareVersion(data, length, info.protocol);
    
    return info;
}

// Identify protocol from packet signature and structure
const char* ProtocolAnalyzer::identifyProtocol(const uint8_t* data, size_t length) {
    if (length < 4) return "Short";
    
    // Meshtastic signature detection (4-byte magic header)
    if (data[0] == 0xFF && data[1] == 0xFF && data[2] == 0xFF && data[3] == 0xFF) {
        return "Meshtastic";
    }
    
    // TTN/LoRaWAN detection (structured frame format)
    if (length >= 12 && length <= 51) {
        uint8_t mtype = (data[0] >> 5) & 0x07;
        // mtype is 3 bits (0-7), so always valid range
        if (mtype <= 0x07) {
            return "LoRaWAN";
        }
    }
    
    // Short packets might be beacons or keep-alives
    if (length <= 8) return "Beacon";
    
    return "Unknown";
}

// Extract node ID from packet (protocol-dependent field locations)
uint32_t ProtocolAnalyzer::extractNodeId(const uint8_t* data, size_t length, const char* protocol) {
    if (strcmp(protocol, "Meshtastic") == 0 && length >= 8) {
        // Meshtastic node ID at offset 4-7 (little-endian)
        return ((uint32_t)data[4]) | ((uint32_t)data[5] << 8) | 
               ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
    }
    
    if (strcmp(protocol, "LoRaWAN") == 0 && length >= 8) {
        // LoRaWAN DevAddr at offset 1-4 (little endian)
        return uint32_t(data[1]) | (uint32_t(data[2]) << 8) | 
               (uint32_t(data[3]) << 16) | (uint32_t(data[4]) << 24);
    }
    
    return 0;
}

// Extract packet ID from packet (protocol-dependent field locations)
uint32_t ProtocolAnalyzer::extractPacketId(const uint8_t* data, size_t length, const char* protocol) {
    if (strcmp(protocol, "Meshtastic") == 0 && length >= 12) {
        // Meshtastic packet ID at offset 8-11 (little-endian)
        return ((uint32_t)data[8]) | ((uint32_t)data[9] << 8) | 
               ((uint32_t)data[10] << 16) | ((uint32_t)data[11] << 24);
    }
    
    if (strcmp(protocol, "LoRaWAN") == 0 && length >= 8) {
        // LoRaWAN frame counter at offset 6-7 (little endian, 16-bit)
        return ((uint32_t)data[6]) | ((uint32_t)data[7] << 8);
    }
    
    return 0;
}

// Device type identification based on packet patterns and RSSI characteristics
const char* ProtocolAnalyzer::identifyDeviceType(const uint8_t* data, size_t length, const char* protocol, float rssi) {
    // Meshtastic device type detection
    if (strcmp(protocol, "Meshtastic") == 0) {
        // High power, likely base station or solar powered
        if (rssi > -50) return "Meshtastic Base/Solar";
        
        // Check for routing patterns in payload
        if (length >= 16) {
            uint8_t hopCount = data[8] & 0x07;  // Hop count in routing header
            if (hopCount > 0) return "Meshtastic Router";
        }
        
        // Power class estimation based on RSSI and packet patterns
        if (rssi > -80) return "Meshtastic Mobile";
        if (rssi > -110) return "Meshtastic Handheld";
        return "Meshtastic Low-Power";
    }
    
    // LoRaWAN device classification
    if (strcmp(protocol, "LoRaWAN") == 0) {
        uint8_t mtype = (data[0] >> 5) & 0x07;
        
        switch (mtype) {
            case 0x00: return "LoRaWAN Join Request";
            case 0x01: return "LoRaWAN Join Accept";
            case 0x02: return "LoRaWAN Unconfirmed Up";
            case 0x03: return "LoRaWAN Unconfirmed Down";
            case 0x04: return "LoRaWAN Confirmed Up";
            case 0x05: return "LoRaWAN Confirmed Down";
            case 0x06: return "LoRaWAN RejoinReq";
            case 0x07: return "LoRaWAN Proprietary";
        }
    }
    
    // Beacon analysis
    if (strcmp(protocol, "Beacon") == 0) {
        if (rssi > -60) return "Beacon High-Power";
        return "Beacon Standard";
    }
    
    return "Unknown Device";
}

// Estimate power class based on RSSI patterns
uint8_t ProtocolAnalyzer::estimatePowerClass(float rssi) {
    if (rssi > -70) return 2;  // High power (>100mW)
    if (rssi > -90) return 1;  // Medium power (10-100mW)
    return 0;                  // Low power (<10mW)
}

// Check if device appears to be routing traffic (forwarding packets)
bool ProtocolAnalyzer::isRoutingDevice(const uint8_t* data, size_t length, const char* protocol) {
    if (strcmp(protocol, "Meshtastic") == 0 && length >= 12) {
        uint8_t hopCount = data[8] & 0x07;
        uint8_t routingFlags = data[9];
        return (hopCount > 0 || (routingFlags & 0x01)); // Has hops or routing flag set
    }
    return false;
}

// Estimate firmware version based on packet patterns
const char* ProtocolAnalyzer::estimateFirmwareVersion(const uint8_t* data, size_t length, const char* protocol) {
    if (strcmp(protocol, "Meshtastic") == 0 && length >= 12) {
        // Analyze packet structure for version clues
        
        // Firmware 2.2+ uses encryption flag in byte 8, bit 7
        if (length >= 9 && (data[8] & 0x80)) {
            return "v2.2+ (encryption enabled)";
        }
        
        // Firmware 2.1+ typically has longer packets (extended routing headers)
        if (length > 50) {
            return "v2.1+ (extended headers)";
        }
        
        // Firmware 2.0.x has specific hop count patterns
        if (length >= 9) {
            uint8_t hopCount = data[8] & 0x07;
            uint8_t flags = data[9];
            
            // v2.0 uses specific flag patterns
            if (hopCount <= 3 && (flags & 0xF0) == 0) {
                return "v2.0.x (classic routing)";
            }
        }
        
        // Check for very short packets (older firmware or beacons)
        if (length <= 16) {
            return "v1.x or beacon";
        }
        
        return "v2.0-2.2 (uncertain)";
    }
    
    if (strcmp(protocol, "LoRaWAN") == 0) {
        // LoRaWAN version can be determined from MIC and frame format
        uint8_t mtype = (data[0] >> 5) & 0x07;
        
        // LoRaWAN 1.1+ uses different join procedures
        if (mtype == 0x00 || mtype == 0x01) {
            return "LoRaWAN 1.0/1.1";
        }
        
        return "LoRaWAN 1.0.x";
    }
    
    return "Unknown";
}
