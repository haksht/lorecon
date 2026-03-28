/**
 * WiFi Handlers Implementation
 * 
 * WiFi configuration REST API handlers extracted from web_server.cpp.
 */

#include "wifi_handlers.h"
#include "wifi_manager.h"
#include "api_security.h"
#include "lora_recon_tool.h"
#include "logger.h"
#include "utils/json_utils.h"
#include <ArduinoJson.h>

namespace WiFiHandlers {

void handleGetWiFiStatus(AsyncWebServerRequest* request) {
    // Debug log to trace setup mode issue
    LOG_INFO("WiFi status requested: setupMode=%s, connected=%s", 
             wifiManager.isSetupMode() ? "true" : "false",
             wifiManager.isConnected() ? "true" : "false");
    
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
    REQUIRE_AUTH(request);

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
    
    // Show reboot message on OLED and restart
    if (g_reconTool) {
        OLEDDisplay* oled = g_reconTool->getDisplay();
        if (oled) oled->showReboot();
    }
    delay(2000);
    ESP.restart();
}

void handleClearWiFiCredentials(AsyncWebServerRequest* request) {
    REQUIRE_AUTH(request);

    if (!wifiManager.clearCredentials()) {
        request->send(500, "application/json", JsonUtils::error("Failed to clear credentials"));
        return;
    }
    
    LOG_INFO("WiFi credentials cleared - returning to setup mode");
    
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", 
        JsonUtils::success("Credentials cleared. Device restarting in setup mode..."));
    response->addHeader("Connection", "close");
    request->send(response);
    
    // Show reboot message on OLED and restart
    if (g_reconTool) {
        OLEDDisplay* oled = g_reconTool->getDisplay();
        if (oled) oled->showReboot();
    }
    delay(2000);
    ESP.restart();
}

void handleSetAPPassword(AsyncWebServerRequest* request) {
    REQUIRE_AUTH(request);

    if (!request->hasParam("password", true)) {
        request->send(400, "application/json", JsonUtils::error("Missing 'password' parameter"));
        return;
    }

    String password = request->getParam("password", true)->value();

    if (password.length() < 8 || password.length() > 63) {
        request->send(400, "application/json", JsonUtils::error("Password must be 8-63 characters"));
        return;
    }

    if (!wifiManager.saveAPPassword(password.c_str())) {
        request->send(500, "application/json", JsonUtils::error("Failed to save AP password"));
        return;
    }

    LOG_INFO("AP password changed - restarting");

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json",
        JsonUtils::success("AP password updated. Device restarting..."));
    response->addHeader("Connection", "close");
    request->send(response);

    if (g_reconTool) {
        OLEDDisplay* oled = g_reconTool->getDisplay();
        if (oled) oled->showReboot();
    }
    delay(2000);
    ESP.restart();
}

}  // namespace WiFiHandlers
