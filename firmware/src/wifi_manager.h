/**
 * WiFi Manager - Network Connectivity Management
 * 
 * Manages WiFi connectivity for web server access:
 * - Access Point (AP) mode: ESP32 creates its own network
 * - Station (STA) mode: ESP32 joins existing WiFi network
 * - Connection monitoring and automatic reconnection
 * 
 * Design: Simple and reliable WiFi management for field operations
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>

/**
 * WiFi operating modes
 */
enum class WiFiMode {
    OFF,        // WiFi disabled
    AP,         // Access Point mode (ESP32 creates network)
    STA,        // Station mode (ESP32 joins network)
    AP_STA      // Both modes simultaneously
};

/**
 * WiFi Manager - Handles network connectivity
 * 
 * Provides simple interface for WiFi setup and monitoring.
 * Designed for minimal memory footprint and reliable operation.
 */
class WiFiManager {
public:
    WiFiManager();
    
    // Initialization
    bool startAP(const char* ssid, const char* password = nullptr);
    bool startStation(const char* ssid, const char* password);
    void stop();
    
    // Status
    bool isConnected() const;
    WiFiMode getMode() const { return currentMode; }
    IPAddress getIPAddress() const;
    String getSSID() const;
    int8_t getRSSI() const;
    
    // Maintenance
    void update();  // Call in loop() for connection monitoring
    
    // mDNS (for easy access like http://esp32-lora.local)
    bool startMDNS(const char* hostname);
    
    // Configuration
    void setAutoReconnect(bool enable) { autoReconnect = enable; }
    void setTimeout(uint32_t ms) { connectionTimeout = ms; }
    
private:
    WiFiMode currentMode;
    bool autoReconnect;
    uint32_t connectionTimeout;
    uint32_t lastConnectionAttempt;
    String staSsid;
    String staPassword;
    
    // Internal helpers
    void handleDisconnect();
};

#endif // WIFI_MANAGER_H
