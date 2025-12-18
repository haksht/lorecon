/**
 * WiFi Handlers - WiFi Configuration REST API Handlers
 * 
 * Extracted from web_server.cpp to improve modularity.
 * Contains WiFi setup and configuration endpoints.
 */

#ifndef WIFI_HANDLERS_H
#define WIFI_HANDLERS_H

#include <ESPAsyncWebServer.h>

namespace WiFiHandlers {

/**
 * GET /api/wifi/status - Get current WiFi status and mode
 */
void handleGetWiFiStatus(AsyncWebServerRequest* request);

/**
 * POST /api/wifi/configure - Save WiFi credentials and restart
 * 
 * Body: { "ssid": "MyHotspot", "password": "secret123" }
 * Requires: X-API-Token header
 */
void handleSetWiFiCredentials(AsyncWebServerRequest* request);

/**
 * POST /api/wifi/clear - Clear stored credentials (return to setup mode)
 * Requires: X-API-Token header
 */
void handleClearWiFiCredentials(AsyncWebServerRequest* request);

}  // namespace WiFiHandlers

#endif // WIFI_HANDLERS_H
