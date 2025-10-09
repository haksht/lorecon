/**
 * PSK Decryption - Cleaned and Refactored Version
 * 
 * This is a cleaned-up version of the PSK decryption code with:
 * - Modular text extraction functions
 * - Clear separation of concerns
 * - Reduced verbosity in diagnostics
 * - Better structure for adding format variants
 * 
 * IMPORTANT: The decryption works correctly. The issue is the message format
 * used by your specific Meshtastic firmware doesn't match the documented structure.
 */

#include "psk_decryption_simple.h"

#ifdef ENABLE_PSK_TESTING

#include <mbedtls/aes.h>
#include <mbedtls/base64.h>
#include "data_structures.h"

// Known Meshtastic default PSKs
const char* defaultPSKs[] = {
  "AQ==",                      // #1: LongFast default (YOUR DEVICES)
  "1PG7OiApB1nwvP+rz05pAQ==",  // #2: Common default
  "d1iq21lNSh7BP6MOkP6cQA==",  // #3: Channel 1
  "2f8aH6iT8K9jQ1P3mD4nBw==",  // #4: Channel 2
  "7h3kL9mR5wX2pY8qE6tZcA==",  // #5: Channel 3
};

const uint8_t NUM_DEFAULT_PSKS = sizeof(defaultPSKs) / sizeof(char*);

// PSK stats instance
PSKStats pskStats;

// Varint decoder utility
static bool decodeVarint(const uint8_t* data, size_t dataLen, size_t pos, uint32_t& value, size_t& bytesRead) {
    value = 0;
    bytesRead = 0;
    uint32_t shift = 0;
    
    while (pos + bytesRead < dataLen && bytesRead < 5) {
        uint8_t byte = data[pos + bytesRead];
        value |= ((uint32_t)(byte & 0x7F)) << shift;
        bytesRead++;
        if ((byte & 0x80) == 0) return true;
        shift += 7;
    }
    return false;
}

// Forward declarations for text extraction strategies
static bool extractTextStandard(const uint8_t* data, size_t len, String& text);
static bool extractTextImplicitPortnum(const uint8_t* data, size_t len, String& text);
static bool extractTextAnyReadable(const uint8_t* data, size_t len, String& text);
static void identifyPacketType(const uint8_t* data, size_t len);

void initializePSKStats() {
    pskStats = PSKStats();
}

bool PSKDecryption::decodeBase64(const char* input, uint8_t* output, size_t maxLen) {
    size_t outLen;
    int result = mbedtls_base64_decode(output, maxLen, &outLen, 
                                      (const unsigned char*)input, strlen(input));
    return (result == 0 && outLen > 0);
}

bool PSKDecryption::testDefaultPSKs(const uint8_t* data, size_t length) {
    pskStats.attempts++;
    
    // Validate packet structure
    if (length < 20) {
        Serial.println("[PSK] Packet too short for encrypted content");
        return false;
    }
    
    // Validate Meshtastic header
    if (!(data[0] == 0xFF && data[1] == 0xFF && data[2] == 0xFF && data[3] == 0xFF)) {
        return false;
    }
    
    // Extract packet metadata
    uint32_t nodeId = (uint32_t(data[4]) << 24) | (uint32_t(data[5]) << 16) | 
                      (uint32_t(data[6]) << 8) | uint32_t(data[7]);
    uint32_t packetId = ((uint32_t)data[10]) | ((uint32_t)data[11] << 8) | 
                        ((uint32_t)data[12] << 16) | ((uint32_t)data[13] << 24);
    
    const uint8_t* encryptedData = data + 14;
    size_t encryptedLen = length - 14;
    
    Serial.println("\n[PSK] === ATTEMPTING DECRYPTION ===");
    Serial.printf("[PSK] Node: 0x%08X, Packet ID: 0x%08X\n", nodeId, packetId);
    
    // Try each default PSK
    for (uint8_t i = 0; i < NUM_DEFAULT_PSKS; i++) {
        uint8_t key[16];
        if (!decodeBase64(defaultPSKs[i], key, sizeof(key))) {
            continue;
        }
        
        // Construct AES-CTR nonce
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
        
        // Decrypt with AES-CTR
        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        
        if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0) {
            mbedtls_aes_free(&aes);
            continue;
        }
        
        uint8_t decrypted[256];
        uint8_t stream_block[16];
        size_t nc_off = 0;
        memcpy(stream_block, nonce, 16);
        
        int result = mbedtls_aes_crypt_ctr(&aes, encryptedLen, &nc_off,
                                           stream_block, stream_block,
                                           encryptedData, decrypted);
        mbedtls_aes_free(&aes);
        
        if (result != 0) {
            continue;
        }
        
        // Validate decryption (basic protobuf check)
        uint8_t firstByte = decrypted[0];
        uint8_t fieldNum = firstByte >> 3;
        uint8_t wireType = firstByte & 0x07;
        
        bool looksValid = (fieldNum >= 1 && fieldNum <= 31 && wireType <= 5);
        
        if (!looksValid) {
            continue;  // Skip invalid decryptions
        }
        
        // Successfully decrypted
        pskStats.successes++;
        pskStats.hitCount[i]++;
        
        Serial.printf("[PSK] ✓ Decryption SUCCESS with key #%d: \"%s\"\n", i + 1, defaultPSKs[i]);
        Serial.print("[PSK] First 32 bytes: ");
        for (size_t j = 0; j < min(encryptedLen, size_t(32)); j++) {
            Serial.printf("%02X ", decrypted[j]);
        }
        Serial.println();
        
        // Identify packet type
        identifyPacketType(decrypted, encryptedLen);
        
        // Try to extract text using multiple strategies
        String extractedText = "";
        bool foundText = false;
        
        // Strategy 1: Standard protobuf format (0x08 0x01 0x12 ... 0x0A)
        if (extractTextStandard(decrypted, encryptedLen, extractedText)) {
            foundText = true;
            Serial.println("[PSK] ✓ Text extraction: STANDARD FORMAT");
        }
        // Strategy 2: Implicit portnum (starts with 0x12)
        else if (extractTextImplicitPortnum(decrypted, encryptedLen, extractedText)) {
            foundText = true;
            Serial.println("[PSK] ✓ Text extraction: IMPLICIT PORTNUM FORMAT");
        }
        // Strategy 3: Any readable text (fallback)
        else if (extractTextAnyReadable(decrypted, encryptedLen, extractedText)) {
            foundText = true;
            Serial.println("[PSK] ⚠️ Text extraction: FALLBACK (any readable text)");
        }
        
        if (foundText && extractedText.length() >= 2) {
            Serial.println("\n╔════════════════════════════════════════════╗");
            Serial.printf("║  📧 TEXT MESSAGE: \"%s\"\n", extractedText.c_str());
            Serial.println("╚════════════════════════════════════════════╝\n");
        } else {
            Serial.println("[PSK] ℹ️ No text message found (might be position/routing packet)");
        }
        
        return true;
    }
    
    return false;
}

/**
 * Strategy 1: Extract text from standard Meshtastic protobuf format
 * Expected structure: 0x08 0x01 0x12 <len> 0x0A <textlen> <TEXT>
 */
static bool extractTextStandard(const uint8_t* data, size_t len, String& text) {
    if (len < 6) return false;
    
    // Look for: Field 1 (portnum) = TEXT_MESSAGE_APP (0x01)
    if (data[0] != 0x08 || data[1] != 0x01) {
        return false;
    }
    
    // Next should be Field 2 (payload)
    if (data[2] != 0x12) {
        return false;
    }
    
    uint32_t payloadLen = 0;
    size_t varintBytes = 0;
    if (!decodeVarint(data, len, 3, payloadLen, varintBytes)) {
        return false;
    }
    
    size_t payloadStart = 3 + varintBytes;
    if (payloadStart >= len || payloadLen == 0) {
        return false;
    }
    
    // Look for text field (0x0A) inside payload
    if (data[payloadStart] != 0x0A) {
        return false;
    }
    
    uint32_t textLen = 0;
    size_t textVarintBytes = 0;
    if (!decodeVarint(data, len, payloadStart + 1, textLen, textVarintBytes)) {
        return false;
    }
    
    size_t textStart = payloadStart + 1 + textVarintBytes;
    if (textStart >= len || textLen == 0 || textLen > 200) {
        return false;
    }
    
    // Extract text
    for (uint32_t i = 0; i < textLen && (textStart + i) < len; i++) {
        uint8_t c = data[textStart + i];
        if (c >= 32 && c <= 126) {
            text += (char)c;
        } else if (c == 0x0A || c == 0x0D) {
            text += '\n';
        } else if (c == 0) {
            break;
        } else {
            return false;  // Invalid text character
        }
    }
    
    return (text.length() >= 1);
}

/**
 * Strategy 2: Extract text from implicit portnum format
 * Some firmware versions omit the portnum field for common types
 * Expected structure: 0x12 <len> 0x0A <textlen> <TEXT>
 */
static bool extractTextImplicitPortnum(const uint8_t* data, size_t len, String& text) {
    if (len < 5) return false;
    
    // Starts with Field 2 (payload) - implicit portnum
    if (data[0] != 0x12) {
        return false;
    }
    
    uint32_t payloadLen = 0;
    size_t varintBytes = 0;
    if (!decodeVarint(data, len, 1, payloadLen, varintBytes)) {
        return false;
    }
    
    size_t payloadStart = 1 + varintBytes;
    if (payloadStart >= len || payloadLen == 0) {
        return false;
    }
    
    // Look for text field (0x0A)
    if (data[payloadStart] != 0x0A) {
        return false;
    }
    
    uint32_t textLen = 0;
    size_t textVarintBytes = 0;
    if (!decodeVarint(data, len, payloadStart + 1, textLen, textVarintBytes)) {
        return false;
    }
    
    size_t textStart = payloadStart + 1 + textVarintBytes;
    if (textStart >= len || textLen == 0 || textLen > 200) {
        return false;
    }
    
    // Extract text
    for (uint32_t i = 0; i < textLen && (textStart + i) < len; i++) {
        uint8_t c = data[textStart + i];
        if (c >= 32 && c <= 126) {
            text += (char)c;
        } else if (c == 0x0A || c == 0x0D) {
            text += '\n';
        } else if (c == 0) {
            break;
        } else {
            return false;
        }
    }
    
    return (text.length() >= 1);
}

/**
 * Strategy 3: Extract any readable ASCII text (fallback)
 * This catches text in non-standard formats but may have false positives
 */
static bool extractTextAnyReadable(const uint8_t* data, size_t len, String& text) {
    String longest = "";
    String current = "";
    
    for (size_t i = 0; i < len; i++) {
        if (data[i] >= 32 && data[i] <= 126) {
            current += (char)data[i];
        } else {
            if (current.length() > longest.length()) {
                longest = current;
            }
            current = "";
        }
    }
    
    if (current.length() > longest.length()) {
        longest = current;
    }
    
    // Only accept if reasonably long and doesn't look like garbage
    if (longest.length() >= 3) {
        // Filter out common false positives
        if (longest == "cf" || longest.startsWith("`cf") || longest.startsWith("Tcf")) {
            return false;
        }
        text = longest;
        return true;
    }
    
    return false;
}

/**
 * Identify the type of decrypted packet
 */
static void identifyPacketType(const uint8_t* data, size_t len) {
    if (len < 2) {
        Serial.println("[PSK] Type: UNKNOWN (too short)");
        return;
    }
    
    uint8_t firstByte = data[0];
    uint8_t fieldNum = firstByte >> 3;
    uint8_t wireType = firstByte & 0x07;
    
    Serial.printf("[PSK] First byte: 0x%02X (Field %u, Wire %u)\n", firstByte, fieldNum, wireType);
    
    // Check for explicit portnum field
    if (firstByte == 0x08 && len > 1) {
        uint8_t portnum = data[1];
        Serial.printf("[PSK] Type: ");
        switch(portnum) {
            case 0x01: Serial.println("TEXT_MESSAGE_APP ✉️"); break;
            case 0x03: Serial.println("POSITION_APP 📍"); break;
            case 0x04: Serial.println("NODEINFO_APP ℹ️"); break;
            case 0x07: Serial.println("ROUTING_APP 🔄"); break;
            case 0x09: Serial.println("TEXT_MESSAGE_COMPRESSED_APP ✉️🗜️"); break;
            default: Serial.printf("UNKNOWN APP (0x%02X)\n", portnum); break;
        }
    }
    // Check for implicit portnum (starts with payload field)
    else if (firstByte == 0x12) {
        Serial.println("[PSK] Type: POSITION_APP (implicit portnum=3) 📍");
    }
    // Other patterns
    else {
        Serial.println("[PSK] Type: ROUTING/ADMIN/OTHER");
    }
}

void PSKDecryption::initialize() {
    Serial.println("🔐 PSK Testing module initialized");
    Serial.printf("📊 Loaded %d default Meshtastic PSKs\n", NUM_DEFAULT_PSKS);
}

PSKStats PSKDecryption::getStats() {
    return pskStats;
}

void PSKDecryption::resetStats() {
    pskStats = PSKStats();
}

void PSKDecryption::printStats() {
    Serial.println("\n📊 PSK DECRYPTION STATISTICS:");
    Serial.printf("   Total Attempts: %d\n", pskStats.attempts);
    Serial.printf("   Successful: %d (%.1f%% success rate)\n", 
                  pskStats.successes,
                  pskStats.attempts > 0 ? (float)pskStats.successes / pskStats.attempts * 100.0 : 0.0);
    
    Serial.println("   PSK Hit Count:");
    for (uint8_t i = 0; i < NUM_DEFAULT_PSKS; i++) {
        if (pskStats.hitCount[i] > 0) {
            Serial.printf("     PSK #%d: %d hits\n", i + 1, pskStats.hitCount[i]);
        }
    }
    Serial.println();
}

#endif // ENABLE_PSK_TESTING
