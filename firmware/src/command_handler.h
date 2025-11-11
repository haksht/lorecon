/**
 * Command Handler - Production-Grade Command Pattern Implementation
 * 
 * Eliminates deep nesting and if/else chains by using a clean command
 * dispatch table. Makes adding new commands trivial and improves testability.
 */

#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>

// Forward declaration
class LoRaReconTool;

/**
 * Command Handler - Processes user input commands
 * 
 * Uses function pointer table for O(1) command dispatch instead of
 * linear if/else chains. Improves maintainability and testability.
 */
class CommandHandler {
public:
    // Command function signature
    typedef void (*CommandFunc)(LoRaReconTool* tool);
    
    CommandHandler(LoRaReconTool* tool);
    
    // Main entry point - dispatches command
    bool handleCommand(char cmd);
    
    // Show available commands (for help menu)
    void showCommands();
    
private:
    LoRaReconTool* reconTool;
    
    // Command implementations (static for use in table)
    static void cmdShowMenu(LoRaReconTool* tool);
    static void cmdFrequencyTargeting(LoRaReconTool* tool);
    static void cmdDeviceTypeSummary(LoRaReconTool* tool);
    static void cmdActivityDetails(LoRaReconTool* tool);
    static void cmdPacketReplay(LoRaReconTool* tool);
    static void cmdResumeRecon(LoRaReconTool* tool);
    static void cmdRebootDevice(LoRaReconTool* tool);
    static void cmdShowSummary(LoRaReconTool* tool);
    static void cmdSecurityAssessment(LoRaReconTool* tool);
    static void cmdCapturePacket(LoRaReconTool* tool);
    static void cmdDeviceTarget(LoRaReconTool* tool, uint8_t deviceIndex);
    static void cmdGeoIntelligence(LoRaReconTool* tool);
    static void cmdExportKML(LoRaReconTool* tool);
    static void cmdExportGeoJSON(LoRaReconTool* tool);
    static void cmdDiagnosticReport(LoRaReconTool* tool);
    static void cmdToggleQuietMode(LoRaReconTool* tool);
    
#ifdef ENABLE_STRESS_TESTING
    static void cmdStressTesting(LoRaReconTool* tool);
#endif

#ifdef ENABLE_OFFENSIVE_TESTING
    static void cmdOffensiveTesting(LoRaReconTool* tool);
#endif
    
    // Command table entry
    struct CommandEntry {
        char key;
        CommandFunc handler;
        const char* description;
        bool requiresMenu;  // Only available from menu
    };
    
    // Command dispatch table
    static const CommandEntry commands[];
    static const uint8_t numCommands;
    
    // Helper to find command in table
    const CommandEntry* findCommand(char cmd);
};

#endif // COMMAND_HANDLER_H
