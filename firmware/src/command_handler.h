/**
 * Command Handler - Production-Grade Command Pattern Implementation
 * 
 * Eliminates deep nesting and if/else chains by using a clean command
 * dispatch table. Makes adding new commands trivial and improves testability.
 */

#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>
#include "irecon_tool.h"  // Interface, not concrete class

/**
 * Command Handler - Processes user input commands
 * 
 * Uses function pointer table for O(1) command dispatch instead of
 * linear if/else chains. Improves maintainability and testability.
 */
class CommandHandler {
public:
    // Command function signature - takes interface, not concrete class
    typedef void (*CommandFunc)(IReconTool* tool);
    
    CommandHandler(IReconTool* tool);
    
    // Main entry point - dispatches command
    bool handleCommand(char cmd);
    
    // Show available commands (for help menu)
    void showCommands();
    
private:
    IReconTool* reconTool;
    
    // Command implementations (static for use in table)
    static void cmdShowMenu(IReconTool* tool);
    static void cmdFrequencyTargeting(IReconTool* tool);
    static void cmdDeviceTypeSummary(IReconTool* tool);
    static void cmdActivityDetails(IReconTool* tool);
    static void cmdPacketReplay(IReconTool* tool);
    static void cmdResumeRecon(IReconTool* tool);
    static void cmdRebootDevice(IReconTool* tool);
    static void cmdShowSummary(IReconTool* tool);
    static void cmdSecurityAssessment(IReconTool* tool);
    static void cmdCapturePacket(IReconTool* tool);
    static void cmdDeviceTarget(IReconTool* tool, uint8_t deviceIndex);
    static void cmdGeoIntelligence(IReconTool* tool);
    static void cmdExportKML(IReconTool* tool);
    static void cmdExportGeoJSON(IReconTool* tool);
    static void cmdDiagnosticReport(IReconTool* tool);
    static void cmdToggleQuietMode(IReconTool* tool);
    
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
