/**
 * API Handlers - REST API Request Handlers for Web Server
 * 
 * Extracted from web_server.cpp to improve modularity.
 * Contains all HTTP endpoint handlers as free functions.
 */

#ifndef API_HANDLERS_H
#define API_HANDLERS_H

#include <ESPAsyncWebServer.h>

namespace APIHandlers {

// Device Management
void handleGetDevices(AsyncWebServerRequest* request);
void handleGetDevice(AsyncWebServerRequest* request);
void handleStartCapture(AsyncWebServerRequest* request);
void handleStopCapture(AsyncWebServerRequest* request);

// Geographic Data
void handleGetPositions(AsyncWebServerRequest* request);
void handleExportGeoJSON(AsyncWebServerRequest* request);
void handleExportKML(AsyncWebServerRequest* request);
void handleExportPCAP(AsyncWebServerRequest* request);

// Status & Configuration
void handleGetStatus(AsyncWebServerRequest* request);
void handleGetDashboard(AsyncWebServerRequest* request);
void handleGetStatistics(AsyncWebServerRequest* request);
void handleGetActivity(AsyncWebServerRequest* request);
void handleGetConfig(AsyncWebServerRequest* request);
void handleGetSystemConfig(AsyncWebServerRequest* request);

// Scan Control
void handleStartScan(AsyncWebServerRequest* request);
void handleStopScan(AsyncWebServerRequest* request);

// Reconnaissance Data
void handleGetReconSummary(AsyncWebServerRequest* request);
void handleGetDeviceTypeSummary(AsyncWebServerRequest* request);
void handleGetSecurityAssessment(AsyncWebServerRequest* request);

// Anomaly & Temporal Analysis
void handleGetAnomalies(AsyncWebServerRequest* request);
void handleAcknowledgeAnomaly(AsyncWebServerRequest* request);
void handleGetTemporalData(AsyncWebServerRequest* request);

// PSK Statistics
void handleGetPSKStats(AsyncWebServerRequest* request);

// Packet Replay
void handleGetReplaySlots(AsyncWebServerRequest* request);
void handleClearReplaySlots(AsyncWebServerRequest* request);
void handleReplayPacket(AsyncWebServerRequest* request);

// Device Management Actions
void handleClearDevices(AsyncWebServerRequest* request);
void handleStartFrequencyTargeting(AsyncWebServerRequest* request);

// Diagnostics
void handleGetDiagnostics(AsyncWebServerRequest* request);
void handleSetVerboseMode(AsyncWebServerRequest* request);

// Commands
void handleCommand(AsyncWebServerRequest* request);

// Consolidated Export
void handleExportReport(AsyncWebServerRequest* request);

}  // namespace APIHandlers

#endif // API_HANDLERS_H
