#include "lora_recon_tool.h"
#include "user_interface.h"
#include "error_handler.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <esp_task_wdt.h>

#ifdef ENABLE_PSK_TESTING
#include "psk_decryption_simple.h"
#endif

// SPI pins for Heltec WiFi LoRa 32 V3
#define SCK  9
#define MISO 11
#define MOSI 10

// Global pointer for interrupt handler
LoRaReconTool* g_reconTool = nullptr;

// Interrupt handler (must be global for ISR)
void IRAM_ATTR onPacketReceived() {
    if (g_reconTool) {
        g_reconTool->markPacketReceived();
    }
}

// Constructor
LoRaReconTool::LoRaReconTool()
    : radio(new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY))
    , commandHandler(nullptr)
    , oledDisplay(nullptr)
    , packetReceived(false)
    , buttonPressed(false)
    , buttonPressStart(0)
    , shutdownInitiated(false)
#ifdef ENABLE_STRESS_TESTING
    , stressTester(nullptr)
#endif
{
    g_reconTool = this;
}

// Initialize the reconnaissance tool
bool LoRaReconTool::initialize() {
    Serial.begin(115200);
    delay(2000);
    
    // Initialize error handler first
    ErrorHandler::initialize();
    
    // Initialize user button
    pinMode(USER_BUTTON, INPUT_PULLUP);
    Serial.printf("[BUTTON] User button configured on GPIO %d\n", USER_BUTTON);
    Serial.println("[BUTTON] Short press = Toggle display | Long press (3s) = Shutdown");
    
    // Initialize watchdog timer for stability (30 second timeout)
    // This will reset the ESP32 if the main loop hangs
    esp_task_wdt_init(30, true);
    esp_task_wdt_add(NULL);
    Serial.println("[WATCHDOG] Hardware watchdog enabled (30s timeout)");
    
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
        LOG_RADIO_ERROR(ErrorCodes::RADIO_INIT_FAILED, "SX1262 initialization failed");
        return false;
    }
    Serial.println("OK");
    
    // Set interrupt handler with explicit pin configuration
    pinMode(LORA_DIO1, INPUT);
    radio.setDio1Action(onPacketReceived);
    Serial.printf("[RADIO] DIO1 interrupt configured on GPIO %d\n", LORA_DIO1);
    
    // Initialize reconnaissance system
    reconState.initialize();
    
    // Initialize OLED display
    oledDisplay = new OLEDDisplay();
    if (oledDisplay->initialize()) {
        oledDisplay->showWelcome();
        delay(2000);  // Show welcome screen
    } else {
        Serial.println("[WARNING] Display initialization failed - continuing without display");
    }
    
    displayReconStartMessage();
    
    // Apply initial configuration and start receiving
    if (!applyConfig(reconState.scanState.currentConfig)) {
        Serial.println("[ERROR] Failed to apply initial config");
        LOG_RADIO_ERROR(ErrorCodes::RADIO_CONFIG_FAILED, "Initial radio configuration failed");
        return false;
    }
    
    radio.startReceive();
    Serial.println("[RECON] Started\n");
    
    // Initialize optional modules
    initializeStressTesting();
    
    // Initialize command handler
    commandHandler = new CommandHandler(this);
    Serial.println("[SYSTEM] Command handler initialized");
    
    return true;
}

// Main update loop
void LoRaReconTool::update() {
    uint32_t now = millis();
    
    // Handle button press (must be first to catch shutdown)
    handleButtonPress(now);
    
    // If shutdown initiated, don't process anything else
    if (shutdownInitiated) {
        return;
    }
    
    // Pet the watchdog
    esp_task_wdt_reset();
    
    // Update display
    if (oledDisplay && oledDisplay->isOn()) {
        oledDisplay->update();
    }
    
    // Handle serial input for mode switching and commands
    if (Serial.available()) {
        char cmd = Serial.read();
        if (commandHandler) {
            commandHandler->handleCommand(cmd);
        } else {
            Serial.println("[ERROR] Command handler not initialized");
        }
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
            
        case MODE_PACKET_REPLAY:
            // Packet replay is handled via command handler
            delay(100);
            return;
    }
    
    delay(10);
}

// Apply radio configuration for scanning
bool LoRaReconTool::applyConfig(uint8_t configIndex) {
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

// Handle reconnaissance mode operations
void LoRaReconTool::handleReconnaissanceMode(uint32_t now) {
    // Handle frequency switching in recon mode
    if (now - reconState.scanState.lastScanSwitch >= SCAN_DWELL_TIME) {
        reconState.scanState.currentConfig = (reconState.scanState.currentConfig + 1) % reconState.getNumConfigs();
        reconState.scanState.lastScanSwitch = now;
        
        // Show progress every full cycle (when back to config 0)
        if (reconState.scanState.currentConfig == 0) {
            uint32_t elapsed = (now - reconState.scanState.reconStartTime) / 1000;
            Serial.printf("[RECON] Cycle complete - %u seconds elapsed, %d targetable devices found\n", 
                          (unsigned int)elapsed, reconState.numTargetableDevices);
        }
        
        if (applyConfig(reconState.scanState.currentConfig)) {
            radio.startReceive();
            
            // Update display with new scanning config
            if (oledDisplay && oledDisplay->isOn()) {
                const ScanConfig& cfg = reconState.getScanConfig(reconState.scanState.currentConfig);
                char freqStr[16];
                snprintf(freqStr, sizeof(freqStr), "%.3f", cfg.frequency);
                oledDisplay->showScanningStatus(freqStr, cfg.spreadingFactor, 
                                                reconState.scanState.currentConfig, 
                                                reconState.getNumConfigs());
            }
        }
    }
    
    // Handle packets in recon mode
    handlePacketReception();
    
    // Enhanced activity monitoring for recon
    handleReconActivityMonitoring(now);
}

// Handle targeted capture mode operations
void LoRaReconTool::handleTargetedCaptureMode(uint32_t now) {
    handlePacketReception();
}

// Handle activity monitoring during reconnaissance
void LoRaReconTool::handleReconActivityMonitoring(uint32_t now) {
    static uint32_t lastActivityCheck = 0;
    
    // Check less frequently - only every 5 seconds to avoid spam
    if (now - lastActivityCheck >= 5000) {
        lastActivityCheck = now;
    }
}

// Unified packet reception handler
void LoRaReconTool::handlePacketReception() {
    // Simple interrupt check
    if (!packetReceived.load(std::memory_order_acquire)) return;
    
    packetReceived.store(false, std::memory_order_release);
    
    uint8_t packetBuffer[PACKET_BUFFER_SIZE];
    size_t packetLength = 0;
    
    int state = radio.readData(packetBuffer, PACKET_BUFFER_SIZE);
    
    if (state == RADIOLIB_ERR_NONE) {
        packetLength = radio.getPacketLength();
        
        if (packetLength > 0 && packetLength <= PACKET_BUFFER_SIZE) {
            // Store for potential replay capture
            if (packetLength <= MAX_PACKET_SIZE) {
                memcpy(reconState.scanState.lastPacket, packetBuffer, packetLength);
                reconState.scanState.lastPacketLength = packetLength;
            }
            reconState.scanState.totalPackets++;
            
            float rssi = radio.getRSSI();
            float snr = radio.getSNR();
            
            const uint8_t* data = packetBuffer;
            size_t length = packetLength;
        
        // Analyze packet using ProtocolAnalyzer
        PacketInfo info = protocolAnalyzer.analyze(data, length, rssi);
    
        // Enhanced packet analysis for Meshtastic
        if (strcmp(info.protocol, "Meshtastic") == 0) {
            // Try to extract GPS position
            if (geoIntel.extractPosition(data, length, info.nodeId)) {
                // Position extracted successfully - displayed by geo module
            }
            
            Serial.printf("[PACKET] Meshtastic structure analysis:\n");
            Serial.printf("  - Length: %d bytes\n", length);
            Serial.printf("  - Node ID: 0x%08X\n", info.nodeId);
            if (length > 8) {
                Serial.printf("  - Payload type: 0x%02X\n", data[8]);
                if (length > 9) {
                    Serial.printf("  - Flags/Seq: 0x%02X\n", data[9]);
                }
            }
        }            if (reconState.scanState.mode == MODE_RECONNAISSANCE) {
                Serial.printf("[RECON] Packet #%d: %s, %d bytes, %.1f dBm, %.1f dB SNR\n",
                              reconState.scanState.totalPackets, info.protocol, length, rssi, snr);
                
                // Update display with packet info
                if (oledDisplay && oledDisplay->isOn()) {
                    oledDisplay->showPacketReceived(rssi, snr, info.protocol, info.nodeId);
                }
                
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
                
                // Update display with packet info in targeting mode
                if (oledDisplay && oledDisplay->isOn()) {
                    oledDisplay->showPacketReceived(rssi, snr, info.protocol, info.nodeId);
                }
                
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
                    PSKDecryption::testDefaultPSKs(data, length);
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
        }
        
    } else if (state != RADIOLIB_ERR_NONE) {
        if (reconState.scanState.mode == MODE_TARGETED_CAPTURE) {
            Serial.printf("[TARGET] RX Error: %d\n", state);
        }
    }
    
    // Restart receiving
    radio.startReceive();
}

// Start targeted capture mode on specific device
void LoRaReconTool::startTargetedCapture(uint8_t deviceIndex) {
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

// Start frequency targeting mode
void LoRaReconTool::startFrequencyTargeting(uint8_t configIndex) {
    if (configIndex >= reconState.getNumConfigs()) {
        Serial.println("❌ Invalid frequency selection");
        return;
    }
    
    const ScanConfig& cfg = reconState.getScanConfig(configIndex);
    
    Serial.println("\n" + String('=').substring(0,50));
    Serial.println("      FREQUENCY TARGETING ACTIVE");
    Serial.println(String('=').substring(0,50));
    Serial.printf("Target: %s\n", cfg.protocol);
    Serial.printf("Frequency: %.3f MHz\n", cfg.frequency);
    Serial.printf("SF: %d, BW: %.0f kHz, SW: 0x%02X\n", 
                  cfg.spreadingFactor, cfg.bandwidth, cfg.syncWord);
    
    // Check for known activity
    uint8_t activityCount = 0;
    for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
        if (reconState.getTargetableDevice(i).configIndex == configIndex) {
            activityCount++;
        }
    }
    
    if (activityCount > 0) {
        Serial.printf("📊 Known devices on this frequency: %d\n", activityCount);
    } else {
        Serial.println("⚠️ No previous activity detected on this frequency");
        Serial.println("   Will monitor for new transmissions...");
    }
    
    Serial.println("\nPress 'm' to return to menu, 'r' to restart recon");
    Serial.println("Monitoring for packets...\n");
    
    // Set up targeting mode
    reconState.scanState.mode = MODE_TARGETED_CAPTURE;
    reconState.scanState.targetConfig = configIndex;
    reconState.scanState.currentConfig = configIndex;
    
    if (applyConfig(configIndex)) {
        radio.startReceive();
        Serial.println("✅ Frequency targeting active");
    } else {
        Serial.println("❌ Failed to configure radio for targeting");
        reconState.scanState.mode = MODE_INTERACTIVE_MENU;
    }
}

// Tracking functions
void LoRaReconTool::trackNode(uint32_t nodeId, const char* protocol, float rssi) {
    reconState.updateNode(nodeId, protocol, rssi);
}

void LoRaReconTool::trackRFActivity(uint8_t configIndex, float rssi) {
    reconState.updateRFActivity(configIndex, rssi);
}

void LoRaReconTool::trackTargetableDevice(uint32_t nodeId, uint8_t configIndex, float rssi, 
                                         const char* protocol, const uint8_t* packetData, size_t packetLength) {
    reconState.addTargetableDevice(nodeId, configIndex, rssi, protocol, packetData, packetLength);
}

// Save packet to JSON log
void LoRaReconTool::logPacket(const uint8_t* data, size_t length, float rssi, float snr, const char* protocol) {
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

// Initialize stress testing
void LoRaReconTool::initializeStressTesting() {
#ifdef ENABLE_STRESS_TESTING
    // Stress tester will be initialized on first use for efficiency
    Serial.println("⚠️ Hardware stress testing available (press 't' in menu)");
#endif
}

// Handle user input commands
void LoRaReconTool::handleUserInput(char cmd) {
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
        // Restart reconnaissance
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
    
    if (cmd == 'v' || cmd == 'V') {
        showSecurityAssessment();
        reconState.scanState.mode = MODE_INTERACTIVE_MENU;
        showReconResults();
        return;
    }
    
    if (cmd == 'c' || cmd == 'C') {
        // Capture last packet for replay
        if (reconState.scanState.lastPacketLength > 0 && reconState.scanState.mode == MODE_TARGETED_CAPTURE) {
            const uint8_t* data = (const uint8_t*)reconState.scanState.lastPacket;
            size_t length = reconState.scanState.lastPacketLength;
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

// Packet Replay Functions
void LoRaReconTool::showReplayMenu() {
    Serial.println("\n📡 PACKET REPLAY MENU");
    Serial.println("====================");
    
    if (reconState.getNumCapturedPackets() == 0) {
        Serial.println("No packets captured for replay yet.");
        Serial.println("\nTo capture packets:");
        Serial.println("  1. Enter targeted capture mode");
        Serial.println("  2. Wait for packet reception");
        Serial.println("  3. Press 'c' to save packet to replay slot");
        Serial.println("\nPress any key to return to menu...");
        
        uint32_t startTime = millis();
        while (!Serial.available()) {
          if (millis() - startTime > 30000) {  // 30 second timeout
            Serial.println("[TIMEOUT] Returning to menu...");
            break;
          }
          delay(10);
        }
        if (Serial.available()) Serial.read();
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
        
        Serial.printf("  %d  | %-12s | %6d | %-19s | %6.1f | %5us\n",
                      i + 1, pkt.protocol, pkt.length, cfg.protocol, 
                      pkt.originalRSSI, (unsigned int)ageSeconds);
    }
    
    Serial.println("\nCOMMANDS:");
    Serial.println("1-9 : Replay packet from slot");
  Serial.println("c   : Clear all replay slots");
  Serial.println("m   : Return to main menu");
  Serial.print("\nSelect option: ");
  
  // Wait for input with timeout
  uint32_t startTime = millis();
  while (!Serial.available()) {
    if (millis() - startTime > 30000) {  // 30 second timeout
      Serial.println("\n[TIMEOUT] Returning to main menu...");
      reconState.scanState.mode = MODE_INTERACTIVE_MENU;
      showReconResults();
      return;
    }
    delay(10);
  }
  char cmd = Serial.read();
  Serial.println(cmd);    if (cmd >= '1' && cmd <= '9') {
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
        
        uint32_t startTime = millis();
        while (!Serial.available()) {
          if (millis() - startTime > 10000) {  // 10 second timeout for confirmation
            Serial.println("\n[TIMEOUT] Cancelled.");
            showReplayMenu();
            return;
          }
          delay(10);
        }
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

void LoRaReconTool::replayPacket(uint8_t slotIndex) {
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
    Serial.println("\n");
    
    // Apply the correct radio configuration
    if (!applyConfig(pkt.configIndex)) {
        Serial.println("❌ Failed to apply radio configuration");
        delay(2000);
        showReplayMenu();
        return;
    }
    
    // Transmit the packet
    Serial.print("📡 Transmitting... ");
    radio.setOutputPower(10);  // Set transmission power
    
    // Create mutable copy for transmission (RadioLib requires non-const)
    uint8_t txBuffer[256];
    memcpy(txBuffer, pkt.data, pkt.length);
    
    int state = radio.transmit(txBuffer, pkt.length);
    radio.setOutputPower(0);   // Back to receive-only mode
    
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("✅ SUCCESS");
        Serial.println("Packet replayed successfully!");
    } else {
        Serial.printf("❌ FAILED (error %d)\n", state);
    }
    
    Serial.println("\nPress any key to return to replay menu...");
    while (!Serial.available()) delay(10);
    Serial.read();
    
    // Resume receiving
    applyConfig(reconState.scanState.currentConfig);
    radio.startReceive();
    
    showReplayMenu();
}

// Handle button press for display toggle and power off
void LoRaReconTool::handleButtonPress(uint32_t now) {
    bool currentButtonState = (digitalRead(USER_BUTTON) == LOW);  // Active low
    
    // Detect button press (transition from not pressed to pressed)
    if (currentButtonState && !buttonPressed) {
        buttonPressed = true;
        buttonPressStart = now;
        Serial.println("[BUTTON] Button pressed");
    }
    
    // Button is being held down
    if (currentButtonState && buttonPressed) {
        uint32_t pressDuration = now - buttonPressStart;
        
        // Long press (3 seconds) = shutdown
        if (pressDuration >= 3000 && !shutdownInitiated) {
            shutdownInitiated = true;
            Serial.println("\n🔴 SHUTDOWN INITIATED (3s press detected)");
            Serial.println("===========================================");
            
            // Show shutdown on display
            if (oledDisplay) {
                oledDisplay->showShutdown();
                delay(100);  // Ensure display updates
            }
            
            // Stop radio safely
            Serial.print("[RADIO] Stopping radio... ");
            radio.standby();
            radio.sleep();
            Serial.println("OK");
            
            // Optional: Save state to flash
            // reconState.saveToFlash();
            
            Serial.println("✅ Safe to remove power");
            Serial.println("Device will enter deep sleep in 2 seconds...");
            Serial.flush();
            
            delay(2000);
            
            // Enter deep sleep (effectively powered off until reset)
            Serial.println("[SLEEP] Entering deep sleep mode");
            Serial.flush();
            esp_deep_sleep_start();
        }
    }
    
    // Button released
    if (!currentButtonState && buttonPressed) {
        uint32_t pressDuration = now - buttonPressStart;
        buttonPressed = false;
        
        // Short press (< 3 seconds) = toggle display
        if (pressDuration < 3000 && !shutdownInitiated) {
            Serial.println("[BUTTON] Short press detected - toggling display");
            if (oledDisplay) {
                oledDisplay->toggle();
                if (oledDisplay->isOn()) {
                    // Refresh display with current info
                    const ScanConfig& cfg = reconState.getScanConfig(reconState.scanState.currentConfig);
                    char freqStr[16];
                    snprintf(freqStr, sizeof(freqStr), "%.3f", cfg.frequency);
                    oledDisplay->showScanningStatus(freqStr, cfg.spreadingFactor, 
                                                    reconState.scanState.currentConfig, 
                                                    reconState.getNumConfigs());
                }
            }
        }
    }
}

