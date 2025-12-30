/**
 * WiFi Manager Implementation
 */

#include "wifi_manager.h"
#include "logger.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_mac.h>
#include <esp_task_wdt.h>

// Preferences instance for secure credential storage
static Preferences wifiPrefs;

WiFiManager::WiFiManager() 
    : currentMode(WiFiMode::OFF)
    , autoReconnect(true)
    , setupMode(false)
    , connectionTimeout(Config::WiFi::STA_CONNECT_TIMEOUT_MS)
    , lastConnectionAttempt(0)    , reconnectAttempts(0){
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
 * Migrate legacy JSON credentials to Preferences (one-time operation)
 * 
 * Checks for old JSON files and migrates them to NVS, then deletes the files.
 * This provides backward compatibility for existing installations.
 */
static bool migrateLegacyCredentials(String& outSsid, String& outPassword) {
    const char* credFile = nullptr;
    
    // Check for legacy JSON files
    if (LittleFS.exists(Config::WiFi::LEGACY_CREDENTIALS_FILE)) {
        credFile = Config::WiFi::LEGACY_CREDENTIALS_FILE;
        LOG_INFO("Found legacy user credentials - migrating to secure storage");
    } else if (LittleFS.exists(Config::WiFi::LEGACY_PREPROVISIONED_FILE)) {
        credFile = Config::WiFi::LEGACY_PREPROVISIONED_FILE;
        LOG_INFO("Found legacy pre-provisioned credentials - migrating to secure storage");
    } else {
        return false;  // No legacy files
    }
    
    File file = LittleFS.open(credFile, "r");
    if (!file) {
        return false;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error || !doc["ssid"].is<const char*>()) {
        return false;
    }
    
    outSsid = doc["ssid"].as<String>();
    outPassword = doc["password"].as<String>();
    
    // Delete legacy files after successful read
    if (LittleFS.exists(Config::WiFi::LEGACY_CREDENTIALS_FILE)) {
        LittleFS.remove(Config::WiFi::LEGACY_CREDENTIALS_FILE);
        LOG_INFO("Removed legacy credentials file");
    }
    if (LittleFS.exists(Config::WiFi::LEGACY_PREPROVISIONED_FILE)) {
        LittleFS.remove(Config::WiFi::LEGACY_PREPROVISIONED_FILE);
        LOG_INFO("Removed legacy pre-provisioned file");
    }
    
    return !outSsid.isEmpty();
}

/**
 * Check if credentials are stored in NVS (Preferences)
 */
bool WiFiManager::hasStoredCredentials() const {
    wifiPrefs.begin(Config::WiFi::NVS_NAMESPACE, true);  // Read-only
    bool hasSSID = wifiPrefs.isKey(Config::WiFi::NVS_KEY_SSID);
    wifiPrefs.end();
    
    // Also check for legacy JSON files (backward compatibility)
    if (!hasSSID) {
        return LittleFS.exists(Config::WiFi::LEGACY_CREDENTIALS_FILE) || 
               LittleFS.exists(Config::WiFi::LEGACY_PREPROVISIONED_FILE);
    }
    return hasSSID;
}

/**
 * Load stored WiFi credentials from NVS (Preferences)
 * 
 * Uses ESP32's Non-Volatile Storage for secure credential storage.
 * Falls back to migrating legacy JSON files if present.
 * 
 * @return true if credentials loaded successfully
 */
bool WiFiManager::loadCredentials() {
    // First, try to load from NVS
    wifiPrefs.begin(Config::WiFi::NVS_NAMESPACE, true);  // Read-only
    
    if (wifiPrefs.isKey(Config::WiFi::NVS_KEY_SSID)) {
        staSsid = wifiPrefs.getString(Config::WiFi::NVS_KEY_SSID, "");
        staPassword = wifiPrefs.getString(Config::WiFi::NVS_KEY_PASSWORD, "");
        wifiPrefs.end();
        
        if (!staSsid.isEmpty()) {
            LOG_INFO("Loaded WiFi credentials from secure storage: %s", staSsid.c_str());
            return true;
        }
    }
    wifiPrefs.end();
    
    // Try migrating legacy JSON credentials
    String legacySsid, legacyPassword;
    if (migrateLegacyCredentials(legacySsid, legacyPassword)) {
        // Save migrated credentials to NVS
        if (saveCredentials(legacySsid.c_str(), legacyPassword.c_str())) {
            LOG_INFO("Successfully migrated credentials to secure storage");
            return true;
        }
    }
    
    LOG_INFO("No stored WiFi credentials found");
    return false;
}

/**
 * Save WiFi credentials to NVS (Preferences)
 * 
 * Credentials are stored in ESP32's Non-Volatile Storage,
 * which is not directly readable like JSON files.
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
    
    wifiPrefs.begin(Config::WiFi::NVS_NAMESPACE, false);  // Read-write
    
    bool success = true;
    // putString returns bytes written (size_t). For SSID, must write > 0.
    // For password, empty string is valid so we just check the SSID write succeeded.
    success = wifiPrefs.putString(Config::WiFi::NVS_KEY_SSID, ssid) > 0;
    wifiPrefs.putString(Config::WiFi::NVS_KEY_PASSWORD, password ? password : "");
    
    wifiPrefs.end();
    
    if (!success) {
        LOG_ERROR("Failed to save WiFi credentials to secure storage");
        return false;
    }
    
    LOG_INFO("WiFi credentials saved to secure storage: %s", ssid);
    
    // Update internal state
    staSsid = ssid;
    staPassword = password ? password : "";
    
    return true;
}

/**
 * Clear stored WiFi credentials from NVS
 * 
 * @return true if cleared successfully
 */
bool WiFiManager::clearCredentials() {
    wifiPrefs.begin(Config::WiFi::NVS_NAMESPACE, false);  // Read-write
    wifiPrefs.clear();  // Remove all keys in this namespace
    wifiPrefs.end();
    
    // Also clean up any legacy JSON files
    if (LittleFS.exists(Config::WiFi::LEGACY_CREDENTIALS_FILE)) {
        LittleFS.remove(Config::WiFi::LEGACY_CREDENTIALS_FILE);
    }
    if (LittleFS.exists(Config::WiFi::LEGACY_PREPROVISIONED_FILE)) {
        LittleFS.remove(Config::WiFi::LEGACY_PREPROVISIONED_FILE);
    }
    
    staSsid = "";
    staPassword = "";
    LOG_INFO("WiFi credentials cleared from secure storage");
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
                
                // Also start AP so device is always reachable at 192.168.4.1
                startBackgroundAP();
                
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
    
    // Start AP mode for setup with unique SSID and device-specific password
    setupMode = true;
    esp_task_wdt_reset();  // Feed watchdog before AP setup
    String uniqueSSID = getUniqueAPSSID();
    String uniquePassword = String(Config::WiFi::DEFAULT_AP_PASSWORD_PREFIX) + getDeviceId();
    if (startAP(uniqueSSID.c_str(), uniquePassword.c_str())) {
        LOG_INFO("");
        LOG_INFO("╔═══════════════════════════════════════════════╗");
        LOG_INFO("║          WIFI SETUP MODE                      ║");
        LOG_INFO("╠═══════════════════════════════════════════════╣");
        LOG_INFO("║  1. Connect phone to: %s", uniqueSSID.c_str());
        LOG_INFO("║  2. Password: %s", uniquePassword.c_str());
        LOG_INFO("║  3. Open browser: http://%s", getIPAddress().toString().c_str());
        LOG_INFO("║  4. Enter your hotspot credentials            ║");
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
 * Start AP alongside existing STA connection (AP_STA mode)
 * 
 * This allows the device to be reachable at 192.168.4.1 even when
 * connected to a hotspot. Useful for field operations where you
 * want a reliable fallback connection method.
 */
void WiFiManager::startBackgroundAP() {
    String uniqueSSID = getUniqueAPSSID();
    String uniquePassword = String(Config::WiFi::DEFAULT_AP_PASSWORD_PREFIX) + getDeviceId();
    
    LOG_INFO("Starting background AP for fallback access...");
    
    // Switch to AP+STA mode (keeps STA connection alive)
    WiFi.mode(WIFI_AP_STA);
    
    if (WiFi.softAP(uniqueSSID.c_str(), uniquePassword.c_str())) {
        currentMode = WiFiMode::AP_STA;
        LOG_INFO("✓ Background AP started: %s", uniqueSSID.c_str());
        LOG_INFO("  Fallback IP: %s", WiFi.softAPIP().toString().c_str());
    } else {
        LOG_WARN("Failed to start background AP - STA-only mode");
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
        // Progress indicator (intentionally raw Serial for inline dots)
        Serial.print(".");
    }
    Serial.println();  // End progress line
    
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
 * In AP_STA mode, returns the Station IP (hotspot) since that's
 * the primary address for Python tools. The AP is always at 192.168.4.1.
 * 
 * @return IP address (Station IP preferred, AP IP as fallback)
 */
IPAddress WiFiManager::getIPAddress() const {
    switch (currentMode) {
        case WiFiMode::AP:
            return WiFi.softAPIP();
        case WiFiMode::AP_STA:
            // Prefer STA IP (hotspot) - AP is always 192.168.4.1
            return WiFi.localIP();
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
    } else if (reconnectAttempts > 0) {
        // Connection restored - reset counter and log success
        LOG_INFO("✓ WiFi reconnected successfully after %d attempts", reconnectAttempts);
        reconnectAttempts = 0;
    }
}

/**
 * Handle WiFi disconnection in Station mode
 * 
 * Attempts automatic reconnection. Since we run AP_STA mode,
 * the device is always reachable at 192.168.4.1 even when
 * the hotspot connection drops.
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
    reconnectAttempts++;
    
    // Only log periodically to avoid spam (every ~30 seconds)
    if (reconnectAttempts % RECONNECT_LOG_INTERVAL == 1) {
        LOG_WARN("WiFi disconnected, reconnecting... (attempt %d, device always reachable at 192.168.4.1)", 
                 reconnectAttempts);
    }
    
    // Keep trying - AP is always running as fallback
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
