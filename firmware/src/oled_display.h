#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "data_structures.h"
#include "recon_state.h"

// OLED Display configuration for Heltec WiFi LoRa 32 V3
// 128x64 OLED connected via I2C
// Note: V3 uses different pins than V2!
#define OLED_SDA    17  // GPIO 17 for V3
#define OLED_SCL    18  // GPIO 18 for V3
#define OLED_RST    21  // GPIO 21 (RST) - REQUIRED for this board variant! DO NOT SHARE WITH OTHER PERIPHERALS
#define OLED_VEXT   36  // GPIO 36 (Vext - power control, active LOW)

/**
 * OLED Display Manager
 * 
 * Handles display initialization, screen updates, and info display
 * for the reconnaissance tool. Displays:
 * - Current scanning status
 * - Packet statistics
 * - Device count
 * - Last packet info (RSSI, SNR, protocol)
 * - System status
 */
class OLEDDisplay {
public:
    OLEDDisplay();
    
    // Lifecycle
    bool initialize();
    bool reinitialize();  // Try to recover if display stops working
    void update();
    
    // Display control
    void turnOn();
    void turnOff();
    bool isOn() const { return displayOn; }
    void toggle();
    
    // Content updates
    void showWelcome();
    void showScanningStatus(const char* frequency, uint8_t sf, uint8_t configIndex, uint8_t totalConfigs);
    void showPacketReceived(float rssi, float snr, const char* protocol, uint32_t nodeId);
    void showDeviceCount(uint8_t rfActivity, uint8_t trackedNodes, uint8_t targetableDevices);
    void showTargetingMode(const char* targetInfo);
    void showShutdown();
    
    // Auto-off timer (turns off display after inactivity)
    void resetAutoOffTimer();
    void setAutoOffTimeout(uint32_t timeoutMs);
    
private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C display;
    bool displayOn;
    uint32_t lastActivityTime;
    uint32_t autoOffTimeout;  // Milliseconds (0 = disabled)
    
    // Display state
    enum DisplayMode {
        MODE_WELCOME,
        MODE_SCANNING,
        MODE_PACKET_INFO,
        MODE_DEVICE_LIST,
        MODE_TARGETING,
        MODE_SHUTDOWN
    };
    
    DisplayMode currentMode;
    
    // Cached info for display
    struct DisplayInfo {
        char frequency[16];
        uint8_t sf;
        uint8_t configIndex;
        uint8_t totalConfigs;
        float lastRSSI;
        float lastSNR;
        char lastProtocol[16];
        uint32_t lastNodeId;
        uint8_t rfActivityCount;
        uint8_t trackedNodeCount;
        uint8_t targetableDeviceCount;
        uint32_t totalPackets;
        char targetInfo[32];
    } info;
    
    // Display rendering helpers
    void drawHeader(const char* title);
    void drawFooter(const char* text);
    void renderWelcome();
    void renderScanning();
    void renderPacketInfo();
    void renderDeviceList();
    void renderTargeting();
    void renderShutdown();
    
    // Utility
    void clearInfo();
};

#endif // OLED_DISPLAY_H
