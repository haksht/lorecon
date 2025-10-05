/**
 * ESP32 LoRa Reconnaissance Tool
 * 
 * A practical LoRa packet capture and analysis tool for security research
 * and RF experimentation. Clean, efficient implementation focusing on
 * core functionality.
 * 
 * Features:
 * - Sequential frequency scanning across ISM band
 * - Protocol identification and packet analysis
 * - Optional default PSK testing for Meshtastic networks
 * - Signal analysis and node tracking
 * - Local storage with JSON export
 * 
 * Hardware: ESP32-S3 + SX1262 LoRa radio (like Heltec WiFi LoRa 32 V3)
 * 
 * License: MIT
 */

#include "lora_recon_tool.h"

#ifdef ENABLE_PSK_TESTING
#include "psk_tests.h"
#endif

// Global tool instance
LoRaReconTool reconTool;

void setup() {
    if (!reconTool.initialize()) {
        while (1) delay(1000);  // Halt on initialization failure
    }
    
#ifdef ENABLE_PSK_TESTING
    Serial.println("\n🧪 Running PSK Decryption Tests...");
    PSKTests::runAll();
    delay(2000);  // Give time to read results
#endif
}

void loop() {
    reconTool.update();
}
