/**
 * Web Server Implementation
 */

#include "web_server.h"
#include "api_controller.h"
#include "logger.h"
#include <LittleFS.h>

// Global instance for static handlers
WebServer* g_webServer = nullptr;

WebServer::WebServer() 
    : server(nullptr)
    , ws(nullptr)
    , reconTool(nullptr)
{
}

/**
 * Start web server
 * 
 * @param tool Reconnaissance tool interface
 * @param port HTTP port (default: 80)
 * @return true if server started successfully
 */
bool WebServer::begin(IReconTool* tool, uint16_t port) {
    if (!tool) {
        LOG_ERROR("Cannot start web server: reconTool is null");
        return false;
    }
    
    reconTool = tool;
    g_webServer = this;
    
    // Initialize API controller
    APIController::setReconTool(tool);
    
    LOG_INFO("Starting web server on port %d...", port);
    
    // Create server and WebSocket
    server = new AsyncWebServer(port);
    ws = new AsyncWebSocket("/ws");
    
    // Setup WebSocket
    setupWebSocket();
    server->addHandler(ws);
    
    // Setup REST API routes
    setupRoutes();
    
    // Serve static files for PWA
    serveStaticFiles();
    
    // Enable CORS for development
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
    
    // Start server
    server->begin();
    
    LOG_INFO("✓ Web server started on port %d", port);
    LOG_INFO("  REST API: http://<ip>/api/");
    LOG_INFO("  WebSocket: ws://<ip>/ws");
    LOG_INFO("  Web App: http://<ip>/");
    
    return true;
}

/**
 * Stop web server
 */
void WebServer::stop() {
    if (server) {
        LOG_INFO("Stopping web server...");
        server->end();
        delete server;
        server = nullptr;
    }
    
    if (ws) {
        delete ws;
        ws = nullptr;
    }
    
    g_webServer = nullptr;
}

/**
 * Setup REST API routes
 */
void WebServer::setupRoutes() {
    // Device Management
    server->on("/api/devices", HTTP_GET, handleGetDevices);
    server->on("/api/device", HTTP_GET, handleGetDevice);  // ?nodeId=0x12345678
    server->on("/api/capture/start", HTTP_POST, handleStartCapture);  // Body: {nodeId: 0x12345678}
    server->on("/api/capture/stop", HTTP_POST, handleStopCapture);
    
    // Geographic Data
    server->on("/api/positions", HTTP_GET, handleGetPositions);
    server->on("/api/export/geojson", HTTP_GET, handleExportGeoJSON);
    server->on("/api/export/kml", HTTP_GET, handleExportKML);
    
    // Status & Config
    server->on("/api/status", HTTP_GET, handleGetStatus);
    server->on("/api/statistics", HTTP_GET, handleGetStatistics);
    server->on("/api/activity", HTTP_GET, handleGetActivity);
    server->on("/api/config", HTTP_GET, handleGetConfig);
    
    // Scan Control
    server->on("/api/scan/start", HTTP_POST, handleStartScan);
    server->on("/api/scan/stop", HTTP_POST, handleStopScan);
    
    // Health check
    server->on("/api/health", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "application/json", "{\"status\":\"ok\",\"service\":\"ESP32 LoRa Sniffer\"}");
    });
    
    LOG_INFO("✓ REST API routes configured");
}

/**
 * Setup WebSocket for real-time updates
 */
void WebServer::setupWebSocket() {
    ws->onEvent(handleWebSocketEvent);
    LOG_INFO("✓ WebSocket configured at /ws");
}

/**
 * Serve static files for Progressive Web App
 */
void WebServer::serveStaticFiles() {
    // Simple test page to verify connectivity
    server->on("/test", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/html", "<html><body><h1>ESP32 Web Server Working!</h1><p>If you see this, the server is running.</p></body></html>");
    });
    
    // API test page
    server->on("/apitest", HTTP_GET, [](AsyncWebServerRequest* request) {
        String html = "<html><head><style>body{font-family:sans-serif;padding:20px;}</style></head><body>";
        html += "<h1>API Test</h1>";
        html += "<button onclick=\"fetch('/api/status').then(r=>r.json()).then(d=>alert(JSON.stringify(d)))\">Test API</button>";
        html += "<button onclick=\"ws=new WebSocket('ws://'+location.host+'/ws');ws.onopen=()=>alert('WS Connected!');ws.onerror=()=>alert('WS Error')\">Test WebSocket</button>";
        html += "</body></html>";
        request->send(200, "text/html", html);
    });
    
    // Root serves index.html
    server->serveStatic("/", LittleFS, "/webapp/").setDefaultFile("index.html");
    
    // Fallback - serve index.html for SPA routing
    server->onNotFound([](AsyncWebServerRequest* request) {
        // Check if it's an API request
        if (request->url().startsWith("/api/")) {
            request->send(404, "application/json", "{\"status\":\"error\",\"error\":\"Endpoint not found\"}");
        } else {
            // Serve index.html for PWA routing
            request->send(LittleFS, "/webapp/index.html", "text/html");
        }
    });
    
    LOG_INFO("✓ Static file serving configured");
}

// =============================================================================
// REST API HANDLERS
// =============================================================================

void WebServer::handleGetDevices(AsyncWebServerRequest* request) {
    String json = APIController::getDevices();
    request->send(200, "application/json", json);
}

void WebServer::handleGetDevice(AsyncWebServerRequest* request) {
    if (!request->hasParam("nodeId")) {
        request->send(400, "application/json", "{\"status\":\"error\",\"error\":\"Missing nodeId parameter\"}");
        return;
    }
    
    String nodeIdStr = request->getParam("nodeId")->value();
    uint32_t nodeId = strtoul(nodeIdStr.c_str(), nullptr, 16);
    
    String json = APIController::getDevice(nodeId);
    request->send(200, "application/json", json);
}

void WebServer::handleStartCapture(AsyncWebServerRequest* request) {
    // Read JSON body (simplified - in production, use AsyncWebServerRequest body handler)
    if (!request->hasParam("nodeId", true)) {
        request->send(400, "application/json", "{\"status\":\"error\",\"error\":\"Missing nodeId in body\"}");
        return;
    }
    
    String nodeIdStr = request->getParam("nodeId", true)->value();
    uint32_t nodeId = strtoul(nodeIdStr.c_str(), nullptr, 16);
    
    String json = APIController::startTargetedCapture(nodeId);
    request->send(200, "application/json", json);
}

void WebServer::handleStopCapture(AsyncWebServerRequest* request) {
    String json = APIController::stopCapture();
    request->send(200, "application/json", json);
}

void WebServer::handleGetPositions(AsyncWebServerRequest* request) {
    String json = APIController::getPositions();
    request->send(200, "application/json", json);
}

void WebServer::handleExportGeoJSON(AsyncWebServerRequest* request) {
    String json = APIController::exportGeoJSON();
    request->send(200, "application/json", json);
}

void WebServer::handleExportKML(AsyncWebServerRequest* request) {
    String kml = APIController::exportKML();
    request->send(200, "application/vnd.google-earth.kml+xml", kml);
}

void WebServer::handleGetStatus(AsyncWebServerRequest* request) {
    String json = APIController::getStatus();
    request->send(200, "application/json", json);
}

void WebServer::handleGetStatistics(AsyncWebServerRequest* request) {
    String json = APIController::getStatistics();
    request->send(200, "application/json", json);
}

void WebServer::handleGetActivity(AsyncWebServerRequest* request) {
    String json = APIController::getRFActivity();
    request->send(200, "application/json", json);
}

void WebServer::handleGetConfig(AsyncWebServerRequest* request) {
    String json = APIController::getConfig();
    request->send(200, "application/json", json);
}

void WebServer::handleStartScan(AsyncWebServerRequest* request) {
    String json = APIController::startScan();
    request->send(200, "application/json", json);
}

void WebServer::handleStopScan(AsyncWebServerRequest* request) {
    String json = APIController::stopScan();
    request->send(200, "application/json", json);
}

// =============================================================================
// WEBSOCKET HANDLERS
// =============================================================================

void WebServer::handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                    AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            LOG_INFO("WebSocket client #%u connected from %s", client->id(), client->remoteIP().toString().c_str());
            // Send initial status
            if (g_webServer) {
                g_webServer->broadcastStatusUpdate();
            }
            break;
            
        case WS_EVT_DISCONNECT:
            LOG_INFO("WebSocket client #%u disconnected", client->id());
            break;
            
        case WS_EVT_DATA:
            handleWebSocketMessage(client, data, len);
            break;
            
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void WebServer::handleWebSocketMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
    // Parse incoming WebSocket messages
    String message = String((char*)data).substring(0, len);
    LOG_INFO("WebSocket message from client #%u: %s", client->id(), message.c_str());
    
    // Could implement commands via WebSocket if needed
    // For now, just echo back
    client->text("{\"status\":\"received\",\"message\":\"" + message + "\"}");
}

/**
 * Broadcast packet to all connected WebSocket clients
 */
void WebServer::broadcastPacket(uint32_t nodeId, const char* protocol, float rssi, float snr, size_t length, const char* message) {
    if (!ws) return;
    
    JsonDocument doc;
    doc["type"] = "packet";
    doc["nodeId"] = String(nodeId, HEX);
    doc["protocol"] = protocol;
    doc["rssi"] = rssi;
    doc["snr"] = snr;
    doc["length"] = length;
    doc["timestamp"] = millis();
    if (message && strlen(message) > 0) {
        doc["message"] = message;
    }
    
    String json;
    serializeJson(doc, json);
    
    ws->textAll(json);
}

/**
 * Broadcast device update to all connected WebSocket clients
 */
void WebServer::broadcastDeviceUpdate(uint32_t nodeId) {
    if (!ws) return;
    
    JsonDocument doc;
    doc["type"] = "deviceUpdate";
    doc["nodeId"] = String(nodeId, HEX);
    doc["timestamp"] = millis();
    
    String json;
    serializeJson(doc, json);
    
    ws->textAll(json);
}

/**
 * Broadcast status update to all connected WebSocket clients
 */
void WebServer::broadcastStatusUpdate() {
    if (!ws) return;
    
    String statusJson = APIController::getStatus();
    
    JsonDocument doc;
    doc["type"] = "status";
    deserializeJson(doc["data"], statusJson);
    doc["timestamp"] = millis();
    
    String json;
    serializeJson(doc, json);
    
    ws->textAll(json);
}

/**
 * Get number of connected WebSocket clients
 */
size_t WebServer::getClientCount() const {
    return ws ? ws->count() : 0;
}
