/**
 * @file recon_service.cpp
 * @brief Service layer for recon operations and API actions
 * 
 * Handles state-changing operations only. JSON building delegated to JsonBuilders.
 * Reduced from 871 lines to ~180 lines by extracting JSON builders.
 */

#include "recon_service.h"
#include "recon_state.h"
#include "radio_controller.h"
#include "mode_manager.h"
#include "text_packet_diagnostic.h"
#include "config.h"
#include "logger.h"
#include <esp_task_wdt.h>
#include "utils/format_utils.h"

IReconTool* ReconService::reconTool = nullptr;

void ReconService::initialize(IReconTool* tool) {
    reconTool = tool;
}

bool ReconService::isInitialized() {
    return reconTool != nullptr;
}

uint8_t ReconService::findDeviceIndex(uint32_t nodeId) {
    for (uint8_t i = 0; i < reconState.getNumTargetableDevices(); i++) {
        TargetableDevice dev = reconState.getTargetableDevice(i);  // Copy for thread safety
        if (dev.nodeId == nodeId) {
            return i;
        }
    }
    return UINT8_MAX;
}

bool ReconService::startTargetedCaptureByNodeId(uint32_t nodeId, String& outMessage) {
    if (!isInitialized()) {
        outMessage = "Recon tool not initialized";
        return false;
    }

    uint8_t index = findDeviceIndex(nodeId);
    if (index == UINT8_MAX) {
        outMessage = "Device not found";
        return false;
    }

    return startTargetedCaptureByIndex(index, outMessage);
}

bool ReconService::startTargetedCaptureByIndex(uint8_t deviceIndex, String& outMessage) {
    if (!isInitialized()) {
        outMessage = "Recon tool not initialized";
        return false;
    }

    if (deviceIndex >= reconState.getNumTargetableDevices()) {
        outMessage = "Invalid device index";
        return false;
    }

    reconTool->startTargetedCapture(deviceIndex);
    TargetableDevice device = reconState.getTargetableDevice(deviceIndex);  // Copy for thread safety
    
    outMessage = "Targeted capture started on device " + FormatUtils::formatNodeIdPadded(device.nodeId);
    return true;
}

bool ReconService::stopCapture(String& outMessage) {
    LOG_INFO("ReconService: Stopping capture, switching to reconnaissance mode");
    
    // Clear persisted targeting mode from NVS
    ModeManager modeManager;
    modeManager.clearPersistedMode("API:stopCapture");
    
    reconState.scanState.mode = MODE_RECONNAISSANCE;
    outMessage = "Capture stopped, resumed reconnaissance";
    return true;
}

bool ReconService::clearReplaySlots(String& outMessage) {
    if (reconState.getNumCapturedPackets() == 0) {
        outMessage = "No replay slots to clear";
        return false;
    }

    reconState.clearReplaySlots();
    outMessage = "Replay slots cleared";
    return true;
}

bool ReconService::clearDevices(String& outMessage) {
    uint8_t deviceCount = reconState.getNumTargetableDevices();
    
    reconState.clearTargetableDevices();
    
    char msg[64];
    snprintf(msg, sizeof(msg), "Cleared %d devices", deviceCount);
    outMessage = msg;
    return true;
}

bool ReconService::replayPacket(uint8_t slotIndex, uint8_t repeatCount, uint16_t delayMs, String& outMessage) {
    if (!isInitialized()) {
        outMessage = "Recon tool not initialized";
        return false;
    }

    // Validate slot index
    if (slotIndex >= reconState.getNumCapturedPackets()) {
        outMessage = "Invalid packet slot";
        return false;
    }

    const CapturedPacket& pkt = reconState.getReplayPacket(slotIndex);
    if (!pkt.valid) {
        outMessage = "Packet slot is empty";
        return false;
    }

    // Validate parameters
    if (repeatCount == 0 || repeatCount > 100) {
        outMessage = "Repeat count must be 1-100";
        return false;
    }

    if (delayMs < 100 || delayMs > 10000) {
        outMessage = "Delay must be 100-10000 ms";
        return false;
    }

    // Get radio controller
    RadioController* radioController = reconTool->getRadioController();
    if (!radioController) {
        outMessage = "Radio controller not available";
        return false;
    }

    // Apply the correct radio configuration for replay
    const ScanConfig& replayCfg = reconState.getScanConfig(pkt.configIndex);
    if (!radioController->applyConfig(replayCfg)) {
        outMessage = "Failed to apply radio configuration";
        return false;
    }

    // Create mutable copy for transmission
    uint8_t txBuffer[256];
    memcpy(txBuffer, pkt.data, pkt.length);

    // Set transmission power
    radioController->getRadio().setOutputPower(10);

    // Transmit the packet(s)
    int successCount = 0;
    int failCount = 0;

    for (int i = 0; i < repeatCount; i++) {
        esp_task_wdt_reset();
        int state = radioController->getRadio().transmit(txBuffer, pkt.length);
        esp_task_wdt_reset();

        if (state == RADIOLIB_ERR_NONE) {
            successCount++;
        } else {
            failCount++;
        }

        // Delay between transmissions (except after last one)
        if (i < repeatCount - 1) {
            delay(delayMs);
        }
    }

    // Build result message
    char msgBuffer[128];
    snprintf(msgBuffer, sizeof(msgBuffer), "Replay complete: %d/%d successful", 
             successCount, repeatCount);
    outMessage = String(msgBuffer);
    
    // Restore scanning configuration and restart receive mode
    if (reconTool) {
        const ScanConfig& scanCfg = reconState.getScanConfig(reconState.scanState.currentConfig);
        if (radioController->applyConfig(scanCfg)) {
            radioController->startReceive();
            Serial.println("[REPLAY] Radio configuration restored, receive mode resumed");
        } else {
            Serial.println("[REPLAY] Warning: Failed to restore radio configuration");
        }
    }
    
    return successCount > 0;
}

bool ReconService::startFrequencyTargeting(uint8_t configIndex, String& outMessage) {
    if (!isInitialized()) {
        outMessage = "Recon tool not initialized";
        return false;
    }

    if (!reconState.isValidConfigIndex(configIndex)) {
        outMessage = "Invalid frequency configuration";
        return false;
    }

    reconTool->startFrequencyTargeting(configIndex);
    const ScanConfig& cfg = reconState.getScanConfig(configIndex);
    
    char msgBuffer[128];
    snprintf(msgBuffer, sizeof(msgBuffer), "Frequency targeting started on %s (%.3f MHz)",
             cfg.protocol, cfg.frequency);
    outMessage = String(msgBuffer);
    return true;
}

bool ReconService::setVerboseMode(bool enableVerbose, String& outMessage) {
    bool current = TextPacketDiagnostic::isVerbose();
    if (current == enableVerbose) {
        outMessage = enableVerbose ? "Verbose mode already enabled" : "Quiet mode already enabled";
        return false;
    }

    TextPacketDiagnostic::enableVerbose(enableVerbose);
    outMessage = enableVerbose ? "Verbose diagnostics enabled" : "Quiet mode enabled";
    return true;
}
