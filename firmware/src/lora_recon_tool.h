#ifndef LORA_RECON_TOOL_H
#define LORA_RECON_TOOL_H

#include <Arduino.h>
#include "data_structures.h"

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

// Hardware configuration
#define USER_BUTTON 0   // PRG button (active low)

// Scanning configuration
#define SCAN_DWELL_TIME     12000  // 12 seconds per frequency
#define SERIAL_TIMEOUT_MS   30000  // 30 second timeout for user input

/**
 * LoRa Reconnaissance Tool - Main application orchestrator
 * 
 * Coordinates RadioController, PacketProcessor, and UI components.
 * Simplified from 1000+ lines to focus on orchestration, not implementation.
 */
class LoRaReconTool {
public:
    LoRaReconTool();
    
    // Lifecycle methods
    bool initialize();
    void update();
    
    // User interaction
    void handleUserInput(char cmd);
    
    // Device targeting (public for UI module access)
    void startTargetedCapture(uint8_t deviceIndex);
    void startFrequencyTargeting(uint8_t configIndex);
    
    // Packet replay (public for command handler)
    void showReplayMenu();
    void replayPacket(uint8_t slotIndex);
    
    // Access to components for command handler
    RadioController* getRadioController() { return radioController; }
    OLEDDisplay* getDisplay() { return oledDisplay; }
    
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
