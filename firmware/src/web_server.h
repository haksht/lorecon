/**
 * Web Server - HTTP Server and WebSocket Manager
 * 
 * Provides web interface for phone/browser access:
 * - REST API endpoints for device control and data access
 * - WebSocket for real-time packet streaming
 * - Static file serving for Progressive Web App
 * - Captive portal for automatic setup page on first connect
 * - CORS support for development
 * 
 * Design: Asynchronous, non-blocking server for reliable operation
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <atomic>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "irecon_tool.h"
#include "data_structures.h"
#include "packet_processor.h"  // For PacketEvent

// Aggregated packet stats for efficient WebSocket updates
struct AggregatedStats {
    uint32_t packetCount;
    uint32_t lastNodeId;
    const char* lastProtocol;
    float lastRSSI;
    float lastSNR;
    char lastMessage[256];  // Copy message to prevent dangling pointer
    bool hasMessage;
    uint32_t timestamp;
    
    AggregatedStats() : packetCount(0), lastNodeId(0), lastProtocol("None"),
                       lastRSSI(0), lastSNR(0), hasMessage(false), timestamp(0) {
        lastMessage[0] = '\0';
    }
};

/**
 * Web Server - Manages HTTP and WebSocket connections
 * 
 * Provides REST API and real-time updates for web/mobile clients.
 * All operations are asynchronous to avoid blocking LoRa reception.
 */
class WebServer {
public:
    WebServer();
    
    // Lifecycle
    bool begin(IReconTool* tool, uint16_t port = 80);
    void stop();
    
    // Packet event callback (called by PacketProcessor)
    void handlePacketEvent(const PacketEvent& evt);
    
    // Real-time updates via WebSocket (aggregated)
    void broadcastAggregatedUpdate();
    void broadcastDeviceUpdate(uint32_t nodeId);
    void broadcastStatusUpdate();
    void periodicUpdate();
    void service();  // Call in loop() - handles DNS for captive portal
    
    // Client management
    size_t getClientCount() const;
    
private:
    AsyncWebServer* server;
    AsyncWebSocket* ws;
    DNSServer* dnsServer;  // For captive portal - redirects all DNS to our IP
    IReconTool* reconTool;
    std::atomic<size_t> activeClients;
    
    // Aggregation for efficient WebSocket updates
    AggregatedStats aggStats;
    uint32_t lastBroadcast;
    uint32_t lastDisconnectTime;  // Track when disconnect occurred for cooldown
    std::atomic<bool> pendingPacketBroadcast;
    std::atomic<bool> pendingClientCleanup;
    std::atomic<bool> disconnectInProgress;  // Block data processing during disconnect
    static constexpr uint32_t BROADCAST_INTERVAL_MS = 500;  // 2 Hz max
    static constexpr uint32_t DISCONNECT_COOLDOWN_MS = 100;  // Wait after disconnect
    
    // Request handlers
    void setupRoutes();
    void setupWebSocket();
    
    // REST API handlers
    static void handleGetDevices(AsyncWebServerRequest* request);
    static void handleGetDevice(AsyncWebServerRequest* request);
    static void handleStartCapture(AsyncWebServerRequest* request);
    static void handleStopCapture(AsyncWebServerRequest* request);
    static void handleGetPositions(AsyncWebServerRequest* request);
    static void handleExportGeoJSON(AsyncWebServerRequest* request);
    static void handleExportKML(AsyncWebServerRequest* request);
    static void handleGetStatus(AsyncWebServerRequest* request);
    static void handleGetDashboard(AsyncWebServerRequest* request);
    static void handleGetStatistics(AsyncWebServerRequest* request);
    static void handleGetActivity(AsyncWebServerRequest* request);
    static void handleGetConfig(AsyncWebServerRequest* request);
    static void handleGetSystemConfig(AsyncWebServerRequest* request);
    static void handleStartScan(AsyncWebServerRequest* request);
    static void handleStopScan(AsyncWebServerRequest* request);
    static void handleGetReconSummary(AsyncWebServerRequest* request);
    static void handleGetDeviceTypeSummary(AsyncWebServerRequest* request);
    static void handleGetSecurityAssessment(AsyncWebServerRequest* request);
    static void handleGetAnomalies(AsyncWebServerRequest* request);
    static void handleAcknowledgeAnomaly(AsyncWebServerRequest* request);
    static void handleGetTemporalData(AsyncWebServerRequest* request);
    static void handleGetReplaySlots(AsyncWebServerRequest* request);
    static void handleClearReplaySlots(AsyncWebServerRequest* request);
    static void handleClearDevices(AsyncWebServerRequest* request);
    static void handleReplayPacket(AsyncWebServerRequest* request);
    static void handleStartFrequencyTargeting(AsyncWebServerRequest* request);
    static void handleGetDiagnostics(AsyncWebServerRequest* request);
    static void handleSetVerboseMode(AsyncWebServerRequest* request);
    static void handleExportPCAP(AsyncWebServerRequest* request);
    static void handleCommand(AsyncWebServerRequest* request);
    
    // WiFi Setup handlers
    static void handleGetWiFiStatus(AsyncWebServerRequest* request);
    static void handleSetWiFiCredentials(AsyncWebServerRequest* request);
    static void handleClearWiFiCredentials(AsyncWebServerRequest* request);
    
    // WebSocket handlers
    static void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                    AwsEventType type, void* arg, uint8_t* data, size_t len);
    static void handleWebSocketMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len);
    
    // Static file serving (for PWA)
    void serveStaticFiles();
    
    // CORS headers for development
    void addCORSHeaders(AsyncWebServerResponse* response);

    bool cleanupWebSocketClients();
};

// Global instance for static handlers
extern WebServer* g_webServer;

#endif // WEB_SERVER_H
