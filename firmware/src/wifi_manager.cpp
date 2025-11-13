/**
 * WiFi Manager Implementation
 */

#include "wifi_manager.h"
#include "logger.h"

WiFiManager::WiFiManager() 
    : currentMode(WiFiMode::OFF)
    , autoReconnect(true)
    , connectionTimeout(10000)
    , lastConnectionAttempt(0)
{
}

/**
 * Start Access Point mode
 * 
 * Creates WiFi network that clients can connect to.
 * Default IP: 192.168.4.1
 * 
 * @param ssid Network name (e.g., "ESP32-LoRa-Sniffer")
 * @param password WPA2 password (nullptr for open network - not recommended)
 * @return true if AP started successfully
 */
bool WiFiManager::startAP(const char* ssid, const char* password) {
    LOG_INFO("Starting WiFi Access Point...");
    LOG_INFO("  SSID: %s", ssid);
    
    WiFi.mode(WIFI_AP);
    
    bool success;
    if (password && strlen(password) >= 8) {
        // WPA2 protected network (recommended)
        success = WiFi.softAP(ssid, password);
        LOG_INFO("  Security: WPA2");
    } else {
        // Open network (not secure, but easier for testing)
        success = WiFi.softAP(ssid);
        LOG_WARN("  Security: OPEN (not recommended for field use)");
    }
    
    if (success) {
        currentMode = WiFiMode::AP;
        IPAddress ip = WiFi.softAPIP();
        LOG_INFO("✓ Access Point started");
        LOG_INFO("  IP Address: %s", ip.toString().c_str());
        LOG_INFO("  Connect phone to WiFi: %s", ssid);
        LOG_INFO("  Then open browser: http://%s", ip.toString().c_str());
        return true;
    } else {
        LOG_ERROR("✗ Failed to start Access Point");
        return false;
    }
}

/**
 * Start Station mode
 * 
 * Connects ESP32 to existing WiFi network.
 * Useful for integration with home/office networks.
 * 
 * @param ssid Network name to connect to
 * @param password Network password
 * @return true if connection successful
 */
bool WiFiManager::startStation(const char* ssid, const char* password) {
    LOG_INFO("Connecting to WiFi network...");
    LOG_INFO("  SSID: %s", ssid);
    
    // Store credentials for auto-reconnect
    staSsid = ssid;
    staPassword = password;
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    // Wait for connection with timeout
    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > connectionTimeout) {
            LOG_ERROR("✗ Connection timeout after %d ms", connectionTimeout);
            return false;
        }
        delay(100);
        Serial.print(".");
    }
    Serial.println();
    
    currentMode = WiFiMode::STA;
    LOG_INFO("✓ Connected to WiFi");
    LOG_INFO("  IP Address: %s", WiFi.localIP().toString().c_str());
    LOG_INFO("  Signal: %d dBm", WiFi.RSSI());
    
    return true;
}

/**
 * Stop WiFi and return to OFF mode
 * 
 * Useful for power saving or when WiFi not needed.
 */
void WiFiManager::stop() {
    if (currentMode != WiFiMode::OFF) {
        LOG_INFO("Stopping WiFi...");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        currentMode = WiFiMode::OFF;
        LOG_INFO("✓ WiFi stopped");
    }
}

/**
 * Check if WiFi is connected
 * 
 * @return true if connected (either AP mode or STA mode connected)
 */
bool WiFiManager::isConnected() const {
    switch (currentMode) {
        case WiFiMode::AP:
            return true;  // AP is always "connected"
        case WiFiMode::STA:
        case WiFiMode::AP_STA:
            return WiFi.status() == WL_CONNECTED;
        case WiFiMode::OFF:
        default:
            return false;
    }
}

/**
 * Get IP address based on current mode
 * 
 * @return IP address (AP address or Station address)
 */
IPAddress WiFiManager::getIPAddress() const {
    switch (currentMode) {
        case WiFiMode::AP:
        case WiFiMode::AP_STA:
            return WiFi.softAPIP();
        case WiFiMode::STA:
            return WiFi.localIP();
        default:
            return IPAddress(0, 0, 0, 0);
    }
}

/**
 * Get current SSID
 * 
 * @return SSID string
 */
String WiFiManager::getSSID() const {
    switch (currentMode) {
        case WiFiMode::AP:
        case WiFiMode::AP_STA:
            return WiFi.softAPSSID();
        case WiFiMode::STA:
            return WiFi.SSID();
        default:
            return "";
    }
}

/**
 * Get signal strength (Station mode only)
 * 
 * @return RSSI in dBm (or 0 if not applicable)
 */
int8_t WiFiManager::getRSSI() const {
    if (currentMode == WiFiMode::STA || currentMode == WiFiMode::AP_STA) {
        return WiFi.RSSI();
    }
    return 0;
}

/**
 * Update WiFi status (call in loop())
 * 
 * Handles automatic reconnection in Station mode.
 */
void WiFiManager::update() {
    // Only monitor Station mode connections
    if (currentMode != WiFiMode::STA && currentMode != WiFiMode::AP_STA) {
        return;
    }
    
    // Check if disconnected
    if (WiFi.status() != WL_CONNECTED) {
        handleDisconnect();
    }
}

/**
 * Handle WiFi disconnection in Station mode
 * 
 * Attempts automatic reconnection if enabled.
 */
void WiFiManager::handleDisconnect() {
    if (!autoReconnect) {
        return;
    }
    
    // Throttle reconnection attempts
    uint32_t now = millis();
    if (now - lastConnectionAttempt < 5000) {
        return;  // Wait at least 5 seconds between attempts
    }
    
    lastConnectionAttempt = now;
    LOG_WARN("WiFi disconnected, attempting to reconnect...");
    
    WiFi.disconnect();
    WiFi.begin(staSsid.c_str(), staPassword.c_str());
}

/**
 * Start mDNS service for easy access
 * 
 * Allows accessing ESP32 via hostname instead of IP:
 * - http://esp32-lora.local instead of http://192.168.1.123
 * 
 * @param hostname Hostname to use (without .local suffix)
 * @return true if mDNS started successfully
 */
bool WiFiManager::startMDNS(const char* hostname) {
    if (!isConnected()) {
        LOG_ERROR("Cannot start mDNS: WiFi not connected");
        return false;
    }
    
    LOG_INFO("Starting mDNS service...");
    LOG_INFO("  Hostname: %s.local", hostname);
    
    if (MDNS.begin(hostname)) {
        MDNS.addService("http", "tcp", 80);
        LOG_INFO("✓ mDNS started");
        LOG_INFO("  Access at: http://%s.local", hostname);
        return true;
    } else {
        LOG_ERROR("✗ Failed to start mDNS");
        return false;
    }
}
