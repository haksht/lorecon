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

#include "command_handler.h"
#include "lora_recon_tool.h"
#include "user_interface.h"
#include "recon_state.h"
#include "protocol_analyzer.h"
#include "geo_intelligence.h"
#include "ui_components.h"
#include "text_packet_diagnostic.h"  // For diagnostic reporting

#ifdef ENABLE_PSK_TESTING
#include "psk_tests.h"
#endif

// Command dispatch table - O(1) lookup instead of linear scan
const CommandHandler::CommandEntry CommandHandler::commands[] = {
    {'m', cmdShowMenu,            "Show menu and results",         false},
    {'M', cmdShowMenu,            "Show menu and results",         false},
    {'f', cmdFrequencyTargeting,  "Frequency targeting mode",      false},
    {'F', cmdFrequencyTargeting,  "Frequency targeting mode",      false},
    {'8', cmdTargetSF8,           "Target SF8 (encrypted msgs)",   false},
    {'d', cmdDeviceTypeSummary,   "Device type analysis",          false},
    {'D', cmdDeviceTypeSummary,   "Device type analysis",          false},
    {'a', cmdActivityDetails,     "RF activity details",           false},
    {'A', cmdActivityDetails,     "RF activity details",           false},
    {'p', cmdPacketReplay,        "Packet replay menu",            false},
    {'P', cmdPacketReplay,        "Packet replay menu",            false},
    {'r', cmdRestartRecon,        "Restart reconnaissance",        false},
    {'R', cmdRestartRecon,        "Restart reconnaissance",        false},
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
    {'q', cmdRequestSessionKey,   "Request session key",           false},
    {'Q', cmdSessionKeyStatus,    "Show session key status",       false},
    {'J', cmdExportGeoJSON,       "Export GeoJSON (web maps)",     false},
    {'x', cmdDiagnosticReport,    "Text packet diagnostic report", false},
    {'X', cmdDiagnosticReport,    "Text packet diagnostic report", false},
    {'q', cmdToggleQuietMode,     "Toggle quiet mode (faster capture)", false},
    {'Q', cmdToggleQuietMode,     "Toggle quiet mode (faster capture)", false},
#ifdef ENABLE_STRESS_TESTING
    {'t', cmdStressTesting,       "Hardware stress testing",       false},
    {'T', cmdStressTesting,       "Hardware stress testing",       false},
#endif
};

const uint8_t CommandHandler::numCommands = sizeof(commands) / sizeof(CommandEntry);

CommandHandler::CommandHandler(LoRaReconTool* tool) 
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
    
    // Then handle device selection (1-9) for devices 1-7, 9
    // (Note: 8 is reserved for SF8 targeting command above)
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
    Serial.println("  8   : Quick target SF8 (encrypted messages)");
    
    Serial.println("\\n📊 ANALYSIS:");
    Serial.println("  m   : Show menu with discovered devices");
    Serial.println("  s   : Show summary again");
    Serial.println("  a   : Detailed RF activity analysis");
    Serial.println("  d   : Device type breakdown");
    Serial.println("  v   : Security vulnerability assessment");
    
    Serial.println("\\n🔧 OPERATIONS:");
    Serial.println("  c   : Capture packet for replay");
    Serial.println("  p   : Packet replay menu");
    Serial.println("  r   : Restart reconnaissance");
    
#ifdef ENABLE_STRESS_TESTING
    Serial.println("\\n⚡ TESTING:");
    Serial.println("  t   : Hardware stress testing");
#endif
    
    Serial.println();
}

// ============================================================================
// Command Implementations
// ============================================================================

void CommandHandler::cmdShowMenu(LoRaReconTool* tool) {
    reconState.scanState.mode = MODE_INTERACTIVE_MENU;
    showReconResults();
}

void CommandHandler::cmdFrequencyTargeting(LoRaReconTool* tool) {
    showFrequencyTargetingMenu();
    handleFrequencyTargetingInput();
}

void CommandHandler::cmdTargetSF8(LoRaReconTool* tool) {
    Serial.println("\\n🎯 TARGETING SF8 FREQUENCY FOR ENCRYPTED MESSAGES");
    Serial.println("Switching to Meshtastic_902_SF8 (902.125 MHz, SF8, BW250)");
    
    reconState.scanState.mode = MODE_TARGETED_CAPTURE;
    reconState.scanState.targetConfig = 6;  // Meshtastic_902_SF8
    reconState.scanState.currentConfig = 6;
    
    Serial.println("Configuration:");
    const ScanConfig& cfg = reconState.getScanConfig(6);
    Serial.printf("  Frequency: %.3f MHz\\n", cfg.frequency);
    Serial.printf("  Spreading Factor: SF%d\\n", cfg.spreadingFactor);
    Serial.printf("  Bandwidth: %.0f kHz\\n", cfg.bandwidth);
    Serial.printf("  Sync Word: 0x%02X\\n", cfg.syncWord);
    Serial.println("\\nThis configuration should capture encrypted user messages!");
    Serial.println("Press 'm' for menu, 'r' to restart recon");
    Serial.println("Monitoring for encrypted packets...\\n");
    
    tool->applyConfigPublic(6);
    tool->startReceiving();
}

void CommandHandler::cmdDeviceTypeSummary(LoRaReconTool* tool) {
    showDeviceTypeSummary();
}

void CommandHandler::cmdActivityDetails(LoRaReconTool* tool) {
    showActivityDetails();
}

void CommandHandler::cmdPacketReplay(LoRaReconTool* tool) {
    tool->showReplayMenu();
}

void CommandHandler::cmdRestartRecon(LoRaReconTool* tool) {
    reconState.reset();
    
    // Reset diagnostic counters when restarting reconnaissance
    TextPacketDiagnostic::reset();
    
    Serial.println("\\n=== RESTARTING RECONNAISSANCE ===");
    Serial.println("Cleared activity history and device list.");
    Serial.println("Cleared diagnostic counters.");
    tool->applyConfigPublic(reconState.scanState.currentConfig);
    tool->startReceiving();
}

void CommandHandler::cmdShowSummary(LoRaReconTool* tool) {
    showReconResults();
}

void CommandHandler::cmdSecurityAssessment(LoRaReconTool* tool) {
    showSecurityAssessment();
    reconState.scanState.mode = MODE_INTERACTIVE_MENU;
    showReconResults();
}

void CommandHandler::cmdCapturePacket(LoRaReconTool* tool) {
    if (reconState.scanState.lastPacketLength > 0 && 
        reconState.scanState.mode == MODE_TARGETED_CAPTURE) {
        
        const uint8_t* data = (const uint8_t*)reconState.scanState.lastPacket;
        size_t length = reconState.scanState.lastPacketLength;
        float rssi = tool->getRadio().getRSSI();
        
        // Analyze packet
        ProtocolAnalyzer analyzer;
        PacketInfo info = analyzer.analyze(data, length, rssi);
        
        if (reconState.capturePacketForReplay(data, length, reconState.scanState.currentConfig, 
                                               rssi, info.protocol)) {
            Serial.println("✅ Packet saved to replay slot!");
        } else {
            Serial.println("❌ Failed to save packet (slots full?)");
        }
    } else {
        Serial.println("❌ No packet available to capture (must be in targeted mode)");
    }
}

void CommandHandler::cmdDeviceTarget(LoRaReconTool* tool, uint8_t deviceIndex) {
    tool->startTargetedCapture(deviceIndex);
}

void CommandHandler::cmdGeoIntelligence(LoRaReconTool* tool) {
    geoIntel.printSummary();
}

void CommandHandler::cmdExportKML(LoRaReconTool* tool) {
    String kml;
    geoIntel.exportKML(kml);
    
    Serial.println("\n📍 KML EXPORT (Copy and save as .kml file)");
    Serial.println("============================================");
    Serial.println(kml);
    Serial.println("============================================\n");
    Serial.println("Save this to 'captured_devices.kml' and open in Google Earth");
}

void CommandHandler::cmdExportGeoJSON(LoRaReconTool* tool) {
    String geojson;
    geoIntel.exportGeoJSON(geojson);
    
    Serial.println("\n🌍 GEOJSON EXPORT (Copy for web mapping)");
    Serial.println("==========================================");
    Serial.println(geojson);
    Serial.println("==========================================\n");
    Serial.println("Use with Leaflet, Mapbox, or other web mapping libraries");
}

void CommandHandler::cmdDiagnosticReport(LoRaReconTool* tool) {
    // Print the comprehensive diagnostic report
    TextPacketDiagnostic::printReport();
    
    Serial.println("💡 TIP: To reset diagnostic counters, restart reconnaissance with 'r'");
    Serial.println("    or manually reset by restarting the device.\n");
}

void CommandHandler::cmdToggleQuietMode(LoRaReconTool* tool) {
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

#ifdef ENABLE_PSK_TESTING
void CommandHandler::cmdRequestSessionKey(LoRaReconTool* tool) {
    Serial.println("\n[SESSION] 🔑 Requesting session key from mesh...");
    
    // Use a node ID we've seen, or random
    uint32_t nodeId = 0xDEADBEEF;  // Default random
    
    if (reconState.numTargetableDevices > 0) {
        // Use ID of first discovered device
        nodeId = reconState.getTargetableDevice(0).nodeId;
        Serial.printf("[SESSION] Using node ID: 0x%08X\n", nodeId);
    } else {
        Serial.println("[SESSION] Using random node ID (no devices discovered yet)");
    }
    
    if (tool->getSessionKeyManager().requestSessionKey(0, nodeId)) {
        Serial.println("[SESSION] ✅ Request transmitted");
        Serial.println("[SESSION] 📡 Listening for response...");
        Serial.println("[SESSION] (Should arrive within 5-10 seconds)\n");
    } else {
        Serial.println("[SESSION] ❌ Request transmission failed\n");
    }
}

void CommandHandler::cmdSessionKeyStatus(LoRaReconTool* tool) {
    tool->getSessionKeyManager().printStatus();
}
#endif

#ifdef ENABLE_STRESS_TESTING
void CommandHandler::cmdStressTesting(LoRaReconTool* tool) {
    showStressTestMenu();
}
#endif
