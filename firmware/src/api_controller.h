/**
 * API Controller - REST API Request Handlers
 * 
 * Handles HTTP request processing for the web server:
 * - Device management endpoints
 * - Packet data access
 * - Configuration management
 * - Export functionality
 * 
 * Design: Stateless request handlers that interface with LoRaReconTool
 */

#ifndef API_CONTROLLER_H
#define API_CONTROLLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "irecon_tool.h"
#include "recon_state.h"

/**
 * API Controller - Handles REST API requests
 * 
 * All methods are static for use as HTTP handler callbacks.
 * Uses IReconTool interface to access reconnaissance functionality.
 */
class APIController {
public:
    // Set tool reference (call once at startup)
    static void setReconTool(IReconTool* tool);
    
    // Device Management
    static String getDevices();
    static String getDevice(uint32_t nodeId);
    static String startTargetedCapture(uint32_t nodeId);
    static String stopCapture();
    
    // Packet Data
    static String getPackets(int limit = 100, int offset = 0);
    static String getPacketsByDevice(uint32_t nodeId, int limit = 50);
    static String getRecentActivity(int seconds = 60);
    
    // Geographic Data
    static String getPositions();
    static String getPositionsByDevice(uint32_t nodeId);
    static String exportGeoJSON();
    static String exportKML();
    
    // Configuration & Status
    static String getStatus();
    
    // Temporal & Anomaly Analysis
    static String getAnomalies(bool unacknowledgedOnly = false);
    static String acknowledgeAnomaly(uint8_t index);
    static String getTemporalData();
    static String getDashboard();  // Combined endpoint for initial load
    static String getConfig();
    static String setConfig(const String& jsonConfig);
    static String startScan();
    static String stopScan();
    static String getReconSummary();
    static String getDeviceTypeSummary();
    static String getSecurityAssessment();
    static String getReplaySlots();
    static String clearReplaySlots();
    static String replayPacket(uint8_t slotIndex, uint8_t repeatCount, uint16_t delayMs);
    static String startFrequencyTargeting(uint8_t configIndex);
    static String getDiagnostics();
    static String setVerboseMode(bool enableVerbose);
    
    // System configuration (read-only)
    static String getSystemConfig();
    
    // Statistics
    static String getStatistics();
    static String getActivityAnalysis();
    static String getRFActivity();
    
private:
    // Helper methods
    static String createErrorResponse(const String& error);
    static String createSuccessResponse(const String& message);
};

#endif // API_CONTROLLER_H
