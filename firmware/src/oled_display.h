#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#if defined(BOARD_HELTEC_V3) || defined(BOARD_T3_S3) || defined(BOARD_TBEAM_SUPREME)

#include <Arduino.h>
#include <U8g2lib.h>
#include "data_structures.h"
#include "recon_state.h"
#include "config.h"

// OLED Display configuration - board-specific pins
// 128x64 OLED connected via I2C on all boards

#if defined(BOARD_T3_S3)
    // LilyGO T3-S3: I2C pins explicitly configured
    #define OLED_SDA    18  // GPIO 18
    #define OLED_SCL    17  // GPIO 17
    #define OLED_RST    -1  // No hardware reset pin (software reset)
    #define OLED_VEXT   -1  // No Vext power control (always powered)
#elif defined(BOARD_TBEAM_SUPREME)
    // LilyGO T-Beam Supreme: SH1106 128x64 on dedicated I2C bus
    // AXP2101 ALDO1 powers the display (always on after PMU init)
    #define OLED_SDA    17  // GPIO 17
    #define OLED_SCL    18  // GPIO 18
    #define OLED_RST    -1  // No hardware reset pin (software reset)
    #define OLED_VEXT   -1  // No Vext control  -  powered by AXP2101 ALDO1
#elif defined(BOARD_HELTEC_V3)
    // Heltec WiFi LoRa 32 V3: Uses default I2C pins with Vext control
    #define OLED_SDA    17  // GPIO 17 for V3
    #define OLED_SCL    18  // GPIO 18 for V3
    #define OLED_RST    21  // GPIO 21 (RST) - REQUIRED for this board variant
    #define OLED_VEXT   36  // GPIO 36 (Vext - power control, active LOW)
#endif

// T-Beam Supreme has SH1106 controller; all other boards use SSD1306.
// Both controllers are pin/protocol compatible but need the correct U8g2 driver.
#if defined(BOARD_TBEAM_SUPREME)
    using DisplayDriver = U8G2_SH1106_128X64_NONAME_F_HW_I2C;
#else
    using DisplayDriver = U8G2_SSD1306_128X64_NONAME_F_HW_I2C;
#endif

/**
 * OLED Display Manager
 *
 * Handles display initialization, screen updates, and info display
 * for the reconnaissance tool. Displays:
 * - Current scanning status
 * - Packet statistics
 * - Device count
 * - Last packet info (RSSI, SNR, protocol)
 * - GPS fix status (when HAS_GPS is defined)
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
    void showBootProgress(const char* stage, uint8_t step, uint8_t totalSteps);
    void showScanningStatus(const char* frequency, uint8_t sf, uint8_t configIndex, uint8_t totalConfigs);
    void showPacketReceived(float rssi, float snr, const char* protocol, uint32_t nodeId);
    void showDeviceCount(uint8_t rfActivity, uint8_t trackedNodes, uint8_t targetableDevices);
    void showTargetingMode(const char* targetInfo);
    void showShutdown();
    void showReboot();
    void clearAndOff();  // Blank GDDRAM and power off (no message shown)
    void showApiToken(const char* token);  // Display API token for mobile users

    // Network info display
    void setNetworkInfo(const char* ipAddr, const char* mdnsName);

    // GPS status (updated each main loop when HAS_GPS is defined)
    void setGpsStatus(bool hasFix, uint32_t satellites);

    // Auto-off timer (turns off display after inactivity)
    void resetAutoOffTimer();
    void setAutoOffTimeout(uint32_t timeoutMs);

private:
    DisplayDriver display;
    bool displayOn;
    uint32_t lastActivityTime;
    uint32_t autoOffTimeout;      // Milliseconds (0 = disabled)
    bool needsRedraw_;            // Set true whenever content changes; cleared after sendBuffer()
    uint32_t lastRenderedPackets_; // Tracks reconState packet count to detect live-data changes

    // Display state
    enum DisplayMode {
        MODE_WELCOME,
        MODE_BOOT,
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
        char targetInfo[32];
        char ipAddress[20];     // IP address (e.g., "192.168.1.100")
        char mdnsName[24];      // mDNS hostname (e.g., "lora-a1b2c3")
        char bootStage[24];     // Current boot stage description
        uint8_t bootStep;       // Current boot step (1-based)
        uint8_t bootTotalSteps; // Total boot steps
        bool gpsHasFix;         // GPS fix status (HAS_GPS boards only)
        uint32_t gpsSatellites; // Number of satellites in fix
    } info;

    // Display rendering helpers
    void drawHeader(const char* title);
    void drawFooter(const char* text);
    void renderWelcome();
    void renderBoot();
    void renderScanning();
    void renderPacketInfo();
    void renderDeviceList();
    void renderTargeting();
    void renderShutdown();

    // Utility
    void clearInfo();
};

#endif // BOARD_HELTEC_V3 || BOARD_T3_S3 || BOARD_TBEAM_SUPREME
#endif // OLED_DISPLAY_H
