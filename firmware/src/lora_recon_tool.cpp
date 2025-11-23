#include "lora_recon_tool.h"
#include "user_interface.h"
#include "error_handler.h"
#include "logger.h"
#include "packet_logger.h"
#include "packet_processor.h"
#include "command_handler.h"
#include "oled_display.h"
#include "web_server.h"
#include "config.h"
#include "psk_decryption_simple.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>

// Global pointer for tool instance
LoRaReconTool* g_reconTool = nullptr;

// Constructor
LoRaReconTool::LoRaReconTool()
    : radioController(nullptr)
    , packetProcessor(nullptr)
    , commandHandler(nullptr)
    , oledDisplay(nullptr)
    , buttonPressed(false)
    , buttonPressStart(0)
    , shutdownInitiated(false)
{
    g_reconTool = this;
}

// Initialize the reconnaissance tool
bool LoRaReconTool::initialize() {
    Serial.begin(Config::UI::SERIAL_BAUD);
    delay(Config::UI::SERIAL_INIT_DELAY_MS);
    
    LOG_INFO("ESP32 LoRa Reconnaissance Tool v2.0");
    
    // Initialize error handler first
    ErrorHandler::initialize();
    
    // Initialize user button
    pinMode(Config::Hardware::USER_BUTTON, INPUT_PULLUP);
    LOG_INFO("User button configured on GPIO %d", Config::Hardware::USER_BUTTON);
    LOG_INFO("Short press = Toggle display | Long press (3s) = Shutdown");
    
    // Initialize watchdog timer for stability
    esp_task_wdt_init(Config::System::WATCHDOG_TIMEOUT_SEC, true);
    esp_task_wdt_add(NULL);
    LOG_INFO("Hardware watchdog enabled (%ds timeout)", Config::System::WATCHDOG_TIMEOUT_SEC);
    
    displayWelcomeMessage();
    
    // Initialize storage
    if (LittleFS.begin()) {
        LOG_INFO("Storage ready");
    } else {
        LOG_WARN("Storage init failed");
    }
    
    // Initialize RadioController
    radioController = new RadioController();
    if (!radioController || !radioController->initialize()) {
        LOG_ERROR("Radio initialization failed");
        return false;
    }
    
    // Initialize PacketProcessor
    packetProcessor = new PacketProcessor();
    if (!packetProcessor) {
        LOG_ERROR("PacketProcessor initialization failed");
        return false;
    }
    
    // Initialize reconnaissance system
    reconState.initialize();
    
    // Initialize OLED display
    oledDisplay = new OLEDDisplay();
    if (oledDisplay && oledDisplay->initialize()) {
        oledDisplay->showWelcome();
        delay(Config::UI::WELCOME_SCREEN_DELAY_MS);
        // Keep display alive during long init
        if (oledDisplay->isOn()) {
            oledDisplay->update();
        }
    } else {
        LOG_WARN("Display initialization failed - continuing without display");
        if (oledDisplay) {
            delete oledDisplay;
            oledDisplay = nullptr;
        }
    }
    
    displayReconStartMessage();
    
    // Apply initial configuration and start receiving
    const ScanConfig& cfg = reconState.getScanConfig(reconState.scanState.currentConfig);
    if (!radioController->applyConfig(cfg)) {
        LOG_ERROR("Failed to apply initial config");
        REPORT_RADIO_ERROR(ErrorCodes::RADIO_CONFIG_FAILED, "Initial radio configuration failed");
        return false;
    }
    
    radioController->startReceive();
    LOG_INFO("Reconnaissance started");
    
    // Update display to show initial scanning status
    if (oledDisplay && oledDisplay->isOn()) {
        static char freqStr[16];
        snprintf(freqStr, sizeof(freqStr), "%.3f", cfg.frequency);
        oledDisplay->showScanningStatus(freqStr, cfg.spreadingFactor,
                                        reconState.scanState.currentConfig,
                                        reconState.getNumConfigs());
        oledDisplay->update();  // Render immediately during setup
    }
    
    // Initialize SD card logger
    if (packetLogger.initialize()) {
        LOG_INFO("SD card logging available");
        packetLogger.startSession();
    } else {
        LOG_INFO("SD card not present - continuing without logging");
    }
    
    // Initialize PSK decryption system
    PSKDecryption::initialize();
    
    // Initialize command handler
    commandHandler = new CommandHandler(this);
    LOG_INFO("Command handler initialized");
    
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
            delay(Config::UI::BUTTON_DEBOUNCE_MS);
            return;
            
        case MODE_PACKET_REPLAY:
            // Packet replay is handled via command handler
            delay(Config::UI::BUTTON_DEBOUNCE_MS);
            return;
    }
    
    delay(Config::UI::LOOP_POLLING_DELAY_MS);
}

// Apply radio configuration for scanning
// Handle reconnaissance mode operations
void LoRaReconTool::handleReconnaissanceMode(uint32_t now) {
    // Handle frequency switching in recon mode
    if (now - reconState.scanState.lastScanSwitch >= Config::Scanning::DWELL_TIME_MS) {
        reconState.scanState.currentConfig = (reconState.scanState.currentConfig + 1) % reconState.getNumConfigs();
        reconState.scanState.lastScanSwitch = now;
        
        // Show progress every full cycle (when back to config 0)
        if (reconState.scanState.currentConfig == 0) {
            uint32_t elapsed = (now - reconState.scanState.reconStartTime) / 1000;
            LOG_INFO("Cycle complete - %u seconds elapsed, %d targetable devices found", 
                     (unsigned int)elapsed, reconState.numTargetableDevices);
        }
        
        const ScanConfig& cfg = reconState.getScanConfig(reconState.scanState.currentConfig);
        if (radioController->applyConfig(cfg)) {
            radioController->startReceive();
            
            // Update display with new scanning config
            if (oledDisplay && oledDisplay->isOn()) {
                static char freqStr[16];  // Static to avoid stack issues
                snprintf(freqStr, sizeof(freqStr), "%.3f", cfg.frequency);
                oledDisplay->showScanningStatus(freqStr, cfg.spreadingFactor, 
                                                reconState.scanState.currentConfig, 
                                                reconState.getNumConfigs());
            }
        }
    }
    
    // Handle packets in recon mode
    handlePacketReception();
}

// Handle targeted capture mode operations
void LoRaReconTool::handleTargetedCaptureMode(uint32_t now) {
    // Update display to show targeting status
    if (oledDisplay && oledDisplay->isOn()) {
        const ScanConfig& cfg = reconState.getScanConfig(reconState.scanState.targetConfig);
        static char targetInfo[32];
        snprintf(targetInfo, sizeof(targetInfo), "%.3f MHz", cfg.frequency);
        oledDisplay->showTargetingMode(targetInfo);
    }
    
    handlePacketReception();
}

// Unified packet reception handler
void LoRaReconTool::handlePacketReception() {
    // Check if radio has packet available
    if (!radioController->hasPacket()) return;
    
    // Quick read from radio and queue it
    uint8_t tempBuffer[256];
    int length = radioController->readPacket(tempBuffer, sizeof(tempBuffer));
    
    if (length > 0) {
        // Get signal metrics (only for larger packets to save time)
        float rssi = 0;
        float snr = 0;
        if (length >= 40) {
            rssi = radioController->getRSSI();
            snr = radioController->getSNR();
        }
        
        // Queue the packet for processing
        uint8_t configIndex = reconState.scanState.currentConfig;
        float frequency = reconState.getScanConfig(configIndex).frequency;
        if (!packetProcessor->queuePacket(tempBuffer, length, rssi, snr, configIndex, frequency)) {
            Serial.println("[WARNING] Packet queue full - packet dropped!");
        }
    }
    
    // Immediately restart receiving
    radioController->startReceive();
    
    // Process queued packets
    packetProcessor->processQueue(oledDisplay);
}

// Start targeted capture mode on specific device
void LoRaReconTool::startTargetedCapture(uint8_t deviceIndex) {
    const TargetableDevice& target = reconState.getTargetableDevice(deviceIndex);
    
    reconState.scanState.mode = MODE_TARGETED_CAPTURE;
    reconState.scanState.targetConfig = target.configIndex;
    reconState.scanState.targetedByDevice = true;  // Device targeting
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
    
    const ScanConfig& targetCfg = reconState.getScanConfig(reconState.scanState.targetConfig);
    
    // Update display to show targeting mode
    if (oledDisplay && oledDisplay->isOn()) {
        static char targetInfo[32];
        snprintf(targetInfo, sizeof(targetInfo), "0x%08X", target.nodeId);
        oledDisplay->showTargetingMode(targetInfo);
    }
    
    radioController->applyConfig(targetCfg);
    radioController->startReceive();
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
    reconState.scanState.targetedByDevice = false;  // Frequency targeting
    reconState.scanState.currentConfig = configIndex;
    
    // Update display to show targeting mode
    if (oledDisplay && oledDisplay->isOn()) {
        static char targetInfo[32];
        snprintf(targetInfo, sizeof(targetInfo), "%.3f MHz", cfg.frequency);
        oledDisplay->showTargetingMode(targetInfo);
    }
    
    if (radioController->applyConfig(cfg)) {
        radioController->startReceive();
        Serial.println("✅ Frequency targeting active");
    } else {
        Serial.println("❌ Failed to configure radio for targeting");
        reconState.scanState.mode = MODE_INTERACTIVE_MENU;
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
                  reconState.getNumCapturedPackets(), Config::Replay::MAX_SLOTS);
    
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
    Serial.flush();  // Ensure prompt is sent
    
    // Clear any stale input
    while (Serial.available()) {
        Serial.read();
    }
    
    uint32_t startTime = millis();
    String input = "";
    bool gotInput = false;
    
    while (!gotInput && (millis() - startTime < 30000)) {
        esp_task_wdt_reset();
        
        if (Serial.available()) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
                if (input.length() > 0) {
                    gotInput = true;
                }
            } else if (c >= '0' && c <= '9') {
                input += c;
                Serial.print(c);  // Echo the character
            }
        }
        delay(10);
    }
    
    if (!gotInput) {
        Serial.println("\n[TIMEOUT] Cancelled.");
        delay(1000);
        showReplayMenu();
        return;
    }
    
    int repeatCount = input.toInt();
    Serial.printf("\n[INPUT] Received: %d\n", repeatCount);
    
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
        Serial.flush();
        
        // Clear any stale input
        while (Serial.available()) {
            Serial.read();
        }
        
        startTime = millis();
        input = "";
        gotInput = false;
        
        while (!gotInput && (millis() - startTime < 10000)) {
            esp_task_wdt_reset();
            
            if (Serial.available()) {
                char c = Serial.read();
                if (c == '\n' || c == '\r') {
                    if (input.length() > 0) {
                        gotInput = true;
                    } else {
                        // Empty input = use default
                        gotInput = true;
                        input = "1000";
                    }
                } else if (c >= '0' && c <= '9') {
                    input += c;
                    Serial.print(c);  // Echo
                }
            }
            delay(10);
        }
        
        if (gotInput) {
            int userDelay = input.toInt();
            if (userDelay >= 100 && userDelay <= 10000) {
                delayMs = userDelay;
                Serial.printf("\n[INPUT] Using %d ms delay\n", delayMs);
            } else {
                Serial.printf("\n[INVALID] Using default %d ms\n", delayMs);
            }
        } else {
            Serial.printf("\n[TIMEOUT] Using default %d ms\n", delayMs);
        }
    }
    
    // Apply the correct radio configuration
    const ScanConfig& replayCfg = reconState.getScanConfig(pkt.configIndex);
    if (!radioController->applyConfig(replayCfg)) {
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
    radioController->getRadio().setOutputPower(10);  // Set transmission power
    
    int successCount = 0;
    int failCount = 0;
    
    for (int i = 0; i < repeatCount; i++) {
        Serial.printf("[%d/%d] ", i + 1, repeatCount);
        
        // Feed watchdog before transmission (can take several seconds with high SF)
        esp_task_wdt_reset();
        int state = radioController->getRadio().transmit(txBuffer, pkt.length);
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
    
    radioController->getRadio().setOutputPower(0);   // Back to receive-only mode
    
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
    const ScanConfig& resumeCfg = reconState.getScanConfig(reconState.scanState.currentConfig);
    radioController->applyConfig(resumeCfg);
    radioController->startReceive();
    
    showReplayMenu();
}

// Handle button press for display toggle and power off
void LoRaReconTool::handleButtonPress(uint32_t now) {
    bool currentButtonState = (digitalRead(Config::Hardware::USER_BUTTON) == LOW);  // Active low
    
    // Detect button press (transition from not pressed to pressed)
    if (currentButtonState && !buttonPressed) {
        buttonPressed = true;
        buttonPressStart = now;
        LOG_DEBUG("Button pressed");
    }
    
    // Button is being held down
    if (currentButtonState && buttonPressed) {
        uint32_t pressDuration = now - buttonPressStart;
        
        // Long press = shutdown
        if (pressDuration >= Config::UI::BUTTON_LONG_PRESS_MS && !shutdownInitiated) {
            shutdownInitiated = true;
            LOG_INFO("🔴 SHUTDOWN INITIATED (long press detected)");
            
            // Show shutdown on display
            if (oledDisplay) {
                oledDisplay->showShutdown();
                delay(100);  // Ensure display updates
            }
            
            // Stop radio safely
            LOG_INFO("Stopping radio...");
            radioController->getRadio().standby();
            radioController->getRadio().sleep();
            LOG_INFO("Radio stopped");
            
            LOG_INFO("✅ Safe to remove power");
            LOG_INFO("Device will enter deep sleep in %dms...", Config::UI::SHUTDOWN_WARNING_MS);
            Serial.flush();
            
            delay(Config::UI::SHUTDOWN_WARNING_MS);
            
            // Enter deep sleep (effectively powered off until reset)
            LOG_INFO("Entering deep sleep mode");
            Serial.flush();
            esp_deep_sleep_start();
        }
    }
    
    // Button released
    if (!currentButtonState && buttonPressed) {
        uint32_t pressDuration = now - buttonPressStart;
        buttonPressed = false;
        
        // Short press = toggle display
        if (pressDuration < Config::UI::BUTTON_LONG_PRESS_MS && !shutdownInitiated) {
            LOG_DEBUG("Short press detected - toggling display");
            if (oledDisplay) {
                oledDisplay->toggle();
                if (oledDisplay->isOn()) {
                    // Refresh display with current info - with safety checks
                    if (reconState.isValidConfigIndex(reconState.scanState.currentConfig)) {
                        const ScanConfig& cfg = reconState.getScanConfig(reconState.scanState.currentConfig);
                        // Validate frequency is in reasonable range
                        if (cfg.frequency >= 100.0 && cfg.frequency <= 1000.0) {
                            static char freqStr[16];  // Static to avoid stack issues
                            snprintf(freqStr, sizeof(freqStr), "%.3f", cfg.frequency);
                            oledDisplay->showScanningStatus(freqStr, cfg.spreadingFactor, 
                                                            reconState.scanState.currentConfig, 
                                                            reconState.getNumConfigs());
                        } else {
                            LOG_ERROR("Invalid frequency: %.3f", cfg.frequency);
                        }
                    } else {
                        LOG_ERROR("Invalid config index: %d", reconState.scanState.currentConfig);
                    }
                }
            }
        }
    }
}

// Set web server for live packet broadcasting
void LoRaReconTool::setWebServer(WebServer* ws) {
    if (packetProcessor && ws) {
        // Wire up callback from PacketProcessor to WebServer
        packetProcessor->setPacketCallback([ws](const PacketEvent& evt) {
            ws->handlePacketEvent(evt);
        });
    }
}
