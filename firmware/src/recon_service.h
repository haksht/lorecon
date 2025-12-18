/**
 * @file recon_service.h
 * @brief Service layer for recon operations and API actions
 * 
 * Handles state-changing operations (start capture, clear, replay).
 * JSON building delegated to JsonBuilders namespace.
 */

#ifndef RECON_SERVICE_H
#define RECON_SERVICE_H

#include <Arduino.h>
#include "irecon_tool.h"

class ReconService {
public:
    static void initialize(IReconTool* tool);
    static bool isInitialized();
    static IReconTool* getReconTool() { return reconTool; }

    // Capture control
    static bool startTargetedCaptureByNodeId(uint32_t nodeId, String& outMessage);
    static bool startTargetedCaptureByIndex(uint8_t deviceIndex, String& outMessage);
    static bool stopCapture(String& outMessage);

    // Data management
    static bool clearReplaySlots(String& outMessage);
    static bool clearDevices(String& outMessage);
    
    // Replay operations
    static bool replayPacket(uint8_t slotIndex, uint8_t repeatCount, uint16_t delayMs, String& outMessage);
    static bool startFrequencyTargeting(uint8_t configIndex, String& outMessage);
    
    // Diagnostics control
    static bool setVerboseMode(bool enableVerbose, String& outMessage);

private:
    static IReconTool* reconTool;
    static uint8_t findDeviceIndex(uint32_t nodeId);
};

#endif // RECON_SERVICE_H
