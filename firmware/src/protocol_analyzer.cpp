#include "protocol_analyzer.h"
#include "utils/format_utils.h"
#include <string.h>

// Comprehensive packet analysis - calls all analysis functions and returns consolidated info
PacketInfo ProtocolAnalyzer::analyze(const uint8_t* data, size_t length, float rssi, uint8_t syncWord) {
    PacketInfo info;

    // Step 1: Identify protocol
    info.protocol = identifyProtocol(data, length, syncWord);
    
    // Step 2: Extract node IDs (source and destination)
    info.nodeId = extractNodeId(data, length, info.protocol);
    info.destId = extractDestId(data, length, info.protocol);
    
    // Step 2b: Extract packet ID (protocol-specific)
    info.packetId = extractPacketId(data, length, info.protocol);
    
    // Step 2c: Extract hop count from flags (lower 3 bits of byte 12)
    info.hopCount = extractHopCount(data, length, info.protocol);
    
    // Step 2d: Extract channel index
    info.channel = extractChannel(data, length, info.protocol);
    
    // Step 2e: Extract flag bits (wantAck, viaMqtt, priority)
    extractFlags(data, length, info.protocol, info.wantAck, info.viaMqtt, info.priority);
    
    // Step 3: Classify device type
    info.deviceType = identifyDeviceType(data, length, info.protocol, rssi);
    
    // Step 4: Estimate power class
    info.powerClass = FormatUtils::estimatePowerClass(rssi);
    
    // Step 5: Check if routing
    info.isRouter = isRoutingDevice(data, length, info.protocol);
    
    // Step 6: Estimate firmware version
    info.firmwareVersion = estimateFirmwareVersion(data, length, info.protocol);
    
    return info;
}

// Identify protocol from packet signature and structure.
// syncWord: the radio sync word active when this packet was received.
// Meshtastic-only sync words (0x2B, 0x48) enable unicast detection  -  the radio hardware
// already filtered to that sync word, so non-Meshtastic packets cannot arrive on those configs.
const char* ProtocolAnalyzer::identifyProtocol(const uint8_t* data, size_t length, uint8_t syncWord) {
    if (length < 4) return "Short";

    // Meshtastic broadcast: destination address 0xFFFFFFFF in bytes 0-3 (little-endian).
    // This is the reliable, sync-word-independent check.
    if (data[0] == 0xFF && data[1] == 0xFF && data[2] == 0xFF && data[3] == 0xFF) {
        return "Meshtastic";
    }

    // LoRaWAN detection (validate actual frame structure, not just length)
    if (length >= 12) {
        uint8_t mhdr = data[0];
        uint8_t mtype = (mhdr >> 5) & 0x07;
        uint8_t major = mhdr & 0x03;

        // LoRaWAN R1 requires major version bits == 0x00
        if (major == 0x00) {
            // Join Request: exactly 23 bytes (MHDR + AppEUI + DevEUI + DevNonce + MIC)
            if (mtype == 0x00 && length == 23) {
                return "LoRaWAN";
            }
            // Join Accept: 17 or 33 bytes (with or without CFList)
            if (mtype == 0x01 && (length == 17 || length == 33)) {
                return "LoRaWAN";
            }
            // Data frames (unconfirmed/confirmed up/down): validate FHDR
            // Minimum: MHDR(1) + DevAddr(4) + FCtrl(1) + FCnt(2) + MIC(4) = 12 bytes
            if (mtype >= 0x02 && mtype <= 0x05 && length >= 12) {
                uint8_t fctrl = data[5];
                uint8_t foptsLen = fctrl & 0x0F;
                // FOptsLen must fit within the frame: MHDR(1)+DevAddr(4)+FCtrl(1)+FCnt(2)+FOpts+MIC(4)
                if (foptsLen <= 15 && (size_t)(8 + foptsLen + 4) <= length) {
                    return "LoRaWAN";
                }
            }
        }
    }
    
    // Meshtastic unicast: destination is a specific node ID (not 0xFFFFFFFF).
    //
    // On Meshtastic-only sync words (0x2B = standard, 0x48 = LongSlow), the SX1262
    // hardware filters by sync word before the ISR fires. Only packets with matching
    // sync word are delivered to firmware. LoRaWAN (0x34) and RadioHead (0x12) cannot
    // arrive on these configs, so any packet >= 14 bytes that wasn't broadcast and
    // didn't match LoRaWAN structure is a unicast Meshtastic packet.
    //
    // Minimum Meshtastic header: 4 dest + 4 src + 4 packet_id + 1 flags + 1 channel = 14 bytes.
    //
    // Sync word 0x12 configs are excluded: they cover both Meshtastic SW12 variants and
    // ISM/RadioHead traffic, so we cannot safely assume Meshtastic on those configs.
    if ((syncWord == 0x2b || syncWord == 0x48) && length >= 14) {
        return "Meshtastic";
    }

    // Short packets might be beacons or keep-alives
    if (length <= 8) return "Beacon";

    // RadioHead RH_RF95: 4-byte header [TO][FROM][ID][FLAGS] + payload
    // FLAGS lower 5 bits are reserved and always 0 in valid frames.
    // Max RH_RF95 payload is 251 bytes (255 - 4 header bytes).
    if (length >= 5 && length <= 251) {
        if ((data[3] & 0x1F) == 0) {
            return "RadioHead";
        }
    }

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

    if (strcmp(protocol, "RadioHead") == 0 && length >= 2) {
        // RH_RF95 FROM address at byte 1 (8-bit, 0-255)
        return data[1];
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

// Extract hop count from flags (Meshtastic byte 12, lower 3 bits)
uint8_t ProtocolAnalyzer::extractHopCount(const uint8_t* data, size_t length, const char* protocol) {
    if (strcmp(protocol, "Meshtastic") == 0 && length >= 13) {
        // Hop count is lower 3 bits of flags byte (byte 12)
        return data[12] & 0x07;
    }
    return 0;
}

// Extract destination node ID (Meshtastic bytes 0-3, little-endian)
uint32_t ProtocolAnalyzer::extractDestId(const uint8_t* data, size_t length, const char* protocol) {
    if (strcmp(protocol, "Meshtastic") == 0 && length >= 4) {
        return ((uint32_t)data[0]) | ((uint32_t)data[1] << 8) | 
               ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    }
    return 0;
}

// Extract channel index (Meshtastic byte 13)
uint8_t ProtocolAnalyzer::extractChannel(const uint8_t* data, size_t length, const char* protocol) {
    if (strcmp(protocol, "Meshtastic") == 0 && length >= 14) {
        return data[13];
    }
    return 0;
}

// Extract flag bits from Meshtastic header
// Byte 12 structure: bits 0-2 = hop limit, bit 3 = want_ack, bit 4 = via_mqtt, bits 5-6 = priority
void ProtocolAnalyzer::extractFlags(const uint8_t* data, size_t length, const char* protocol,
                                     bool& wantAck, bool& viaMqtt, uint8_t& priority) {
    wantAck = false;
    viaMqtt = false;
    priority = 0;
    
    if (strcmp(protocol, "Meshtastic") == 0 && length >= 13) {
        uint8_t flags = data[12];
        wantAck = (flags >> 3) & 0x01;   // Bit 3
        viaMqtt = (flags >> 4) & 0x01;   // Bit 4
        priority = (flags >> 5) & 0x03;  // Bits 5-6 (0-3)
    }
}

// Device type identification based on packet patterns and RSSI characteristics
const char* ProtocolAnalyzer::identifyDeviceType(const uint8_t* data, size_t length, const char* protocol, float rssi) {
    // Meshtastic device type detection
    if (strcmp(protocol, "Meshtastic") == 0) {
        // High power, likely base station or solar powered
        if (rssi > -50) return "Meshtastic Base/Solar";
        
        // Check for routing patterns in payload
        if (length >= 16) {
            uint8_t hopCount = data[12] & 0x07;  // Hop count in routing header (byte 12)
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
    
    // RadioHead RH_RF95 device classification
    if (strcmp(protocol, "RadioHead") == 0) {
        uint8_t toAddr = data[0];
        uint8_t flags  = data[3];
        bool isAckReply = (flags >> 6) & 0x01;
        if (toAddr == 0xFF) return "RadioHead Broadcast";
        if (isAckReply)     return "RadioHead ACK";
        return "RadioHead Node";
    }

    // Beacon analysis
    if (strcmp(protocol, "Beacon") == 0) {
        if (rssi > -60) return "Beacon High-Power";
        return "Beacon Standard";
    }

    return "Unknown Device";
}

// estimatePowerClass moved to FormatUtils::estimatePowerClass() in utils/format_utils.h

// Check if device appears to be routing traffic (forwarding packets)
bool ProtocolAnalyzer::isRoutingDevice(const uint8_t* data, size_t length, const char* protocol) {
    if (strcmp(protocol, "Meshtastic") == 0 && length >= 13) {
        // Hop count is in byte 12, lower 3 bits
        uint8_t hopCount = data[12] & 0x07;
        // A device is likely routing if we see packets with decremented hop count
        // Original packets typically start with hop limit 3, routers decrement it
        // If hop count < 3, this packet has been relayed
        return (hopCount > 0 && hopCount < 3);
    }
    return false;
}

// Estimate firmware version based on packet patterns
// NOTE: These are heuristic guesses based on packet structure, not definitive.
// Packet length and flag patterns correlate with firmware versions but are not proof.
const char* ProtocolAnalyzer::estimateFirmwareVersion(const uint8_t* data, size_t length, const char* protocol) {
    if (strcmp(protocol, "Meshtastic") == 0 && length >= 12) {
        // Analyze packet structure for version clues

        // Firmware 2.2+ uses encryption flag in byte 12, bit 7
        if (length >= 13 && (data[12] & 0x80)) {
            return "~v2.2+ (est: encryption flag)";
        }

        // Firmware 2.1+ typically has longer packets (extended routing headers)
        if (length > 50) {
            return "~v2.1+ (est: extended headers)";
        }

        // Firmware 2.0.x has specific hop count patterns
        if (length >= 13) {
            uint8_t hopCount = data[12] & 0x07;
            uint8_t flags = data[13];

            // v2.0 uses specific flag patterns
            if (hopCount <= 3 && (flags & 0xF0) == 0) {
                return "~v2.0.x (est: flag pattern)";
            }
        }

        // Check for very short packets (older firmware or beacons)
        if (length <= 16) {
            return "~v1.x or beacon (est)";
        }

        return "~v2.0-2.2 (est)";
    }

    if (strcmp(protocol, "LoRaWAN") == 0) {
        // LoRaWAN version can be determined from MIC and frame format
        uint8_t mtype = (data[0] >> 5) & 0x07;

        // LoRaWAN 1.1+ uses different join procedures
        if (mtype == 0x00 || mtype == 0x01) {
            return "~LoRaWAN 1.0/1.1 (est)";
        }

        return "~LoRaWAN 1.0.x (est)";
    }

    return "Unknown";
}
