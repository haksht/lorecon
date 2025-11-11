#include "lora_recon_tool.h"
#include "user_interface.h"
#include "error_handler.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <esp_task_wdt.h>
#include <mbedtls/base64.h>
#include <mbedtls/aes.h>
#include "text_packet_diagnostic.h"  // For packet diagnostic analysis

#ifdef ENABLE_PSK_TESTING
#include "psk_decryption_simple.h"
#endif

// SPI pins for Heltec WiFi LoRa 32 V3
#define SCK  9
#define MISO 11
#define MOSI 10

// Timing constants
#define SERIAL_INIT_DELAY_MS       2000  // Wait for serial console to initialize
#define OLED_WELCOME_DELAY_MS      2000  // Display welcome screen duration
#define BUTTON_DEBOUNCE_DELAY_MS   100   // Debounce delay for button presses
#define BUTTON_LONG_PRESS_MS       3000  // Long press threshold for shutdown
#define SHUTDOWN_WARNING_DELAY_MS  2000  // Display shutdown warning duration
#define SHUTDOWN_FINAL_DELAY_MS    1000  // Final delay before shutdown
#define LOOP_POLLING_DELAY_MS      10    // Main loop polling delay

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
{
    g_reconTool = this;
}

// Initialize the reconnaissance tool
bool LoRaReconTool::initialize() {
    Serial.begin(115200);
    delay(SERIAL_INIT_DELAY_MS);
    
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
    if (oledDisplay && oledDisplay->initialize()) {
        oledDisplay->showWelcome();
        delay(OLED_WELCOME_DELAY_MS);
    } else {
        Serial.println("[WARNING] Display initialization failed - continuing without display");
        if (oledDisplay) {
            delete oledDisplay;
            oledDisplay = nullptr;
        }
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
    
    // Initialize PSK decryption system
    #ifdef ENABLE_PSK_TESTING
    PSKDecryption::initialize();
    #endif
    
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
            delay(BUTTON_DEBOUNCE_DELAY_MS);
            return;
            
        case MODE_PACKET_REPLAY:
            // Packet replay is handled via command handler
            delay(BUTTON_DEBOUNCE_DELAY_MS);
            return;
    }
    
    delay(LOOP_POLLING_DELAY_MS);
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
    // Check if interrupt fired
    if (!packetReceived.load(std::memory_order_acquire)) return;
    
    packetReceived.store(false, std::memory_order_release);
    
    // Quick read from radio and queue it - minimize time before returning to receive
    uint8_t tempBuffer[PACKET_BUFFER_SIZE];
    int state = radio.readData(tempBuffer, PACKET_BUFFER_SIZE);
    
    if (state == RADIOLIB_ERR_NONE) {
        size_t length = radio.getPacketLength();
        
        if (length > 0 && length <= PACKET_BUFFER_SIZE) {
            // Get signal metrics ONLY for large packets (saves ~5ms for small packets)
            float rssi = 0;
            float snr = 0;
            if (length >= 40) {
                rssi = radio.getRSSI();
                snr = radio.getSNR();
            }
            
            // Queue the packet if space available
            if (packetQueue.size() < MAX_QUEUE_SIZE) {
                QueuedPacket qp;
                memcpy(qp.data, tempBuffer, length);
                qp.length = length;
                qp.rssi = rssi;
                qp.snr = snr;
                qp.timestamp = millis();
                packetQueue.push(qp);
            } else {
                Serial.println("[WARNING] Packet queue full - dropping packet!");
            }
        }
    }
    
    // Immediately restart receiving - this is KEY to catching rapid packets
    radio.startReceive();
    
    // Debug: Show queue size if it's building up
    if (packetQueue.size() > 1) {
        Serial.printf("[QUEUE] %d packets buffered\n", packetQueue.size());
    }
    
    // Now process queued packets at our leisure
    processQueuedPackets();
}

// Process packets from queue
void LoRaReconTool::processQueuedPackets() {
    static uint8_t packetBuffer[PACKET_BUFFER_SIZE];
    
    while (!packetQueue.empty()) {
        QueuedPacket qp = packetQueue.front();
        packetQueue.pop();
        
        // Copy to processing buffer
        memcpy(packetBuffer, qp.data, qp.length);
        size_t packetLength = qp.length;
        float rssi = qp.rssi;
        float snr = qp.snr;
        
        // Store for potential replay capture
        if (packetLength <= MAX_PACKET_SIZE) {
            memcpy(reconState.scanState.lastPacket, packetBuffer, packetLength);
            reconState.scanState.lastPacketLength = packetLength;
        }
        reconState.scanState.totalPackets++;
        
        const uint8_t* data = packetBuffer;
        size_t length = packetLength;
        
        // Analyze packet using ProtocolAnalyzer (skip verbose diagnostics for speed)
        PacketInfo info = protocolAnalyzer.analyze(data, length, rssi);
        
        // Enhanced packet analysis for Meshtastic (minimal output for speed)
        if (strcmp(info.protocol, "Meshtastic") == 0) {
            // Try to extract GPS position silently
            geoIntel.extractPosition(data, length, info.nodeId);
        }
        
        // Track as targetable device in ALL modes if we have a real node ID
        if (info.nodeId != 0) {
            trackTargetableDevice(info.nodeId, reconState.scanState.currentConfig, rssi, info.protocol, data, length);
        }
        
        // Mode-specific handling
        if (reconState.scanState.mode == MODE_RECONNAISSANCE) {
                Serial.printf("[RECON] Packet #%d: %s, %d bytes, %.1f dBm, %.1f dB SNR\n",
                              reconState.scanState.totalPackets, info.protocol, length, rssi, snr);
                
                // Update display with packet info
                if (oledDisplay && oledDisplay->isOn()) {
                    oledDisplay->showPacketReceived(rssi, snr, info.protocol, info.nodeId);
                }
                
                // Track RF activity (for situational awareness)
                trackRFActivity(reconState.scanState.currentConfig, rssi);
            } else if (reconState.scanState.mode == MODE_TARGETED_CAPTURE) {
                // Show ALL packets with full decryption to find text messages
                if (length < 40) {
                    Serial.printf("\n[SMALL %d bytes] ", length);
                } else {
                    Serial.printf("\n🎯 [CAPTURE] Packet #%d: %s, %d bytes, %.1f dBm, %.1f dB SNR\n",
                                  reconState.scanState.totalPackets, info.protocol, length, rssi, snr);
                }
                
                // Update display silently
                if (oledDisplay && oledDisplay->isOn()) {
                    oledDisplay->showPacketReceived(rssi, snr, info.protocol, info.nodeId);
                }
                
                #ifdef ENABLE_PSK_TESTING
                // Try decryption on ALL packets to find text messages
                if (length >= 20) {
                    if (length < 40) {
                        // For small packets, only print if we find actual readable text
                        Serial.printf("Decrypting... ");
                    } else {
                        Serial.printf("[PSK] Analyzing %d-byte packet (text message size)\n", length);
                    }
                    // For "Unknown" packets that might be routed Meshtastic, try to find the header
                    const uint8_t* payload = data;
                    size_t payloadLen = length;
                    
                    // If it doesn't start with FF FF FF FF, it might be a routed packet
                    // Try to locate Meshtastic header within the packet
                    if (data[0] != 0xFF && length >= 16) {
                        // Look for 0xFF 0xFF 0xFF 0xFF pattern in first 20 bytes
                        for (size_t i = 0; i < min(length - 4, size_t(20)); i++) {
                            if (data[i] == 0xFF && data[i+1] == 0xFF && data[i+2] == 0xFF && data[i+3] == 0xFF) {
                                payload = data + i;
                                payloadLen = length - i;
                                Serial.printf("[ROUTED] Found Meshtastic header at offset %d in %d-byte packet\n", i, length);
                                break;
                            }
                        }
                    }
                    
                    // Try decryption on the located packet (or original if standard Meshtastic)
                    // IMPORTANT: Try decryption on ALL packets, not just identified Meshtastic!
                    // Raw packets without 0xFFFFFFFF header may not be identified correctly
                    PSKDecryption::testDefaultPSKs(payload, payloadLen);
                } // End if length >= 20
                #endif
            }
            
            // Always track nodes and log packets
            if (info.nodeId != 0) {
                trackNode(info.nodeId, info.protocol, rssi);
            }
            
            logPacket(data, length, rssi, snr, info.protocol);
            
            // Show hex data for ALL packets in targeting mode (to see what we're really getting)
            if (reconState.scanState.mode == MODE_TARGETED_CAPTURE) {
                Serial.print("     Data: ");
                for (size_t i = 0; i < min(length, size_t(16)); i++) {
                    Serial.printf("%02X ", data[i]);
                }
                if (length > 16) Serial.print("...");
                Serial.println();
            } else if (reconState.scanState.mode == MODE_RECONNAISSANCE) {
                // Always show data in recon mode
                Serial.print("     Data: ");
                for (size_t i = 0; i < min(length, size_t(16)); i++) {
                    Serial.printf("%02X ", data[i]);
                }
                if (length > 16) Serial.print("...");
                Serial.println();
            }
    } // End while loop - all queued packets processed
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
    Serial.println("\nPress 'm' to return to menu, 'r' to resume recon");
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
    
    Serial.println("\nPress 'm' to return to menu, 'r' to resume recon");
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

// Handle user input commands
void LoRaReconTool::handleUserInput(char cmd) {
    if (cmd == 'm' || cmd == 'M') {
        reconState.scanState.mode = MODE_INTERACTIVE_MENU;
        showReconResults();
        return;
    }

    if (cmd == 'f' || cmd == 'F') {
        showFrequencyTargetingMenu();
        handleFrequencyTargetingInput();
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
    
    // All commands now handled by CommandHandler
    // This fallback should rarely be reached
    
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
          esp_task_wdt_reset();  // Feed watchdog while waiting
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
    esp_task_wdt_reset();  // Feed watchdog while waiting
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
          esp_task_wdt_reset();  // Feed watchdog while waiting
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
    
    // Ask for repeat count
    Serial.print("Repeat count (1-100, or 0 to cancel): ");
    uint32_t startTime = millis();
    String input = "";
    while (!Serial.available()) {
        if (millis() - startTime > 30000) {  // 30 second timeout
            Serial.println("\n[TIMEOUT] Cancelled.");
            delay(1000);
            showReplayMenu();
            return;
        }
        esp_task_wdt_reset();
        delay(10);
    }
    
    // Read the number
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') break;
        if (c >= '0' && c <= '9') {
            input += c;
        }
        delay(10);
    }
    
    int repeatCount = input.toInt();
    Serial.println(repeatCount);
    
    if (repeatCount <= 0 || repeatCount > 100) {
        Serial.println("❌ Cancelled or invalid count");
        delay(1000);
        showReplayMenu();
        return;
    }
    
    // Optional delay between transmissions
    uint16_t delayMs = 1000;  // Default 1 second between packets
    if (repeatCount > 1) {
        Serial.print("Delay between packets in ms (100-10000, default 1000): ");
        startTime = millis();
        input = "";
        while (!Serial.available()) {
            if (millis() - startTime > 10000) {  // 10 second timeout
                Serial.printf("\n[TIMEOUT] Using default %d ms\n", delayMs);
                break;
            }
            esp_task_wdt_reset();
            delay(10);
        }
        
        if (Serial.available()) {
            while (Serial.available()) {
                char c = Serial.read();
                if (c == '\n' || c == '\r') break;
                if (c >= '0' && c <= '9') {
                    input += c;
                }
                delay(10);
            }
            int userDelay = input.toInt();
            if (userDelay >= 100 && userDelay <= 10000) {
                delayMs = userDelay;
                Serial.println(delayMs);
            } else {
                Serial.printf("\n[Invalid] Using default %d ms\n", delayMs);
            }
        }
    }
    
    // Apply the correct radio configuration
    if (!applyConfig(pkt.configIndex)) {
        Serial.println("❌ Failed to apply radio configuration");
        delay(2000);
        showReplayMenu();
        return;
    }
    
    // Create mutable copy for transmission (RadioLib requires non-const)
    uint8_t txBuffer[256];
    memcpy(txBuffer, pkt.data, pkt.length);
    
    // Transmit the packet multiple times
    Serial.printf("\n📡 Transmitting %d time(s) with %d ms delay...\n", repeatCount, delayMs);
    radio.setOutputPower(10);  // Set transmission power
    
    int successCount = 0;
    int failCount = 0;
    
    for (int i = 0; i < repeatCount; i++) {
        Serial.printf("[%d/%d] ", i + 1, repeatCount);
        
        // Feed watchdog before transmission (can take several seconds with high SF)
        esp_task_wdt_reset();
        int state = radio.transmit(txBuffer, pkt.length);
        esp_task_wdt_reset();  // Feed again after transmission completes
        
        if (state == RADIOLIB_ERR_NONE) {
            Serial.println("✅");
            successCount++;
        } else {
            Serial.printf("❌ (error %d)\n", state);
            failCount++;
        }
        
        // Delay between transmissions (except after last one)
        if (i < repeatCount - 1) {
            delay(delayMs);
        }
    }
    
    radio.setOutputPower(0);   // Back to receive-only mode
    
    Serial.println("\n📊 REPLAY SUMMARY");
    Serial.println("=================");
    Serial.printf("Total attempts: %d\n", repeatCount);
    Serial.printf("Successful: %d (%.1f%%)\n", successCount, (float)successCount / repeatCount * 100.0);
    Serial.printf("Failed: %d (%.1f%%)\n", failCount, (float)failCount / repeatCount * 100.0);
    
    if (successCount == repeatCount) {
        Serial.println("✅ All packets transmitted successfully!");
    } else if (successCount > 0) {
        Serial.println("⚠️ Some packets failed to transmit");
    } else {
        Serial.println("❌ All transmissions failed");
    }
    
    Serial.println("\nPress any key to return to replay menu...");
    while (!Serial.available()) {
        esp_task_wdt_reset();  // Feed watchdog while waiting
        delay(10);
    }
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
