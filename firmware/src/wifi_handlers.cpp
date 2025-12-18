/**
 * WiFi Handlers Implementation
 * 
 * WiFi configuration REST API handlers extracted from web_server.cpp.
 */

#include "wifi_handlers.h"
#include "wifi_manager.h"
#include "api_security.h"
#include "logger.h"
#include "utils/json_utils.h"
#include <ArduinoJson.h>

// External WiFi manager (from main.cpp)
extern WiFiManager wifiManager;

namespace WiFiHandlers {

void handleGetWiFiStatus(AsyncWebServerRequest* request) {
    String json = JsonUtils::successWithData([](JsonDocument& doc) {
        doc["mode"] = wifiManager.isSetupMode() ? "setup" : "normal";
        doc["connected"] = wifiManager.isConnected();
        doc["ip"] = wifiManager.getIPAddress().toString();
        doc["ssid"] = wifiManager.getSSID();
        doc["rssi"] = wifiManager.getRSSI();
        doc["hasStoredCredentials"] = wifiManager.hasStoredCredentials();
        doc["storedSSID"] = wifiManager.getStoredSSID();
        
        // Unique device identifiers (for conference scenarios)
        doc["deviceId"] = wifiManager.getDeviceId();
        doc["apSSID"] = wifiManager.getUniqueAPSSID();
        doc["mdnsHostname"] = wifiManager.getUniqueMDNSHostname();
        
        // WiFi mode as string
        switch (wifiManager.getMode()) {
            case WiFiMode::AP: doc["wifiMode"] = "AP"; break;
            case WiFiMode::STA: doc["wifiMode"] = "STA"; break;
            case WiFiMode::AP_STA: doc["wifiMode"] = "AP_STA"; break;
            default: doc["wifiMode"] = "OFF"; break;
        }
    });
    
    request->send(200, "application/json", json);
}

void handleSetWiFiCredentials(AsyncWebServerRequest* request) {
    // Authentication required - credentials are sensitive
    if (!APISecurity::isAuthenticated(request)) {
        APISecurity::sendUnauthorized(request);
        return;
    }
    
    if (!request->hasParam("ssid", true)) {
        request->send(400, "application/json", JsonUtils::error("Missing 'ssid' parameter"));
        return;
    }
    
    String ssid = request->getParam("ssid", true)->value();
    String password = "";
    
    if (request->hasParam("password", true)) {
        password = request->getParam("password", true)->value();
    }
    
    // Validate SSID
    if (ssid.isEmpty() || ssid.length() > 32) {
        request->send(400, "application/json", JsonUtils::error("Invalid SSID (1-32 characters)"));
        return;
    }
    
    // Save credentials
    if (!wifiManager.saveCredentials(ssid.c_str(), password.c_str())) {
        request->send(500, "application/json", JsonUtils::error("Failed to save credentials"));
        return;
    }
    
    LOG_INFO("WiFi credentials saved for: %s", ssid.c_str());
    LOG_INFO("Device will restart in 3 seconds to connect...");
    
    // Send success response
    String json = JsonUtils::successWithData([&ssid](JsonDocument& doc) {
        doc["message"] = "Credentials saved. Device restarting to connect to your hotspot...";
        doc["ssid"] = ssid;
    });
    
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Connection", "close");
    request->send(response);
    
    // Schedule restart to apply new credentials
    delay(2000);
    ESP.restart();
}

void handleClearWiFiCredentials(AsyncWebServerRequest* request) {
    // Authentication required - clears credentials and reboots
    if (!APISecurity::isAuthenticated(request)) {
        APISecurity::sendUnauthorized(request);
        return;
    }
    
    if (!wifiManager.clearCredentials()) {
        request->send(500, "application/json", JsonUtils::error("Failed to clear credentials"));
        return;
    }
    
    LOG_INFO("WiFi credentials cleared - returning to setup mode");
    
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", 
        JsonUtils::success("Credentials cleared. Device restarting in setup mode..."));
    response->addHeader("Connection", "close");
    request->send(response);
    
    // Restart to enter setup mode
    delay(2000);
    ESP.restart();
}

}  // namespace WiFiHandlers
