#include "oled_display.h"
#include <Wire.h>

OLEDDisplay::OLEDDisplay()
    : display(U8G2_R0, OLED_RST, OLED_SCL, OLED_SDA)
    , displayOn(false)
    , lastActivityTime(0)
    , autoOffTimeout(30000)  // 30 seconds default
    , currentMode(MODE_WELCOME)
{
    clearInfo();
}

bool OLEDDisplay::initialize() {
    Serial.print("[DISPLAY] Initializing OLED... ");
    
    // Initialize I2C with custom pins
    Wire.begin(OLED_SDA, OLED_SCL);
    
    // Initialize display
    if (!display.begin()) {
        Serial.println("FAILED");
        return false;
    }
    
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.sendBuffer();
    
    displayOn = true;
    lastActivityTime = millis();
    
    Serial.println("OK");
    Serial.printf("[DISPLAY] OLED ready (128x64, I2C: SDA=%d, SCL=%d)\n", OLED_SDA, OLED_SCL);
    
    return true;
}

void OLEDDisplay::update() {
    if (!displayOn) {
        return;
    }
    
    // Check auto-off timeout
    if (autoOffTimeout > 0 && (millis() - lastActivityTime > autoOffTimeout)) {
        turnOff();
        return;
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
    
    // Spreading Factor
    snprintf(buffer, sizeof(buffer), "SF: %d", info.sf);
    display.drawStr(0, 48, buffer);
    
    // Packet count
    snprintf(buffer, sizeof(buffer), "Pkts: %lu", info.totalPackets);
    display.drawStr(64, 48, buffer);
    
    drawFooter("Press for menu");
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
        snprintf(buffer, sizeof(buffer), "Node: %08lX", info.lastNodeId);
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
    snprintf(buffer, sizeof(buffer), "Packets: %lu", info.totalPackets);
    display.drawStr(0, 44, buffer);
    
    snprintf(buffer, sizeof(buffer), "RSSI: %.1f dBm", info.lastRSSI);
    display.drawStr(0, 56, buffer);
}

void OLEDDisplay::renderShutdown() {
    display.setFont(u8g2_font_9x15_tf);
    display.drawStr(15, 30, "SHUTDOWN");
    
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(5, 50, "Safe to power off");
}
