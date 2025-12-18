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
 * 
 * Note: REST API handlers are in api_handlers.cpp/h
 *       WiFi handlers are in wifi_handlers.cpp/h
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <atomic>
#include "irecon_tool.h"
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
 * 
 * Handler modules:
 * - api_handlers.cpp: REST API endpoints
 * - wifi_handlers.cpp: WiFi configuration endpoints
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
    
    // Server setup (routes use handlers from api_handlers.cpp and wifi_handlers.cpp)
    void setupRoutes();
    void setupWebSocket();
    void serveStaticFiles();
    
    // WebSocket handlers (kept here as they manage internal state)
    static void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                    AwsEventType type, void* arg, uint8_t* data, size_t len);
    static void handleWebSocketMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len);

    bool cleanupWebSocketClients();
};

// Global instance for static handlers
extern WebServer* g_webServer;

#endif // WEB_SERVER_H
