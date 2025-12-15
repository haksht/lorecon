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
#include <LittleFS.h>
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

// Global instances
LoRaReconTool reconTool;
SerialLogger serialLogger(LogLevel::INFO);
WiFiManager wifiManager;
WebServer webServer;

void setup() {
    // Disable brownout detector to prevent reboot during USB disconnect
    // The Heltec V3 battery circuit causes a brief voltage dip when switching power sources
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    
    // Initialize logger first
    Logger::setInstance(&serialLogger);
    
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
            LOG_INFO("  Open browser: http://%s", wifiManager.getIPAddress().toString().c_str());
            LOG_INFO("  Or use: http://%s.local", mdnsHost.c_str());
            
            // Update OLED with network info
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
    
    // Small delay to prevent watchdog triggers
    delay(10);
}
