/**
 * IReconTool - Interface for LoRa Reconnaissance Tool
 * 
 * Breaks circular dependency between LoRaReconTool and CommandHandler.
 * Similar to Python's ABC (Abstract Base Class) - defines contract without implementation.
 * 
 * Why this helps:
 * - CommandHandler depends on interface (not concrete class)
 * - LoRaReconTool implements interface
 * - Enables testing with mock implementations
 * - Follows Dependency Inversion Principle
 */

#ifndef IRECON_TOOL_H
#define IRECON_TOOL_H

#include <cstdint>

// Forward declarations
class RadioController;
class OLEDDisplay;

/**
 * Interface for reconnaissance tool operations
 * 
 * Think of this as Python's ABC - defines the contract that
 * LoRaReconTool must implement for CommandHandler to use it.
 */
class IReconTool {
public:
    virtual ~IReconTool() = default;
    
    // Component access
    virtual RadioController* getRadioController() = 0;
    virtual OLEDDisplay* getDisplay() = 0;
    
    // Device targeting operations
    virtual void startTargetedCapture(uint8_t deviceIndex) = 0;
    virtual void startFrequencyTargeting(uint8_t configIndex) = 0;
    
    // Packet replay operations
    virtual void showReplayMenu() = 0;
    virtual void replayPacket(uint8_t slotIndex) = 0;
};

#endif // IRECON_TOOL_H
