/**
 * PSK Decryption - Simplified and Clean Version
 * 
 * Tests default Meshtastic PSKs against captured packets.
 * Extracts text messages from successfully decrypted data.
 */

#include "psk_decryption_simple.h"

#ifdef ENABLE_PSK_TESTING

#include <mbedtls/aes.h>
#include <mbedtls/base64.h>
#include "geo_intelligence.h"
#include "text_packet_diagnostic.h"

// Protocol constants
#define MIN_PACKET_LENGTH           5    // Minimum protobuf packet size
#define MAX_TEXT_MESSAGE_LENGTH     200  // Meshtastic text message limit
#define ASCII_PRINTABLE_MIN         32   // First printable ASCII character
#define ASCII_PRINTABLE_MAX         126  // Last printable ASCII character
#define MAX_VARINT_BYTES            5    // Maximum bytes in a varint encoding

// Known Meshtastic default PSKs
static constexpr const char* DEFAULT_PSKS[] = {
    "AQ==",                      // #1: Single byte (0x01) - often expanded to 16 bytes
    "1PG7OiApB1nwvP+rz05pAQ==",  // #2: Standard 16-byte key (most common)
    "d1iq21lNSh7BP6MOkP6cQA==",  // #3: Channel variant 1
    "2f8aH6iT8K9jQ1P3mD4nBw==",  // #4: Channel variant 2
    "7h3kL9mR5wX2pY8qE6tZcA==",  // #5: Channel variant 3
    "/u7k03L8N3Q=",              // #6: Short key variant (8 bytes)
    "Ag==",                      // #7: Single byte (0x02)
    "Aw==",                      // #8: Single byte (0x03)
    "BA==",                      // #9: Single byte (0x04)
    "BQ==",                      // #10: Single byte (0x05)
    "AAAAAAAAAAAAAAAAAAAAAA==",  // #11: All zeros (16 bytes)
    "MTIzNDU2Nzg5MDEyMzQ1Ng==",  // #12: "1234567890123456" ASCII
    "dGVzdHRlc3R0ZXN0dGVzdA==",  // #13: "testtesttesttest" ASCII
    "bWVzaHRhc3RpY21lc2h0YXN0",  // #14: "meshtasticmeshtast" ASCII
};


static constexpr uint8_t NUM_PSKS = sizeof(DEFAULT_PSKS) / sizeof(char*);

PSKStats pskStats;

// ============================================================================
// Helper Functions
// ============================================================================

static bool decodeVarint(const uint8_t* data, size_t dataLen, size_t pos, 
                         uint32_t& value, size_t& bytesRead) {
    value = 0;
    bytesRead = 0;
    uint32_t shift = 0;
    
    while (pos + bytesRead < dataLen && bytesRead < MAX_VARINT_BYTES) {
        uint8_t byte = data[pos + bytesRead];
        value |= ((uint32_t)(byte & 0x7F)) << shift;
        bytesRead++;
        if ((byte & 0x80) == 0) return true;
        shift += 7;
    }
    return false;
}

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

void PSKDecryption::initialize() {
    pskStats = PSKStats();
    Serial.println("🔐 PSK Testing initialized");
    Serial.printf("📊 Testing %d default keys\n", NUM_PSKS);
}

int PSKDecryption::decodeBase64(const char* input, uint8_t* output, size_t maxLen) {
    size_t outLen = 0;
    int result = mbedtls_base64_decode(output, maxLen, &outLen,
                                       (const unsigned char*)input, strlen(input));
    return (result == 0 && outLen > 0) ? (int)outLen : 0;
}

bool PSKDecryption::testDefaultPSKs(const uint8_t* data, size_t length) {
    pskStats.attempts++;
    
    // Validate minimum packet structure
    if (length < 20) {
        Serial.printf("[PSK] Packet too small (%d bytes, need ≥20)\n", length);
        return false;
    }
    
    // Check for standard Meshtastic header
    bool hasHeader = (data[0] == 0xFF && data[1] == 0xFF && 
                      data[2] == 0xFF && data[3] == 0xFF);
    
    if (!hasHeader) {
        // Try to find header in packet (for routed packets)
        for (size_t i = 0; i < length - 4; i++) {
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
        Serial.println("[PSK] ⚠️  Packet appears unencrypted (0x08 + valid portnum detected)");
        Serial.printf("[PSK] First bytes: 0x%02X 0x%02X (field 1, portnum %d)\n", 
                     firstByte, secondByte, secondByte);
        
        // Try to extract text message
        String plaintext;
        if (extractMessage(encryptedData, encryptedLen, plaintext)) {
            Serial.println("\n╔════════════════════════════════════════════╗");
            Serial.printf("║  📧 PLAINTEXT MESSAGE: \"%s\"\n", plaintext.c_str());
            Serial.println("╚════════════════════════════════════════════╝\n");
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
            Serial.println("[PSK] 📍 Unencrypted position packet - extracting GPS...");
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
        if (keyLen != 16 && keyLen != 1) {
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
        } else {
            // Single byte: skip (key #2 in list has full key)
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
        if (!looksValid) {
            Serial.printf("[PSK] Key #%d: Doesn't look like typical protobuf start (0x%02X)\n", i + 1, firstByte);
            // Don't skip - still try to extract text in case it's a different format
        }
        
        // Success!
        pskStats.successes++;
        pskStats.hitCount[i]++;
        
        // Update diagnostic statistics for activity analysis
        TextPacketDiagnostic::analyzeDecryptedPacket(decrypted, encryptedLen, length);
        
        Serial.printf("\n[PSK] ✓ Decrypted with key #%d (\"%s\")\n", i + 1, DEFAULT_PSKS[i]);
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
            Serial.println("\n╔════════════════════════════════════════════╗");
            Serial.printf("║  📧 TEXT MESSAGE: \"%s\"\n", messageText.c_str());
            Serial.println("╚════════════════════════════════════════════╝\n");
        } else {
            // Try to extract telemetry data (if portnum suggests it)
            if (firstByte == 0x08 && encryptedLen > 1) {
                uint8_t portnum = decrypted[1];
                if (portnum == 0x03) {  // POSITION_APP
                    geoIntel.extractPositionFromDecrypted(decrypted, encryptedLen, nodeId);
                } else if (portnum == 0x08) {  // TELEMETRY_APP (device metrics)
                    // Scan for telemetry fields
                    for (size_t j = 0; j < encryptedLen - 4; j++) {
                        // Battery level: field 1, varint (0x08 followed by value)
                        if (decrypted[j] == 0x08 && j + 1 < encryptedLen && j > 2) {
                            uint8_t batteryLevel = decrypted[j+1];
                            if (batteryLevel <= 101) {  // Valid battery percentage
                                Serial.printf("[PSK]   🔋 Battery: %d%%\n", batteryLevel);
                            }
                        }
                        // Voltage: field 2, fixed32 (0x15 followed by 4-byte float)
                        if (decrypted[j] == 0x15 && j + 4 < encryptedLen) {
                            float voltage;
                            memcpy(&voltage, &decrypted[j+1], 4);
                            if (voltage > 0.0f && voltage < 10.0f) {  // Valid voltage range
                                Serial.printf("[PSK]   ⚡ Voltage: %.2fV\n", voltage);
                            }
                        }
                        // Channel utilization: field 3, fixed32 (0x1d followed by 4-byte float)
                        if (decrypted[j] == 0x1d && j + 4 < encryptedLen) {
                            float chUtil;
                            memcpy(&chUtil, &decrypted[j+1], 4);
                            if (chUtil >= 0.0f && chUtil <= 100.0f) {
                                Serial.printf("[PSK]   📡 Channel util: %.1f%%\n", chUtil);
                            }
                        }
                        // Air util TX: field 4, fixed32 (0x25 followed by 4-byte float)
                        if (decrypted[j] == 0x25 && j + 4 < encryptedLen) {
                            float airUtil;
                            memcpy(&airUtil, &decrypted[j+1], 4);
                            if (airUtil >= 0.0f && airUtil <= 100.0f) {
                                Serial.printf("[PSK]   📻 Air util TX: %.1f%%\n", airUtil);
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
    Serial.println("\n📊 PSK STATISTICS:");
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

#endif // ENABLE_PSK_TESTING
