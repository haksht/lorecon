/**
 * PSK Decryption - Simplified and Clean Version
 * 
 * Tests default Meshtastic PSKs against captured packets.
 * Extracts text messages from successfully decrypted data.
 */

#include "psk_decryption_simple.h"
#include <mbedtls/aes.h>
#include <mbedtls/base64.h>
#include "geo_intelligence.h"
#include "text_packet_diagnostic.h"
#include "utils/protobuf_utils.h"

// Static storage for last decrypted message (protected by mutex)
char PSKDecryption::lastMessage[PSKDecryption::MAX_MESSAGE_LEN] = {0};
SemaphoreHandle_t PSKDecryption::messageMutex = nullptr;

// Static storage for last DeviceMetrics battery telemetry
int16_t PSKDecryption::lastBatteryLevel = -1;
float PSKDecryption::lastBatteryVoltage = 0.0f;

// Static storage for last firmware version / hw model
char PSKDecryption::lastFirmwareVersion[32] = {0};
char PSKDecryption::lastHwModel[24] = {0};

// Thread-safe setter for lastMessage
void PSKDecryption::setLastMessage(const char* msg) {
    if (!messageMutex) return;
    if (xSemaphoreTake(messageMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        strncpy(lastMessage, msg, MAX_MESSAGE_LEN - 1);
        lastMessage[MAX_MESSAGE_LEN - 1] = '\0';
        xSemaphoreGive(messageMutex);
    }
}

// Thread-safe getter - copies to caller buffer
void PSKDecryption::getLastMessageSafe(char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    buffer[0] = '\0';
    if (!messageMutex) return;
    if (xSemaphoreTake(messageMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        strncpy(buffer, lastMessage, bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
        xSemaphoreGive(messageMutex);
    }
}

void PSKDecryption::clearLastBattery() {
    if (!messageMutex) return;
    if (xSemaphoreTake(messageMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        lastBatteryLevel = -1;
        lastBatteryVoltage = 0.0f;
        xSemaphoreGive(messageMutex);
    }
}

void PSKDecryption::setLastBattery(int16_t level, float voltage) {
    if (!messageMutex) return;
    if (xSemaphoreTake(messageMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        lastBatteryLevel = level;
        lastBatteryVoltage = voltage;
        xSemaphoreGive(messageMutex);
    }
}

void PSKDecryption::getLastBattery(int16_t& level, float& voltage) {
    level = -1;
    voltage = 0.0f;
    if (!messageMutex) return;
    if (xSemaphoreTake(messageMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        level = lastBatteryLevel;
        voltage = lastBatteryVoltage;
        xSemaphoreGive(messageMutex);
    }
}

void PSKDecryption::clearLastFirmware() {
    if (!messageMutex) return;
    if (xSemaphoreTake(messageMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        lastFirmwareVersion[0] = '\0';
        lastHwModel[0] = '\0';
        xSemaphoreGive(messageMutex);
    }
}

void PSKDecryption::setLastFirmware(const char* fw, const char* hw) {
    if (!messageMutex) return;
    if (xSemaphoreTake(messageMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (fw && fw[0] != '\0') {
            strncpy(lastFirmwareVersion, fw, sizeof(lastFirmwareVersion) - 1);
            lastFirmwareVersion[sizeof(lastFirmwareVersion) - 1] = '\0';
        }
        if (hw && hw[0] != '\0') {
            strncpy(lastHwModel, hw, sizeof(lastHwModel) - 1);
            lastHwModel[sizeof(lastHwModel) - 1] = '\0';
        }
        xSemaphoreGive(messageMutex);
    }
}

void PSKDecryption::getLastFirmware(char* fwBuf, size_t fwBufLen,
                                    char* hwBuf, size_t hwBufLen) {
    if (fwBuf && fwBufLen > 0) fwBuf[0] = '\0';
    if (hwBuf && hwBufLen > 0) hwBuf[0] = '\0';
    if (!messageMutex) return;
    if (xSemaphoreTake(messageMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (fwBuf && fwBufLen > 0) {
            strncpy(fwBuf, lastFirmwareVersion, fwBufLen - 1);
            fwBuf[fwBufLen - 1] = '\0';
        }
        if (hwBuf && hwBufLen > 0) {
            strncpy(hwBuf, lastHwModel, hwBufLen - 1);
            hwBuf[hwBufLen - 1] = '\0';
        }
        xSemaphoreGive(messageMutex);
    }
}

void PSKDecryption::clearLastMessage() {
    if (!messageMutex) return;
    if (xSemaphoreTake(messageMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        lastMessage[0] = '\0';
        xSemaphoreGive(messageMutex);
    }
}

// Protocol constants
#define MIN_PACKET_LENGTH           5    // Minimum protobuf packet size
#define MAX_TEXT_MESSAGE_LENGTH     200  // Meshtastic text message limit
#define ASCII_PRINTABLE_MIN         32   // First printable ASCII character
#define ASCII_PRINTABLE_MAX         126  // Last printable ASCII character
#define MAX_VARINT_BYTES            5    // Maximum bytes in a varint encoding

// Known Meshtastic default PSKs (Base64-encoded)
// 
// These are the most common pre-shared keys found in Meshtastic networks:
// - Keys 1-2: Current default keys (AQ== is the standard default)
// - Keys 3-10: Legacy single-byte keys (pre-2.0 firmware, expanded to 16 bytes)
// - Keys 11-14: Test/development keys commonly left in deployments
// - Keys 15-18: Historic defaults from older firmware (admin channel, etc.)
// - Keys 19-23: Channel preset keys (MediumFast, ShortFast, LongSlow, etc.)
//
// Security Note: These keys are publicly documented. Production networks
// should use unique PSKs. This tool tests default keys for research/analysis.
//
// Key Format: Base64-encoded AES keys (8, 16, or 32 bytes when decoded)
// Decoding: Base64 -> binary key -> AES-256-CTR decryption
static constexpr const char* DEFAULT_PSKS[] = {
    // === Standard defaults (most common) ===
    "AQ==",                              // #1: Single byte 0x01 - THE classic default
    "1PG7OiApB1nwvP+rz05pAQ==",          // #2: Default public channel key (LongFast)
    
    // === Legacy single-byte keys (pre-2.0, expanded to 16 bytes) ===
    "Ag==",                              // #3: Single byte 0x02
    "Aw==",                              // #4: Single byte 0x03
    "BA==",                              // #5: Single byte 0x04
    "BQ==",                              // #6: Single byte 0x05
    "Bg==",                              // #7: Single byte 0x06
    "Bw==",                              // #8: Single byte 0x07
    "CA==",                              // #9: Single byte 0x08
    "CQ==",                              // #10: Single byte 0x09
    
    // === Test/development keys often left in production ===
    "AAAAAAAAAAAAAAAAAAAAAA==",          // #11: All zeros (16 bytes)
    "MTIzNDU2Nzg5MDEyMzQ1Ng==",          // #12: "1234567890123456" ASCII
    "dGVzdHRlc3R0ZXN0dGVzdA==",          // #13: "testtesttesttest" ASCII
    "bWVzaHRhc3RpY21lc2h0YXN0",          // #14: "meshtasticmeshtast" ASCII
    
    // === Historic defaults from older firmware versions ===
    // Admin channel default (pre-2.5) - used for remote device config
    "PKdTs51e4EB0BoOevIN0Dw==",          // #15: Admin channel default (pre-2.5)
    // Default secondary channel key
    "shmLkA9H74gAeLH3eGCqsw==",          // #16: Secondary channel default
    // Debug/dev key found in firmware source code
    "ogDPnKVRN7wz/VF8nt6LkA==",          // #17: Debug/dev key from source
    // EU868 regional default (some EU deployments)
    "ZQ+HdKKbbAU4dSCGt66Qqw==",          // #18: EU regional default
    
    // === Channel preset derived keys (hash of preset name) ===
    // These are derived from channel preset names - vulnerable if using defaults
    "d1iq21lNSh7BP6MOkP6cQA==",          // #19: MediumFast preset
    "/u7k03L8N3Q=",                      // #20: ShortFast preset (8 bytes)
    "GGC5DDnv8FKFm7WCZ5rXBA==",          // #21: LongSlow preset
    "LHrwq5nxPIJlqFU/K5dKKQ==",          // #22: MediumSlow preset
    "sb6GxC62sdwGXxJz2sERuQ==",          // #23: ShortSlow preset
};


static constexpr uint8_t NUM_PSKS = sizeof(DEFAULT_PSKS) / sizeof(char*);

PSKStats pskStats;

// Use shared varint decoder from protobuf_utils.h
using ProtobufUtils::decodeVarint;

// ============================================================================
// Helper Functions
// ============================================================================

static bool extractTextField(const uint8_t* data, size_t len, size_t startPos, String& text) {
    // Expect: 0x0A <length_varint> <text_bytes>
    if (startPos >= len || data[startPos] != 0x0A) return false;
    
    uint32_t textLen = 0;
    size_t varintBytes = 0;
    if (!decodeVarint(data, len, startPos + 1, textLen, varintBytes)) return false;
    
    size_t textStart = startPos + 1 + varintBytes;
    if (textStart >= len || textLen == 0 || textLen > MAX_TEXT_MESSAGE_LENGTH) return false;
    
    // Extract printable text
    for (uint32_t i = 0; i < textLen && (textStart + i) < len; i++) {
        uint8_t c = data[textStart + i];
        if (c >= ASCII_PRINTABLE_MIN && c <= ASCII_PRINTABLE_MAX) {
            text += (char)c;
        } else if (c == '\n' || c == '\r') {
            text += '\n';
        } else if (c == 0) {
            break;  // Null terminator
        }
    }
    
    return text.length() > 0;
}

static bool extractMessage(const uint8_t* data, size_t len, String& text) {
    if (len < MIN_PACKET_LENGTH) return false;
    
    // Pattern 1: TEXT_MESSAGE_APP format (0x08 0x01 0x12 <len> <text_bytes>)
    // The text is the raw payload bytes after the length!
    if (data[0] == 0x08 && data[1] == 0x01 && data[2] == 0x12) {
        uint32_t payloadLen = 0;
        size_t varintBytes = 0;
        if (decodeVarint(data, len, 3, payloadLen, varintBytes)) {
            size_t textStart = 3 + varintBytes;
            if (textStart < len && payloadLen > 0 && payloadLen <= MAX_TEXT_MESSAGE_LENGTH) {
                // Extract the text directly (it's the payload!)
                for (uint32_t i = 0; i < payloadLen && (textStart + i) < len; i++) {
                    uint8_t c = data[textStart + i];
                    if (c >= ASCII_PRINTABLE_MIN && c <= ASCII_PRINTABLE_MAX) {
                        text += (char)c;
                    } else if (c == '\n' || c == '\r') {
                        text += '\n';
                    } else if (c == 0) {
                        break;  // Null terminator
                    }
                }
                if (text.length() > 0) return true;
            }
        }
    }
    
    // Pattern 2: Standard nested format (0x08 <portnum> 0x12 <len> 0x0A <textlen> <text>)
    if (data[0] == 0x08 && data[2] == 0x12) {
        uint32_t payloadLen = 0;
        size_t varintBytes = 0;
        if (decodeVarint(data, len, 3, payloadLen, varintBytes)) {
            size_t payloadStart = 3 + varintBytes;
            if (extractTextField(data, len, payloadStart, text)) {
                return true;
            }
        }
    }
    
    // Pattern 3: Implicit portnum (0x12 <len> 0x0A <textlen> <text>)
    if (data[0] == 0x12) {
        uint32_t payloadLen = 0;
        size_t varintBytes = 0;
        if (decodeVarint(data, len, 1, payloadLen, varintBytes)) {
            size_t payloadStart = 1 + varintBytes;
            if (extractTextField(data, len, payloadStart, text)) {
                return true;
            }
        }
    }
    
    // Pattern 4: Search for 0x0A field tag anywhere (less reliable)
    for (size_t i = 0; i < len - 3; i++) {
        if (data[i] == 0x0A) {
            String candidate;
            if (extractTextField(data, len, i, candidate) && candidate.length() >= 4) {
                text = candidate;
                return true;
            }
        }
    }
    
    return false;
}

// ============================================================================
// Public Interface
// ============================================================================

// Map Meshtastic HardwareModel enum to display string.
// Values from meshtastic/mesh.proto. Returns nullptr for unknown models
// (caller should skip the deviceType update rather than show a wrong name).
const char* PSKDecryption::hwModelToString(uint32_t model) {
    switch (model) {
        case 1:  return "TLora V2";
        case 2:  return "TLora V1";
        case 3:  return "TLora V2.1-1.6";
        case 4:  return "T-Beam";
        case 9:  return "Heltec V2.0";
        case 10: return "T-Echo";
        case 11: return "TLora V2.1-1.8";
        case 12: return "RAK4631";
        case 13: return "Heltec V2.1";
        case 15: return "T-Beam v0.7";
        case 16: return "T-Beam M8N";
        case 17: return "TLora V1.3";
        case 37: return "Nano G1";
        case 38: return "T-Beam S3 Core";
        case 39: return "TLora T3-S3";
        case 40: return "Nano G1 Explorer";
        case 47: return "Heltec V3";
        case 48: return "Heltec WSL V3";
        case 52: return "Heltec Tracker";
        case 53: return "Heltec Paper";
        case 54: return "T-Deck";
        case 55: return "T-Watch S3";
        default: return nullptr;
    }
}

// Parse a MapReport inner payload (portnum 0x43).
// Extracts firmware_version (field 4, tag 0x22) and hw_model (field 3, tag 0x18).
// Unknown or skipped fields are walked past safely.
void PSKDecryption::parseMAPReport(const uint8_t* data, size_t len,
                                   char* fwBuf, size_t fwBufLen, uint32_t& hwModel) {
    size_t pos = 0;
    while (pos < len) {
        uint32_t tag = 0;
        size_t tagBytes = 0;
        if (!decodeVarint(data, len, pos, tag, tagBytes)) break;
        pos += tagBytes;

        uint8_t wireType = tag & 0x07;
        uint8_t fieldNum = tag >> 3;

        if (wireType == 0) {
            // Varint field
            uint32_t val = 0;
            size_t valBytes = 0;
            if (!decodeVarint(data, len, pos, val, valBytes)) break;
            pos += valBytes;
            if (fieldNum == 3) hwModel = val;  // hw_model
        } else if (wireType == 2) {
            // Length-delimited field
            uint32_t fieldLen = 0;
            size_t lenBytes = 0;
            if (!decodeVarint(data, len, pos, fieldLen, lenBytes)) break;
            pos += lenBytes;
            if (pos + fieldLen > len) break;

            if (fieldNum == 4 && fwBuf && fwBufLen > 1) {
                // firmware_version string
                size_t copyLen = fieldLen < fwBufLen - 1 ? fieldLen : fwBufLen - 1;
                // Validate printable ASCII
                bool valid = true;
                for (size_t i = 0; i < copyLen; i++) {
                    uint8_t c = data[pos + i];
                    if (c < 32 || c > 126) { valid = false; break; }
                }
                if (valid && copyLen > 0) {
                    memcpy(fwBuf, data + pos, copyLen);
                    fwBuf[copyLen] = '\0';
                }
            }
            pos += fieldLen;
        } else if (wireType == 1) {
            pos += 8;  // 64-bit
        } else if (wireType == 5) {
            pos += 4;  // 32-bit
        } else {
            break;  // Unknown wire type — stop parsing
        }
    }
}

// Parse a User inner payload (portnum 0x04).
// Extracts hw_model (field 6, tag 0x30).
void PSKDecryption::parseNODEINFO(const uint8_t* data, size_t len, uint32_t& hwModel) {
    size_t pos = 0;
    while (pos < len) {
        uint32_t tag = 0;
        size_t tagBytes = 0;
        if (!decodeVarint(data, len, pos, tag, tagBytes)) break;
        pos += tagBytes;

        uint8_t wireType = tag & 0x07;
        uint8_t fieldNum = tag >> 3;

        if (wireType == 0) {
            uint32_t val = 0;
            size_t valBytes = 0;
            if (!decodeVarint(data, len, pos, val, valBytes)) break;
            pos += valBytes;
            if (fieldNum == 6) { hwModel = val; return; }  // hw_model — done
        } else if (wireType == 2) {
            uint32_t fieldLen = 0;
            size_t lenBytes = 0;
            if (!decodeVarint(data, len, pos, fieldLen, lenBytes)) break;
            pos += lenBytes;
            if (pos + fieldLen > len) break;
            pos += fieldLen;
        } else if (wireType == 1) {
            pos += 8;
        } else if (wireType == 5) {
            pos += 4;
        } else {
            break;
        }
    }
}

void PSKDecryption::initialize() {
    if (!messageMutex) messageMutex = xSemaphoreCreateMutex();
    pskStats = PSKStats();
    Serial.println(" PSK Testing initialized");
    Serial.printf(" Testing %d default keys\n", NUM_PSKS);
}

int PSKDecryption::decodeBase64(const char* input, uint8_t* output, size_t maxLen) {
    size_t outLen = 0;
    int result = mbedtls_base64_decode(output, maxLen, &outLen,
                                       (const unsigned char*)input, strlen(input));
    return (result == 0 && outLen > 0) ? (int)outLen : 0;
}

bool PSKDecryption::testDefaultPSKs(const uint8_t* data, size_t length) {
    pskStats.attempts++;
    
    // Clear stale data from previous packets (thread-safe)
    clearLastMessage();
    clearLastBattery();
    clearLastFirmware();
    
    // Validate minimum packet structure
    if (length < 20) {
        Serial.printf("[PSK] Packet too small (%d bytes, need >=20)\n", length);
        return false;
    }
    
    // Check for standard Meshtastic header
    bool hasHeader = (data[0] == 0xFF && data[1] == 0xFF && 
                      data[2] == 0xFF && data[3] == 0xFF);
    
    if (!hasHeader) {
        // Try to find header in packet (for routed packets)
        for (size_t i = 0; i + 4 <= length; i++) {
            if (data[i] == 0xFF && data[i+1] == 0xFF && 
                data[i+2] == 0xFF && data[i+3] == 0xFF) {
                data += i;
                length -= i;
                hasHeader = true;
                Serial.printf("[PSK] Found header at offset %d\n", i);
                break;
            }
        }
    }
    
    if (!hasHeader) {
        Serial.printf("[PSK] No Meshtastic header found (first 4 bytes: %02X %02X %02X %02X)\n",
                      data[0], data[1], data[2], data[3]);
        return false;
    }
    
    if (length < 20) {
        Serial.printf("[PSK] Packet too short after header search (%d bytes)\n", length);
        return false;
    }
    
    // Extract packet header fields
    // Meshtastic PacketHeader structure (16 bytes total):
    // Bytes 0-3:   To (destination node, little-endian)
    // Bytes 4-7:   From (source node, little-endian)  
    // Bytes 8-11:  ID (packet ID, little-endian)
    // Byte 12:     Flags
    // Byte 13:     Channel index
    // Byte 14:     Next hop (for routing)
    // Byte 15:     Relay node (for routing)
    // NOTE: All multi-byte fields are LITTLE-ENDIAN in the packet!
    
    uint32_t nodeId = ((uint32_t)data[4]) | ((uint32_t)data[5] << 8) | 
                      ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
    uint32_t packetId = ((uint32_t)data[8]) | ((uint32_t)data[9] << 8) | 
                        ((uint32_t)data[10] << 16) | ((uint32_t)data[11] << 24);
    uint8_t flags = data[12];
    uint8_t channelIdx = data[13];
    
    // Encrypted payload starts at byte 16 (after full header)
    const uint8_t* encryptedData = data + 16;
    size_t encryptedLen = length - 16;
    
    Serial.printf("[PSK] Testing packet: NodeID=0x%08X, PacketID=0x%08X, Flags=0x%02X, Chan=%d\n",
                  nodeId, packetId, flags, channelIdx);
    Serial.printf("[PSK] Encrypted payload: %d bytes at offset 16\n", encryptedLen);
    Serial.print("[PSK] First 16 bytes of encrypted: ");
    for (size_t j = 0; j < min(encryptedLen, size_t(16)); j++) {
        Serial.printf("%02X ", encryptedData[j]);
    }
    Serial.println();
    
    if (encryptedLen > 256 || encryptedLen < 6) {
        Serial.printf("[PSK] Invalid encrypted length (%d bytes, expect 6-256)\n", encryptedLen);
        return false;
    }
    
    // Check if packet is actually unencrypted (stricter validation)
    // Valid protobuf wire types: 0, 1, 2, 5 (types 3,4,6,7 are deprecated/invalid)
    // But encrypted data can randomly have valid wire types!
    // Better heuristic: Check if first byte is 0x08 (field 1, varint) AND second byte is valid portnum
    uint8_t firstByte = encryptedData[0];
    uint8_t secondByte = (encryptedLen > 1) ? encryptedData[1] : 0;
    uint8_t wireType = firstByte & 0x07;
    
    // Strong unencrypted indicator: 0x08 followed by known portnum (0x01, 0x03, 0x04, 0x07, 0x08, 0x09)
    bool looksUnencrypted = (firstByte == 0x08 && 
                             (secondByte == 0x01 || secondByte == 0x03 || 
                              secondByte == 0x04 || secondByte == 0x07 || 
                              secondByte == 0x08 || secondByte == 0x09));
    
    if (looksUnencrypted) {
        Serial.println("[PSK] [!]  Packet appears unencrypted (0x08 + valid portnum detected)");
        Serial.printf("[PSK] First bytes: 0x%02X 0x%02X (field 1, portnum %d)\n", 
                     firstByte, secondByte, secondByte);
        
        // Try to extract text message
        String plaintext;
        if (extractMessage(encryptedData, encryptedLen, plaintext)) {
            Serial.println("\n+============================================+");
            Serial.printf("|   PLAINTEXT MESSAGE: \"%s\"\n", plaintext.c_str());
            Serial.println("+============================================+\n");
            // Store for web broadcast (thread-safe)
            setLastMessage(plaintext.c_str());
            return true;
        }
        
        // Try to extract telemetry/position data
        Serial.println("[PSK] Parsing unencrypted packet...");
        for (size_t j = 0; j < min(encryptedLen, size_t(32)); j++) {
            Serial.printf("%02X ", encryptedData[j]);
        }
        Serial.println();
        
        // Check if this is a position packet and extract GPS
        if (secondByte == 0x03) {  // POSITION_APP
            Serial.println("[PSK]  Unencrypted position packet - extracting GPS...");
            geoIntel.extractPositionFromDecrypted(encryptedData, encryptedLen, nodeId);
        }
        
        return true;
    }
    
    // Build AES-CTR nonce (PacketID + NodeID)
    uint8_t nonce[16];
    memset(nonce, 0, sizeof(nonce));
    
    nonce[0] = (packetId) & 0xFF;
    nonce[1] = (packetId >> 8) & 0xFF;
    nonce[2] = (packetId >> 16) & 0xFF;
    nonce[3] = (packetId >> 24) & 0xFF;
    
    nonce[8] = data[4];
    nonce[9] = data[5];
    nonce[10] = data[6];
    nonce[11] = data[7];
    
    // Try each PSK
    for (uint8_t i = 0; i < NUM_PSKS; i++) {
        uint8_t key[32];
        int keyLen = decodeBase64(DEFAULT_PSKS[i], key, sizeof(key));
        
        // Validate key length
        if (keyLen != 16 && keyLen != 32 && keyLen != 1 && keyLen != 8) {
            continue;  // Invalid key length
        }
        
        // Expand single-byte keys to 16 bytes (defaultpsk)  
        uint8_t expandedKey[32];
        int keyBits = 0;
        
        if (keyLen == 16) {
            memcpy(expandedKey, key, 16);
            keyBits = 128;
        } else if (keyLen == 32) {
            memcpy(expandedKey, key, 32);
            keyBits = 256;
        } else if (keyLen == 1) {
            // Single byte: expand to 16 bytes by repeating
            memset(expandedKey, key[0], 16);
            keyBits = 128;
        } else if (keyLen == 8) {
            // 8-byte key: duplicate to fill 16 bytes
            memcpy(expandedKey, key, 8);
            memcpy(expandedKey + 8, key, 8);
            keyBits = 128;
        } else {
            continue;
        }
        
        // Decrypt with AES-CTR
        uint8_t decrypted[256];
        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        
        if (mbedtls_aes_setkey_enc(&aes, expandedKey, keyBits) != 0) {
            mbedtls_aes_free(&aes);
            continue;
        }
        
        uint8_t nonce_counter[16];
        uint8_t stream_block[16];
        memcpy(nonce_counter, nonce, 16);
        memset(stream_block, 0, 16);
        size_t nc_off = 0;
        
        int result = mbedtls_aes_crypt_ctr(&aes, encryptedLen, &nc_off,
                                           nonce_counter, stream_block,
                                           encryptedData, decrypted);
        mbedtls_aes_free(&aes);
        
        if (result != 0) {
            continue;
        }
        
        // Validate decryption (basic protobuf check)
        uint8_t firstByte = decrypted[0];
        uint8_t fieldNum = firstByte >> 3;
        uint8_t wireType = firstByte & 0x07;
        
        // More lenient validation - wireType 6 and 7 are invalid, but others might work
        if (wireType == 6 || wireType == 7) {
            continue;
        }
        
        if (fieldNum < 1 || fieldNum > 31) {
            continue;  // Invalid protobuf structure
        }
        
        // Check if it looks like valid protobuf (0x08 = field 1, wire type 0)
        bool looksValid = (firstByte == 0x08) || (firstByte == 0x10) || (firstByte == 0x12) || (firstByte == 0x18);
        
        // Update diagnostic statistics for activity analysis
        TextPacketDiagnostic::analyzeDecryptedPacket(decrypted, encryptedLen, length);
        
        if (!looksValid) {
            // Possible match but not confirmed - don't count as success
            Serial.printf("\n[PSK] [!] Possible match with key #%d (\"%s\") - unverified\n", i + 1, DEFAULT_PSKS[i]);
            Serial.printf("[PSK] First byte 0x%02X doesn't match expected protobuf start\n", firstByte);
            // Continue to next key - this one didn't produce valid output
            continue;
        }
        
        // Confirmed success - valid protobuf structure
        pskStats.successes++;
        pskStats.hitCount[i]++;
        
        Serial.printf("\n[PSK] + Decrypted with key #%d (\"%s\")\n", i + 1, DEFAULT_PSKS[i]);
        Serial.printf("[PSK] Node: 0x%08X, Packet: 0x%08X, Flags: 0x%02X\n", 
                      nodeId, packetId, flags);
        
        // Identify packet type
        if (firstByte == 0x08 && encryptedLen > 1) {
            uint8_t portnum = decrypted[1];
            const char* typeStr = "UNKNOWN";
            switch(portnum) {
                case 0x01: typeStr = "TEXT_MESSAGE_APP"; break;
                case 0x03: typeStr = "POSITION_APP"; break;
                case 0x04: typeStr = "NODEINFO_APP"; break;
                case 0x07: typeStr = "ADMIN_APP"; break;
                case 0x08: typeStr = "TELEMETRY_APP"; break;
                case 0x09: typeStr = "TEXT_MESSAGE_COMPRESSED_APP"; break;
                case 0x42: typeStr = "TRACEROUTE_APP"; break;
                case 0x43: typeStr = "MAP_REPORT_APP"; break;
            }
            Serial.printf("[PSK] Type: %s (portnum 0x%02X)\n", typeStr, portnum);
        }
        
        // Try to extract text message
        String messageText;
        if (extractMessage(decrypted, encryptedLen, messageText)) {
            Serial.println("\n+============================================+");
            Serial.printf("|   TEXT MESSAGE: \"%s\"\n", messageText.c_str());
            Serial.println("+============================================+\n");
            // Store for web broadcast (thread-safe)
            setLastMessage(messageText.c_str());
        } else {
            // Try to extract telemetry data (if portnum suggests it)
            if (firstByte == 0x08 && encryptedLen > 1) {
                uint8_t portnum = decrypted[1];
                if (portnum == 0x03) {  // POSITION_APP
                    geoIntel.extractPositionFromDecrypted(decrypted, encryptedLen, nodeId);
                } else if (portnum == 0x08) {  // TELEMETRY_APP (device metrics)
                    // Scan for telemetry fields
                    int16_t parsedBattery = -1;
                    float parsedVoltage = 0.0f;
                    for (size_t j = 0; j < encryptedLen - 4; j++) {
                        // Battery level: field 1, varint (0x08 followed by value)
                        if (decrypted[j] == 0x08 && j + 1 < encryptedLen && j > 2) {
                            uint8_t batteryLevel = decrypted[j+1];
                            if (batteryLevel <= 101) {  // Valid battery percentage
                                Serial.printf("[PSK]    Battery: %d%%\n", batteryLevel);
                                parsedBattery = (int16_t)batteryLevel;
                            }
                        }
                        // Voltage: field 2, fixed32 (0x15 followed by 4-byte float)
                        if (decrypted[j] == 0x15 && j + 4 < encryptedLen) {
                            float voltage;
                            memcpy(&voltage, &decrypted[j+1], 4);
                            if (voltage > 0.0f && voltage < 10.0f) {  // Valid voltage range
                                Serial.printf("[PSK]   ~ Voltage: %.2fV\n", voltage);
                                parsedVoltage = voltage;
                            }
                        }
                        // Channel utilization: field 3, fixed32 (0x1d followed by 4-byte float)
                        if (decrypted[j] == 0x1d && j + 4 < encryptedLen) {
                            float chUtil;
                            memcpy(&chUtil, &decrypted[j+1], 4);
                            if (chUtil >= 0.0f && chUtil <= 100.0f) {
                                Serial.printf("[PSK]    Channel util: %.1f%%\n", chUtil);
                            }
                        }
                        // Air util TX: field 4, fixed32 (0x25 followed by 4-byte float)
                        if (decrypted[j] == 0x25 && j + 4 < encryptedLen) {
                            float airUtil;
                            memcpy(&airUtil, &decrypted[j+1], 4);
                            if (airUtil >= 0.0f && airUtil <= 100.0f) {
                                Serial.printf("[PSK]    Air util TX: %.1f%%\n", airUtil);
                            }
                        }
                    }
                    if (parsedBattery >= 0 || parsedVoltage > 0.0f) {
                        setLastBattery(parsedBattery, parsedVoltage);
                    }
                } else if (portnum == 0x43 || portnum == 0x04) {
                    // MAP_REPORT_APP (0x43): contains firmware_version (field 4) + hw_model (field 3)
                    // NODEINFO_APP  (0x04): contains hw_model (field 6) in User protobuf
                    //
                    // Data protobuf layout after decryption:
                    //   0x08 <portnum>    field 1 varint  (already consumed above)
                    //   0x12 <len> ...    field 2 bytes   inner payload starts here
                    if (encryptedLen > 4 && decrypted[2] == 0x12) {
                        uint32_t payloadLen = 0;
                        size_t varintBytes = 0;
                        if (decodeVarint(decrypted, encryptedLen, 3, payloadLen, varintBytes)) {
                            size_t payloadStart = 3 + varintBytes;
                            if (payloadStart + payloadLen <= encryptedLen && payloadLen > 0) {
                                char fwBuf[32] = {0};
                                uint32_t hwModel = 0;
                                if (portnum == 0x43) {
                                    parseMAPReport(decrypted + payloadStart, payloadLen,
                                                   fwBuf, sizeof(fwBuf), hwModel);
                                } else {
                                    parseNODEINFO(decrypted + payloadStart, payloadLen, hwModel);
                                }
                                const char* hwStr = hwModelToString(hwModel);
                                if (fwBuf[0] != '\0' || hwStr != nullptr) {
                                    setLastFirmware(fwBuf[0] ? fwBuf : nullptr, hwStr);
                                    Serial.printf("[PSK] Firmware: \"%s\", HW: \"%s\"\n",
                                                  fwBuf[0] ? fwBuf : "(none)",
                                                  hwStr ? hwStr : "(unknown)");
                                }
                            }
                        }
                    }
                }
            }
        }

        return true;
    }
    
    return false;
}

PSKStats PSKDecryption::getStats() {
    return pskStats;
}

void PSKDecryption::resetStats() {
    pskStats = PSKStats();
}

void PSKDecryption::printStats() {
    Serial.println("\n PSK STATISTICS:");
    Serial.printf("   Attempts: %d\n", pskStats.attempts);
    Serial.printf("   Successful: %d (%.1f%%)\n", 
                  pskStats.successes,
                  pskStats.attempts > 0 ? (float)pskStats.successes / pskStats.attempts * 100.0 : 0.0);
    
    Serial.println("   Key hits:");
    for (uint8_t i = 0; i < NUM_PSKS; i++) {
        if (pskStats.hitCount[i] > 0) {
            Serial.printf("     #%d: %d\n", i + 1, pskStats.hitCount[i]);
        }
    }
    Serial.println();
}
