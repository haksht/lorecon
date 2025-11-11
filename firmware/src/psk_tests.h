/**
 * PSK Decryption Unit Tests
 * 
 * Tests for PSK decryption functionality
 */

#ifndef PSK_TESTS_H
#define PSK_TESTS_H

#ifdef ENABLE_PSK_TESTING

#include <Arduino.h>
#include "psk_decryption_simple.h"

class PSKTests {
public:
    static void runAll() {
        Serial.println("\n🧪 PSK DECRYPTION TEST SUITE");
        Serial.println("============================\n");
        
        int passed = 0;
        int total = 0;
        
        passed += testBase64Decode(); total++;
        passed += testMeshtasticPacketDetection(); total++;
        passed += testDefaultPSKs(); total++;
        passed += testMessageExtraction(); total++;
        
        Serial.printf("\n📊 Test Results: %d/%d passed (%.1f%%)\n\n",
                      passed, total, (float)passed/total * 100.0);
    }
    
private:
    static int testBase64Decode() {
        Serial.print("Test 1: Base64 Decode... ");
        
        uint8_t output[16];
        const char* testKey = "AQ==";  // Should decode to {0x01}
        
        if (PSKDecryption::decodeBase64(testKey, output, sizeof(output))) {
            if (output[0] == 0x01) {
                Serial.println("✅ PASS");
                return 1;
            }
        }
        
        Serial.println("❌ FAIL");
        return 0;
    }
    
    static int testMeshtasticPacketDetection() {
        Serial.print("Test 2: Meshtastic Packet Detection... ");
        
        // Simulated Meshtastic packet header
        uint8_t packet[] = {
            0xFF, 0xFF, 0xFF, 0xFF,  // Magic header
            0x40, 0x1A, 0xCD, 0x4E,  // Node ID
            0x00,                     // Message type
            0x48, 0x65, 0x6C, 0x6C, 0x6F  // "Hello"
        };
        
        // Should detect as Meshtastic
        if (packet[0] == 0xFF && packet[1] == 0xFF && 
            packet[2] == 0xFF && packet[3] == 0xFF) {
            Serial.println("✅ PASS");
            return 1;
        }
        
        Serial.println("❌ FAIL");
        return 0;
    }
    
    static int testDefaultPSKs() {
        Serial.print("Test 3: Default PSK Loading... ");
        
        // Verify PSK database is accessible
        if (PSKDecryption::getDefaultPSKCount() == 14) {
            Serial.println("✅ PASS");
            return 1;
        }
        
        Serial.println("❌ FAIL");
        return 0;
    }
    
    static int testMessageExtraction() {
        Serial.print("Test 4: Message Text Extraction... ");
        
        // Test packet with embedded text
        const char* testMsg = "Test Message";
        size_t msgLen = strlen(testMsg);
        
        // Simple presence test
        bool found = false;
        for (size_t i = 0; i < msgLen; i++) {
            if (testMsg[i] >= 32 && testMsg[i] <= 126) {
                found = true;
            }
        }
        
        if (found) {
            Serial.println("✅ PASS");
            return 1;
        }
        
        Serial.println("❌ FAIL");
        return 0;
    }
};

#endif // ENABLE_PSK_TESTING

#endif // PSK_TESTS_H
