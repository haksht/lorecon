/**
 * TFT Display Implementation for LilyGO T-Deck
 * 
 * 320x240 ST7789 TFT display with color support
 */

#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#if defined(BOARD_TDECK) || defined(BOARD_TDECK_PLUS)

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "data_structures.h"
#include "display_interface.h"
#include "config.h"

/**
 * TFT Display Manager for T-Deck using Arduino_GFX
 * 
 * Larger color display with more space for information.
 * Can show more details than the small OLED.
 */
class TFTDisplay : public IDisplay {
public:
    TFTDisplay();
    
    // Lifecycle
    bool initialize() override;
    bool reinitialize() override;
    void update() override;
    
    // Display control
    void turnOn() override;
    void turnOff() override;
    bool isOn() const override { return displayOn; }
    void toggle() override;
    
    // Simple drawing helpers (used by T-Deck test harness in main.cpp)
    void clear();
    void drawText(int16_t x, int16_t y, const char* text, uint8_t size);
    
    // Content updates
    void showWelcome() override;
    void showScanningStatus(const char* frequency, uint8_t sf, uint8_t configIndex, uint8_t totalConfigs) override;
    void showPacketReceived(float rssi, float snr, const char* protocol, uint32_t nodeId) override;
    void showDeviceCount(uint8_t rfActivity, uint8_t trackedNodes, uint8_t targetableDevices) override;
    void showTargetingMode(const char* targetInfo) override;
    void showShutdown() override;
    
    // Auto-off timer
    void resetAutoOffTimer() override;
    void setAutoOffTimeout(uint32_t timeoutMs) override;
    
    // Force redraw after hardware events
    void forceRefresh() {
        if (displayOn && gfx) {
            Serial.println("[TFT] Force refresh - redrawing current mode");
            switch(currentMode) {
                case MODE_WELCOME: renderWelcome(); break;
                case MODE_SCANNING: renderScanning(); break;
                case MODE_PACKET_INFO: renderPacketInfo(); break;
                case MODE_DEVICE_LIST: renderDeviceList(); break;
                case MODE_TARGETING: renderTargeting(); break;
                case MODE_SHUTDOWN: renderShutdown(); break;
            }
        }
    }
    
    // Display capabilities
    bool supportsColor() const override { return true; }
    bool supportsTouch() const override { return false; }
    uint16_t getWidth() const override { return width; }
    uint16_t getHeight() const override { return height; }
    const char* getDisplayType() const override { return displayLabel; }
    
private:
    Arduino_DataBus *bus;
    Arduino_GFX *gfx;
    bool displayOn;
    uint32_t lastActivityTime;
    uint32_t autoOffTimeout;
    uint16_t width;
    uint16_t height;
    char displayLabel[32];
    
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
    void drawHeader(const char* title, uint16_t bgColor);
    void drawStatusBar();
    void renderWelcome();
    void renderScanning();
    void renderPacketInfo();
    void renderDeviceList();
    void renderTargeting();
    void renderShutdown();
    
    // Color scheme (RGB565)
    static constexpr uint16_t COLOR_BG = 0x0000;        // Black
    static constexpr uint16_t COLOR_TEXT = 0xFFFF;      // White
    static constexpr uint16_t COLOR_HEADER = 0x001F;    // Blue
    static constexpr uint16_t COLOR_ACCENT = 0x07FF;    // Cyan
    static constexpr uint16_t COLOR_SUCCESS = 0x07E0;   // Green
    static constexpr uint16_t COLOR_WARNING = 0xFD20;   // Orange
    static constexpr uint16_t COLOR_ERROR = 0xF800;     // Red
    static constexpr uint16_t HEADER_HEIGHT = 30;
    static constexpr uint16_t STATUS_BAR_HEIGHT = 26;
    
    // Utility
    void clearInfo();
    void setBacklight(bool on);
};

#endif // BOARD_TDECK variants
#endif // TFT_DISPLAY_H
