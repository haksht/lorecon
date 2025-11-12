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
    
    // Command dispatch table (constexpr for compile-time evaluation)
    static constexpr CommandEntry commands[] = {
        {'m', cmdShowMenu,            "Show menu and results",         false},
        {'M', cmdShowMenu,            "Show menu and results",         false},
        {'f', cmdFrequencyTargeting,  "Frequency targeting mode",      false},
        {'F', cmdFrequencyTargeting,  "Frequency targeting mode",      false},
        {'d', cmdDeviceTypeSummary,   "Device type analysis",          false},
        {'D', cmdDeviceTypeSummary,   "Device type analysis",          false},
        {'a', cmdActivityDetails,     "RF activity details",           false},
        {'p', cmdPacketReplay,        "Packet replay menu",            false},
        {'P', cmdPacketReplay,        "Packet replay menu",            false},
        {'r', cmdResumeRecon,         "Resume reconnaissance",         false},
        {'R', cmdResumeRecon,         "Resume reconnaissance",         false},
        {'b', cmdRebootDevice,        "Reboot device",                 false},
        {'B', cmdRebootDevice,        "Reboot device",                 false},
        {'s', cmdShowSummary,         "Show summary",                  false},
        {'S', cmdShowSummary,         "Show summary",                  false},
        {'v', cmdSecurityAssessment,  "Security assessment",           false},
        {'V', cmdSecurityAssessment,  "Security assessment",           false},
        {'c', cmdCapturePacket,       "Capture packet for replay",     false},
        {'C', cmdCapturePacket,       "Capture packet for replay",     false},
        {'g', cmdGeoIntelligence,     "Geographic intelligence",       false},
        {'G', cmdGeoIntelligence,     "Geographic intelligence",       false},
        {'k', cmdExportKML,           "Export KML (Google Earth)",     false},
        {'K', cmdExportKML,           "Export KML (Google Earth)",     false},
        {'j', cmdExportGeoJSON,       "Export GeoJSON (web maps)",     false},
        {'J', cmdExportGeoJSON,       "Export GeoJSON (web maps)",     false},
        {'q', cmdToggleQuietMode,     "Toggle quiet/verbose mode",     false},
        {'Q', cmdToggleQuietMode,     "Toggle quiet/verbose mode",     false},
        {'x', cmdDiagnosticReport,    "Text packet diagnostic report", false},
        {'X', cmdDiagnosticReport,    "Text packet diagnostic report", false},
    };
    
    static constexpr uint8_t numCommands = sizeof(commands) / sizeof(CommandEntry);
    
    // Helper to find command in table
    const CommandEntry* findCommand(char cmd);
};

#endif // COMMAND_HANDLER_H
