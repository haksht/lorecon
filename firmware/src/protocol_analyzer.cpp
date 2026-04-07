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
    // Checked BEFORE MeshCore: both use sync word 0x12, and RadioHead's FLAGS
    // constraint is a stronger structural test than MeshCore's header bit check.
    if (syncWord == 0x12 && length >= 5 && length <= 251) {
        if ((data[3] & 0x1F) == 0) {
            return "RadioHead";
        }
    }

    // MeshCore: sync word 0x12 + valid header bits + structural validity.
    // Header format: 0bVVPPPPRR — VV=version (0-1), PPPP=payload type (0-11), RR=route type (0-3).
    // Structural check: path_length byte must resolve to a payload offset within the packet,
    // ensuring the packet is actually parseable as MeshCore and not a RadioHead false positive
    // that slipped through the FLAGS check above.
    if (syncWord == 0x12 && length >= 4) {
        uint8_t hdr        = data[0];
        uint8_t version    = (hdr >> 6) & 0x03;
        uint8_t payloadType = (hdr >> 2) & 0x0F;
        if (version <= 1 && payloadType <= 11) {
            // Validate packet structure: compute payload offset and verify it lands within bounds.
            uint8_t routeType     = hdr & 0x03;
            size_t pathLenOffset  = (routeType == 2 || routeType == 3) ? 5 : 1;
            if (pathLenOffset + 1 <= length) {
                uint8_t pathLenByte = data[pathLenOffset];
                uint8_t hopCount    = pathLenByte & 0x3F;
                uint8_t hashSize    = ((pathLenByte >> 6) & 0x03) + 1;
                size_t payloadOffset = pathLenOffset + 1 + (size_t)(hopCount * hashSize);
                if (payloadOffset < length) {
                    return "MeshCore";
                }
            }
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

    if (strcmp(protocol, "MeshCore") == 0) {
        uint8_t routeType = data[0] & 0x03;

        // Route types 2/3 (TRANSPORT_FLOOD/DIRECT) carry 4-byte transport_codes at bytes 1-4
        // set by the originator — stable per sender across all relay copies.
        if ((routeType == 2 || routeType == 3) && length >= 5) {
            return ((uint32_t)data[1]) | ((uint32_t)data[2] << 8) |
                   ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 24);
        }

        // Route types 0/1: no transport_codes. Path starts at byte 1.
        // path_length byte: bits 0-5 = hop_count, bits 6-7 = hash_size - 1.
        // path[0] is the originator's node hash — stable per sender.
        // Build a uint32 from up to 4 bytes of the originator hash (zero-padded).
        if (length >= 3) {
            uint8_t pathLenByte = data[1];
            uint8_t hopCount = pathLenByte & 0x3F;
            uint8_t hashSize = ((pathLenByte >> 6) & 0x03) + 1;
            if (hopCount > 0 && (size_t)(2 + hashSize) <= length) {
                uint32_t id = 0;
                uint8_t take = (hashSize < 4) ? hashSize : 4;
                for (uint8_t i = 0; i < take; i++) {
                    id |= ((uint32_t)data[2 + i]) << (i * 8);
                }
                return id;
            }
        }
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
    if (strcmp(protocol, "MeshCore") == 0 && length >= 2) {
        // path_length byte: bits 0-5 = hop_count (number of nodes this packet has traversed)
        uint8_t routeType = data[0] & 0x03;
        size_t pathLenOffset = (routeType == 2 || routeType == 3) ? 5 : 1;
        if (pathLenOffset < length) {
            return data[pathLenOffset] & 0x3F;
        }
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
    // Meshtastic device type detection.
    // Hardware class (Base/Solar/Handheld) cannot be determined from RSSI alone —
    // an external antenna on a handheld will appear as strong as a base station.
    // We only note whether the first observed packet was already relayed (informational).
    if (strcmp(protocol, "Meshtastic") == 0) {
        if (length >= 13) {
            uint8_t hopCount = data[12] & 0x07;
            if (hopCount > 0 && hopCount < 3) return "Meshtastic Node (relayed pkt)";
        }
        return "Meshtastic Node";
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

    if (strcmp(protocol, "MeshCore") == 0 && length >= 1) {
        // Header byte: 0bVVPPPPRR — PPPP = payload type (bits 2-5)
        uint8_t payloadType = (data[0] >> 2) & 0x0F;
        switch (payloadType) {
            case 0:  return "MeshCore Msg";
            case 1:  return "MeshCore ACK";
            case 2:  return "MeshCore Signed Msg";
            case 3:  return "MeshCore Trace";
            case 4:  return "MeshCore Advert";
            case 5:  return "MeshCore Direct";
            case 6:  return "MeshCore Ping";
            case 7:  return "MeshCore Pong";
            case 8:  return "MeshCore Position";
            default: return "MeshCore Node";
        }
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
// NOTE: A single packet cannot reliably determine whether its *source* is a router.
// A decremented hop count only tells us *this copy* was relayed — not that the source
// node is doing the relaying. Router classification is accumulated over multiple packets
// in DeviceRepository::updateExistingDevice() and is more reliable.
bool ProtocolAnalyzer::isRoutingDevice(const uint8_t* data, size_t length, const char* protocol) {
    (void)data; (void)length; (void)protocol;
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

    // MeshCore: version bits [6:7] of header byte are the actual protocol version field.
    if (strcmp(protocol, "MeshCore") == 0 && length >= 1) {
        uint8_t version = (data[0] >> 6) & 0x03;
        return version == 0 ? "MeshCore v0" : "MeshCore v1";
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
