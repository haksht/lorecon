#include "oled_display.h"
#include <Wire.h>
#include <esp_task_wdt.h>  // For watchdog reset during init

OLEDDisplay::OLEDDisplay()
    : display(U8G2_R0, OLED_RST)  // HW I2C: rotation, reset - uses Wire object we configure
    , displayOn(false)
    , lastActivityTime(0)
    , autoOffTimeout(0)  // 0 = disabled (display stays on)
    , currentMode(MODE_WELCOME)
{
    clearInfo();
}

bool OLEDDisplay::initialize() {
    Serial.println("[DISPLAY] Initializing OLED...");
    
    // Step 1: Enable Vext power (always, regardless of previous state)
    Serial.printf("[DISPLAY] Configuring Vext pin %d (active LOW)...\n", OLED_VEXT);
    pinMode(OLED_VEXT, OUTPUT);
    digitalWrite(OLED_VEXT, LOW);  // Turn on power to display
    delay(100);  // Wait for power stabilization
    
    // Step 2: Send reset pulse to OLED (ALWAYS do this - handles unknown states)
    // This is critical after power cycling, brownouts, or undefined states
    Serial.printf("[DISPLAY] Sending reset pulse on pin %d...\n", OLED_RST);
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);   // Assert reset (active LOW)
    delay(20);                      // Hold reset for 20ms (conservative)
    digitalWrite(OLED_RST, HIGH);  // De-assert reset
    delay(50);                      // Wait for OLED to finish reset sequence
    
    // Reset watchdog before potentially long I2C operations
    esp_task_wdt_reset();
    
    // Step 3: Initialize I2C with custom pins
    Serial.printf("[DISPLAY] Initializing I2C: SDA=%d, SCL=%d\n", OLED_SDA, OLED_SCL);
    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.setClock(100000);  // 100kHz for reliability
    delay(50);
    
    // Reset watchdog again
    esp_task_wdt_reset();
    
    // Step 4: Scan I2C to confirm device presence (with retry)
    Serial.println("[DISPLAY] Scanning for OLED at 0x3C...");
    bool deviceFound = false;
    
    // Try up to 3 times with increasing delays
    for (int attempt = 1; attempt <= 3; attempt++) {
        Wire.beginTransmission(0x3C);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            deviceFound = true;
            Serial.printf("[DISPLAY] ✓ OLED found at 0x3C (attempt %d)\n", attempt);
            break;
        }
        
        if (attempt < 3) {
            Serial.printf("[DISPLAY]   Attempt %d: Not found (error %d), retrying...\n", attempt, error);
            delay(50 * attempt);  // Progressive delay: 50ms, 100ms
        } else {
            Serial.printf("[DISPLAY] ❌ No device found at 0x3C after %d attempts (error %d)\n", attempt, error);
        }
    }
    
    if (!deviceFound) {
        Serial.println("[DISPLAY] OLED initialization failed - continuing without display");
        Serial.println("[DISPLAY] This is normal if:");
        Serial.println("[DISPLAY]   - Board variant without OLED");
        Serial.println("[DISPLAY]   - OLED hardware fault");
        Serial.println("[DISPLAY] Tool will continue to work via serial monitor.");
        return false;
    }
    
    // Step 5: Initialize U8g2 display (with retry)
    Serial.println("[DISPLAY] Initializing U8g2 library...");
    
    bool initSuccess = false;
    for (int attempt = 1; attempt <= 2; attempt++) {
        // Reset watchdog before display init
        esp_task_wdt_reset();
        
        // Set I2C clock for U8g2
        display.setBusClock(100000);
        
        // Initialize display
        if (display.begin()) {
            initSuccess = true;
            Serial.printf("[DISPLAY] ✓ U8g2 initialized successfully (attempt %d)\n", attempt);
            break;
        }
        
        if (attempt < 2) {
            Serial.printf("[DISPLAY]   Attempt %d: U8g2 begin() failed, retrying...\n", attempt);
            delay(100);
            
            // Try another reset pulse before retry
            digitalWrite(OLED_RST, LOW);
            delay(10);
            digitalWrite(OLED_RST, HIGH);
            delay(50);
        } else {
            Serial.println("[DISPLAY] ❌ U8g2 begin() failed after retries");
        }
    }
    
    if (!initSuccess) {
        Serial.println("[DISPLAY] Could not initialize display library - continuing without display");
        return false;
    }
    
    // Reset watchdog after init
    esp_task_wdt_reset();
    
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.sendBuffer();
    
    displayOn = true;
    lastActivityTime = millis();
    
    Serial.println("OK");
    Serial.printf("[DISPLAY] OLED ready (128x64, I2C: SDA=%d, SCL=%d, Vext=%d)\n", 
                  OLED_SDA, OLED_SCL, OLED_VEXT);
    
    return true;
}

void OLEDDisplay::update() {
    if (!displayOn) {
        return;
    }
    
    // Never turn off during shutdown
    if (currentMode != MODE_SHUTDOWN) {
        // Check auto-off timeout
        if (autoOffTimeout > 0 && (millis() - lastActivityTime > autoOffTimeout)) {
            turnOff();
            return;
        }
    }
    
    // Render current mode
    display.clearBuffer();
    
    switch (currentMode) {
        case MODE_WELCOME:
            renderWelcome();
            break;
        case MODE_SCANNING:
            renderScanning();
            break;
        case MODE_PACKET_INFO:
            renderPacketInfo();
            break;
        case MODE_DEVICE_LIST:
            renderDeviceList();
            break;
        case MODE_TARGETING:
            renderTargeting();
            break;
        case MODE_SHUTDOWN:
            renderShutdown();
            break;
    }
    
    display.sendBuffer();
}

bool OLEDDisplay::reinitialize() {
    Serial.println("[DISPLAY] Attempting to reinitialize OLED...");
    
    // Turn off display first
    if (displayOn) {
        display.setPowerSave(1);
        displayOn = false;
    }
    
    // Power cycle the OLED
    digitalWrite(OLED_VEXT, HIGH);  // Power OFF
    delay(100);
    digitalWrite(OLED_VEXT, LOW);   // Power ON
    delay(100);
    
    // Send reset pulse
    digitalWrite(OLED_RST, LOW);
    delay(20);
    digitalWrite(OLED_RST, HIGH);
    delay(50);
    
    // Try to reinit U8g2
    display.setBusClock(100000);
    if (display.begin()) {
        Serial.println("[DISPLAY] ✓ Reinitialize successful!");
        displayOn = false;  // Start in off state
        return true;
    }
    
    Serial.println("[DISPLAY] ✗ Reinitialize failed");
    return false;
}

void OLEDDisplay::turnOn() {
    if (!displayOn) {
        display.setPowerSave(0);
        displayOn = true;
        lastActivityTime = millis();
        Serial.println("[DISPLAY] Display ON");
    }
}

void OLEDDisplay::turnOff() {
    if (displayOn) {
        display.setPowerSave(1);
        displayOn = false;
        Serial.println("[DISPLAY] Display OFF");
    }
}

void OLEDDisplay::toggle() {
    if (displayOn) {
        turnOff();
    } else {
        turnOn();
    }
}

void OLEDDisplay::resetAutoOffTimer() {
    lastActivityTime = millis();
}

void OLEDDisplay::setAutoOffTimeout(uint32_t timeoutMs) {
    autoOffTimeout = timeoutMs;
}

void OLEDDisplay::showWelcome() {
    currentMode = MODE_WELCOME;
    resetAutoOffTimer();
    update();
}

void OLEDDisplay::showScanningStatus(const char* frequency, uint8_t sf, uint8_t configIndex, uint8_t totalConfigs) {
    currentMode = MODE_SCANNING;
    strncpy(info.frequency, frequency, sizeof(info.frequency) - 1);
    info.sf = sf;
    info.configIndex = configIndex;
    info.totalConfigs = totalConfigs;
    resetAutoOffTimer();
}

void OLEDDisplay::showPacketReceived(float rssi, float snr, const char* protocol, uint32_t nodeId) {
    currentMode = MODE_PACKET_INFO;
    info.lastRSSI = rssi;
    info.lastSNR = snr;
    strncpy(info.lastProtocol, protocol, sizeof(info.lastProtocol) - 1);
    info.lastNodeId = nodeId;
    info.totalPackets++;
    resetAutoOffTimer();
}

void OLEDDisplay::showDeviceCount(uint8_t rfActivity, uint8_t trackedNodes, uint8_t targetableDevices) {
    currentMode = MODE_DEVICE_LIST;
    info.rfActivityCount = rfActivity;
    info.trackedNodeCount = trackedNodes;
    info.targetableDeviceCount = targetableDevices;
    resetAutoOffTimer();
}

void OLEDDisplay::showTargetingMode(const char* targetInfo) {
    currentMode = MODE_TARGETING;
    strncpy(info.targetInfo, targetInfo, sizeof(info.targetInfo) - 1);
    resetAutoOffTimer();
}

void OLEDDisplay::showShutdown() {
    currentMode = MODE_SHUTDOWN;
    update();
}

void OLEDDisplay::clearInfo() {
    memset(&info, 0, sizeof(info));
    strcpy(info.lastProtocol, "None");
    strcpy(info.frequency, "---");
}

// Display rendering helpers

void OLEDDisplay::drawHeader(const char* title) {
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(0, 10, title);
    display.drawHLine(0, 12, 128);
}

void OLEDDisplay::drawFooter(const char* text) {
    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(0, 63, text);
}

void OLEDDisplay::renderWelcome() {
    display.setFont(u8g2_font_9x15_tf);
    display.drawStr(10, 20, "ESP32 LoRa");
    display.drawStr(5, 38, "Recon Tool");
    
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(15, 55, "Initializing...");
}

void OLEDDisplay::renderScanning() {
    drawHeader("SCANNING");
    
    display.setFont(u8g2_font_6x10_tf);
    
    // Config number
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Config: %d/%d", info.configIndex + 1, info.totalConfigs);
    display.drawStr(0, 24, buffer);
    
    // Frequency
    snprintf(buffer, sizeof(buffer), "Freq: %s MHz", info.frequency);
    display.drawStr(0, 36, buffer);
    
    // Spreading Factor and Packet count
    snprintf(buffer, sizeof(buffer), "SF:%d Pkts:%u", info.sf, info.totalPackets);
    display.drawStr(0, 48, buffer);
    
    // Button help at bottom
    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(0, 62, "Tap:Hide Hold:OFF");
    
    display.sendBuffer();
}

void OLEDDisplay::renderPacketInfo() {
    drawHeader("PACKET RX");
    
    display.setFont(u8g2_font_6x10_tf);
    
    char buffer[32];
    
    // Protocol
    snprintf(buffer, sizeof(buffer), "Proto: %s", info.lastProtocol);
    display.drawStr(0, 24, buffer);
    
    // Node ID (if available)
    if (info.lastNodeId != 0) {
        snprintf(buffer, sizeof(buffer), "Node: %08X", info.lastNodeId);
        display.drawStr(0, 36, buffer);
    }
    
    // RSSI and SNR
    snprintf(buffer, sizeof(buffer), "RSSI: %.1f dBm", info.lastRSSI);
    display.drawStr(0, 48, buffer);
    
    snprintf(buffer, sizeof(buffer), "SNR: %.1f dB", info.lastSNR);
    display.drawStr(0, 60, buffer);
}

void OLEDDisplay::renderDeviceList() {
    drawHeader("DEVICES");
    
    display.setFont(u8g2_font_6x10_tf);
    
    char buffer[32];
    
    // RF Activity
    snprintf(buffer, sizeof(buffer), "RF Act: %d", info.rfActivityCount);
    display.drawStr(0, 28, buffer);
    
    // Tracked Nodes
    snprintf(buffer, sizeof(buffer), "Tracked: %d", info.trackedNodeCount);
    display.drawStr(0, 42, buffer);
    
    // Targetable Devices
    snprintf(buffer, sizeof(buffer), "Targets: %d", info.targetableDeviceCount);
    display.drawStr(0, 56, buffer);
}

void OLEDDisplay::renderTargeting() {
    drawHeader("TARGETING");
    
    display.setFont(u8g2_font_6x10_tf);
    
    // Target info (wrapped if needed)
    display.drawStr(0, 28, info.targetInfo);
    
    char buffer[32];
    
    // Current stats
    snprintf(buffer, sizeof(buffer), "Packets: %u", info.totalPackets);
    display.drawStr(0, 44, buffer);
    
    snprintf(buffer, sizeof(buffer), "RSSI: %.1f dBm", info.lastRSSI);
    display.drawStr(0, 56, buffer);
}

void OLEDDisplay::renderShutdown() {
    // Large, centered, persistent shutdown message
    display.setFont(u8g2_font_10x20_tf);
    display.drawStr(10, 25, "SHUTDOWN");
    
    display.setFont(u8g2_font_8x13_tf);
    display.drawStr(5, 45, "Safe to remove");
    display.drawStr(25, 60, "power now");
}
