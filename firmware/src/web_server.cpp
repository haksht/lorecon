/**
 * Web Server Implementation
 */

#include "web_server.h"
#include "api_controller.h"
#include "logger.h"
#include "recon_state.h"
#include "packet_logger.h"
#include <LittleFS.h>
#include <SD.h>
#include <Update.h>

// Global instance for static handlers
WebServer* g_webServer = nullptr;

WebServer::WebServer() 
    : server(nullptr)
    , ws(nullptr)
    , reconTool(nullptr)
    , activeClients(0)
    , lastBroadcast(0)
    , pendingPacketBroadcast(false)
    , pendingClientCleanup(false)
    , disconnectInProgress(false)
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
    server->on("/api/export/pcap", HTTP_GET, handleExportPCAP);
    
    // Status & Config
    server->on("/api/status", HTTP_GET, handleGetStatus);
    server->on("/api/dashboard", HTTP_GET, handleGetDashboard);
    server->on("/api/statistics", HTTP_GET, handleGetStatistics);
    server->on("/api/activity", HTTP_GET, handleGetActivity);
    server->on("/api/config", HTTP_GET, handleGetConfig);
    server->on("/api/config/system", HTTP_GET, handleGetSystemConfig);
    server->on("/api/recon/summary", HTTP_GET, handleGetReconSummary);
    server->on("/api/recon/device-types", HTTP_GET, handleGetDeviceTypeSummary);
    server->on("/api/recon/security", HTTP_GET, handleGetSecurityAssessment);
    server->on("/api/replay/slots", HTTP_GET, handleGetReplaySlots);
    server->on("/api/replay/clear", HTTP_POST, handleClearReplaySlots);
    server->on("/api/replay/transmit", HTTP_POST, handleReplayPacket);
    server->on("/api/frequency/target", HTTP_POST, handleStartFrequencyTargeting);
    server->on("/api/diagnostics", HTTP_GET, handleGetDiagnostics);
    server->on("/api/diagnostics/verbose", HTTP_POST, handleSetVerboseMode);
    
    // Command handling (reboot, etc.)
    server->on("/api/command", HTTP_POST, handleCommand);
    
    // Scan Control
    server->on("/api/scan/start", HTTP_POST, handleStartScan);
    server->on("/api/scan/stop", HTTP_POST, handleStopScan);
    
    // OTA Firmware Update
    server->on("/api/firmware/upload", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            // Response after upload completes
            bool success = !Update.hasError();
            String response = success ? 
                "{\"status\":\"success\",\"message\":\"Firmware uploaded successfully. Rebooting in 3 seconds...\"}" :
                "{\"status\":\"error\",\"message\":\"Firmware upload failed. Check serial output.\"}";
            
            AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", response);
            resp->addHeader("Connection", "close");
            request->send(resp);
            
            if (success) {
                LOG_INFO("OTA Update Success - Rebooting...");
                delay(3000);
                ESP.restart();
            }
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            // Handle firmware upload chunks
            if (index == 0) {
                LOG_INFO("OTA Update Start: %s", filename.c_str());
                
                // Validate file extension
                if (!filename.endsWith(".bin")) {
                    LOG_ERROR("Invalid firmware file - must be .bin");
                    return;
                }
                
                // Begin OTA update
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                    LOG_ERROR("OTA begin failed");
                    return;
                }
                
                LOG_INFO("OTA upload started, free space: %d bytes", UPDATE_SIZE_UNKNOWN);
            }
            
            // Write firmware chunk
            if (len) {
                if (Update.write(data, len) != len) {
                    Update.printError(Serial);
                    LOG_ERROR("OTA write failed at %d bytes", index);
                    return;
                }
                
                // Log progress every 100KB
                if (index % 102400 == 0) {
                    LOG_INFO("OTA progress: %d bytes written", index + len);
                }
            }
            
            // Finalize upload
            if (final) {
                if (Update.end(true)) {
                    LOG_INFO("OTA Update complete: %d bytes", index + len);
                } else {
                    Update.printError(Serial);
                    LOG_ERROR("OTA end failed");
                }
            }
        }
    );
    
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

void WebServer::handleExportPCAP(AsyncWebServerRequest* request) {
    #ifdef ENABLE_PCAP_EXPORT
    // Get current PCAP session file from packet logger
    extern PacketLogger packetLogger;
    
    // Check if SD card is available
    if (!packetLogger.isAvailable()) {
        request->send(404, "application/json", 
            "{\"status\":\"error\",\"message\":\"SD card not available. Insert SD card and restart device to enable PCAP capture.\"}");
        LOG_WARN("PCAP download requested but SD card not available");
        return;
    }
    
    String sessionFile = packetLogger.getCurrentSessionFile();
    
    // Check if a session is active (session file is just the filename, e.g. "snf_12345.csv")
    if (sessionFile.isEmpty()) {
        request->send(404, "application/json", 
            "{\"status\":\"error\",\"message\":\"No active logging session. Wait for packet capture to start.\"}");
        LOG_WARN("PCAP download requested but no active session");
        return;
    }
    
    // Build PCAP file path: /logs/<session>.pcap
    String pcapFile = "/logs/";
    if (sessionFile.endsWith(".csv")) {
        pcapFile += sessionFile.substring(0, sessionFile.length() - 4) + ".pcap";
    } else {
        pcapFile += sessionFile + ".pcap";
    }
    
    // Check if PCAP file exists
    if (SD.exists(pcapFile.c_str())) {
        // Send file with proper MIME type and force download
        request->send(SD, pcapFile, "application/vnd.tcpdump.pcap", true);
        LOG_INFO("PCAP file downloaded: %s", pcapFile.c_str());
    } else {
        // PCAP file doesn't exist - provide helpful error message
        String csvPath = "/logs/" + sessionFile;
        bool csvExists = SD.exists(csvPath.c_str());
        
        String errorMsg;
        if (csvExists) {
            errorMsg = "{\"status\":\"error\",\"message\":\"PCAP file not found. CSV logging is active but PCAP generation may have failed. Check SD card space and PCAP logger status.\"}";
        } else {
            errorMsg = "{\"status\":\"error\",\"message\":\"No packet capture file available. Ensure SD card is inserted and packets have been captured.\"}";
        }
        
        request->send(404, "application/json", errorMsg);
        LOG_WARN("PCAP download requested but file not found: %s (CSV exists: %s)", 
                 pcapFile.c_str(), csvExists ? "yes" : "no");
    }
    #else
    // PCAP export disabled
    request->send(501, "application/json", 
        "{\"status\":\"error\",\"message\":\"PCAP export is disabled. Enable ENABLE_PCAP_EXPORT in config.h and recompile.\"}");
    #endif
}

void WebServer::handleGetStatus(AsyncWebServerRequest* request) {
    String json = APIController::getStatus();
    request->send(200, "application/json", json);
}

void WebServer::handleGetDashboard(AsyncWebServerRequest* request) {
    String json = APIController::getDashboard();
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

void WebServer::handleGetSystemConfig(AsyncWebServerRequest* request) {
    String json = APIController::getSystemConfig();
    request->send(200, "application/json", json);
}

void WebServer::handleCommand(AsyncWebServerRequest* request) {
    AsyncWebParameter* param = nullptr;
    if (request->hasParam("command", true)) {
        param = request->getParam("command", true);
    } else if (request->hasParam("command")) {
        param = request->getParam("command");
    }

    if (!param) {
        request->send(400, "application/json", "{\"status\":\"error\",\"error\":\"Missing command\"}");
        return;
    }

    String cmd = param->value();
    
    // Handle reboot command
    if (cmd == "b") {
        String response = "{\"status\":\"success\",\"message\":\"Rebooting device...\"}";
        AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", response);
        resp->addHeader("Connection", "close");
        request->send(resp);
        
        LOG_INFO("Reboot command received via web API");
        delay(1000);
        ESP.restart();
        return;
    }
    
    // Unknown command
    request->send(400, "application/json", "{\"status\":\"error\",\"error\":\"Unknown command\"}");
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

void WebServer::handleReplayPacket(AsyncWebServerRequest* request) {
    // Get parameters from POST body
    AsyncWebParameter* slotParam = nullptr;
    AsyncWebParameter* repeatParam = nullptr;
    AsyncWebParameter* delayParam = nullptr;

    if (request->hasParam("slotIndex", true)) {
        slotParam = request->getParam("slotIndex", true);
    } else if (request->hasParam("slotIndex")) {
        slotParam = request->getParam("slotIndex");
    }

    if (request->hasParam("repeatCount", true)) {
        repeatParam = request->getParam("repeatCount", true);
    } else if (request->hasParam("repeatCount")) {
        repeatParam = request->getParam("repeatCount");
    }

    if (request->hasParam("delayMs", true)) {
        delayParam = request->getParam("delayMs", true);
    } else if (request->hasParam("delayMs")) {
        delayParam = request->getParam("delayMs");
    }

    if (!slotParam) {
        request->send(400, "application/json", "{\"status\":\"error\",\"error\":\"Missing slotIndex\"}");
        return;
    }

    uint8_t slotIndex = strtoul(slotParam->value().c_str(), nullptr, 0);
    uint8_t repeatCount = repeatParam ? strtoul(repeatParam->value().c_str(), nullptr, 0) : 1;
    uint16_t delayMs = delayParam ? strtoul(delayParam->value().c_str(), nullptr, 0) : 1000;

    String json = APIController::replayPacket(slotIndex, repeatCount, delayMs);
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
            // Don't access client object - can cause heap corruption
            LOG_INFO("WebSocket client connected");
            if (g_webServer) {
                g_webServer->disconnectInProgress.store(false, std::memory_order_release);  // Clear flag
                g_webServer->activeClients.fetch_add(1, std::memory_order_relaxed);
                g_webServer->lastBroadcast = millis();
                // Send a simple ACK to keep client heartbeat alive
                if (g_webServer->ws) {
                    g_webServer->ws->textAll("{\"type\":\"connected\"}");
                }
            }
            break;
            
        case WS_EVT_DISCONNECT:
            // Don't access client object - it may already be freed
            LOG_INFO("WebSocket client disconnected");
            if (g_webServer) {
                // Set flag FIRST to block any pending data events
                g_webServer->disconnectInProgress.store(true, std::memory_order_release);
                
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
            // CRITICAL: Ignore all data events if disconnect in progress
            // AsyncWebServer can deliver stale WS_EVT_DATA after WS_EVT_DISCONNECT
            if (g_webServer && !g_webServer->disconnectInProgress.load(std::memory_order_acquire)) {
                handleWebSocketMessage(client, data, len);
            }
            break;
            
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void WebServer::handleWebSocketMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
    // Safety checks: validate all inputs
    // Don't access client methods - it may be freed/disconnected
    if (!client || !data) {
        return;
    }
    
    // Ignore invalid or garbage messages
    if (len == 0 || len > 1024) {
        return;  // Silently ignore
    }

    // Check if it looks like JSON (starts with '{')
    if (len > 0 && data[0] == '{') {
        // Just log that we received a message - no client ID access
        LOG_INFO("WebSocket JSON command (len=%u)", len);
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

    // Build JSON (keep it small)
    JsonDocument doc;
    doc["type"] = "packet";
    doc["nodeId"] = String(aggStats.lastNodeId, HEX);
    doc["rssi"] = aggStats.lastRSSI;
    doc["count"] = aggStats.packetCount;
    
    String json;
    serializeJson(doc, json);

    // Non-blocking send
    if (json.length() < 256) {  // Safety limit
        ws->textAll(json);
    }
    
    aggStats.packetCount = 0;
    lastBroadcast = millis();
    pendingPacketBroadcast.store(false, std::memory_order_relaxed);
}

/**
 * Periodic maintenance - cleanup dead connections
 */
void WebServer::periodicUpdate() {
    // Disabled - cleanupClients() causes stack overflow
    // Let AsyncWebServer handle cleanup internally
    return;
    
    if (!ws) {
        return;
    }

    if (activeClients.load(std::memory_order_relaxed) == 0) {
        return;
    }

    if (ws) {
        ws->cleanupClients();
        ws->pingAll();
    }
}

void WebServer::service() {
    // Throttled broadcast to prevent watchdog timeout
    uint32_t now = millis();
    if (now - lastBroadcast >= BROADCAST_INTERVAL_MS) {
        bool broadcastPending = pendingPacketBroadcast.load(std::memory_order_relaxed);
        if (broadcastPending && activeClients.load(std::memory_order_relaxed) > 0) {
            broadcastAggregatedUpdate();
        }
    }

    // Clear cleanup flag
    pendingClientCleanup.store(false, std::memory_order_relaxed);
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
    
    ws->textAll(json);
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

    ws->textAll(json);
}

/**
 * Get number of connected WebSocket clients
 */
size_t WebServer::getClientCount() const {
    return activeClients.load(std::memory_order_relaxed);
}

bool WebServer::cleanupWebSocketClients() {
    if (!ws) {
        return true;
    }

    ws->cleanupClients();
    return true;
}
