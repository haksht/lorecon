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
#include <esp_task_wdt.h>
#include <time.h>
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

// Crash context tracking - saved to NVS for post-mortem analysis
// IMPORTANT: NVS writes wear flash. Save every 5 minutes, not seconds.
// Previous bug: 10-second saves caused ~41K writes/day, likely triggering
// NVS corruption and spontaneous reboots after ~23 hours.
namespace CrashContext {
    static Preferences prefs;
    static constexpr const char* NVS_NAMESPACE = "crash";
    static const char* lastAction = "boot";

    // Cached reset reason from current boot (persists in RAM for serial queries)
    static esp_reset_reason_t bootResetReason = ESP_RST_UNKNOWN;
    static const char* bootResetReasonStr = "Unknown";

    void setLastAction(const char* action) {
        lastAction = action;
    }

    void saveState(uint8_t mode, uint32_t uptimeSec, uint32_t freeHeap) {
        if (!prefs.begin(NVS_NAMESPACE, false)) return;
        prefs.putUChar("lastMode", mode);
        prefs.putULong("lastUptime", uptimeSec);
        prefs.putULong("lastHeap", freeHeap);
        prefs.putULong("saveCount", prefs.getULong("saveCount", 0) + 1);
        prefs.putString("lastAction", lastAction);
        prefs.end();
    }

    // Save the reset reason to NVS so it survives across subsequent resets.
    // Called once at boot before the crash context gets overwritten.
    void saveResetReason(esp_reset_reason_t reason, const char* reasonStr) {
        bootResetReason = reason;
        bootResetReasonStr = reasonStr;
        if (!prefs.begin(NVS_NAMESPACE, false)) return;
        prefs.putUChar("rstReason", (uint8_t)reason);
        prefs.putString("rstStr", reasonStr);
        prefs.end();
    }

    void loadAndReport() {
        if (!prefs.begin(NVS_NAMESPACE, true)) return;
        uint8_t lastMode = prefs.getUChar("lastMode", 255);
        uint32_t lastUptime = prefs.getULong("lastUptime", 0);
        uint32_t lastHeap = prefs.getULong("lastHeap", 0);
        uint32_t saveCount = prefs.getULong("saveCount", 0);
        String lastActionStr = prefs.getString("lastAction", "unknown");
        uint8_t prevReason = prefs.getUChar("rstReason", 255);
        String prevReasonStr = prefs.getString("rstStr", "unknown");
        prefs.end();

        if (lastMode != 255 && lastUptime > 0) {
            const char* modeStr = (lastMode == 0) ? "RECON" :
                                  (lastMode == 1) ? "TARGETED" :
                                  (lastMode == 2) ? "MENU" :
                                  (lastMode == 3) ? "REPLAY" : "UNKNOWN";
            LOG_INFO("Pre-crash context: mode=%s, uptime=%lu sec, heap=%lu bytes",
                     modeStr, lastUptime, lastHeap);
            LOG_INFO("   Last action: %s", lastActionStr.c_str());
            if (prevReason != 255) {
                LOG_INFO("   Previous boot reason: %s (code %d)", prevReasonStr.c_str(), prevReason);
            }
            LOG_INFO("   (crash context saved %lu times since flash)", saveCount);
        }
    }

    // Print current reset reason (callable from serial command 'i')
    void printResetInfo() {
        Serial.println("\n=== RESET / HEALTH INFO ===");
        Serial.printf("Current boot reason: %s (code %d)\n", bootResetReasonStr, (int)bootResetReason);
        Serial.printf("Uptime: %lu seconds (%.1f hours)\n", millis() / 1000, millis() / 3600000.0f);
        Serial.printf("Free heap: %lu bytes\n", (unsigned long)ESP.getFreeHeap());
        Serial.printf("Min free heap: %lu bytes\n", (unsigned long)ESP.getMinFreeHeap());

        if (prefs.begin(NVS_NAMESPACE, true)) {
            uint8_t prevReason = prefs.getUChar("rstReason", 255);
            String prevReasonStr = prefs.getString("rstStr", "unknown");
            uint8_t lastMode = prefs.getUChar("lastMode", 255);
            uint32_t lastUptime = prefs.getULong("lastUptime", 0);
            uint32_t lastHeap = prefs.getULong("lastHeap", 0);
            uint32_t saveCount = prefs.getULong("saveCount", 0);
            String lastActionStr = prefs.getString("lastAction", "unknown");
            prefs.end();

            if (prevReason != 255) {
                Serial.printf("Previous boot reason: %s (code %d)\n", prevReasonStr.c_str(), prevReason);
            }
            if (lastMode != 255 && lastUptime > 0) {
                const char* modeStr = (lastMode == 0) ? "RECON" :
                                      (lastMode == 1) ? "TARGETED" :
                                      (lastMode == 2) ? "MENU" :
                                      (lastMode == 3) ? "REPLAY" : "UNKNOWN";
                Serial.printf("Last crash context: mode=%s, uptime=%u sec, heap=%u bytes\n",
                             modeStr, (unsigned)lastUptime, (unsigned)lastHeap);
                Serial.printf("   Last action: %s\n", lastActionStr.c_str());
            }
            Serial.printf("Total NVS context saves: %u\n", (unsigned)saveCount);
        }
        Serial.println("===========================\n");
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

    // Always show crash context for non-power-on resets (including after power-on,
    // since the previous session's context is still in NVS)
    if (resetReason == ESP_RST_PANIC || resetReason == ESP_RST_TASK_WDT ||
        resetReason == ESP_RST_INT_WDT || resetReason == ESP_RST_WDT) {
        LOG_WARN("ABNORMAL RESTART - check for bugs or blocking code");
    }
    // Always load crash context — shows previous session info regardless of reset type
    CrashContext::loadAndReport();

    // Persist this boot's reset reason to NVS so it survives the NEXT reboot
    CrashContext::saveResetReason(resetReason, resetReasonStr);
    
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

    // Boot progress tracking on OLED
    OLEDDisplay* bootDisplay = reconTool.getDisplay();
    const uint8_t BOOT_STEPS = 5;

    // Step 1: PSK tests
    if (bootDisplay) bootDisplay->showBootProgress("PSK self-test...", 1, BOOT_STEPS);
    Serial.println("\n🧪 Running PSK Decryption Tests...");
    PSKTests::runAll();
    delay(2000);  // Give time to read results

    // Step 2: WiFi
    if (bootDisplay) bootDisplay->showBootProgress("Connecting WiFi...", 2, BOOT_STEPS);
    LOG_INFO("\n=== Starting WiFi & Web Server ===");
    if (wifiManager.autoConnect()) {
        // Start mDNS for easy access (unique per device)
        String mdnsHost = wifiManager.getUniqueMDNSHostname();
        wifiManager.startMDNS(mdnsHost.c_str());

        // Sync system clock via NTP (only works in STA mode with internet)
        if (!wifiManager.isSetupMode()) {
            LOG_INFO("Syncing time via NTP...");
            configTime(0, 0, "pool.ntp.org", "time.nist.gov");

            // Wait for NTP sync with timeout
            struct tm timeinfo;
            uint8_t ntpRetries = 0;
            while (!getLocalTime(&timeinfo, 100) && ntpRetries < 50) {
                ntpRetries++;
                esp_task_wdt_reset();
            }

            if (ntpRetries < 50) {
                char timeStr[32];
                strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
                LOG_INFO("✓ Time synced: %s UTC", timeStr);
            } else {
                LOG_WARN("NTP sync timed out - file timestamps will be incorrect");
            }
        }

        // Step 3: Web server
        if (bootDisplay) bootDisplay->showBootProgress("Starting server...", 3, BOOT_STEPS);
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
    
    // Step 4: Radio diagnostics
    if (bootDisplay) bootDisplay->showBootProgress("Radio diagnostics...", 4, BOOT_STEPS);
    if (reconTool.getRadioController()) {
        reconTool.getRadioController()->runDiagnostics();

        // Diagnostics calls standby() which kills RX mode — restore it
        const ScanConfig& restoreCfg = reconState.getScanConfig(reconState.scanState.currentConfig);
        if (reconTool.getRadioController()->applyConfig(restoreCfg)) {
            reconTool.getRadioController()->startReceive();
            LOG_INFO("Radio restored to active receive on %s @ %.3f MHz",
                     restoreCfg.protocol, restoreCfg.frequency);
        } else {
            LOG_ERROR("Failed to restore radio after diagnostics");
        }
    }

    // Step 5: Done — show READY then transition to normal display
    if (bootDisplay) {
        bootDisplay->showBootProgress("READY", 5, BOOT_STEPS);
        delay(800);

        // Restore normal display mode (scanning or targeting)
        const ScanConfig& dispCfg = reconState.getScanConfig(reconState.scanState.currentConfig);
        static char freqBuf[16];
        snprintf(freqBuf, sizeof(freqBuf), "%.3f", dispCfg.frequency);
        if (reconState.scanState.mode == MODE_TARGETED_CAPTURE) {
            bootDisplay->showTargetingMode(freqBuf);
        } else {
            bootDisplay->showScanningStatus(freqBuf, dispCfg.spreadingFactor,
                                            reconState.scanState.currentConfig,
                                            reconState.getNumConfigs());
        }
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
    
    // Periodically save crash context to NVS (every 5 minutes)
    // Reduced from 10 seconds to prevent NVS flash wear — 10s interval caused
    // ~41K writes/day which can corrupt the NVS partition after ~23 hours.
    static uint32_t lastCrashContextSave = 0;
    static uint32_t lastHeapLog = 0;
    uint32_t now = millis();
    if (now - lastCrashContextSave >= 300000) {
        CrashContext::setLastAction("loop");
        CrashContext::saveState(
            reconState.scanState.mode,
            now / 1000,
            ESP.getFreeHeap()
        );
        lastCrashContextSave = now;
    }
    
    // Log heap status every 5 minutes for long-duration monitoring
    if (now - lastHeapLog >= 300000) {
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t minFreeHeap = ESP.getMinFreeHeap();
        uint32_t uptimeMin = now / 60000;
        LOG_INFO("📊 Heap status @ %lu min: free=%lu bytes, min=%lu bytes", 
                 uptimeMin, freeHeap, minFreeHeap);
        lastHeapLog = now;
    }
    
    // Check WiFi AP health every 60 seconds and recover if dead
    static uint32_t lastWiFiHealthCheck = 0;
    static uint32_t wifiRecoveryCount = 0;
    if (now - lastWiFiHealthCheck >= 60000) {
        if (!wifiManager.checkAPHealth()) {
            wifiRecoveryCount++;
            LOG_WARN("📡 WiFi AP recovery #%lu at uptime %lu min", 
                     wifiRecoveryCount, now / 60000);
        }
        lastWiFiHealthCheck = now;
    }
    
    // Small delay to prevent watchdog triggers
    delay(10);
}

// Extern-callable wrapper for CrashContext::printResetInfo (used by command handler)
void crashContextPrintResetInfo() {
    CrashContext::printResetInfo();
}
