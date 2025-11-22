/**
 * TFT Display Implementation for LilyGO T-Deck using Arduino_GFX
 */

#if defined(BOARD_TDECK) || defined(BOARD_TDECK_PLUS)

#include "tft_display.h"
#include "config.h"
#include <SPI.h>

TFTDisplay::TFTDisplay()
    : bus(nullptr)
    , gfx(nullptr)
    , displayOn(false)
    , lastActivityTime(0)
    , autoOffTimeout(0)
    , width(Config::Hardware::ACTIVE_BOARD.display.width)
    , height(Config::Hardware::ACTIVE_BOARD.display.height)
    , currentMode(MODE_WELCOME)
{
    clearInfo();
    snprintf(displayLabel, sizeof(displayLabel), "%s %dx%d",
             Config::Hardware::DISPLAY_TYPE == DisplayDriver::GC9A01 ? "GC9A01" : "ST7789",
             width,
             height);
}

bool TFTDisplay::initialize() {
    Serial.println("[TFT] Initializing T-Deck TFT display with Arduino_GFX...");

    // 1) Enable peripheral power
    if (Config::Hardware::PERI_POWERON >= 0) {
        pinMode(Config::Hardware::PERI_POWERON, OUTPUT);
        digitalWrite(Config::Hardware::PERI_POWERON, HIGH);
        delay(100);
    }

    // 2) Configure backlight but keep off during init
    if (Config::Hardware::TDECK_TFT_BL >= 0) {
        pinMode(Config::Hardware::TDECK_TFT_BL, OUTPUT);
        setBacklight(false);
        delay(50);
    }

    // 3) Configure SPI bus selection
    auto busConfig = Config::Hardware::DISPLAY_SHARES_RADIO_SPI
        ? Config::Hardware::RADIO_SPI_BUS
        : Config::Hardware::DISPLAY_SPI_BUS;

    const uint8_t spiHost = (busConfig.host == SpiHostId::HostFSPI) ? FSPI : HSPI;

    // 4) Create Arduino_GFX bus implementation
    Serial.println("[TFT] Creating Arduino_ESP32SPI bus...");
    bus = new Arduino_ESP32SPI(
        Config::Hardware::TDECK_TFT_DC,
        Config::Hardware::TDECK_TFT_CS,
        busConfig.sck,
        busConfig.mosi,
        busConfig.miso,
        spiHost,
        Config::Hardware::DISPLAY_SHARES_RADIO_SPI
    );

    if (!bus) {
        Serial.println("[TFT] ERROR: Failed to create SPI bus!");
        return false;
    }

    // 5) Instantiate the correct panel driver
    Serial.println("[TFT] Creating display driver...");
    switch (Config::Hardware::DISPLAY_TYPE) {
        case DisplayDriver::ST7789:
            gfx = new Arduino_ST7789(
                bus,
                Config::Hardware::TDECK_TFT_RST,
                Config::Hardware::TDECK_TFT_ROTATION,
                true,
                width,
                height
            );
            break;
        case DisplayDriver::GC9A01:
            gfx = new Arduino_GC9A01(
                bus,
                Config::Hardware::TDECK_TFT_RST,
                Config::Hardware::TDECK_TFT_ROTATION,
                true,
                width,
                height
            );
            break;
        default:
            Serial.println("[TFT] ERROR: Unsupported display driver for this board");
            return false;
    }

    if (!gfx) {
        Serial.println("[TFT] ERROR: Failed to create display!");
        return false;
    }

    // 5) Initialize display
    Serial.println("[TFT] Calling gfx->begin()...");
    gfx->begin();
    Serial.println("[TFT] Display initialized");

    // 6) Test with colors
    Serial.println("[TFT] Filling screen with test colors...");
    gfx->fillScreen(0xF800);  // Red
    delay(200);
    gfx->fillScreen(0x07E0);  // Green
    delay(200);
    gfx->fillScreen(0x001F);  // Blue
    delay(200);
    gfx->fillScreen(COLOR_BG);  // Black

    // 7) Turn on backlight
    setBacklight(true);

    displayOn = true;  // Set state AFTER backlight is on
    autoOffTimeout = 0;
    lastActivityTime = millis();
    clearInfo();

    Serial.println("[TFT] TFT initialized successfully!");
    return true;
}

bool TFTDisplay::reinitialize() {
    Serial.println("[TFT] Refreshing display after SPI bus reset...");
    // DON'T call gfx->begin() - it tries to re-register callbacks and crashes
    // The display hardware is already initialized, we just need to redraw
    if (gfx && displayOn) {
        // Redraw current screen based on mode
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
            default:
                // Just clear to background
                gfx->fillScreen(COLOR_BG);
                break;
        }
    }
    return true;
}

void TFTDisplay::update() {
    if (!displayOn || !gfx) return;
    
    // Check auto-off timer (disabled for demo mode)
    if (autoOffTimeout > 0) {
        if (millis() - lastActivityTime > autoOffTimeout) {
            turnOff();
            return;
        }
    }
    
    // Periodic refresh to combat display blanking issue
    // GC9A01 seems to lose content, so redraw current mode periodically
    static uint32_t lastRefresh = 0;
    if (millis() - lastRefresh > 2000) {  // Every 2 seconds
        lastRefresh = millis();
        Serial.println("[TFT] Periodic refresh triggered");
        switch (currentMode) {
            case MODE_SCANNING:
                renderScanning();
                break;
            case MODE_PACKET_INFO:
                renderPacketInfo();
                break;
            case MODE_DEVICE_LIST:
                renderDeviceList();
                break;
            default:
                break;
        }
    }
}

void TFTDisplay::turnOn() {
    Serial.printf("[TFT] turnOn() called (current displayOn=%d)\n", displayOn);
    if (displayOn) {
        Serial.println("[TFT] Display already ON, skipping");
        return;
    }
    setBacklight(true);
    displayOn = true;
    lastActivityTime = millis();
    Serial.println("[TFT] Display state changed to ON");
}

void TFTDisplay::turnOff() {
    Serial.printf("[TFT] turnOff() called (current displayOn=%d)\n", displayOn);
    if (!displayOn) {
        Serial.println("[TFT] Display already OFF, skipping");
        return;
    }
    setBacklight(false);
    displayOn = false;
    Serial.println("[TFT] Display state changed to OFF");
}

void TFTDisplay::toggle() {
    if (displayOn) {
        turnOff();
    } else {
        turnOn();
    }
}

void TFTDisplay::showWelcome() {
    currentMode = MODE_WELCOME;
    resetAutoOffTimer();
}

void TFTDisplay::showScanningStatus(const char* frequency, uint8_t sf, uint8_t configIndex, uint8_t totalConfigs) {
    Serial.printf("[TFT] showScanningStatus called: %s SF%d [%d/%d] (displayOn=%d)\n", 
                  frequency, sf, configIndex, totalConfigs, displayOn);
    strncpy(info.frequency, frequency, sizeof(info.frequency) - 1);
    info.sf = sf;
    info.configIndex = configIndex;
    info.totalConfigs = totalConfigs;
    currentMode = MODE_SCANNING;
    resetAutoOffTimer();
    renderScanning();
    Serial.println("[TFT] renderScanning complete");
}

void TFTDisplay::showPacketReceived(float rssi, float snr, const char* protocol, uint32_t nodeId) {
    info.lastRSSI = rssi;
    info.lastSNR = snr;
    strncpy(info.lastProtocol, protocol, sizeof(info.lastProtocol) - 1);
    info.lastNodeId = nodeId;
    info.totalPackets++;
    currentMode = MODE_PACKET_INFO;
    resetAutoOffTimer();
    renderPacketInfo();
}

void TFTDisplay::showDeviceCount(uint8_t rfActivity, uint8_t trackedNodes, uint8_t targetableDevices) {
    info.rfActivityCount = rfActivity;
    info.trackedNodeCount = trackedNodes;
    info.targetableDeviceCount = targetableDevices;
    currentMode = MODE_DEVICE_LIST;
    resetAutoOffTimer();
    renderDeviceList();
}

void TFTDisplay::showTargetingMode(const char* targetInfo) {
    strncpy(info.targetInfo, targetInfo, sizeof(info.targetInfo) - 1);
    currentMode = MODE_TARGETING;
    resetAutoOffTimer();
    renderTargeting();
}

void TFTDisplay::showShutdown() {
    currentMode = MODE_SHUTDOWN;
    resetAutoOffTimer();
}

void TFTDisplay::resetAutoOffTimer() {
    lastActivityTime = millis();
}

void TFTDisplay::setAutoOffTimeout(uint32_t timeoutMs) {
    autoOffTimeout = timeoutMs;
}

// Simple helpers for early hardware tests
void TFTDisplay::clear() {
    if (gfx) {
        gfx->fillScreen(COLOR_BG);
    }
}

void TFTDisplay::drawText(int16_t x, int16_t y, const char* text, uint8_t size) {
    if (!gfx) return;
    gfx->setTextColor(COLOR_TEXT);
    gfx->setTextSize(size);
    gfx->setCursor(x, y);
    gfx->print(text);
}

// ============================================================================
// Rendering Functions
// ============================================================================

void TFTDisplay::drawHeader(const char* title, uint16_t bgColor) {
    if (!gfx) return;
    gfx->fillRect(0, 0, width, HEADER_HEIGHT, bgColor);
    gfx->setTextColor(COLOR_TEXT);
    gfx->setTextSize(2);
    gfx->setCursor(10, 8);
    gfx->print(title);
}

void TFTDisplay::drawStatusBar() {
    if (!gfx) return;
    // Bottom status bar
    uint16_t y = height - STATUS_BAR_HEIGHT;
    gfx->fillRect(0, y, width, STATUS_BAR_HEIGHT, 0x4208);  // Dark grey
    gfx->setTextColor(COLOR_TEXT);
    gfx->setTextSize(1);
    gfx->setCursor(10, y + 10);
    gfx->printf("Packets: %u | RF: %d | Nodes: %d | Devices: %d",
               info.totalPackets, info.rfActivityCount, 
               info.trackedNodeCount, info.targetableDeviceCount);
}

void TFTDisplay::renderWelcome() {
    if (!gfx) return;
    gfx->fillScreen(COLOR_BG);
    drawHeader("ESP32 LoRa Recon", COLOR_HEADER);
    
    gfx->setTextColor(COLOR_ACCENT);
    gfx->setTextSize(2);
    gfx->setCursor(20, HEADER_HEIGHT + 30);
    gfx->print("T-Deck Edition");
    
    gfx->setTextColor(COLOR_TEXT);
    gfx->setTextSize(1);
    gfx->setCursor(20, HEADER_HEIGHT + 70);
    gfx->print("Initializing...");
    
    gfx->setCursor(20, HEADER_HEIGHT + 100);
    gfx->printf("Board: %s", Config::Hardware::BOARD_NAME);
}

void TFTDisplay::renderScanning() {
    Serial.printf("[TFT] renderScanning: gfx=%p displayOn=%d\n", gfx, displayOn);
    if (!gfx) return;
    
    // Aggressively ensure display is awake and enabled
    // GC9A01 seems to go into display-off state
    Serial.println("[TFT] Calling displayOn() to wake display...");
    gfx->displayOn();
    delay(10);  // Give display time to wake up
    
    // Also ensure backlight is on
    if (Config::Hardware::TDECK_TFT_BL >= 0) {
        digitalWrite(Config::Hardware::TDECK_TFT_BL, Config::Hardware::TDECK_TFT_BACKLIGHT_HIGH ? HIGH : LOW);
    }
    
    Serial.println("[TFT] Starting render...");
    
    // Start transaction
    gfx->fillScreen(COLOR_BG);
    drawHeader("Scanning", COLOR_HEADER);
    
    gfx->setTextColor(COLOR_TEXT);
    gfx->setTextSize(2);
    
    gfx->setCursor(20, HEADER_HEIGHT + 20);
    gfx->print("Freq:");
    gfx->setTextColor(COLOR_ACCENT);
    gfx->setCursor(100, HEADER_HEIGHT + 20);
    gfx->print(info.frequency);
    
    gfx->setTextColor(COLOR_TEXT);
    gfx->setCursor(20, HEADER_HEIGHT + 50);
    gfx->printf("SF: %d", info.sf);
    
    gfx->setCursor(20, HEADER_HEIGHT + 80);
    gfx->printf("Config: %d/%d", info.configIndex, info.totalConfigs);
    
    // Progress bar
    const int barWidth = width - 40;
    const int barY = HEADER_HEIGHT + 120;
    int progress = info.totalConfigs > 0 ? (info.configIndex * barWidth) / info.totalConfigs : 0;
    gfx->fillRect(20, barY, barWidth, 20, 0x4208);
    gfx->fillRect(20, barY, progress, 20, COLOR_SUCCESS);
    
    drawStatusBar();
    
    // Force flush to display - ensure all SPI transactions complete
    delay(1);
    Serial.println("[TFT] renderScanning complete");
}

void TFTDisplay::renderPacketInfo() {
    if (!gfx) return;
    gfx->fillScreen(COLOR_BG);
    drawHeader("Packet Received", COLOR_SUCCESS);
    
    gfx->setTextColor(COLOR_TEXT);
    gfx->setTextSize(2);
    
    gfx->setCursor(20, HEADER_HEIGHT + 20);
    gfx->print("Protocol:");
    gfx->setTextColor(COLOR_ACCENT);
    gfx->setCursor(20, HEADER_HEIGHT + 50);
    gfx->print(info.lastProtocol);
    
    gfx->setTextColor(COLOR_TEXT);
    gfx->setCursor(20, HEADER_HEIGHT + 80);
    gfx->printf("RSSI: %.1f dBm", info.lastRSSI);
    
    gfx->setCursor(20, HEADER_HEIGHT + 110);
    gfx->printf("SNR: %.1f dB", info.lastSNR);
    
    if (info.lastNodeId != 0) {
        gfx->setCursor(20, HEADER_HEIGHT + 140);
        gfx->printf("Node: 0x%08X", info.lastNodeId);
    }
    
    drawStatusBar();
}

void TFTDisplay::renderDeviceList() {
    if (!gfx) return;
    gfx->fillScreen(COLOR_BG);
    drawHeader("Device Summary", COLOR_HEADER);
    
    gfx->setTextColor(COLOR_TEXT);
    gfx->setTextSize(2);
    
    gfx->setCursor(20, HEADER_HEIGHT + 30);
    gfx->printf("RF Activity: %d", info.rfActivityCount);
    
    gfx->setCursor(20, HEADER_HEIGHT + 70);
    gfx->printf("Tracked Nodes: %d", info.trackedNodeCount);
    
    gfx->setCursor(20, HEADER_HEIGHT + 110);
    gfx->printf("Targetable: %d", info.targetableDeviceCount);
    
    drawStatusBar();
}

void TFTDisplay::renderTargeting() {
    if (!gfx) return;
    gfx->fillScreen(COLOR_BG);
    drawHeader("Targeting Mode", COLOR_WARNING);
    
    gfx->setTextColor(COLOR_TEXT);
    gfx->setTextSize(2);
    
    gfx->setCursor(20, HEADER_HEIGHT + 30);
    gfx->print("Target:");
    
    gfx->setTextColor(COLOR_ACCENT);
    gfx->setCursor(20, HEADER_HEIGHT + 70);
    gfx->print(info.targetInfo);
    
    drawStatusBar();
}

void TFTDisplay::renderShutdown() {
    if (!gfx) return;
    gfx->fillScreen(COLOR_BG);
    drawHeader("Shutdown", COLOR_ERROR);
    
    gfx->setTextColor(COLOR_TEXT);
    gfx->setTextSize(3);
    gfx->setCursor(width / 2 - 60, height / 2);
    gfx->print("Goodbye!");
}

void TFTDisplay::clearInfo() {
    memset(&info, 0, sizeof(info));
    strcpy(info.frequency, "---");
    strcpy(info.lastProtocol, "---");
    strcpy(info.targetInfo, "---");
}

void TFTDisplay::setBacklight(bool on) {
    if (Config::Hardware::TDECK_TFT_BL < 0) {
        Serial.println("[TFT] Backlight control: no BL pin configured");
        return;
    }
    const bool activeHigh = Config::Hardware::TDECK_TFT_BACKLIGHT_HIGH;
    const bool level = on ? activeHigh : !activeHigh;
    digitalWrite(Config::Hardware::TDECK_TFT_BL, level ? HIGH : LOW);
    Serial.printf("[TFT] Backlight %s (GPIO %d = %s, activeHigh=%d, displayOn=%d)\n",
                  on ? "ON" : "OFF",
                  Config::Hardware::TDECK_TFT_BL,
                  level ? "HIGH" : "LOW",
                  activeHigh,
                  displayOn);
}

#endif // BOARD_TDECK variants
