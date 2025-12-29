/**
 * ESP32 LoRa Reconnaissance Tool
 * 
 * A practical LoRa packet capture and analysis tool for security research
 * and RF experimentation. Clean, efficient implementation focusing on
 * core functionality.
 * 
 * Features:
 * - Sequential frequency scanning across ISM band
 * - Protocol identification and packet analysis
 * - Optional default PSK testing for Meshtastic networks
 * - Signal analysis and node tracking
 * - Local storage with JSON export
 * - WiFi web server with REST API and WebSocket
 * - Progressive Web App for mobile/browser access
 * 
 * Hardware: ESP32-S3 + SX1262 LoRa radio (like Heltec WiFi LoRa 32 V3)
 * 
 * License: MIT
 */

#include "lora_recon_tool.h"
#include "logger.h"
#include "config.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "recon_service.h"
#include "psk_tests.h"
#include "oled_display.h"
#include "api_security.h"
#include "recon_state.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <esp_system.h>
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

// Crash context tracking - saved to NVS for post-mortem analysis
namespace CrashContext {
    static Preferences prefs;
    static constexpr const char* NVS_NAMESPACE = "crash";
    
    void saveState(uint8_t mode, uint32_t uptimeSec, uint32_t freeHeap) {
        if (!prefs.begin(NVS_NAMESPACE, false)) return;
        prefs.putUChar("lastMode", mode);
        prefs.putULong("lastUptime", uptimeSec);
        prefs.putULong("lastHeap", freeHeap);
        prefs.putULong("saveCount", prefs.getULong("saveCount", 0) + 1);
        prefs.end();
    }
    
    void loadAndReport() {
        if (!prefs.begin(NVS_NAMESPACE, true)) return;
        uint8_t lastMode = prefs.getUChar("lastMode", 255);
        uint32_t lastUptime = prefs.getULong("lastUptime", 0);
        uint32_t lastHeap = prefs.getULong("lastHeap", 0);
        uint32_t saveCount = prefs.getULong("saveCount", 0);
        prefs.end();
        
        if (lastMode != 255 && lastUptime > 0) {
            const char* modeStr = (lastMode == 0) ? "RECON" : 
                                  (lastMode == 1) ? "TARGETED" :
                                  (lastMode == 2) ? "MENU" :
                                  (lastMode == 3) ? "REPLAY" : "UNKNOWN";
            LOG_INFO("📊 Pre-crash context: mode=%s, uptime=%lu sec, heap=%lu bytes", 
                     modeStr, lastUptime, lastHeap);
            LOG_INFO("   (crash context saved %lu times since flash)", saveCount);
        }
    }
}

// Global instances
LoRaReconTool reconTool;
SerialLogger serialLogger(LogLevel::INFO);
WiFiManager wifiManager;
WebServer webServer;

void setup() {
    // Disable brownout detector to prevent reboot during USB disconnect
    // The Heltec V3 battery circuit causes a brief voltage dip when switching power sources
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    
    // Initialize Serial FIRST so reset reason logging works
    Serial.begin(Config::UI::SERIAL_BAUD);
    delay(100);  // Brief delay for serial to stabilize
    
    // Initialize logger
    Logger::setInstance(&serialLogger);
    
    // Log restart reason for debugging spontaneous reboots
    esp_reset_reason_t resetReason = esp_reset_reason();
    const char* resetReasonStr;
    switch (resetReason) {
        case ESP_RST_POWERON:   resetReasonStr = "Power-on"; break;
        case ESP_RST_EXT:       resetReasonStr = "External pin"; break;
        case ESP_RST_SW:        resetReasonStr = "Software reset (esp_restart)"; break;
        case ESP_RST_PANIC:     resetReasonStr = "Exception/panic"; break;
        case ESP_RST_INT_WDT:   resetReasonStr = "Interrupt watchdog"; break;
        case ESP_RST_TASK_WDT:  resetReasonStr = "Task watchdog"; break;
        case ESP_RST_WDT:       resetReasonStr = "Other watchdog"; break;
        case ESP_RST_DEEPSLEEP: resetReasonStr = "Deep sleep wake"; break;
        case ESP_RST_BROWNOUT:  resetReasonStr = "Brownout"; break;
        case ESP_RST_SDIO:      resetReasonStr = "SDIO"; break;
        default:                resetReasonStr = "Unknown"; break;
    }
    LOG_INFO("\n========================================");
    LOG_INFO("     ESP32 RESTART DETECTED");
    LOG_INFO("========================================");
    LOG_INFO("Reset reason: %s (code %d)", resetReasonStr, resetReason);
    if (resetReason == ESP_RST_PANIC || resetReason == ESP_RST_TASK_WDT || 
        resetReason == ESP_RST_INT_WDT || resetReason == ESP_RST_WDT) {
        LOG_WARN("⚠️  ABNORMAL RESTART - check for bugs or blocking code");
        // Show what state device was in before crash
        CrashContext::loadAndReport();
    } else if (resetReason != ESP_RST_POWERON) {
        // Also show context for software resets (useful for debugging)
        CrashContext::loadAndReport();
    }
    
    // Initialize LittleFS for web app files
    if (!LittleFS.begin(true)) {
        LOG_ERROR("LittleFS mount failed");
    } else {
        LOG_INFO("✓ LittleFS mounted successfully");
    }
    
    // Initialize LoRa reconnaissance tool
    if (!reconTool.initialize()) {
        LOG_ERROR("Failed to initialize LoRa recon tool");
        while (1) delay(1000);  // Halt on initialization failure
    }

    ReconService::initialize(&reconTool);
    
    Serial.println("\n🧪 Running PSK Decryption Tests...");
    PSKTests::runAll();
    delay(2000);  // Give time to read results

    // Initialize WiFi (auto-detects first-run vs returning user)
    LOG_INFO("\n=== Starting WiFi & Web Server ===");
    if (wifiManager.autoConnect()) {
        // Start mDNS for easy access (unique per device)
        String mdnsHost = wifiManager.getUniqueMDNSHostname();
        wifiManager.startMDNS(mdnsHost.c_str());
        
        // Initialize web server
        if (webServer.begin(&reconTool)) {
            // Connect web server to packet processor for live updates
            reconTool.setWebServer(&webServer);
            
            LOG_INFO("✓ Web interface ready!");
            
            // Show connection info based on mode
            if (wifiManager.getMode() == WiFiMode::AP_STA) {
                // Dual mode - show both with clear labels
                LOG_INFO("  📡 Hotspot IP: http://%s (use for Python tools)", wifiManager.getIPAddress().toString().c_str());
                LOG_INFO("  📱 Fallback AP: http://192.168.4.1 (connect to %s)", wifiManager.getUniqueAPSSID().c_str());
            } else {
                LOG_INFO("  Open browser: http://%s", wifiManager.getIPAddress().toString().c_str());
            }
            LOG_INFO("  Or use: http://%s.local", mdnsHost.c_str());
            
            // Display API token for authenticated access
            if (Config::Security::AUTH_ENABLED) {
                LOG_INFO("\n🔐 API Security Enabled");
                LOG_INFO("  Token: %s", APISecurity::getToken().c_str());
                LOG_INFO("  Header: %s", Config::Security::AUTH_HEADER);
                LOG_INFO("  Protected endpoints require this token");
            }
            
            // Update OLED with network info - show hotspot IP as primary
            OLEDDisplay* display = reconTool.getDisplay();
            if (display) {
                display->setNetworkInfo(
                    wifiManager.getIPAddress().toString().c_str(),
                    mdnsHost.c_str()
                );
            }
            
            if (wifiManager.isSetupMode()) {
                LOG_INFO("  📱 Configure your hotspot in the web UI");
            }
        } else {
            LOG_ERROR("Failed to start web server");
        }
    } else {
        LOG_WARN("WiFi/Web server not started - continuing with serial only");
    }
    
    LOG_INFO("\n=== System Ready ===");
    LOG_INFO("LoRa reconnaissance active");
    if (wifiManager.isConnected()) {
        LOG_INFO("Web interface active at http://%s", wifiManager.getIPAddress().toString().c_str());
    }
    LOG_INFO("Serial commands available (press 'm' for menu)\n");
}

void loop() {
    // Update LoRa reconnaissance (handles radio, packets, commands)
    reconTool.update();
    
    // Flush pending web server updates
    webServer.service();

    // Update WiFi connection monitoring
    wifiManager.update();
    
    // Periodically save crash context (every 60 seconds)
    // If device crashes, we'll know what mode it was in
    static uint32_t lastCrashContextSave = 0;
    uint32_t now = millis();
    if (now - lastCrashContextSave >= 60000) {
        CrashContext::saveState(
            reconState.scanState.mode,
            now / 1000,
            ESP.getFreeHeap()
        );
        lastCrashContextSave = now;
    }
    
    // Small delay to prevent watchdog triggers
    delay(10);
}
