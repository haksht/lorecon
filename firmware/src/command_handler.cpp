/**
 * Command Handler Implementation
 * 
 * Clean command dispatch using function pointer table.
 * Eliminates 200+ lines of nested if/else chains.
 * 
 * Uses:
 * - O(1) command lookup via dispatch table
 * - Static handler functions
 * - Grouped command display
 */

#include <esp_task_wdt.h>
#include "command_handler.h"
#include "irecon_tool.h"  // Interface only
#include "lora_recon_tool.h"  // For g_reconTool global
#include "radio_controller.h"  // Need full definition for method calls
#include "user_interface.h"
#include "recon_state.h"
#include "protocol_analyzer.h"
#include "geo_intelligence.h"
#include "ui_components.h"
#include "text_packet_diagnostic.h"  // For diagnostic reporting
#include "psk_decryption_simple.h"   // For accessing decrypted text
#include "psk_tests.h"

// Explicit definition required for constexpr static members (C++ linkage rule)
constexpr CommandHandler::CommandEntry CommandHandler::commands[];
constexpr uint8_t CommandHandler::numCommands;

CommandHandler::CommandHandler(IReconTool* tool) 
    : reconTool(tool) 
{
}

bool CommandHandler::handleCommand(char cmd) {
    // First check if it's a special command in the dispatch table
    const CommandEntry* entry = findCommand(cmd);
    if (entry) {
        entry->handler(reconTool);
        return true;
    }
    
    // Then handle device selection (1-9)
    if (cmd >= '1' && cmd <= '9') {
        uint8_t deviceIndex = cmd - '1';
        if (deviceIndex < reconState.numTargetableDevices) {
            cmdDeviceTarget(reconTool, deviceIndex);
            return true;
        } else {
            Serial.println("[ERROR] Invalid device number. Press 'm' for menu.");
            return false;
        }
    }
    
    // Unknown command
    Serial.printf("[ERROR] Unknown command '%c'. Press 'm' for menu.\\n", cmd);
    return false;
}

const CommandHandler::CommandEntry* CommandHandler::findCommand(char cmd) {
    for (uint8_t i = 0; i < numCommands; i++) {
        if (commands[i].key == cmd) {
            return &commands[i];
        }
    }
    return nullptr;
}

void CommandHandler::showCommands() {
    Serial.println("\\n=== AVAILABLE COMMANDS ===");
    
    // Group by functionality
    Serial.println("\\n📡 TARGETING:");
    Serial.println("  1-9 : Target device by number");
    Serial.println("  f   : Frequency targeting (skip device)");
    
    Serial.println("\\n📊 ANALYSIS:");
    Serial.println("  m   : Show menu with discovered devices");
    Serial.println("  s   : Show summary again");
    Serial.println("  a   : Detailed RF activity analysis");
    Serial.println("  d   : Device type breakdown");
    Serial.println("  v   : Security vulnerability assessment");
    
    Serial.println("\n🔧 OPERATIONS:");
    Serial.println("  c   : Capture packet for replay");
    Serial.println("  p   : Packet replay menu");
    Serial.println("  r   : Resume reconnaissance (keep devices)");
    Serial.println("  b   : Reboot device (clears all data)");
    
    Serial.println();
}

// ============================================================================
// Command Implementations
// ============================================================================

void CommandHandler::cmdShowMenu(IReconTool* tool) {
    reconState.scanState.mode = MODE_INTERACTIVE_MENU;
    // Track when menu mode was entered for auto-timeout
    if (g_reconTool) {
        g_reconTool->setMenuModeEntered();
    }
    showReconResults();
}

void CommandHandler::cmdFrequencyTargeting(IReconTool* tool) {
    showFrequencyTargetingMenu();
    handleFrequencyTargetingInput();
}

void CommandHandler::cmdDeviceTypeSummary(IReconTool* tool) {
    showDeviceTypeSummary();
}

void CommandHandler::cmdActivityDetails(IReconTool* tool) {
    showActivityDetails();
}

void CommandHandler::cmdPacketReplay(IReconTool* tool) {
    tool->showReplayMenu();
}

void CommandHandler::cmdResumeRecon(IReconTool* tool) {
    // Resume reconnaissance WITHOUT clearing discovered devices/data
    Serial.println("\n=== RESUMING RECONNAISSANCE ===");
    Serial.println("Restarting scan cycle while keeping discovered devices.");
    Serial.printf("Current devices: %d, Nodes: %d, Replay slots: %d\n", 
                  reconState.numTargetableDevices, 
                  reconState.nodeCount,
                  reconState.numCapturedPackets);
    
    // Reset only the scan state to restart the cycle
    reconState.scanState.mode = MODE_RECONNAISSANCE;
    reconState.scanState.currentConfig = 0;
    reconState.scanState.lastScanSwitch = millis();
    
    // Clear menu timeout since we're leaving menu mode
    if (g_reconTool) {
        g_reconTool->clearMenuTimeout();
    }
    reconState.scanState.packetPending = false;
    reconState.scanState.waitingForUserInput = false;
    
    const ScanConfig& cfg = reconState.getScanConfig(reconState.scanState.currentConfig);
    tool->getRadioController()->applyConfig(cfg);
    tool->getRadioController()->startReceive();
}

void CommandHandler::cmdRebootDevice(IReconTool* tool) {
    Serial.println("\n=== REBOOTING DEVICE ===");
    Serial.println("⚠️  This will clear ALL discovered devices, nodes, and replay slots!");
    Serial.println("⚠️  Diagnostic counters will also be reset.");
    Serial.print("\nAre you sure? Type 'YES' to confirm or anything else to cancel: ");
    
    // Wait for user confirmation with watchdog feeding
    String confirmation = "";
    uint32_t startTime = millis();
    while (millis() - startTime < 10000) {  // 10 second timeout
        esp_task_wdt_reset();  // Feed watchdog while waiting
        if (Serial.available()) {
            confirmation = Serial.readStringUntil('\n');
            confirmation.trim();
            break;
        }
        delay(100);
    }
    
    if (confirmation == "YES") {
        Serial.println("\n✅ Confirmed. Clearing all data and restarting...");
        
        reconState.reset();
        TextPacketDiagnostic::reset();
        
        Serial.println("Cleared activity history and device list.");
        Serial.println("Cleared diagnostic counters.");
        Serial.println("Cleared replay slots.");
        
        const ScanConfig& cfg = reconState.getScanConfig(reconState.scanState.currentConfig);
        tool->getRadioController()->applyConfig(cfg);
        tool->getRadioController()->startReceive();
    } else {
        Serial.println("\n❌ Reboot cancelled. Returning to menu.");
    }
}

void CommandHandler::cmdShowSummary(IReconTool* tool) {
    showReconResults();
}

void CommandHandler::cmdSecurityAssessment(IReconTool* tool) {
    showSecurityAssessment();
    reconState.scanState.mode = MODE_INTERACTIVE_MENU;
    if (g_reconTool) {
        g_reconTool->setMenuModeEntered();
    }
    showReconResults();
}

void CommandHandler::cmdCapturePacket(IReconTool* tool) {
    if (reconState.scanState.lastPacketLength > 0 && 
        reconState.scanState.mode == MODE_TARGETED_CAPTURE) {
        
        const uint8_t* data = (const uint8_t*)reconState.scanState.lastPacket;
        size_t length = reconState.scanState.lastPacketLength;
        float rssi = tool->getRadioController()->getRSSI();
        
        // Analyze packet
        ProtocolAnalyzer analyzer;
        PacketInfo info = analyzer.analyze(data, length, rssi);
        
        // Extract node ID from packet header if it's a Meshtastic packet
        uint32_t nodeId = 0;
        if (length >= 16 && data[0] == 0xFF && data[1] == 0xFF && 
            data[2] == 0xFF && data[3] == 0xFF) {
            nodeId = ((uint32_t)data[4]) | ((uint32_t)data[5] << 8) |
                     ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
        }
        
        // Get decrypted text if available
        const char* decryptedText = PSKDecryption::getLastMessage();
        
        if (reconState.capturePacketForReplay(data, length, reconState.scanState.currentConfig, 
                                               rssi, info.protocol, decryptedText, nodeId)) {
            Serial.println("✅ Packet saved to replay slot!");
            if (decryptedText && decryptedText[0] != '\0') {
                Serial.printf("   📧 Decrypted text: \"%s\"\n", decryptedText);
            }
        } else {
            Serial.println("❌ Failed to save packet (slots full?)");
        }
    } else {
        Serial.println("❌ No packet available to capture (must be in targeted mode)");
    }
}

void CommandHandler::cmdDeviceTarget(IReconTool* tool, uint8_t deviceIndex) {
    tool->startTargetedCapture(deviceIndex);
}

void CommandHandler::cmdGeoIntelligence(IReconTool* tool) {
    geoIntel.printSummary();
}

void CommandHandler::cmdExportKML(IReconTool* tool) {
    String kml;
    geoIntel.exportKML(kml);
    
    Serial.println("\n📍 KML EXPORT (Copy and save as .kml file)");
    Serial.println("============================================");
    Serial.println(kml);
    Serial.println("============================================\n");
    Serial.println("Save this to 'captured_devices.kml' and open in Google Earth");
}

void CommandHandler::cmdExportGeoJSON(IReconTool* tool) {
    String geojson;
    geoIntel.exportGeoJSON(geojson);
    
    Serial.println("\n🌍 GEOJSON EXPORT (Copy for web mapping)");
    Serial.println("==========================================");
    Serial.println(geojson);
    Serial.println("==========================================\n");
    Serial.println("Use with Leaflet, Mapbox, or other web mapping libraries");
}

void CommandHandler::cmdDiagnosticReport(IReconTool* tool) {
    // Print the comprehensive diagnostic report
    TextPacketDiagnostic::printReport();
    
    Serial.println("💡 TIP: To reset diagnostic counters, reboot device with 'b'");
    Serial.println("    or manually reset by restarting the device.\n");
}

void CommandHandler::cmdToggleQuietMode(IReconTool* tool) {
    bool currentMode = TextPacketDiagnostic::isVerbose();
    TextPacketDiagnostic::enableVerbose(!currentMode);
    
    if (!currentMode) {
        Serial.println("\n📢 VERBOSE MODE ENABLED");
        Serial.println("   All packet details will be shown (slower, more gaps)");
    } else {
        Serial.println("\n🔇 QUIET MODE ENABLED");
        Serial.println("   Minimal output for faster packet capture");
        Serial.println("   Only TEXT MESSAGES will be displayed");
        Serial.println("   Press 'x' to see statistics, 'q' to toggle back to verbose");
    }
    Serial.println();
}

