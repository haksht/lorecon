/**
 * Web Server - HTTP Server and WebSocket Manager
 * 
 * Provides web interface for phone/browser access:
 * - REST API endpoints for device control and data access
 * - WebSocket for real-time packet streaming
 * - Static file serving for Progressive Web App
 * - CORS support for development
 * 
 * Design: Asynchronous, non-blocking server for reliable operation
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "irecon_tool.h"
#include "data_structures.h"

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
    
    // Real-time updates via WebSocket
    void broadcastPacket(uint32_t nodeId, const char* protocol, float rssi, float snr, size_t length, const char* message = nullptr);
    void broadcastDeviceUpdate(uint32_t nodeId);
    void broadcastStatusUpdate();
    
    // Client management
    size_t getClientCount() const;
    
private:
    AsyncWebServer* server;
    AsyncWebSocket* ws;
    IReconTool* reconTool;
    
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
    static void handleGetStatistics(AsyncWebServerRequest* request);
    static void handleGetActivity(AsyncWebServerRequest* request);
    static void handleGetConfig(AsyncWebServerRequest* request);
    static void handleStartScan(AsyncWebServerRequest* request);
    static void handleStopScan(AsyncWebServerRequest* request);
    
    // WebSocket handlers
    static void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                    AwsEventType type, void* arg, uint8_t* data, size_t len);
    static void handleWebSocketMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len);
    
    // Static file serving (for PWA)
    void serveStaticFiles();
    
    // CORS headers for development
    void addCORSHeaders(AsyncWebServerResponse* response);
};

// Global instance for static handlers
extern WebServer* g_webServer;

#endif // WEB_SERVER_H
