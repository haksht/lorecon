/**
 * Web Server Implementation
 */

#include "web_server.h"
#include "api_controller.h"
#include "logger.h"
#include "recon_state.h"
#include <LittleFS.h>

// Global instance for static handlers
WebServer* g_webServer = nullptr;

WebServer::WebServer() 
    : server(nullptr)
    , ws(nullptr)
    , reconTool(nullptr)
    , wsMutex(nullptr)
    , activeClients(0)
    , lastBroadcast(0)
    , pendingPacketBroadcast(false)
    , pendingClientCleanup(false)
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

    // Initialize synchronization primitive for safe websocket access
    wsMutex = xSemaphoreCreateMutex();
    if (!wsMutex) {
        LOG_WARN("WebSocket mutex creation failed; concurrency safeguards disabled");
    }

    activeClients.store(0);
    pendingPacketBroadcast.store(false, std::memory_order_relaxed);
    pendingClientCleanup.store(false, std::memory_order_relaxed);
    
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

    if (wsMutex) {
        vSemaphoreDelete(wsMutex);
        wsMutex = nullptr;
    }

    activeClients.store(0);
    pendingPacketBroadcast.store(false, std::memory_order_relaxed);
    pendingClientCleanup.store(false, std::memory_order_relaxed);
    
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
    server->on("/api/recon/summary", HTTP_GET, handleGetReconSummary);
    server->on("/api/recon/device-types", HTTP_GET, handleGetDeviceTypeSummary);
    server->on("/api/recon/security", HTTP_GET, handleGetSecurityAssessment);
    server->on("/api/replay/slots", HTTP_GET, handleGetReplaySlots);
    server->on("/api/replay/clear", HTTP_POST, handleClearReplaySlots);
    server->on("/api/frequency/target", HTTP_POST, handleStartFrequencyTargeting);
    server->on("/api/diagnostics", HTTP_GET, handleGetDiagnostics);
    server->on("/api/diagnostics/verbose", HTTP_POST, handleSetVerboseMode);
    
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

void WebServer::handleGetReconSummary(AsyncWebServerRequest* request) {
    String json = APIController::getReconSummary();
    request->send(200, "application/json", json);
}

void WebServer::handleGetDeviceTypeSummary(AsyncWebServerRequest* request) {
    String json = APIController::getDeviceTypeSummary();
    request->send(200, "application/json", json);
}

void WebServer::handleGetSecurityAssessment(AsyncWebServerRequest* request) {
    String json = APIController::getSecurityAssessment();
    request->send(200, "application/json", json);
}

void WebServer::handleGetReplaySlots(AsyncWebServerRequest* request) {
    String json = APIController::getReplaySlots();
    request->send(200, "application/json", json);
}

void WebServer::handleClearReplaySlots(AsyncWebServerRequest* request) {
    String json = APIController::clearReplaySlots();
    request->send(200, "application/json", json);
}

void WebServer::handleStartFrequencyTargeting(AsyncWebServerRequest* request) {
    AsyncWebParameter* param = nullptr;
    if (request->hasParam("configIndex", true)) {
        param = request->getParam("configIndex", true);
    } else if (request->hasParam("configIndex")) {
        param = request->getParam("configIndex");
    }

    if (!param) {
        request->send(400, "application/json", "{\"status\":\"error\",\"error\":\"Missing configIndex\"}");
        return;
    }

    String indexStr = param->value();
    uint32_t rawIndex = strtoul(indexStr.c_str(), nullptr, 0);

    if (rawIndex > UINT8_MAX) {
        request->send(400, "application/json", "{\"status\":\"error\",\"error\":\"configIndex out of range\"}");
        return;
    }

    uint8_t configIndex = static_cast<uint8_t>(rawIndex);
    if (!reconState.isValidConfigIndex(configIndex) && rawIndex > 0) {
        uint32_t adjusted = rawIndex - 1;
        if (adjusted <= UINT8_MAX && reconState.isValidConfigIndex(static_cast<uint8_t>(adjusted))) {
            configIndex = static_cast<uint8_t>(adjusted);
        }
    }

    String json = APIController::startFrequencyTargeting(configIndex);
    request->send(200, "application/json", json);
}

void WebServer::handleGetDiagnostics(AsyncWebServerRequest* request) {
    String json = APIController::getDiagnostics();
    request->send(200, "application/json", json);
}

void WebServer::handleSetVerboseMode(AsyncWebServerRequest* request) {
    AsyncWebParameter* param = nullptr;
    if (request->hasParam("enable", true)) {
        param = request->getParam("enable", true);
    } else if (request->hasParam("enable")) {
        param = request->getParam("enable");
    }

    if (!param) {
        request->send(400, "application/json", "{\"status\":\"error\",\"error\":\"Missing enable parameter\"}");
        return;
    }

    String enableStr = param->value();
    enableStr.trim();
    enableStr.toLowerCase();

    bool enableVerbose;
    if (enableStr == "true" || enableStr == "1" || enableStr == "yes" || enableStr == "on" || enableStr == "verbose") {
        enableVerbose = true;
    } else if (enableStr == "false" || enableStr == "0" || enableStr == "no" || enableStr == "off" || enableStr == "quiet") {
        enableVerbose = false;
    } else {
        request->send(400, "application/json", "{\"status\":\"error\",\"error\":\"Invalid enable value\"}");
        return;
    }

    String json = APIController::setVerboseMode(enableVerbose);
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
                g_webServer->activeClients.fetch_add(1, std::memory_order_relaxed);
                g_webServer->broadcastStatusUpdate();
                g_webServer->lastBroadcast = millis();
            }
            break;
            
        case WS_EVT_DISCONNECT:
            LOG_INFO("WebSocket client #%u disconnected", client->id());
            if (g_webServer) {
                size_t previous = g_webServer->activeClients.load(std::memory_order_relaxed);
                while (previous != 0 && !g_webServer->activeClients.compare_exchange_weak(
                           previous, previous - 1, std::memory_order_relaxed)) {
                    // Loop until the decrement succeeds or count hits zero
                }
                g_webServer->aggStats = AggregatedStats{};
                g_webServer->lastBroadcast = millis();
                g_webServer->pendingPacketBroadcast.store(false, std::memory_order_relaxed);
                g_webServer->pendingClientCleanup.store(true, std::memory_order_relaxed);
            }
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
    // Ignore invalid or garbage messages
    if (len == 0 || len > 1024) {
        return;  // Silently ignore
    }

    if (data[0] == '{') {
        // Build a bounded, printable preview without assuming null termination
        const size_t previewLen = std::min(len, static_cast<size_t>(128));
        String preview;
        preview.reserve(previewLen + 3);
        for (size_t i = 0; i < previewLen; ++i) {
            char c = static_cast<char>(data[i]);
            if (c >= 32 && c <= 126) {
                preview += c;
            } else {
                preview += '?';
            }
        }
        if (len > previewLen) {
            preview += "...";
        }

        LOG_INFO("WebSocket command from client #%u: %s", client->id(), preview.c_str());
        // Future: handle commands routed over WebSocket
    }
    // Silently ignore other messages (pings, pongs, binary data, garbage)
}

/**
 * Handle packet event from PacketProcessor (callback)
 */
void WebServer::handlePacketEvent(const PacketEvent& evt) {
    // Aggregate packet data
    aggStats.packetCount++;
    aggStats.lastNodeId = evt.nodeId;
    aggStats.lastProtocol = evt.protocol;
    aggStats.lastRSSI = evt.rssi;
    aggStats.lastSNR = evt.snr;
    
    // Copy message safely to prevent dangling pointer
    if (evt.message && strlen(evt.message) > 0) {
        strncpy(aggStats.lastMessage, evt.message, sizeof(aggStats.lastMessage) - 1);
        aggStats.lastMessage[sizeof(aggStats.lastMessage) - 1] = '\0';
        aggStats.hasMessage = true;
    } else {
        aggStats.lastMessage[0] = '\0';
        aggStats.hasMessage = false;
    }
    
    aggStats.timestamp = evt.timestamp;
    pendingPacketBroadcast.store(true, std::memory_order_relaxed);
}

/**
 * Broadcast aggregated packet update to all WebSocket clients
 */
void WebServer::broadcastAggregatedUpdate() {
    // Safety checks: no broadcast if no websocket, no stats, or no clients
    if (!ws || !server || aggStats.packetCount == 0 || activeClients.load(std::memory_order_relaxed) == 0) {
        pendingPacketBroadcast.store(false, std::memory_order_relaxed);
        return;
    }

    // Build JSON
    JsonDocument doc;
    doc["type"] = "packet";
    doc["nodeId"] = String(aggStats.lastNodeId, HEX);
    doc["protocol"] = aggStats.lastProtocol;
    doc["rssi"] = aggStats.lastRSSI;
    doc["snr"] = aggStats.lastSNR;
    doc["count"] = aggStats.packetCount;
    doc["timestamp"] = aggStats.timestamp;
    if (aggStats.hasMessage) {
        doc["message"] = aggStats.lastMessage;
    }
    
    String json;
    serializeJson(doc, json);

    if (!acquireWebSocketLock()) {
        return;
    }

    ws->textAll(json);
    releaseWebSocketLock();
    aggStats.packetCount = 0;
    lastBroadcast = millis();
    pendingPacketBroadcast.store(false, std::memory_order_relaxed);
}

/**
 * Periodic maintenance - cleanup dead connections
 */
void WebServer::periodicUpdate() {
    if (!ws) {
        return;
    }

    if (activeClients.load(std::memory_order_relaxed) == 0) {
        return;
    }

    if (!acquireWebSocketLock()) {
        return;
    }

    if (ws) {
        ws->cleanupClients();
        ws->pingAll();
    }

    releaseWebSocketLock();
}

void WebServer::service() {
    bool broadcastPending = pendingPacketBroadcast.load(std::memory_order_relaxed);
    if (broadcastPending) {
        uint32_t now = millis();
        if (now - lastBroadcast >= BROADCAST_INTERVAL_MS) {
            broadcastAggregatedUpdate();
        }
    }

    if (pendingClientCleanup.load(std::memory_order_relaxed)) {
        if (cleanupWebSocketClients()) {
            pendingClientCleanup.store(false, std::memory_order_relaxed);
        }
    }
}

/**
 * Broadcast device update to all connected WebSocket clients
 */
void WebServer::broadcastDeviceUpdate(uint32_t nodeId) {
    if (!ws || !server || activeClients.load(std::memory_order_relaxed) == 0) return;
    
    JsonDocument doc;
    doc["type"] = "deviceUpdate";
    doc["nodeId"] = String(nodeId, HEX);
    doc["timestamp"] = millis();
    
    String json;
    serializeJson(doc, json);
    
    if (!acquireWebSocketLock()) {
        return;
    }

    ws->textAll(json);
    releaseWebSocketLock();
}

/**
 * Broadcast status update to all connected WebSocket clients
 */
void WebServer::broadcastStatusUpdate() {
    if (!ws || !server || activeClients.load(std::memory_order_relaxed) == 0) return;

    String statusJson = APIController::getStatus();
    
    JsonDocument doc;
    doc["type"] = "status";
    deserializeJson(doc["data"], statusJson);
    doc["timestamp"] = millis();
    
    String json;
    serializeJson(doc, json);

    if (!acquireWebSocketLock()) {
        return;
    }

    ws->textAll(json);
    releaseWebSocketLock();
}

/**
 * Get number of connected WebSocket clients
 */
size_t WebServer::getClientCount() const {
    return activeClients.load(std::memory_order_relaxed);
}

bool WebServer::acquireWebSocketLock(TickType_t timeout) const {
    if (!wsMutex) {
        return true;
    }

    if (xSemaphoreTake(wsMutex, timeout) == pdTRUE) {
        return true;
    }

    LOG_DEBUG("WebSocket mutex acquisition timed out");
    return false;
}

void WebServer::releaseWebSocketLock() const {
    if (wsMutex) {
        xSemaphoreGive(wsMutex);
    }
}

bool WebServer::cleanupWebSocketClients() {
    if (!ws) {
        return true;
    }

    if (!acquireWebSocketLock()) {
        return false;
    }

    if (ws) {
        ws->cleanupClients();
    }

    releaseWebSocketLock();
    return true;
}
