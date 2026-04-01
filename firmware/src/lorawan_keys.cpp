/**
 * LoRaWAN Key Testing Module Implementation
 * 
 * Tests default/common AppKeys against captured Join Requests using AES-CMAC
 * to verify the Message Integrity Code (MIC).
 * 
 * LoRaWAN Join Request format (23 bytes):
 *   [0]      MHDR (MType=000 for Join Request)
 *   [1-8]    AppEUI (little-endian)
 *   [9-16]   DevEUI (little-endian)
 *   [17-18]  DevNonce (little-endian)
 *   [19-22]  MIC (first 4 bytes of AES-CMAC)
 * 
 * MIC calculation: AES-CMAC(AppKey, MHDR|AppEUI|DevEUI|DevNonce)
 */

#include "lorawan_keys.h"
#include <Arduino.h>
#include <mbedtls/aes.h>
#include <cstring>

namespace LoRaWANKeys {

// Statistics
static LoRaWANStats stats;

// Known/common AppKeys with descriptions
// These are test keys, manufacturer defaults, and tutorial examples
struct DefaultKey {
    const char* name;
    uint8_t key[16];
};

static const DefaultKey DEFAULT_KEYS[] = {
    // Test/Development keys (commonly left in production)
    {"All Zeros (test)", 
     {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    
    {"All Ones (test)",
     {0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11}},
    
    {"All 0xAA (test)",
     {0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA}},
    
    {"Sequential 01-16 (tutorial)",
     {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x10,0x11,0x12,0x13,0x14,0x15,0x16}},
    
    {"AES Test Vector",
     {0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3C}},
    
    // TTN example keys (from documentation/tutorials)
    {"TTN Example 1",
     {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01}},
    
    {"TTN Quickstart",
     {0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01}},
    
    // Manufacturer defaults (generic sensors)
    {"Dragino Default",
     {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF}},
    
    {"RAK Default",
     {0x60,0x81,0xF9,0x1B,0xE3,0x9B,0x69,0xBC,0x36,0x3D,0x3D,0x04,0x0F,0x46,0x92,0x94}},
    
    {"Heltec Default",
     {0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88}},
    
    // Helium/Sensor defaults
    {"Helium Test",
     {0xDE,0xAD,0xBE,0xEF,0xDE,0xAD,0xBE,0xEF,0xDE,0xAD,0xBE,0xEF,0xDE,0xAD,0xBE,0xEF}},
    
    {"LHT65 Default",
     {0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3C}},
    
    // Chirpstack/LoRaServer examples
    {"Chirpstack Example",
     {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08}},
    
    {"LoRaServer Demo",
     {0xAB,0xCD,0xEF,0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,0x01,0x23,0x45,0x67,0x89}},
    
    // Common default keys from various platforms
    {"AWS IoT Default",
     {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    
    {"Parking Sensor Default",
     {0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0,0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0}},
};

static const uint8_t NUM_KEYS = sizeof(DEFAULT_KEYS) / sizeof(DEFAULT_KEYS[0]);

/**
 * XOR two 16-byte blocks: out = a XOR b
 */
static void xorBlock(uint8_t* out, const uint8_t* a, const uint8_t* b) {
    for (int i = 0; i < 16; i++) {
        out[i] = a[i] ^ b[i];
    }
}

/**
 * Left-shift a 16-byte block by 1 bit
 */
static void shiftLeft(uint8_t* block) {
    uint8_t overflow = 0;
    for (int i = 15; i >= 0; i--) {
        uint8_t next_overflow = (block[i] & 0x80) ? 1 : 0;
        block[i] = (block[i] << 1) | overflow;
        overflow = next_overflow;
    }
}

/**
 * Generate AES-CMAC subkeys K1 and K2
 */
static void generateSubkeys(const uint8_t* key, uint8_t* K1, uint8_t* K2) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 128);
    
    // L = AES(K, 0^128)
    uint8_t L[16] = {0};
    uint8_t zeros[16] = {0};
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, zeros, L);
    
    // K1 = L << 1
    memcpy(K1, L, 16);
    shiftLeft(K1);
    if (L[0] & 0x80) {
        K1[15] ^= 0x87;  // XOR with Rb (polynomial for GF(2^128))
    }
    
    // K2 = K1 << 1
    memcpy(K2, K1, 16);
    shiftLeft(K2);
    if (K1[0] & 0x80) {
        K2[15] ^= 0x87;
    }
    
    mbedtls_aes_free(&aes);
}

/**
 * Calculate AES-CMAC for MIC verification
 * Returns first 4 bytes of CMAC as MIC
 * 
 * Based on RFC 4493 AES-CMAC algorithm
 */
static bool calculateMIC(const uint8_t* key, const uint8_t* data, size_t length, uint8_t* mic) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 128);
    
    // Generate subkeys
    uint8_t K1[16], K2[16];
    generateSubkeys(key, K1, K2);
    
    // Number of blocks
    size_t n = (length + 15) / 16;
    if (n == 0) n = 1;
    
    bool lastBlockComplete = (length > 0) && (length % 16 == 0);
    
    // Prepare last block
    uint8_t lastBlock[16] = {0};
    size_t lastBlockStart = (n - 1) * 16;
    size_t lastBlockLen = length - lastBlockStart;
    
    if (lastBlockLen > 0) {
        memcpy(lastBlock, data + lastBlockStart, lastBlockLen);
    }
    
    if (lastBlockComplete) {
        // XOR with K1
        xorBlock(lastBlock, lastBlock, K1);
    } else {
        // Pad with 10*: append 0x80 then zeros
        lastBlock[lastBlockLen] = 0x80;
        // XOR with K2
        xorBlock(lastBlock, lastBlock, K2);
    }
    
    // CBC-MAC calculation
    uint8_t X[16] = {0};  // Previous block (starts with zero)
    uint8_t Y[16];
    
    // Process all blocks except last
    for (size_t i = 0; i < n - 1; i++) {
        xorBlock(Y, X, data + (i * 16));
        mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, Y, X);
    }
    
    // Process last block
    xorBlock(Y, X, lastBlock);
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, Y, X);
    
    // MIC is first 4 bytes of CMAC
    memcpy(mic, X, 4);
    
    mbedtls_aes_free(&aes);
    return true;
}

JoinRequest parseJoinRequest(const uint8_t* data, size_t length) {
    JoinRequest jr;
    
    // Join Request is exactly 23 bytes
    if (length != 23) {
        return jr;
    }
    
    // Check MHDR - MType should be 000 (Join Request)
    uint8_t mtype = (data[0] >> 5) & 0x07;
    if (mtype != 0x00) {
        return jr;
    }
    
    stats.joinRequestsSeen++;
    
    // Parse fields (all little-endian in LoRaWAN)
    memcpy(jr.appEUI, &data[1], 8);   // AppEUI at bytes 1-8
    memcpy(jr.devEUI, &data[9], 8);   // DevEUI at bytes 9-16
    jr.devNonce = data[17] | (data[18] << 8);  // DevNonce at bytes 17-18
    memcpy(jr.mic, &data[19], 4);     // MIC at bytes 19-22
    
    jr.valid = true;
    stats.joinRequestsDecoded++;
    
    return jr;
}

KeyTestResult testDefaultKeys(const JoinRequest& jr, const uint8_t* data, size_t length) {
    KeyTestResult result;
    
    if (!jr.valid || length != 23) {
        return result;
    }
    
    // Data for MIC calculation is MHDR|AppEUI|DevEUI|DevNonce (19 bytes)
    // (excludes the MIC itself)
    const size_t micDataLen = 19;
    
    uint8_t calculatedMIC[4];
    
    for (uint8_t i = 0; i < NUM_KEYS; i++) {
        if (calculateMIC(DEFAULT_KEYS[i].key, data, micDataLen, calculatedMIC)) {
            // Compare with packet MIC
            if (memcmp(calculatedMIC, jr.mic, 4) == 0) {
                // Key found!
                result.success = true;
                result.keyIndex = i;
                result.keyName = DEFAULT_KEYS[i].name;
                memcpy(result.appKey, DEFAULT_KEYS[i].key, 16);
                
                stats.keysFound++;
                if (i < 16) {
                    stats.keyHits[i]++;
                }
                
                return result;
            }
        }
    }
    
    return result;
}

bool analyzePacket(const uint8_t* data, size_t length) {
    // Quick check for LoRaWAN packet
    if (length < 12) return false;
    
    uint8_t mtype = (data[0] >> 5) & 0x07;
    
    // Only process Join Requests (MType = 0)
    if (mtype != 0x00) {
        return false;
    }
    
    // Parse Join Request
    JoinRequest jr = parseJoinRequest(data, length);
    if (!jr.valid) {
        return false;
    }
    
    // Format EUIs for logging
    char appEUIStr[24], devEUIStr[24];
    formatEUI(jr.appEUI, appEUIStr);
    formatEUI(jr.devEUI, devEUIStr);
    
    Serial.println("\n+==========================================================+");
    Serial.println("|   LoRaWAN JOIN REQUEST CAPTURED                        |");
    Serial.println("+==========================================================+");
    Serial.printf("|  AppEUI:   %s                  |\n", appEUIStr);
    Serial.printf("|  DevEUI:   %s                  |\n", devEUIStr);
    Serial.printf("|  DevNonce: 0x%04X                                        |\n", jr.devNonce);
    Serial.printf("|  MIC:      %02X:%02X:%02X:%02X                                    |\n",
                  jr.mic[0], jr.mic[1], jr.mic[2], jr.mic[3]);
    
    // Test default keys
    KeyTestResult keyResult = testDefaultKeys(jr, data, length);
    
    if (keyResult.success) {
        Serial.println("+==========================================================+");
        Serial.println("|   DEFAULT KEY FOUND!                                   |");
        Serial.printf("|  Key: %s", keyResult.keyName);
        // Pad to align
        size_t nameLen = strlen(keyResult.keyName);
        for (size_t i = nameLen; i < 40; i++) Serial.print(" ");
        Serial.println("|");
        Serial.print("|  AppKey: ");
        for (int i = 0; i < 16; i++) {
            Serial.printf("%02X", keyResult.appKey[i]);
        }
        Serial.println("    |");
        Serial.println("|                                                          |");
        Serial.println("|  [!]  SECURITY VULNERABILITY: Device using default key!   |");
        Serial.println("+==========================================================+\n");
        return true;
    } else {
        Serial.println("|                                                          |");
        Serial.printf("|   Tested %d default keys - no match                    |\n", NUM_KEYS);
        Serial.println("|     (Device appears to use custom AppKey)                |");
        Serial.println("+==========================================================+\n");
        return true;  // Still successfully analyzed
    }
}

const LoRaWANStats& getStats() {
    return stats;
}

uint8_t getKeyCount() {
    return NUM_KEYS;
}

void printSummary() {
    Serial.println("\n=== LoRaWAN Key Testing Statistics ===");
    Serial.printf("Join Requests seen:    %d\n", stats.joinRequestsSeen);
    Serial.printf("Join Requests decoded: %d\n", stats.joinRequestsDecoded);
    Serial.printf("Default keys found:    %d\n", stats.keysFound);
    Serial.printf("Keys tested per packet: %d\n", NUM_KEYS);
    
    if (stats.keysFound > 0) {
        Serial.println("\nKey hits:");
        for (uint8_t i = 0; i < NUM_KEYS && i < 16; i++) {
            if (stats.keyHits[i] > 0) {
                Serial.printf("  %s: %d\n", DEFAULT_KEYS[i].name, stats.keyHits[i]);
            }
        }
    }
    Serial.println("=====================================\n");
}

void formatEUI(const uint8_t* eui, char* buffer) {
    // Format as XX:XX:XX:XX:XX:XX:XX:XX (MSB first for readability)
    // LoRaWAN EUIs are stored little-endian, so reverse for display
    snprintf(buffer, 24, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
             eui[7], eui[6], eui[5], eui[4], eui[3], eui[2], eui[1], eui[0]);
}

} // namespace LoRaWANKeys
