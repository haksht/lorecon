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
#include "crash_context.h"
#include "mode_manager.h"
#include "irecon_tool.h"  // Interface only - no concrete class needed
#include "radio_controller.h"  // Need full definition for method calls
#include "api_security.h"  // For token display
#include "oled_display.h"  // For OLED token display
#include "logger.h"
#include "user_interface.h"
#include "recon_state.h"
#include "protocol_analyzer.h"
#include "geo_intelligence.h"
#include "ui_components.h"
#include "text_packet_diagnostic.h"  // For diagnostic reporting
#include "psk_decryption_simple.h"   // For accessing decrypted text
#include "psk_tests.h"
#include "lorawan_keys.h"            // For LoRaWAN key testing stats

// Explicit definition required for constexpr static members (C++ linkage rule)
constexpr CommandHandler::CommandEntry CommandHandler::commands[];
constexpr uint8_t CommandHandler::numCommands;

// Pre-menu snapshot: captured when 'm' is pressed so 'e' can restore exactly.
// Overwritten on every menu entry, cleared on exit. If menu times out instead
// of explicit 'e', the stale snapshot is harmless — next 'm' overwrites it.
static OperationMode s_preMenuMode = MODE_RECONNAISSANCE;
static uint8_t       s_preMenuConfig = 0;
static bool          s_preMenuByDevice = false;
static bool          s_hasPreMenuSnapshot = false;

CommandHandler::CommandHandler(IReconTool* tool) 
    : reconTool(tool) 
{
}

bool CommandHandler::handleCommand(char cmd) {
    uint32_t now = millis();
    
    // Auto-deactivate serial after 5 minutes of inactivity (prevents overnight noise issues)
    if (serialActivated && lastCommandTime > 0 && (now - lastCommandTime) > INACTIVITY_TIMEOUT_MS) {
        serialActivated = false;
        firstEnterTime = 0;
        Serial.println("\n[SERIAL] Auto-deactivated due to inactivity. Press Enter twice to reactivate.");
    }
    
    // SERIAL ACTIVATION CHECK: Prevent phantom commands from USB noise
    // Requires DOUBLE-ENTER within 1.5 seconds to activate
    // Single Enter or random noise will NOT trigger activation
    if (!serialActivated) {
        if (cmd == '\n' || cmd == '\r') {
            // Debounce: Ignore \r\n arriving as single keypress (within 100ms)
            if (lastEnterTime > 0 && (now - lastEnterTime) < ENTER_DEBOUNCE_MS) {
                return false;  // Silently ignore, same keypress
            }
            lastEnterTime = now;
            
            if (firstEnterTime == 0) {
                // First Enter - start the window
                firstEnterTime = now;
                Serial.println("\n[SERIAL] Press Enter again within 1.5s to activate console...");
                return true;
            } else if ((now - firstEnterTime) <= DOUBLE_ENTER_WINDOW_MS) {
                // Second Enter within window - activate!
                serialActivated = true;
                firstEnterTime = 0;
                lastCommandTime = now;
                Serial.println("[SERIAL] ✓ Serial console activated. Press 'm' for menu.");
                return true;
            } else {
                // Too slow - reset and start over
                firstEnterTime = now;
                Serial.println("\n[SERIAL] Timeout. Press Enter again within 1.5s to activate...");
                return true;
            }
        }
        // Silently ignore all non-Enter input until activated (likely noise)
        return false;
    }
    
    // Track last command time for auto-deactivation
    lastCommandTime = now;
    
    // Filter out newlines after activation (common after commands)
    if (cmd == '\n' || cmd == '\r') {
        return false;
    }
    
    // Filter out non-printable characters
    if (cmd < 0x20 || cmd > 0x7E) {
        LOG_DEBUG("Ignoring non-printable byte: 0x%02X", (uint8_t)cmd);
        return false;
    }
    
    // STRICT VALIDATION: Only accept known commands to prevent phantom serial noise
    const CommandEntry* entry = findCommand(cmd);
    bool isDeviceSelect = (cmd >= '1' && cmd <= '9');
    
    if (!entry && !isDeviceSelect) {
        // Not a valid command - ignore silently (whitespace) or warn
        if (cmd != ' ' && cmd != '\t') {
            LOG_DEBUG("Unknown command: '%c'. Press 'm' for menu.", cmd);
        }
        return false;
    }
    
    // Log valid serial input for debugging
    LOG_DEBUG("Received: '%c' (0x%02X) in mode %d",
              cmd, (uint8_t)cmd, (int)reconState.scanState.mode.load());
    
    // Execute command from dispatch table
    if (entry) {
        LOG_DEBUG("Executing: %s", entry->description);
        entry->handler(reconTool);
        return true;
    }
    
    // Handle device selection (1-9)
    if (cmd >= '1' && cmd <= '9') {
        uint8_t deviceIndex = cmd - '1';
        if (deviceIndex < reconState.getNumTargetableDevices()) {
            cmdDeviceTarget(reconTool, deviceIndex);
            return true;
        } else {
            Serial.println("[ERROR] Invalid device number. Press 'm' for menu.");
            return false;
        }
    }
    
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
    Serial.println("\n=== AVAILABLE COMMANDS ===");
    
    // Group by functionality
    Serial.println("\n📡 TARGETING:");
    Serial.println("  1-9 : Target device by number");
    Serial.println("  f   : Frequency targeting (skip device)");
    
    Serial.println("\n📊 ANALYSIS:");
    Serial.println("  m   : Show menu with discovered devices");
    Serial.println("  s   : Show summary again");
    Serial.println("  a   : Detailed RF activity analysis");
    Serial.println("  d   : Device type breakdown");
    Serial.println("  v   : Security vulnerability assessment");
    
    Serial.println("\n🌍 GEO / EXPORT:");
    Serial.println("  g   : Geographic intelligence");
    Serial.println("  k   : Export KML (Google Earth)");
    Serial.println("  j   : Export GeoJSON (web maps)");
    Serial.println("  w   : LoRaWAN key testing stats");
    Serial.println("  x   : Text packet diagnostic report");

    Serial.println("\n🔧 OPERATIONS:");
    Serial.println("  c   : Capture packet for replay");
    Serial.println("  p   : Packet replay menu");
    Serial.println("  l   : Clear all captured packets");
    Serial.println("  n   : Clear all discovered devices");
    Serial.println("  e   : Exit menu (resume current mode)");
    Serial.println("  r   : Resume reconnaissance (keep devices)");
    Serial.println("  q   : Toggle quiet/verbose mode");
    Serial.println("  t   : Show API token (for mobile)");
    Serial.println("  i   : Reset reason & health info");
    Serial.println("  b   : Reboot device (clears all data)");

    Serial.println();
}

// ============================================================================
// Command Implementations
// ============================================================================

void CommandHandler::cmdShowMenu(IReconTool* tool) {
    // Snapshot the current mode before switching to menu so 'e' can restore it exactly
    s_preMenuMode = reconState.scanState.mode;
    s_preMenuConfig = reconState.scanState.targetConfig;
    s_preMenuByDevice = reconState.scanState.targetedByDevice;
    s_hasPreMenuSnapshot = true;

    ModeManager modeManager;
    modeManager.logModeTransition(reconState.scanState.mode, MODE_INTERACTIVE_MENU, "Serial:showMenu");
    reconState.scanState.mode = MODE_INTERACTIVE_MENU;
    // Track when menu mode was entered for auto-timeout
    if (tool) {
        tool->setMenuModeEntered();
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
    Serial.printf("Current devices: %d, Replay slots: %d\n", 
                  reconState.getNumTargetableDevices(), 
                  reconState.getNumCapturedPackets());
    
    // Clear persisted targeting mode from NVS
    ModeManager modeManager;
    modeManager.clearPersistedMode("Serial:resumeRecon");
    Serial.println("[MODE] Cleared persisted targeting mode");
    
    // Log the mode transition
    modeManager.logModeTransition(reconState.scanState.mode, MODE_RECONNAISSANCE, "Serial:resumeRecon");
    
    // Reset only the scan state to restart the cycle
    reconState.scanState.mode = MODE_RECONNAISSANCE;
    reconState.scanState.currentConfig = 0;
    reconState.scanState.lastScanSwitch = millis();
    
    // Clear menu timeout since we're leaving menu mode
    if (tool) {
        tool->clearMenuTimeout();
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

        // Show reboot message on OLED
        OLEDDisplay* display = tool->getDisplay();
        if (display) display->showReboot();

        Serial.flush();  // Ensure output is sent before restart
        delay(100);
        ESP.restart();
    } else {
        Serial.println("\n❌ Reboot cancelled. Returning to menu.");
    }
}

void CommandHandler::cmdShowSummary(IReconTool* tool) {
    showReconResults();
}

void CommandHandler::cmdSecurityAssessment(IReconTool* tool) {
    showSecurityAssessment();
    ModeManager modeManager;
    modeManager.logModeTransition(reconState.scanState.mode, MODE_INTERACTIVE_MENU, "Serial:securityAssess");
    reconState.scanState.mode = MODE_INTERACTIVE_MENU;
    if (tool) {
        tool->setMenuModeEntered();
    }
    showReconResults();
}

void CommandHandler::cmdCapturePacket(IReconTool* tool) {
    // Copy lastPacket under lock to avoid race with packet_processor writer
    uint8_t localPacket[Config::PacketProcessing::MAX_PACKET_SIZE];
    size_t length = 0;
    bool inTargetedMode = false;
    {
        ReconState::ScopedLock lock(reconState);
        length = reconState.scanState.lastPacketLength;
        inTargetedMode = (reconState.scanState.mode == MODE_TARGETED_CAPTURE);
        if (length > 0 && length <= sizeof(localPacket)) {
            memcpy(localPacket, reconState.scanState.lastPacket, length);
        }
    }

    if (length > 0 && inTargetedMode) {
        const uint8_t* data = localPacket;
        float rssi = tool->getRadioController()->getRSSI();
        
        // Analyze packet
        ProtocolAnalyzer analyzer;
        PacketInfo info = analyzer.analyze(data, length, rssi);
        
        // Extract all header fields from Meshtastic packet
        uint32_t nodeId = 0;
        uint32_t destId = 0xFFFFFFFF;
        uint32_t packetId = 0;
        uint8_t hopCount = 0;
        uint8_t channel = 0;
        bool wantAck = false;
        bool viaMqtt = false;
        uint8_t priority = 0;
        
        if (length >= 16 && data[0] == 0xFF && data[1] == 0xFF && 
            data[2] == 0xFF && data[3] == 0xFF) {
            // Destination ID at bytes 0-3 (little-endian)
            destId = ((uint32_t)data[0]) | ((uint32_t)data[1] << 8) |
                     ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
            // Source/From ID at bytes 4-7 (little-endian)
            nodeId = ((uint32_t)data[4]) | ((uint32_t)data[5] << 8) |
                     ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
            // Packet ID at offset 8-11 (little-endian)
            if (length >= 12) {
                packetId = ((uint32_t)data[8]) | ((uint32_t)data[9] << 8) |
                           ((uint32_t)data[10] << 16) | ((uint32_t)data[11] << 24);
            }
            // Flags at byte 12
            if (length >= 13) {
                uint8_t flags = data[12];
                hopCount = flags & 0x07;           // Bits 0-2: hop count
                wantAck = (flags >> 3) & 0x01;     // Bit 3: want acknowledgment
                viaMqtt = (flags >> 4) & 0x01;     // Bit 4: via MQTT gateway
                priority = (flags >> 5) & 0x03;   // Bits 5-6: priority (0-3)
            }
            // Channel at byte 13
            if (length >= 14) {
                channel = data[13];
            }
        }
        
        // Get decrypted text if available (thread-safe copy)
        char decryptedBuf[256];
        PSKDecryption::getLastMessageSafe(decryptedBuf, sizeof(decryptedBuf));
        const char* decryptedText = decryptedBuf[0] != '\0' ? decryptedBuf : nullptr;

        if (reconState.capturePacketForReplay(data, length, reconState.scanState.currentConfig,
                                               rssi, 0.0f, info.protocol, decryptedText, nodeId, packetId, hopCount,
                                               destId, channel, wantAck, viaMqtt, priority)) {
            Serial.println("✅ Packet saved to replay slot!");
            if (decryptedText) {
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

void CommandHandler::cmdClearPackets(IReconTool* tool) {
    uint8_t count = reconState.getNumCapturedPackets();
    reconState.clearReplaySlots();
    Serial.printf("\n✅ Cleared %d captured packet(s) from replay slots.\n\n", count);
}

void CommandHandler::cmdClearDevices(IReconTool* tool) {
    uint8_t deviceCount = reconState.getNumTargetableDevices();
    reconState.clearTargetableDevices();
    Serial.printf("\n✅ Cleared %d device(s).\n\n", deviceCount);
}

void CommandHandler::cmdShowToken(IReconTool* tool) {
    String token = APISecurity::getToken();
    
    Serial.println("\n🔑 API TOKEN (for protected endpoints)");
    Serial.println("======================================");
    Serial.printf("Token: %s\n", token.c_str());
    Serial.println("======================================");
    Serial.println("Enter this in Settings > API Authentication on the web UI");
    Serial.println("Or use header: X-API-Token: <token>\n");
    
    // Show on OLED for mobile users
    #ifdef HAS_OLED_DISPLAY
    OLEDDisplay* display = tool->getDisplay();
    if (display) {
        display->showApiToken(token.c_str());
        Serial.println("📱 TOKEN DISPLAYED ON OLED");
        Serial.println("   (Look at device screen)\n");
    }
    #endif
}

void CommandHandler::cmdLoRaWANStats(IReconTool* tool) {
    LoRaWANKeys::printSummary();
}

void CommandHandler::cmdResetInfo(IReconTool* tool) {
    CrashContext::printResetInfo();
}

void CommandHandler::cmdExitMenu(IReconTool* tool) {
    // Restore to whichever mode was active before the menu opened,
    // without touching NVS (unlike 'r' which clears persisted targeting).
    ModeManager modeManager;
    OperationMode resumeMode = MODE_RECONNAISSANCE;
    uint8_t resumeConfig = 0;
    bool resumeByDevice = false;

    if (s_hasPreMenuSnapshot && s_preMenuMode == MODE_TARGETED_CAPTURE) {
        // In-memory snapshot is the most accurate: captured at the moment 'm' was pressed
        resumeMode = s_preMenuMode;
        resumeConfig = s_preMenuConfig;
        resumeByDevice = s_preMenuByDevice;
        reconState.scanState.targetConfig = resumeConfig;
        reconState.scanState.currentConfig = resumeConfig;
        reconState.scanState.targetedByDevice = resumeByDevice;
        Serial.printf("\n▶  Returning to TARGETING mode (config %d)\n\n", resumeConfig);
    } else if (modeManager.loadPersistedMode(resumeMode, resumeConfig, resumeByDevice)) {
        // Fall back to NVS (menu entered via webapp stopScan, not serial 'm')
        reconState.scanState.targetConfig = resumeConfig;
        reconState.scanState.currentConfig = resumeConfig;
        reconState.scanState.targetedByDevice = resumeByDevice;
        Serial.printf("\n▶  Returning to %s mode (config %d)\n\n",
                      resumeMode == MODE_TARGETED_CAPTURE ? "TARGETING" : "RECONNAISSANCE",
                      resumeConfig);
    } else {
        Serial.println("\n▶  Returning to RECONNAISSANCE mode\n");
    }

    s_hasPreMenuSnapshot = false;

    modeManager.logModeTransition(MODE_INTERACTIVE_MENU, resumeMode, "Serial:exitMenu");
    reconState.scanState.mode = resumeMode;

    if (tool) {
        tool->clearMenuTimeout();
    }
    reconState.scanState.packetPending = false;
    reconState.scanState.waitingForUserInput = false;

    while (Serial.available()) Serial.read();  // discard buffered noise before resuming

    const ScanConfig& cfg = reconState.getScanConfig(reconState.scanState.currentConfig);
    tool->getRadioController()->applyConfig(cfg);
    tool->getRadioController()->startReceive();
}

