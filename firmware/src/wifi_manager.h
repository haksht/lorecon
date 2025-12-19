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
 * 
 * First-run behavior:
 * - No stored credentials → AP mode with setup page
 * - Has credentials → Station mode (connect to user's hotspot)
 * - Station fails → Fall back to AP mode
 */
class WiFiManager {
public:
    WiFiManager();
    
    // Initialization
    bool startAP(const char* ssid, const char* password = nullptr);
    bool startStation(const char* ssid, const char* password);
    void stop();
    
    // Smart initialization (handles first-run setup)
    bool autoConnect();  // Checks for stored creds, connects or starts AP
    
    // Credential storage (LittleFS)
    bool hasStoredCredentials() const;
    bool loadCredentials();
    bool saveCredentials(const char* ssid, const char* password);
    bool clearCredentials();
    String getStoredSSID() const { return staSsid; }
    
    // Unique device identifiers (based on MAC address)
    String getUniqueAPSSID() const;      // e.g., "LoRa-A1B2C3"
    String getUniqueMDNSHostname() const; // e.g., "lora-a1b2c3"
    String getDeviceId();  // e.g., "A1B2C3" - generates on first call
    
    // Status
    bool isConnected() const;
    bool isSetupMode() const { return setupMode; }
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
    bool setupMode;  // True when in first-run AP setup mode
    uint32_t connectionTimeout;
    uint32_t lastConnectionAttempt;
    uint8_t reconnectAttempts;  // Counter for logging
    static const uint8_t RECONNECT_LOG_INTERVAL = 6;  // Log every 6 attempts (~30 sec)
    String staSsid;
    String staPassword;
    String deviceId;  // Last 3 bytes of MAC as hex (e.g., "A1B2C3")
    
    // Internal helpers
    void handleDisconnect();
    void generateDeviceId();  // Called once to set deviceId from MAC
    void startBackgroundAP(); // Start AP alongside STA for fallback access
};

#endif // WIFI_MANAGER_H
