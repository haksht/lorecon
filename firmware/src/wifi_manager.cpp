/**
 * WiFi Manager Implementation
 */

#include "wifi_manager.h"
#include "logger.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <esp_mac.h>
#include <esp_task_wdt.h>

WiFiManager::WiFiManager() 
    : currentMode(WiFiMode::OFF)
    , autoReconnect(true)
    , setupMode(false)
    , connectionTimeout(Config::WiFi::STA_CONNECT_TIMEOUT_MS)
    , lastConnectionAttempt(0)
{
    // Device ID will be generated on first use (after WiFi is initialized)
    // Don't call generateDeviceId() here - WiFi hardware not ready yet
}

/**
 * Generate unique device ID from ESP32's MAC address
 * 
 * Uses last 3 bytes of MAC for a short but unique identifier.
 * Example: MAC 24:6F:28:A1:B2:C3 → deviceId "A1B2C3"
 * 
 * Safe to call multiple times - only generates once.
 */
void WiFiManager::generateDeviceId() {
    // Only generate once
    if (deviceId.length() > 0) {
        return;
    }
    
    uint8_t mac[6];
    // Use ESP-IDF function that works without WiFi being fully initialized
    esp_efuse_mac_get_default(mac);
    
    char id[7];
    snprintf(id, sizeof(id), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    deviceId = String(id);
    
    LOG_INFO("Device ID: %s (from MAC)", deviceId.c_str());
}

/**
 * Get unique AP SSID for this device
 * 
 * @return SSID like "LoRa-A1B2C3"
 */
String WiFiManager::getUniqueAPSSID() const {
    // Ensure device ID is generated (safe to call multiple times)
    const_cast<WiFiManager*>(this)->generateDeviceId();
    return String(Config::WiFi::AP_SSID_PREFIX) + deviceId;
}

/**
 * Get unique mDNS hostname for this device
 * 
 * @return Hostname like "lora-a1b2c3" (lowercase for DNS compatibility)
 */
String WiFiManager::getUniqueMDNSHostname() const {
    // Ensure device ID is generated (safe to call multiple times)
    const_cast<WiFiManager*>(this)->generateDeviceId();
    String hostname = String(Config::WiFi::MDNS_PREFIX) + deviceId;
    hostname.toLowerCase();
    return hostname;
}

/**
 * Get unique device ID
 * 
 * @return Device ID like "A1B2C3"
 */
String WiFiManager::getDeviceId() {
    generateDeviceId();  // Ensure it's generated
    return deviceId;
}

/**
 * Check if credentials are stored in LittleFS
 */
bool WiFiManager::hasStoredCredentials() const {
    return LittleFS.exists(Config::WiFi::CREDENTIALS_FILE) || 
           LittleFS.exists(Config::WiFi::PREPROVISIONED_CREDS_FILE);
}

/**
 * Load stored WiFi credentials from LittleFS
 * 
 * Checks two locations:
 * 1. /wifi_config.json - User-configured via web UI (takes priority)
 * 2. /wifi_creds.json - Pre-provisioned by developer (fallback)
 * 
 * @return true if credentials loaded successfully
 */
bool WiFiManager::loadCredentials() {
    // Check for user-configured credentials first
    const char* credFile = nullptr;
    if (LittleFS.exists(Config::WiFi::CREDENTIALS_FILE)) {
        credFile = Config::WiFi::CREDENTIALS_FILE;
        LOG_INFO("Found user-configured WiFi credentials");
    } else if (LittleFS.exists(Config::WiFi::PREPROVISIONED_CREDS_FILE)) {
        credFile = Config::WiFi::PREPROVISIONED_CREDS_FILE;
        LOG_INFO("Found pre-provisioned WiFi credentials (from uploadfs)");
    } else {
        LOG_INFO("No stored WiFi credentials found");
        return false;
    }
    
    File file = LittleFS.open(credFile, "r");
    if (!file) {
        LOG_ERROR("Failed to open WiFi credentials file");
        return false;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        LOG_ERROR("Failed to parse WiFi credentials: %s", error.c_str());
        return false;
    }
    
    staSsid = doc["ssid"].as<String>();
    staPassword = doc["password"].as<String>();
    
    if (staSsid.isEmpty()) {
        LOG_ERROR("Stored SSID is empty");
        return false;
    }
    
    LOG_INFO("Loaded WiFi credentials for: %s", staSsid.c_str());
    return true;
}

/**
 * Save WiFi credentials to LittleFS
 * 
 * @param ssid Network SSID
 * @param password Network password
 * @return true if saved successfully
 */
bool WiFiManager::saveCredentials(const char* ssid, const char* password) {
    if (!ssid || strlen(ssid) == 0) {
        LOG_ERROR("Cannot save empty SSID");
        return false;
    }
    
    JsonDocument doc;
    doc["ssid"] = ssid;
    doc["password"] = password ? password : "";
    
    File file = LittleFS.open(Config::WiFi::CREDENTIALS_FILE, "w");
    if (!file) {
        LOG_ERROR("Failed to create WiFi credentials file");
        return false;
    }
    
    if (serializeJson(doc, file) == 0) {
        LOG_ERROR("Failed to write WiFi credentials");
        file.close();
        return false;
    }
    
    file.close();
    LOG_INFO("✓ WiFi credentials saved for: %s", ssid);
    
    // Update internal state
    staSsid = ssid;
    staPassword = password ? password : "";
    
    return true;
}

/**
 * Clear stored WiFi credentials
 * 
 * @return true if cleared successfully
 */
bool WiFiManager::clearCredentials() {
    if (LittleFS.exists(Config::WiFi::CREDENTIALS_FILE)) {
        if (!LittleFS.remove(Config::WiFi::CREDENTIALS_FILE)) {
            LOG_ERROR("Failed to remove WiFi credentials file");
            return false;
        }
    }
    
    staSsid = "";
    staPassword = "";
    LOG_INFO("✓ WiFi credentials cleared");
    return true;
}

/**
 * Auto-connect: Smart WiFi initialization
 * 
 * Behavior:
 * 1. Check for stored credentials
 * 2. If found: Try Station mode (connect to user's hotspot)
 * 3. If no creds or connection fails: Start AP mode (setup page)
 * 
 * @return true if connected (either mode)
 */
bool WiFiManager::autoConnect() {
    LOG_INFO("=== WiFi Auto-Connect ===");
    esp_task_wdt_reset();  // Feed watchdog at start of WiFi operations
    
    // Try to load stored credentials
    if (loadCredentials()) {
        LOG_INFO("Attempting to connect to: %s", staSsid.c_str());
        
        // Try connecting with retries
        for (uint8_t attempt = 1; attempt <= Config::WiFi::STA_MAX_RETRIES; attempt++) {
            LOG_INFO("Connection attempt %d/%d...", attempt, Config::WiFi::STA_MAX_RETRIES);
            
            if (startStation(staSsid.c_str(), staPassword.c_str())) {
                setupMode = false;
                LOG_INFO("✓ Connected to hotspot: %s", staSsid.c_str());
                return true;
            }
            
            if (attempt < Config::WiFi::STA_MAX_RETRIES) {
                esp_task_wdt_reset();  // Feed watchdog between retries
                delay(1000);  // Brief delay between retries
            }
        }
        
        LOG_WARN("Failed to connect after %d attempts", Config::WiFi::STA_MAX_RETRIES);
        LOG_INFO("Falling back to AP mode for reconfiguration...");
    } else {
        LOG_INFO("No credentials stored - starting setup mode");
    }
    
    // Start AP mode for setup with unique SSID
    setupMode = true;
    esp_task_wdt_reset();  // Feed watchdog before AP setup
    String uniqueSSID = getUniqueAPSSID();
    if (startAP(uniqueSSID.c_str(), Config::WiFi::DEFAULT_AP_PASSWORD)) {
        LOG_INFO("");
        LOG_INFO("╔═══════════════════════════════════════════════╗");
        LOG_INFO("║          WIFI SETUP MODE                      ║");
        LOG_INFO("╠═══════════════════════════════════════════════╣");
        LOG_INFO("║  1. Connect phone to: %s", uniqueSSID.c_str());
        LOG_INFO("║  2. Open browser: http://%s", getIPAddress().toString().c_str());
        LOG_INFO("║  3. Enter your hotspot credentials            ║");
        LOG_INFO("╚═══════════════════════════════════════════════╝");
        LOG_INFO("");
        return true;
    }
    
    return false;
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
    LOG_INFO("  Password length: %d", password ? strlen(password) : 0);
    
    // Scan for available networks first (debug)
    LOG_INFO("Scanning for available networks...");
    esp_task_wdt_reset();  // Feed watchdog before potentially long scan
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    int numNetworks = WiFi.scanNetworks();
    esp_task_wdt_reset();  // Feed watchdog after scan completes
    LOG_INFO("Found %d networks:", numNetworks);
    bool targetFound = false;
    for (int i = 0; i < numNetworks && i < 10; i++) {
        String foundSSID = WiFi.SSID(i);
        LOG_INFO("  [%d] %s (RSSI: %d dBm, Ch: %d)", 
                 i, foundSSID.c_str(), WiFi.RSSI(i), WiFi.channel(i));
        if (foundSSID == ssid) {
            targetFound = true;
            LOG_INFO("       ^^^ TARGET NETWORK FOUND ^^^");
        }
    }
    WiFi.scanDelete();
    
    if (!targetFound) {
        LOG_WARN("Target network '%s' not found in scan - attempting connection anyway", ssid);
    }
    
    // Store credentials for auto-reconnect
    staSsid = ssid;
    staPassword = password;
    
    LOG_INFO("Attempting connection to %s...", ssid);
    WiFi.begin(ssid, password);
    
    // Wait for connection with timeout
    // Feed watchdog during wait to prevent system reset
    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > connectionTimeout) {
            LOG_ERROR("✗ Connection timeout after %d ms", connectionTimeout);
            return false;
        }
        esp_task_wdt_reset();  // Feed watchdog during long wait
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
