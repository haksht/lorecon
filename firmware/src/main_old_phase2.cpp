/**
 * ESP32 LoRa Reconnaissance Tool
 * 
 * A practical LoRa packet capture and analysis tool for security research
 * and RF experimentation. Clean, efficient implementation focusing on
 * core functionality rather than marketing fluff.
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
 * Author: [Your Name]
 * License: MIT
 */

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <atomic>
#include <esp_task_wdt.h>  // Watchdog timer for stability

// Optional PSK testing - enable to test against known default keys
// Note: Also defined in platformio.ini build_flags

#ifdef ENABLE_PSK_TESTING
#include <mbedtls/aes.h>
#include <mbedtls/base64.h>
#include "psk_decryption_simple.h"
#endif

// Optional hardware stress testing
#ifdef ENABLE_STRESS_TESTING
#include "hardware_stress_tester.h"
#endif

// Shared data structures
#include "data_structures.h"
// User Interface Module
#include "user_interface.h"
// State management
#include "recon_state.h"
// Protocol analysis
#include "protocol_analyzer.h"

// Hardware configuration for Heltec WiFi LoRa 32 V3
#define LORA_NSS    8
#define LORA_DIO1   14
#define LORA_RST    12
#define LORA_BUSY   13
#define SCK         9
#define MISO        11
#define MOSI        10

// Function declarations
void handleUserInput(char cmd);
void startTargetedCapture(uint8_t deviceIndex);
void handleReconnaissanceMode(uint32_t now);
void handleTargetedCaptureMode(uint32_t now);
void handlePacketReception();
void handleReconActivityMonitoring(uint32_t now);
void initializeStressTesting();
void logPacket(const uint8_t* data, size_t length, float rssi, float snr, const char* protocol);
void showReplayMenu();
void replayPacket(uint8_t slotIndex);

#ifdef ENABLE_STRESS_TESTING
HardwareStressTester* stressTester = nullptr;
#endif

// Scanning configuration
#define SCAN_DWELL_TIME     12000  // 12 seconds per frequency
#define MAX_TRACKED_NODES   20
#define PACKET_BUFFER_SIZE  256

// Radio instance
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

// Protocol analyzer instance
ProtocolAnalyzer protocolAnalyzer;

// Frequency scan configurations (struct defined in data_structures.h)

// All state is now managed by ReconState class
// No more global arrays - cleaner architecture!



// Interrupt handler for packet reception - atomic for thread safety
std::atomic<bool> packetReceived{false};
void IRAM_ATTR onPacketReceived() {
  packetReceived.store(true, std::memory_order_release);
  // Note: Can't use Serial.print in IRAM interrupt handler
}

// Apply radio configuration for scanning
bool applyConfig(uint8_t configIndex) {
  if (!reconState.isValidConfigIndex(configIndex)) return false;
  
  const ScanConfig& cfg = reconState.getScanConfig(configIndex);
  
  Serial.printf("[SCAN] %s (%.3f MHz, SF%d, BW%.0f, SW:0x%02X)\n", 
                cfg.protocol, cfg.frequency, cfg.spreadingFactor, 
                cfg.bandwidth, cfg.syncWord);
  
  // Apply configuration with error checking
  int state = radio.setFrequency(cfg.frequency);
  if (state != RADIOLIB_ERR_NONE) return false;
  
  state = radio.setBandwidth(cfg.bandwidth);
  if (state != RADIOLIB_ERR_NONE) return false;
  
  state = radio.setSpreadingFactor(cfg.spreadingFactor);
  if (state != RADIOLIB_ERR_NONE) return false;
  
  state = radio.setSyncWord(cfg.syncWord);
  if (state != RADIOLIB_ERR_NONE) return false;
  
  // Try different parameters based on protocol type
  if (strstr(cfg.protocol, "Meshtastic_MSG") != nullptr) {
    radio.setCodingRate(5);         // 4/5 for message content
    radio.setPreambleLength(8);     // Standard for messages
    radio.setCRC(false);            // Promiscuous mode for better capture
  } else if (strstr(cfg.protocol, "Meshtastic_MF") != nullptr) {
    radio.setCodingRate(8);         // 4/8 for MediumFast
    radio.setPreambleLength(12);    // Longer preamble
    radio.setCRC(false);            // Promiscuous mode
  } else {
    radio.setCodingRate(5);         // 4/5 for routing packets
    radio.setPreambleLength(8);     // Standard
    radio.setCRC(false);            // Promiscuous mode
  }
  radio.explicitHeader();
  
  // Receive-only mode (no transmission)
  radio.setCurrentLimit(100);
  radio.setOutputPower(0);
  
  return true;
}

// Protocol identification based on packet analysis
// Update node tracking database
void trackNode(uint32_t nodeId, const char* protocol, float rssi) {
  reconState.updateNode(nodeId, protocol, rssi);
}

// Option C: Separated activity monitoring and device tracking
void trackRFActivity(uint8_t configIndex, float rssi) {
  reconState.updateRFActivity(configIndex, rssi);
}

void trackTargetableDevice(uint32_t nodeId, uint8_t configIndex, float rssi, const char* protocol, const uint8_t* packetData = nullptr, size_t packetLength = 0) {
  reconState.addTargetableDevice(nodeId, configIndex, rssi, protocol, packetData, packetLength);
}

// UI functions moved to user_interface.cpp

// showActivityDetails() moved to user_interface.cpp

// showReconResults() moved to user_interface.cpp

#ifdef ENABLE_PSK_TESTING
// PSK testing wrapper function - implementation moved to psk_decryption_simple.cpp
bool testPacketWithPSK(const uint8_t* data, size_t length) {
  return PSKDecryption::testDefaultPSKs(data, length);
}
#endif

// Save packet to JSON log
void logPacket(const uint8_t* data, size_t length, float rssi, float snr, const char* protocol) {
  if (!LittleFS.begin()) return;
  
  File logFile = LittleFS.open("/packets.jsonl", "a");
  if (!logFile) return;
  
  JsonDocument doc;
  doc["timestamp"] = millis();
  doc["config"] = reconState.getScanConfig(reconState.scanState.currentConfig).protocol;
  doc["frequency"] = reconState.getScanConfig(reconState.scanState.currentConfig).frequency;
  doc["protocol"] = protocol;
  doc["length"] = length;
  doc["rssi"] = rssi;
  doc["snr"] = snr;
  
  // Store packet data as hex string
  String hexData = "";
  for (size_t i = 0; i < min(length, size_t(64)); i++) {
    char hex[3];
    sprintf(hex, "%02X", data[i]);
    hexData += hex;
  }
  doc["data"] = hexData;
  
  serializeJson(doc, logFile);
  logFile.println();
  logFile.close();
}

// printStats() moved to user_interface.cpp

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  // Initialize watchdog timer for security tool stability (30 second timeout)
  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL);
  Serial.println("[SECURITY] Watchdog timer enabled (30s timeout)");
  
  displayWelcomeMessage();
  
  // Initialize storage
  if (LittleFS.begin()) {
    Serial.println("[FS] Storage ready");
  } else {
    Serial.println("[FS] Storage init failed");
  }
  
  // Initialize SPI and radio
  SPI.begin(SCK, MISO, MOSI, LORA_NSS);
  
  Serial.print("[RADIO] Initializing SX1262... ");
  int state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("FAILED (%d)\n", state);
    while (1) delay(1000);
  }
  Serial.println("OK");
  
  // Set interrupt handler with explicit pin configuration
  pinMode(LORA_DIO1, INPUT);  // Ensure DIO1 is input
  radio.setDio1Action(onPacketReceived);
  Serial.printf("[RADIO] DIO1 interrupt configured on GPIO %d\n", LORA_DIO1);
  
  // Initialize reconnaissance system with ReconState
  reconState.initialize();
  
  displayReconStartMessage();
  
  // Apply initial configuration and start receiving
  if (applyConfig(reconState.scanState.currentConfig)) {
    radio.startReceive();
    Serial.println("[RECON] Started\n");
  } else {
    Serial.println("[ERROR] Failed to apply initial config");
    while (1) delay(1000);
  }
  
  // Initialize optional modules
  initializeStressTesting();
}

// Handle user input commands
void handleUserInput(char cmd) {
  if (cmd == 'm' || cmd == 'M') {
    reconState.scanState.mode = MODE_INTERACTIVE_MENU;
    showReconResults();
    return;
  }
  
#ifdef ENABLE_STRESS_TESTING
  if (cmd == 't' || cmd == 'T') {
    showStressTestMenu();
    return;
  }
#endif

  if (cmd == 'f' || cmd == 'F') {
    showFrequencyTargetingMenu();
    handleFrequencyTargetingInput();
    return;
  }
  
  // Quick SF8 targeting (for encrypted user messages)
  if (cmd == '8') {
    Serial.println("\n🎯 TARGETING SF8 FREQUENCY FOR ENCRYPTED MESSAGES");
    Serial.println("Switching to Meshtastic_902_SF8 (902.125 MHz, SF8, BW250)");
    
    reconState.scanState.mode = MODE_TARGETED_CAPTURE;
    reconState.scanState.targetConfig = 6;  // Meshtastic_902_SF8
    reconState.scanState.currentConfig = 6;
    
    Serial.println("Configuration:");
    const ScanConfig& cfg = reconState.getScanConfig(6);
    Serial.printf("  Frequency: %.3f MHz\n", cfg.frequency);
    Serial.printf("  Spreading Factor: SF%d\n", cfg.spreadingFactor);
    Serial.printf("  Bandwidth: %.0f kHz\n", cfg.bandwidth);
    Serial.printf("  Sync Word: 0x%02X\n", cfg.syncWord);
    Serial.println("\nThis configuration should capture encrypted user messages!");
    Serial.println("Press 'm' for menu, 'r' to restart recon");
    Serial.println("Monitoring for encrypted packets...\n");
    
    applyConfig(6);
    radio.startReceive();
    return;
  }
  
  if (cmd == 'd' || cmd == 'D') {
    showDeviceTypeSummary();
    Serial.print("\nPress any key to continue: ");
    return;
  }
  
  if (cmd == 'a' || cmd == 'A') {
    showActivityDetails();
    Serial.print("\nPress any key to continue: ");
    return;
  }
  
  if (cmd == 'p' || cmd == 'P') {
    showReplayMenu();
    return;
  }
  
  if (cmd == 'r' || cmd == 'R') {
    // Restart reconnaissance - clear both activity and device tracking
    reconState.reset();
    
    Serial.println("\n=== RESTARTING RECONNAISSANCE ===");
    Serial.println("Cleared activity history and device list.");
    applyConfig(reconState.scanState.currentConfig);
    radio.startReceive();
    return;
  }
  
  if (cmd == 's' || cmd == 'S') {
    showReconResults();
    return;
  }
  
  if (cmd == 'c' || cmd == 'C') {
    // Capture last packet for replay
    if (reconState.scanState.lastPacket.length() > 0 && reconState.scanState.mode == MODE_TARGETED_CAPTURE) {
      const uint8_t* data = (const uint8_t*)reconState.scanState.lastPacket.c_str();
      size_t length = reconState.scanState.lastPacket.length();
      float rssi = radio.getRSSI();
      
      // Analyze packet
      PacketInfo info = protocolAnalyzer.analyze(data, length, rssi);
      
      if (reconState.capturePacketForReplay(data, length, reconState.scanState.currentConfig, rssi, info.protocol)) {
        Serial.println("✅ Packet saved to replay slot!");
      } else {
        Serial.println("❌ Failed to save packet (slots full?)");
      }
    } else {
      Serial.println("❌ No packet available to capture (must be in targeted mode)");
    }
    return;
  }
  
  // Device selection (1-9)
  if (cmd >= '1' && cmd <= '9') {
    uint8_t deviceIndex = cmd - '1';
    if (deviceIndex < reconState.numTargetableDevices) {
      startTargetedCapture(deviceIndex);
    } else {
      Serial.println("Invalid device number. Try again.");
      showReconResults();
    }
    return;
  }
}

// Start targeted capture mode on specific device
void startTargetedCapture(uint8_t deviceIndex) {
  const TargetableDevice& target = reconState.getTargetableDevice(deviceIndex);
  
  reconState.scanState.mode = MODE_TARGETED_CAPTURE;
  reconState.scanState.targetConfig = target.configIndex;
  reconState.scanState.currentConfig = target.configIndex;
  
  Serial.println("\n" + String('=').substring(0,60));
  Serial.println("          TARGETED CAPTURE MODE ACTIVE");
  Serial.println(String('=').substring(0,60));
  Serial.printf("Target: Device #%d\n", deviceIndex + 1);
  Serial.printf("Node ID: 0x%08X\n", target.nodeId);
  Serial.printf("Type: %s\n", target.deviceType);
  Serial.printf("Config: %s\n", reconState.getScanConfig(target.configIndex).protocol);
  Serial.printf("Frequency: %.3f MHz\n", reconState.getScanConfig(target.configIndex).frequency);
  Serial.printf("Best RSSI: %.1f dBm\n", target.bestRSSI);
  Serial.printf("Packets Seen: %d\n", target.packetCount);
  Serial.println("\nPress 'm' to return to menu, 'r' to restart recon");
  Serial.println("Monitoring for packets...\n");
  
  applyConfig(reconState.scanState.targetConfig);
  radio.startReceive();
}

// Handle reconnaissance mode operations
void handleReconnaissanceMode(uint32_t now) {
  // Handle frequency switching in recon mode
  if (now - reconState.scanState.lastScanSwitch >= SCAN_DWELL_TIME) {
    reconState.scanState.currentConfig = (reconState.scanState.currentConfig + 1) % reconState.getNumConfigs();
    reconState.scanState.lastScanSwitch = now;
    
    // Show progress every full cycle (when back to config 0)
    if (reconState.scanState.currentConfig == 0) {
      uint32_t elapsed = (now - reconState.scanState.reconStartTime) / 1000;
      Serial.printf("[RECON] Cycle complete - %lu seconds elapsed, %d targetable devices found\n", 
                    elapsed, reconState.numTargetableDevices);
    }
    
    if (applyConfig(reconState.scanState.currentConfig)) {
      radio.startReceive();
    }
  }
  
  // Handle packets in recon mode
  handlePacketReception();
  
  // Enhanced activity monitoring for recon
  handleReconActivityMonitoring(now);
}

// Handle targeted capture mode operations  
void handleTargetedCaptureMode(uint32_t now) {
  // Handle packets - simplified like working simple version
  handlePacketReception();
}

// Unified packet reception handler - SIMPLIFIED LIKE WORKING SIMPLE VERSION
void handlePacketReception() {
  // Simple interrupt check - NO rate limiting that drops packets!
  if (!packetReceived.load(std::memory_order_acquire)) return;
  
  packetReceived.store(false, std::memory_order_release);
  
  String packet;
  int state = radio.readData(packet);
  
  if (state == RADIOLIB_ERR_NONE && packet.length() > 0) {
    // Store for potential replay capture
    reconState.scanState.lastPacket = packet;
    
    reconState.scanState.totalPackets++;
    
    float rssi = radio.getRSSI();
    float snr = radio.getSNR();
    
    const uint8_t* data = (const uint8_t*)packet.c_str();
    size_t length = packet.length();
    
    // Analyze packet using ProtocolAnalyzer
    PacketInfo info = protocolAnalyzer.analyze(data, length, rssi);
    
    // Enhanced packet analysis for Meshtastic
    if (strcmp(info.protocol, "Meshtastic") == 0) {
      Serial.printf("[PACKET] Meshtastic structure analysis:\n");
      Serial.printf("  - Length: %d bytes\n", length);
      Serial.printf("  - Node ID: 0x%08X\n", info.nodeId);
      if (length > 8) {
        Serial.printf("  - Payload type: 0x%02X\n", data[8]);
        if (length > 9) {
          Serial.printf("  - Flags/Seq: 0x%02X\n", data[9]);
        }
      }
    }
    
    if (reconState.scanState.mode == MODE_RECONNAISSANCE) {
      Serial.printf("[RECON] Packet #%d: %s, %d bytes, %.1f dBm, %.1f dB SNR\n",
                    reconState.scanState.totalPackets, info.protocol, length, rssi, snr);
      
      // Track RF activity (for situational awareness)
      trackRFActivity(reconState.scanState.currentConfig, rssi);
      
      // Track as targetable device only if we have a real node ID
      if (info.nodeId != 0) {
        trackTargetableDevice(info.nodeId, reconState.scanState.currentConfig, rssi, info.protocol, data, length);
      }
    
    } else if (reconState.scanState.mode == MODE_TARGETED_CAPTURE) {
      Serial.printf("[CAPTURE] Packet #%d: %s, %d bytes, %.1f dBm, %.1f dB SNR\n",
                    reconState.scanState.totalPackets, info.protocol, length, rssi, snr);
      Serial.println("         Press 'c' to capture this packet for replay");
      
      // Enhanced SF8 analysis for encrypted message detection
      if (reconState.scanState.currentConfig == 6) {  // SF8 configuration
        Serial.printf("[SF8] 🎯 Encrypted message candidate detected!\n");
        if (strcmp(info.protocol, "Meshtastic") == 0) {
          if (length > 20) {
            Serial.printf("[SF8] ✅ Large packet (%d bytes) - likely encrypted user data\n", length);
          } else {
            Serial.printf("[SF8] ⚠️ Small packet (%d bytes) - likely control message\n", length);
          }
          
          // Analyze packet structure for encryption indicators
          if (length >= 16) {
            bool hasEncryptedPayload = false;
            
            // Check for high entropy (encrypted data indicator)
            uint8_t nonZeroBytes = 0;
            for (size_t i = 12; i < length && i < 32; i++) {
              if (data[i] != 0x00) nonZeroBytes++;
            }
            
            if (nonZeroBytes > (length - 12) * 0.7) {
              hasEncryptedPayload = true;
              Serial.printf("[SF8] 🔒 High entropy detected - likely encrypted payload\n");
            }
            
            if (hasEncryptedPayload) {
              Serial.printf("[SF8] 🎉 ENCRYPTED USER MESSAGE CAPTURED!\n");
              Serial.printf("[SF8]     Node: 0x%08X, Length: %d bytes\n", info.nodeId, length);
              Serial.printf("[SF8]     This is what we were looking for!\n");
            }
          }
        }
      }
      
      #ifdef ENABLE_PSK_TESTING
      // PSK testing only in targeted mode
      if (strcmp(info.protocol, "Meshtastic") == 0 && length >= 8) {
        Serial.printf("[PSK] Attempting decryption on %d-byte packet\n", length);
        testPacketWithPSK(data, length);
      }
      #endif
    }
    
    // Always track nodes and log packets
    if (info.nodeId != 0) {
      trackNode(info.nodeId, info.protocol, rssi);
    }
    
    logPacket(data, length, rssi, snr, info.protocol);
    
    // Show hex data
    Serial.print("     Data: ");
    for (size_t i = 0; i < min(length, size_t(16)); i++) {
      Serial.printf("%02X ", data[i]);
    }
    if (length > 16) Serial.print("...");
    Serial.println();
    
  } else if (state != RADIOLIB_ERR_NONE) {
    if (reconState.scanState.mode == MODE_TARGETED_CAPTURE) {
      Serial.printf("[TARGET] RX Error: %d\n", state);
    }
  }
  
  // Restart receiving
  radio.startReceive();
}

// Handle activity monitoring during reconnaissance - SIMPLIFIED
void handleReconActivityMonitoring(uint32_t now) {
  static uint32_t lastActivityCheck = 0;
  
  // Check less frequently - only every 5 seconds to avoid spam
  if (now - lastActivityCheck >= 5000) {
    lastActivityCheck = now;
  }
}

void loop() {
  uint32_t now = millis();
  
  // Pet the watchdog - security tool stability
  esp_task_wdt_reset();
  
  // Handle serial input for mode switching and commands
  if (Serial.available()) {
    char cmd = Serial.read();
    handleUserInput(cmd);
    return;
  }
  
  // Mode-specific operations
  switch (reconState.scanState.mode) {
    case MODE_RECONNAISSANCE:
      handleReconnaissanceMode(now);
      break;
      
    case MODE_TARGETED_CAPTURE:
      handleTargetedCaptureMode(now);
      break;
      
    case MODE_INTERACTIVE_MENU:
      // Just wait for user input
      delay(100);
      return;
  }
  
  delay(10);
}

// UI functions (stress testing and intelligence) moved to user_interface.cpp

// Initialize stress testing - keep this minimal function in main.cpp
void initializeStressTesting() {
  #ifdef ENABLE_STRESS_TESTING
  // Stress tester will be initialized on first use for efficiency
  Serial.println("⚠️ Hardware stress testing available (press 't' in menu)");
  #endif
}

// Packet Replay Functions
void showReplayMenu() {
  Serial.println("\n📡 PACKET REPLAY MENU");
  Serial.println("====================");
  
  if (reconState.getNumCapturedPackets() == 0) {
    Serial.println("No packets captured for replay yet.");
    Serial.println("\nTo capture packets:");
    Serial.println("  1. Enter targeted capture mode");
    Serial.println("  2. Wait for packet reception");
    Serial.println("  3. Press 'c' to save packet to replay slot");
    Serial.println("\nPress any key to return to menu...");
    while (!Serial.available()) delay(10);
    Serial.read();
    reconState.scanState.mode = MODE_INTERACTIVE_MENU;
    showReconResults();
    return;
  }
  
  Serial.printf("Captured Packets: %d/%d slots used\n\n", 
                reconState.getNumCapturedPackets(), MAX_REPLAY_SLOTS);
  
  Serial.println("Slot | Protocol     | Length | Config              | RSSI   | Age");
  Serial.println("-----|--------------|--------|---------------------|--------|-------");
  
  for (uint8_t i = 0; i < reconState.getNumCapturedPackets(); i++) {
    const CapturedPacket& pkt = reconState.getReplayPacket(i);
    const ScanConfig& cfg = reconState.getScanConfig(pkt.configIndex);
    uint32_t ageSeconds = (millis() - pkt.captureTime) / 1000;
    
    Serial.printf("  %d  | %-12s | %6d | %-19s | %6.1f | %5lus\n",
                  i + 1, pkt.protocol, pkt.length, cfg.protocol, 
                  pkt.originalRSSI, ageSeconds);
  }
  
  Serial.println("\nCOMMANDS:");
  Serial.println("1-9 : Replay packet from slot");
  Serial.println("c   : Clear all replay slots");
  Serial.println("m   : Return to main menu");
  Serial.print("\nSelect option: ");
  
  while (!Serial.available()) delay(10);
  char cmd = Serial.read();
  Serial.println(cmd);
  
  if (cmd >= '1' && cmd <= '9') {
    uint8_t slotIndex = cmd - '1';
    if (slotIndex < reconState.getNumCapturedPackets()) {
      replayPacket(slotIndex);
    } else {
      Serial.println("❌ Invalid slot number");
      delay(2000);
      showReplayMenu();
    }
  } else if (cmd == 'c' || cmd == 'C') {
    Serial.print("Clear all replay slots? (y/N): ");
    while (!Serial.available()) delay(10);
    char confirm = Serial.read();
    Serial.println(confirm);
    
    if (confirm == 'y' || confirm == 'Y') {
      reconState.clearReplaySlots();
      Serial.println("✅ All replay slots cleared");
      delay(1000);
    }
    showReplayMenu();
  } else if (cmd == 'm' || cmd == 'M') {
    reconState.scanState.mode = MODE_INTERACTIVE_MENU;
    showReconResults();
  } else {
    Serial.println("❌ Invalid option");
    delay(1000);
    showReplayMenu();
  }
}

void replayPacket(uint8_t slotIndex) {
  const CapturedPacket& pkt = reconState.getReplayPacket(slotIndex);
  
  if (!pkt.valid) {
    Serial.println("❌ Invalid packet slot");
    return;
  }
  
  Serial.println("\n📡 REPLAYING PACKET");
  Serial.println("===================");
  Serial.printf("Slot: #%d\n", slotIndex + 1);
  Serial.printf("Protocol: %s\n", pkt.protocol);
  Serial.printf("Length: %d bytes\n", pkt.length);
  Serial.printf("Config: %s\n", reconState.getScanConfig(pkt.configIndex).protocol);
  Serial.printf("Original RSSI: %.1f dBm\n", pkt.originalRSSI);
  
  // Show packet data
  Serial.print("Data: ");
  for (size_t i = 0; i < min(pkt.length, size_t(32)); i++) {
    Serial.printf("%02X ", pkt.data[i]);
  }
  if (pkt.length > 32) Serial.print("...");
  Serial.println();
  
  // Apply radio configuration for replay
  if (!applyConfig(pkt.configIndex)) {
    Serial.println("❌ Failed to configure radio");
    delay(2000);
    showReplayMenu();
    return;
  }
  
  // Transmit the packet
  Serial.println("\n⚠️  TRANSMITTING...");
  int state = radio.transmit(const_cast<uint8_t*>(pkt.data), pkt.length);
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("✅ PACKET TRANSMITTED SUCCESSFULLY");
    Serial.printf("   %d bytes sent on %s\n", pkt.length, 
                  reconState.getScanConfig(pkt.configIndex).protocol);
  } else {
    Serial.printf("❌ TRANSMISSION FAILED (error %d)\n", state);
  }
  
  Serial.println("\nPress any key to return to replay menu...");
  while (!Serial.available()) delay(10);
  Serial.read();
  
  // Return to receive mode
  radio.startReceive();
  showReplayMenu();
}
