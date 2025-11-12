#ifndef LORA_RECON_TOOL_H
#define LORA_RECON_TOOL_H

#include <Arduino.h>
#include "data_structures.h"
#include "irecon_tool.h"  // Interface to implement
#include "config.h"

// Forward declarations
class CommandHandler;
class OLEDDisplay;
class RadioController;
class PacketProcessor;

#include "recon_state.h"
#include "radio_controller.h"
#include "packet_processor.h"
#include "command_handler.h"
#include "oled_display.h"

// Hardware configuration moved to config.h

// Scanning configuration moved to config.h

/**
 * LoRa Reconnaissance Tool - Main application orchestrator
 * 
 * Coordinates RadioController, PacketProcessor, and UI components.
 * Simplified from 1000+ lines to focus on orchestration, not implementation.
 * 
 * Implements IReconTool interface to break circular dependency with CommandHandler.
 */
class LoRaReconTool : public IReconTool {
public:
    LoRaReconTool();
    
    // Lifecycle methods
    bool initialize();
    void update();
    
    // IReconTool interface implementation
    RadioController* getRadioController() override { return radioController; }
    OLEDDisplay* getDisplay() override { return oledDisplay; }
    void startTargetedCapture(uint8_t deviceIndex) override;
    void startFrequencyTargeting(uint8_t configIndex) override;
    void showReplayMenu() override;
    void replayPacket(uint8_t slotIndex) override;
    
private:
    // Component instances
    RadioController* radioController;
    PacketProcessor* packetProcessor;
    CommandHandler* commandHandler;
    OLEDDisplay* oledDisplay;
    
    // Button state tracking
    bool buttonPressed;
    uint32_t buttonPressStart;
    bool shutdownInitiated;
    
    // Button handler
    void handleButtonPress(uint32_t now);
    
    // Mode handlers
    void handleReconnaissanceMode(uint32_t now);
    void handleTargetedCaptureMode(uint32_t now);
    
    // Packet reception
    void handlePacketReception();
};

// Global interrupt handler needs access to tool instance
extern LoRaReconTool* g_reconTool;

#endif // LORA_RECON_TOOL_H
