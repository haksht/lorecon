/**
 * Web Server Implementation
 * 
 * Core server lifecycle, WebSocket handling, and route setup.
 * REST API handlers are in api_handlers.cpp
 * WiFi handlers are in wifi_handlers.cpp
 */

#include "web_server.h"
#include "api_handlers.h"
#include "wifi_handlers.h"
#include "api_controller.h"
#include "api_security.h"
#include "lora_recon_tool.h"
#include "logger.h"
#include "recon_state.h"
#include "wifi_manager.h"
#include "config.h"
#include "utils/format_utils.h"
#include "utils/json_utils.h"
#include <LittleFS.h>
#include <Update.h>
#include <ArduinoJson.h>

// Global instance for static handlers
WebServer* g_webServer = nullptr;

WebServer::WebServer() 
    : server(nullptr)
    , ws(nullptr)
    , dnsServer(nullptr)
    , reconTool(nullptr)
    , activeClients(0)
    , lastBroadcast(0)
    , lastDisconnectTime(0)
    , pendingPacketBroadcast(false)
    , pendingClientCleanup(false)
    , disconnectInProgress(false)
{
}

/**
 * Start web server
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

    // Pre-check heap before allocating server objects
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 40000) {
        LOG_ERROR("Insufficient heap for web server (%lu bytes, need 40000+)", freeHeap);
        return false;
    }

    // Create server and WebSocket
    server = new AsyncWebServer(port);
    ws = new AsyncWebSocket("/ws");
    
    // Check allocations succeeded
    if (!server || !ws) {
        LOG_ERROR("Failed to allocate web server or WebSocket - out of memory");
        if (server) { delete server; server = nullptr; }
        if (ws) { delete ws; ws = nullptr; }
        return false;
    }
    
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
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, " + String(Config::Security::AUTH_HEADER));
    // Force browsers to revalidate cached files on each request.
    // Local device on LAN  -  latency is negligible, prevents stale JS/CSS after OTA updates.
    DefaultHeaders::Instance().addHeader("Cache-Control", "no-cache");
    
    // Initialize API security
    APISecurity::begin();
    
    // Start DNS server for captive portal (only in AP mode)
    if (wifiManager.isSetupMode()) {
        dnsServer = new DNSServer();
        dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer->start(53, "*", WiFi.softAPIP());
        LOG_INFO("+ Captive portal DNS started (all domains -> %s)", WiFi.softAPIP().toString().c_str());
    }
    
    // Start server
    server->begin();
    
    LOG_INFO("+ Web server started on port %d", port);
    LOG_INFO("  REST API: http://<ip>/api/");
    LOG_INFO("  WebSocket: ws://<ip>/ws");
    LOG_INFO("  Web App: http://<ip>/");
    
    return true;
}

/**
 * Stop web server
 */
void WebServer::stop() {
    if (dnsServer) {
        dnsServer->stop();
        delete dnsServer;
        dnsServer = nullptr;
    }
    
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
 * 
 * Routes delegate to handlers in api_handlers.cpp and wifi_handlers.cpp
 */
void WebServer::setupRoutes() {
    // Device Management
    server->on("/api/devices", HTTP_GET, APIHandlers::handleGetDevices);
    server->on("/api/device", HTTP_GET, APIHandlers::handleGetDevice);
    server->on("/api/capture/start", HTTP_POST, APIHandlers::handleStartCapture);
    server->on("/api/capture/stop", HTTP_POST, APIHandlers::handleStopCapture);
    
    // Geographic Data
    server->on("/api/positions", HTTP_GET, APIHandlers::handleGetPositions);
    server->on("/api/export/geojson", HTTP_GET, APIHandlers::handleExportGeoJSON);
    server->on("/api/export/kml", HTTP_GET, APIHandlers::handleExportKML);
    server->on("/api/export/pcap", HTTP_GET, APIHandlers::handleExportPCAP);
    server->on("/api/export/csv", HTTP_GET, APIHandlers::handleExportCSV);
    server->on("/api/export/report", HTTP_GET, APIHandlers::handleExportReport);

    // SD File Browser
    server->on("/api/files", HTTP_GET, APIHandlers::handleListFiles);
    server->on("/api/files/download", HTTP_GET, APIHandlers::handleDownloadFile);

    // Status & Config
    server->on("/api/status", HTTP_GET, APIHandlers::handleGetStatus);
    server->on("/api/dashboard", HTTP_GET, APIHandlers::handleGetDashboard);
    server->on("/api/statistics", HTTP_GET, APIHandlers::handleGetStatistics);
    server->on("/api/activity", HTTP_GET, APIHandlers::handleGetActivity);
    server->on("/api/config", HTTP_GET, APIHandlers::handleGetConfig);
    server->on("/api/config/system", HTTP_GET, APIHandlers::handleGetSystemConfig);
    server->on("/api/recon/summary", HTTP_GET, APIHandlers::handleGetReconSummary);
    server->on("/api/recon/device-types", HTTP_GET, APIHandlers::handleGetDeviceTypeSummary);
    server->on("/api/recon/security", HTTP_GET, APIHandlers::handleGetSecurityAssessment);
    server->on("/api/replay/slots", HTTP_GET, APIHandlers::handleGetReplaySlots);
    server->on("/api/replay/clear", HTTP_POST, APIHandlers::handleClearReplaySlots);
    server->on("/api/devices/clear", HTTP_POST, APIHandlers::handleClearDevices);
    server->on("/api/replay/transmit", HTTP_POST, APIHandlers::handleReplayPacket);
    server->on("/api/frequency/target", HTTP_POST, APIHandlers::handleStartFrequencyTargeting);
    server->on("/api/diagnostics", HTTP_GET, APIHandlers::handleGetDiagnostics);
    server->on("/api/diagnostics/verbose", HTTP_POST, APIHandlers::handleSetVerboseMode);
    
    // Temporal & Anomaly Analysis
    server->on("/api/anomalies", HTTP_GET, APIHandlers::handleGetAnomalies);
    server->on("/api/anomaly/acknowledge", HTTP_POST, APIHandlers::handleAcknowledgeAnomaly);
    server->on("/api/temporal", HTTP_GET, APIHandlers::handleGetTemporalData);
    
    // PSK/Decryption Stats
    server->on("/api/psk/stats", HTTP_GET, APIHandlers::handleGetPSKStats);
    
    // Command handling
    server->on("/api/command", HTTP_POST, APIHandlers::handleCommand);
    
    // Scan Control
    server->on("/api/scan/start", HTTP_POST, APIHandlers::handleStartScan);
    server->on("/api/scan/stop", HTTP_POST, APIHandlers::handleStopScan);
    
    // WiFi Setup
    server->on("/api/wifi/status", HTTP_GET, WiFiHandlers::handleGetWiFiStatus);
    server->on("/api/wifi/configure", HTTP_POST, WiFiHandlers::handleSetWiFiCredentials);
    server->on("/api/wifi/clear", HTTP_POST, WiFiHandlers::handleClearWiFiCredentials);
    server->on("/api/wifi/ap-password", HTTP_POST, WiFiHandlers::handleSetAPPassword);
    
    // OTA Firmware Update (kept inline - complex state management)
    server->on("/api/firmware/upload", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            if (!APISecurity::isAuthenticated(request)) {
                APISecurity::sendUnauthorized(request);
                return;
            }
            
            bool success = !Update.hasError();
            String response = success ? 
                JsonUtils::success("Firmware uploaded successfully. Rebooting in 3 seconds...") :
                JsonUtils::error("Firmware upload failed. Check serial output.");
            
            AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", response);
            resp->addHeader("Connection", "close");
            request->send(resp);
            
            if (success) {
                LOG_INFO("OTA Update Success - Rebooting...");
                if (g_reconTool) {
                    OLEDDisplay* oled = g_reconTool->getDisplay();
                    if (oled) oled->showReboot();
                }
                delay(3000);
                ESP.restart();
            }
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (index == 0 && !APISecurity::isAuthenticated(request)) {
                LOG_ERROR("OTA upload rejected - unauthorized");
                return;
            }
            
            if (index == 0) {
                LOG_INFO("OTA Update Start: %s", filename.c_str());
                
                if (!filename.endsWith(".bin")) {
                    LOG_ERROR("Invalid firmware file - must be .bin");
                    return;
                }
                
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                    LOG_ERROR("OTA begin failed");
                    return;
                }
                
                LOG_INFO("OTA upload started");
            }
            
            if (len) {
                if (Update.write(data, len) != len) {
                    Update.printError(Serial);
                    LOG_ERROR("OTA write failed at %d bytes", index);
                    return;
                }
                
                if (index % 102400 == 0) {
                    LOG_INFO("OTA progress: %d bytes written", index + len);
                }
            }
            
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
    
    // OTA Filesystem Update
    server->on("/api/filesystem/upload", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            if (!APISecurity::isAuthenticated(request)) {
                APISecurity::sendUnauthorized(request);
                return;
            }

            bool success = !Update.hasError();
            String response = success ?
                JsonUtils::success("Filesystem uploaded successfully. Rebooting in 3 seconds...") :
                JsonUtils::error("Filesystem upload failed. Check serial output.");

            AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", response);
            resp->addHeader("Connection", "close");
            request->send(resp);

            if (success) {
                LOG_INFO("OTA Filesystem Update Success - Rebooting...");
                if (g_reconTool) {
                    OLEDDisplay* oled = g_reconTool->getDisplay();
                    if (oled) oled->showReboot();
                }
                delay(3000);
                ESP.restart();
            }
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (index == 0 && !APISecurity::isAuthenticated(request)) {
                LOG_ERROR("Filesystem OTA upload rejected - unauthorized");
                return;
            }

            if (index == 0) {
                LOG_INFO("Filesystem OTA Update Start: %s", filename.c_str());

                if (!filename.endsWith(".bin")) {
                    LOG_ERROR("Invalid filesystem file - must be .bin");
                    return;
                }

                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
                    Update.printError(Serial);
                    LOG_ERROR("Filesystem OTA begin failed");
                    return;
                }

                LOG_INFO("Filesystem OTA upload started");
            }

            if (len) {
                if (Update.write(data, len) != len) {
                    Update.printError(Serial);
                    LOG_ERROR("Filesystem OTA write failed at %d bytes", index);
                    return;
                }
            }

            if (final) {
                if (Update.end(true)) {
                    LOG_INFO("Filesystem OTA complete: %d bytes", index + len);
                } else {
                    Update.printError(Serial);
                    LOG_ERROR("Filesystem OTA end failed");
                }
            }
        }
    );

    // Health check
    server->on("/api/health", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "application/json", JsonUtils::healthOk());
    });
    
    // Token retrieval  -  AP subnet clients get unconditional access (physical proximity);
    // auto-trusted LAN clients (STA/AP_STA mode, private network) also receive the token
    // since they already have full API access via isAuthenticated().
    server->on("/api/auth/token", HTTP_GET, [](AsyncWebServerRequest* request) {
        IPAddress clientIP = request->client()->remoteIP();
        bool isAPSubnet = (clientIP[0] == 192 && clientIP[1] == 168 && clientIP[2] == 4);
        if (!isAPSubnet && !APISecurity::isAuthenticated(request)) {
            APISecurity::sendUnauthorized(request);
            return;
        }
        String json = JsonUtils::successWithData([](JsonDocument& doc) {
            doc["token"] = APISecurity::getToken();
        });
        request->send(200, "application/json", json);
    });

    // Auth info
    server->on("/api/auth/info", HTTP_GET, [](AsyncWebServerRequest* request) {
        String json = JsonUtils::successWithData([](JsonDocument& doc) {
            doc["authEnabled"] = Config::Security::AUTH_ENABLED;
            doc["authHeader"] = Config::Security::AUTH_HEADER;
            doc["tokenHint"] = "Check serial output at boot for API token";
            JsonArray protected_eps = doc["protectedEndpoints"].to<JsonArray>();
            protected_eps.add("/api/devices/clear");
            protected_eps.add("/api/replay/transmit");
            protected_eps.add("/api/replay/clear");
            protected_eps.add("/api/wifi/configure");
            protected_eps.add("/api/wifi/clear");
            protected_eps.add("/api/wifi/ap-password");
            protected_eps.add("/api/command");
            protected_eps.add("/api/firmware/upload");
        });
        request->send(200, "application/json", json);
    });
    
    LOG_INFO("+ REST API routes configured");
}

/**
 * Setup WebSocket for real-time updates
 */
void WebServer::setupWebSocket() {
    ws->onEvent(handleWebSocketEvent);
    LOG_INFO("+ WebSocket configured at /ws");
}

/**
 * Serve static files for Progressive Web App
 */
void WebServer::serveStaticFiles() {
    // Simple test pages
    server->on("/test", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/html", "<html><body><h1>ESP32 Web Server Working!</h1></body></html>");
    });
    
    server->on("/apitest", HTTP_GET, [](AsyncWebServerRequest* request) {
        String html = "<html><head><style>body{font-family:sans-serif;padding:20px;}</style></head><body>";
        html += "<h1>API Test</h1>";
        html += "<button onclick=\"fetch('/api/status').then(r=>r.json()).then(d=>alert(JSON.stringify(d)))\">Test API</button>";
        html += "<button onclick=\"ws=new WebSocket('ws://'+location.host+'/ws');ws.onopen=()=>alert('WS Connected!')\">Test WebSocket</button>";
        html += "</body></html>";
        request->send(200, "text/html", html);
    });
    
    // Captive portal detection endpoints
    server->on("/generate_204", HTTP_GET, [](AsyncWebServerRequest* request) { request->redirect("/"); });
    server->on("/gen_204", HTTP_GET, [](AsyncWebServerRequest* request) { request->redirect("/"); });
    server->on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest* request) { request->redirect("/"); });
    server->on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest* request) { request->redirect("/"); });
    server->on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest* request) { request->redirect("/"); });
    server->on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest* request) { request->redirect("/"); });
    server->on("/success.txt", HTTP_GET, [](AsyncWebServerRequest* request) { request->redirect("/"); });
    server->on("/fwlink", HTTP_GET, [](AsyncWebServerRequest* request) { request->redirect("/"); });
    
    // Root serves index.html
    server->serveStatic("/", LittleFS, "/webapp/").setDefaultFile("index.html");
    
    // Fallback handler
    server->onNotFound([](AsyncWebServerRequest* request) {
        String url = request->url();
        
        // Silently handle browser icon requests
        if (url.indexOf("apple-touch-icon") >= 0 || url.indexOf("favicon") >= 0 || url.endsWith(".ico")) {
            request->send(204);
            return;
        }
        
        // API 404
        if (url.startsWith("/api/")) {
            request->send(404, "application/json", JsonUtils::error("Endpoint not found"));
            return;
        }
        
        // Captive portal redirect for external hosts
        String host = request->host();
        if (!host.startsWith("192.168.") && !host.endsWith(".local") && host != "localhost") {
            request->redirect("http://192.168.4.1/");
            return;
        }
        
        // SPA routing fallback
        request->send(LittleFS, "/webapp/index.html", "text/html");
    });
    
    LOG_INFO("+ Static file serving configured");
}

// =============================================================================
// WEBSOCKET HANDLERS
// =============================================================================

void WebServer::handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                    AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            LOG_INFO("WebSocket client connected");
            if (g_webServer) {
                g_webServer->disconnectInProgress.store(false, std::memory_order_release);
                g_webServer->activeClients.fetch_add(1, std::memory_order_relaxed);
                g_webServer->lastBroadcast = millis();
                vTaskDelay(pdMS_TO_TICKS(50));
                if (g_webServer->ws && g_webServer->ws->count() > 0 &&
                    !g_webServer->disconnectInProgress.load(std::memory_order_acquire)) {
                    g_webServer->ws->textAll("{\"type\":\"connected\"}");
                }
            }
            break;
            
        case WS_EVT_DISCONNECT:
            LOG_INFO("WebSocket client disconnected");
            if (g_webServer) {
                g_webServer->disconnectInProgress.store(true, std::memory_order_release);
                g_webServer->lastDisconnectTime = millis();
                
                size_t previous = g_webServer->activeClients.load(std::memory_order_relaxed);
                while (previous != 0 && !g_webServer->activeClients.compare_exchange_weak(
                           previous, previous - 1, std::memory_order_relaxed)) {}
                g_webServer->aggStats = AggregatedStats{};
                g_webServer->lastBroadcast = millis();
                g_webServer->pendingPacketBroadcast.store(false, std::memory_order_relaxed);
                g_webServer->pendingClientCleanup.store(true, std::memory_order_relaxed);
            }
            break;
            
        case WS_EVT_DATA:
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
    if (!client || !data || len == 0 || len > 1024) return;

    if (len > 0 && data[0] == '{') {
        LOG_INFO("WebSocket JSON command (len=%u)", len);
    }
}

/**
 * Handle packet event from PacketProcessor
 */
void WebServer::handlePacketEvent(const PacketEvent& evt) {
    aggStats.packetCount++;
    aggStats.lastNodeId = evt.nodeId;
    aggStats.lastProtocol = evt.protocol;
    aggStats.lastRSSI = evt.rssi;
    aggStats.lastSNR = evt.snr;
    
    if (evt.message && strlen(evt.message) > 0) {
        strncpy(aggStats.lastMessage, evt.message, sizeof(aggStats.lastMessage) - 1);
        aggStats.lastMessage[sizeof(aggStats.lastMessage) - 1] = '\0';
        aggStats.hasMessage = true;
    } else {
        aggStats.lastMessage[0] = '\0';
        aggStats.hasMessage = false;
    }
    
    aggStats.timestamp = evt.timestamp;
    aggStats.lastLat = evt.lat;
    aggStats.lastLon = evt.lon;
    aggStats.lastAlt = evt.alt;
    aggStats.hasPosition = evt.hasPosition;
    pendingPacketBroadcast.store(true, std::memory_order_relaxed);
}

/**
 * Broadcast aggregated packet update
 */
void WebServer::broadcastAggregatedUpdate() {
    if (!ws || !server || aggStats.packetCount == 0) {
        pendingPacketBroadcast.store(false, std::memory_order_relaxed);
        return;
    }
    
    if (disconnectInProgress.load(std::memory_order_acquire)) {
        pendingPacketBroadcast.store(false, std::memory_order_relaxed);
        return;
    }
    
    uint32_t timeSinceDisconnect = millis() - lastDisconnectTime;
    if (timeSinceDisconnect < DISCONNECT_COOLDOWN_MS) return;
    
    size_t clientCount = ws->count();
    if (clientCount == 0) {
        pendingPacketBroadcast.store(false, std::memory_order_relaxed);
        activeClients.store(0, std::memory_order_relaxed);
        return;
    }

    JsonDocument doc;
    doc["type"] = "packet";
    doc["nodeId"] = FormatUtils::formatNodeIdJson(aggStats.lastNodeId);
    doc["rssi"] = aggStats.lastRSSI;
    doc["count"] = aggStats.packetCount;
    if (aggStats.hasPosition) {
        doc["lat"] = aggStats.lastLat;
        doc["lon"] = aggStats.lastLon;
    }

    String json;
    serializeJson(doc, json);

    if (json.length() > 0 && json.length() < 384 && 
        !disconnectInProgress.load(std::memory_order_acquire) &&
        ws->count() > 0) {
        // Clean up stale clients before sending to prevent queue buildup
        ws->cleanupClients();
        // Only send to clients that aren't backed up
        for (auto& client : ws->getClients()) {
            if (client->status() == WS_CONNECTED && !client->queueIsFull()) {
                client->text(json);
            }
        }
    }
    
    aggStats.packetCount = 0;
    lastBroadcast = millis();
    pendingPacketBroadcast.store(false, std::memory_order_relaxed);
}

void WebServer::periodicUpdate() {
    // Intentionally empty - AsyncWebServer handles cleanup internally
}

void WebServer::service() {
    if (dnsServer) {
        dnsServer->processNextRequest();
    }
    
    uint32_t now = millis();
    
    // Periodic WebSocket cleanup every 30 seconds to catch ghost connections
    static uint32_t lastCleanup = 0;
    if (ws && (now - lastCleanup >= 30000)) {
        ws->cleanupClients();
        lastCleanup = now;
    }
    
    if (now - lastBroadcast >= BROADCAST_INTERVAL_MS) {
        bool broadcastPending = pendingPacketBroadcast.load(std::memory_order_relaxed);
        if (broadcastPending && activeClients.load(std::memory_order_relaxed) > 0) {
            broadcastAggregatedUpdate();
        }
    }

    pendingClientCleanup.store(false, std::memory_order_relaxed);
}

void WebServer::broadcastDeviceUpdate(uint32_t nodeId) {
    if (!ws || !server || ws->count() == 0 || 
        disconnectInProgress.load(std::memory_order_acquire)) return;
    
    uint32_t timeSinceDisconnect = millis() - lastDisconnectTime;
    if (timeSinceDisconnect < DISCONNECT_COOLDOWN_MS) return;
    
    JsonDocument doc;
    doc["type"] = "deviceUpdate";
    doc["nodeId"] = FormatUtils::formatNodeIdJson(nodeId);
    doc["timestamp"] = millis();
    
    String json;
    serializeJson(doc, json);
    
    ws->cleanupClients();
    if (!disconnectInProgress.load(std::memory_order_acquire)) {
        for (auto& client : ws->getClients()) {
            if (client->status() == WS_CONNECTED && !client->queueIsFull()) {
                client->text(json);
            }
        }
    }
}

void WebServer::broadcastStatusUpdate() {
    if (!ws || !server || ws->count() == 0 ||
        disconnectInProgress.load(std::memory_order_acquire)) return;
    
    uint32_t timeSinceDisconnect = millis() - lastDisconnectTime;
    if (timeSinceDisconnect < DISCONNECT_COOLDOWN_MS) return;

    String statusJson = APIController::getStatus();
    
    JsonDocument doc;
    doc["type"] = "status";
    deserializeJson(doc["data"], statusJson);
    doc["timestamp"] = millis();
    
    String json;
    serializeJson(doc, json);

    ws->cleanupClients();
    if (!disconnectInProgress.load(std::memory_order_acquire)) {
        for (auto& client : ws->getClients()) {
            if (client->status() == WS_CONNECTED && !client->queueIsFull()) {
                client->text(json);
            }
        }
    }
}

size_t WebServer::getClientCount() const {
    return activeClients.load(std::memory_order_relaxed);
}

bool WebServer::cleanupWebSocketClients() {
    if (!ws) return true;
    ws->cleanupClients();
    return true;
}
