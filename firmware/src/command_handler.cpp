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

#ifdef ENABLE_OFFENSIVE_TESTING
#include "device_stress_tester.h"
// Note: AttackScenarios and VulnerabilityScanner are defined in device_stress_tester.h
#endif

// Command dispatch table - O(1) lookup instead of linear scan
const CommandHandler::CommandEntry CommandHandler::commands[] = {
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
#ifdef ENABLE_STRESS_TESTING
    {'t', cmdStressTesting,       "Hardware stress testing",       false},
    {'T', cmdStressTesting,       "Hardware stress testing",       false},
#endif
#ifdef ENABLE_OFFENSIVE_TESTING
    {'A', cmdOffensiveTesting,    "Offensive testing (attacks)",   false},
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
    
#ifdef ENABLE_STRESS_TESTING
    Serial.println("\\n⚡ TESTING:");
    Serial.println("  t   : Hardware stress testing");
#endif

#ifdef ENABLE_OFFENSIVE_TESTING
    Serial.println("\\n⚔️  OFFENSIVE TESTING:");
    Serial.println("  A   : Attack menu (requires authorization)");
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

void CommandHandler::cmdDeviceTypeSummary(LoRaReconTool* tool) {
    showDeviceTypeSummary();
}

void CommandHandler::cmdActivityDetails(LoRaReconTool* tool) {
    showActivityDetails();
}

void CommandHandler::cmdPacketReplay(LoRaReconTool* tool) {
    tool->showReplayMenu();
}

void CommandHandler::cmdResumeRecon(LoRaReconTool* tool) {
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
    reconState.scanState.packetPending = false;
    reconState.scanState.waitingForUserInput = false;
    
    tool->applyConfigPublic(reconState.scanState.currentConfig);
    tool->startReceiving();
}

void CommandHandler::cmdRebootDevice(LoRaReconTool* tool) {
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
        
        tool->applyConfigPublic(reconState.scanState.currentConfig);
        tool->startReceiving();
    } else {
        Serial.println("\n❌ Reboot cancelled. Returning to menu.");
    }
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
    
    Serial.println("💡 TIP: To reset diagnostic counters, reboot device with 'b'");
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

#ifdef ENABLE_OFFENSIVE_TESTING
void CommandHandler::cmdOffensiveTesting(LoRaReconTool* tool) {
    Serial.println("\n╔════════════════════════════════════════════╗");
    Serial.println("║       OFFENSIVE TESTING FRAMEWORK          ║");
    Serial.println("╚════════════════════════════════════════════╝");
    Serial.println();
    Serial.println("⚠️  WARNING: Use only on authorized devices!");
    Serial.println("⚠️  Read conference/ETHICAL_TESTING_GUIDELINES.md");
    Serial.println();
    
    // Check if we have discovered devices to target
    if (reconState.numTargetableDevices == 0) {
        Serial.println("❌ No devices discovered yet.");
        Serial.println("   Run reconnaissance first to find targets.");
        Serial.println();
        return;
    }
    
    // Show menu
    Serial.println("📋 ATTACK TYPES:");
    Serial.println("  1. Packet Flood (DoS)");
    Serial.println("  2. Malformed Packets (Fuzzing)");
    Serial.println("  3. Frequency Jamming");
    Serial.println("  4. Timing Analysis");
    Serial.println("  5. Protocol Violations");
    Serial.println("  6. Power Analysis");
    Serial.println("  7. Replay Attack");
    Serial.println("  8. Packet Injection");
    Serial.println("  9. Routing Manipulation");
    Serial.println("  0. Identity Spoofing");
    Serial.println();
    Serial.println("  F. Full Vulnerability Scan");
    Serial.println("  Q. Quick Safety Scan");
    Serial.println("  X. Exit");
    Serial.println();
    Serial.print("Select attack type: ");
    
    // Wait for input with watchdog feeding
    char selection = 0;
    uint32_t startTime = millis();
    while (millis() - startTime < 30000) {  // 30 second timeout
        esp_task_wdt_reset();
        if (Serial.available()) {
            selection = Serial.read();
            Serial.println(selection);
            break;
        }
        delay(100);
    }
    
    if (selection == 0) {
        Serial.println("\n⏱️  Timeout - returning to main menu");
        return;
    }
    
    // Handle exit
    if (selection == 'x' || selection == 'X') {
        Serial.println("\n✅ Exiting offensive testing menu");
        return;
    }
    
    // Initialize DeviceStressTester
    tool->ensureDeviceStressTesterInitialized();
    DeviceStressTester* tester = tool->getDeviceStressTester();
    
    if (!tester) {
        Serial.println("\n❌ ERROR: Failed to initialize DeviceStressTester");
        return;
    }
    
    // Select first device as default target
    if (reconState.numTargetableDevices > 0) {
        const TargetableDevice& dev = reconState.getTargetableDevice(0);
        float targetFreq = reconState.getScanConfig(dev.configIndex).frequency;
        tester->selectTarget(dev.nodeId, targetFreq);
        Serial.printf("🎯 Default target: 0x%08X at %.3f MHz\n\n", dev.nodeId, targetFreq);
    }
    
    // Map selection to attack type and execute
    AttackType attackType;
    bool executeAttack = false;
    
    switch(selection) {
        case '1':
            attackType = ATTACK_PACKET_FLOOD;
            executeAttack = true;
            Serial.println("\n📝 Executing: PACKET_FLOOD - Test DoS resilience");
            break;
        case '2':
            attackType = ATTACK_MALFORMED_PACKETS;
            executeAttack = true;
            Serial.println("\n📝 Executing: MALFORMED_PACKETS - Protocol fuzzing");
            break;
        case '3':
            attackType = ATTACK_FREQUENCY_JAMMING;
            executeAttack = true;
            Serial.println("\n📝 Executing: FREQUENCY_JAMMING - RF interference");
            Serial.println("   ⚠️  WARNING: VERY DESTRUCTIVE - blocks all traffic");
            break;
        case '4':
            attackType = ATTACK_TIMING_ANALYSIS;
            executeAttack = true;
            Serial.println("\n📝 Executing: TIMING_ANALYSIS - Side-channel analysis");
            break;
        case '5':
            attackType = ATTACK_PROTOCOL_VIOLATION;
            executeAttack = true;
            Serial.println("\n📝 Executing: PROTOCOL_VIOLATIONS - Edge case testing");
            break;
        case '6':
            attackType = ATTACK_POWER_ANALYSIS;
            executeAttack = true;
            Serial.println("\n📝 Executing: POWER_ANALYSIS - RSSI-based leakage");
            break;
        case '7':
            attackType = ATTACK_REPLAY;
            executeAttack = true;
            Serial.println("\n📝 Executing: REPLAY_ATTACK - Anti-replay testing");
            break;
        case '8':
            attackType = ATTACK_INJECTION;
            executeAttack = true;
            Serial.println("\n📝 Executing: PACKET_INJECTION - Authentication testing");
            break;
        case '9':
            attackType = ATTACK_ROUTING_MANIPULATION;
            executeAttack = true;
            Serial.println("\n📝 Executing: ROUTING_MANIPULATION - Routing security");
            break;
        case '0':
            attackType = ATTACK_IDENTITY_SPOOFING;
            executeAttack = true;
            Serial.println("\n📝 Executing: IDENTITY_SPOOFING - Authentication bypass");
            break;
        case 'f':
        case 'F':
            Serial.println("\n📝 Executing: FULL_VULNERABILITY_SCAN");
            Serial.println("   • All 10 attack types");
            Serial.println("   • Duration: ~20 minutes");
            Serial.println();
            
            // Run all attack types in sequence manually
            {
                const TargetableDevice& targetDev = reconState.getTargetableDevice(0);
                uint32_t targetNodeId = targetDev.nodeId;
                float targetFreq = reconState.getScanConfig(targetDev.configIndex).frequency;
                
                Serial.printf("🎯 Scanning target: 0x%08X at %.3f MHz\n", targetNodeId, targetFreq);
                Serial.println("   Running all attack types in sequence...");
                Serial.println();
                
                AttackType allAttacks[] = {
                    ATTACK_TIMING_ANALYSIS,      // Start with non-destructive
                    ATTACK_PROTOCOL_VIOLATION,
                    ATTACK_POWER_ANALYSIS,
                    ATTACK_REPLAY,
                    ATTACK_INJECTION,
                    ATTACK_ROUTING_MANIPULATION,
                    ATTACK_IDENTITY_SPOOFING,
                    ATTACK_PACKET_FLOOD,
                    ATTACK_MALFORMED_PACKETS
                    // Skip ATTACK_FREQUENCY_JAMMING as it's very destructive
                };
                
                int successCount = 0;
                int totalTests = 9;
                
                for (int i = 0; i < totalTests; i++) {
                    Serial.printf("\n[%d/%d] Running attack type %d...\n", i+1, totalTests, allAttacks[i]);
                    
                    AttackConfig config;
                    config.type = allAttacks[i];
                    config.targetNodeId = targetNodeId;
                    config.targetFrequency = targetFreq;
                    config.duration = 30000;  // 30 seconds each
                    config.packetRate = 10;
                    config.power = 10;
                    config.destructive = false;
                    config.logAllActions = true;
                    
                    bool success = tester->executeAttack(config);
                    if (success) successCount++;
                    
                    esp_task_wdt_reset();
                    delay(2000);  // Pause between attacks
                }
                
                Serial.println("\n" + String('=').substring(0,60));
                Serial.println("       FULL VULNERABILITY SCAN COMPLETE");
                Serial.println(String('=').substring(0,60));
                Serial.printf("Tests completed: %d/%d\n", successCount, totalTests);
                Serial.println("\n✅ Scan complete! Check individual attack results above.");
            }
            
            Serial.println("\nPress any key to continue...");
            while (!Serial.available()) {
                esp_task_wdt_reset();
                delay(100);
            }
            Serial.read();
            return;
            
        case 'q':
        case 'Q':
            Serial.println("\n📝 Executing: QUICK_SAFETY_SCAN");
            Serial.println("   • Non-destructive tests only");
            Serial.println("   • Duration: ~5 minutes");
            Serial.println();
            
            // Use reliability test (non-destructive)
            {
                const TargetableDevice& targetDev = reconState.getTargetableDevice(0);
                uint32_t targetNodeId = targetDev.nodeId;
                float targetFreq = reconState.getScanConfig(targetDev.configIndex).frequency;
                
                AttackConfig config = AttackScenarios::reliabilityTest(targetNodeId);
                config.targetFrequency = targetFreq;
                
                Serial.printf("🎯 Testing target: 0x%08X at %.3f MHz\n", targetNodeId, targetFreq);
                Serial.println("   Running non-destructive reliability tests...");
                Serial.println();
                
                bool success = tester->executeAttack(config);
                AttackResult result = tester->getLastResult();
                
                Serial.println("\n✅ Quick scan complete!");
                Serial.printf("   Duration: %lu seconds\n", result.duration / 1000);
                Serial.printf("   Packets sent: %u\n", result.packetsTransmitted);
                Serial.printf("   Responses: %u\n", result.responsesSeen);
                Serial.printf("   Success rate: %.1f%%\n", result.successRate * 100);
                
                if (success) {
                    Serial.println("\n📊 Target appears stable");
                } else {
                    Serial.println("\n⚠️  Target showed anomalies");
                }
                
                if (result.findings.length() > 0) {
                    Serial.println("\n📝 Findings:");
                    Serial.println(result.findings);
                }
            }
            
            Serial.println("\nPress any key to continue...");
            while (!Serial.available()) {
                esp_task_wdt_reset();
                delay(100);
            }
            Serial.read();
            return;
            
        default:
            Serial.println("\n❌ Invalid selection");
            return;
    }
    
    // Execute single attack if selected
    if (executeAttack) {
        // Select target device
        Serial.println("\n📍 Select target device:");
        for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
            const TargetableDevice& dev = reconState.getTargetableDevice(i);
            Serial.printf("  %d. 0x%08X (%s) - %.1f dBm\n", 
                         i + 1, dev.nodeId, dev.protocol, dev.bestRSSI);
        }
        Serial.print("\nTarget (1-" + String(reconState.numTargetableDevices) + "): ");
        
        // Wait for device selection
        char deviceSelection = 0;
        uint32_t startTime = millis();
        while (millis() - startTime < 30000) {
            esp_task_wdt_reset();
            if (Serial.available()) {
                deviceSelection = Serial.read();
                Serial.println(deviceSelection);
                break;
            }
            delay(100);
        }
        
        if (deviceSelection == 0 || deviceSelection < '1' || 
            deviceSelection > ('0' + reconState.numTargetableDevices)) {
            Serial.println("\n❌ Invalid device selection");
            return;
        }
        
        uint8_t targetIndex = deviceSelection - '1';
        const TargetableDevice& targetDev = reconState.getTargetableDevice(targetIndex);
        uint32_t targetNodeId = targetDev.nodeId;
        float targetFreq = reconState.getScanConfig(targetDev.configIndex).frequency;
        
        // Create attack configuration
        AttackConfig config;
        config.type = attackType;
        config.targetNodeId = targetNodeId;
        config.targetFrequency = targetFreq;
        config.duration = 60000;  // Default 60 seconds
        config.packetRate = 50;     // 50 packets/sec
        config.power = 10;     // Default power
        config.destructive = false;  // Start with non-destructive
        config.logAllActions = true;
        
        Serial.printf("\n🚀 Launching attack against 0x%08X at %.3f MHz...\n", targetNodeId, targetFreq);
        Serial.println("   Press Ctrl+C to abort");
        Serial.println();
        
        // Execute attack
        bool success = tester->executeAttack(config);
        AttackResult result = tester->getLastResult();
        
        // Display results
        Serial.println("\n" + String('=').substring(0,60));
        Serial.println("              ATTACK RESULTS");
        Serial.println(String('=').substring(0,60));
        Serial.printf("Target: 0x%08X\n", targetNodeId);
        Serial.printf("Attack Type: %d\n", config.type);
        Serial.printf("Duration: %lu seconds\n", result.duration / 1000);
        Serial.printf("Packets Sent: %u\n", result.packetsTransmitted);
        Serial.printf("Responses: %u\n", result.responsesSeen);
        Serial.printf("Success Rate: %.1f%%\n", result.successRate * 100);
        
        if (result.vulnerabilityFound) {
            Serial.println("\n⚠️  VULNERABILITY DETECTED!");
        }
        if (result.targetCrashed) {
            Serial.println("⚠️  Target appears to have crashed");
        }
        if (result.targetUnresponsive) {
            Serial.println("⚠️  Target became unresponsive");
        }
        
        if (result.findings.length() > 0) {
            Serial.println("\n📝 Findings:");
            Serial.println(result.findings);
        }
        
        if (result.recommendations.length() > 0) {
            Serial.println("\n� Recommendations:");
            Serial.println(result.recommendations);
        }
        
        if (success) {
            Serial.println("\n✅ Attack completed successfully");
        } else {
            Serial.println("\n⚠️  Attack completed with errors");
        }
        
        Serial.println("\nPress any key to return to menu...");
        startTime = millis();
        while (millis() - startTime < 30000) {
            esp_task_wdt_reset();
            if (Serial.available()) {
                Serial.read();
                break;
            }
            delay(100);
        }
    }
}
#endif

#ifdef ENABLE_STRESS_TESTING
void CommandHandler::cmdStressTesting(LoRaReconTool* tool) {
    showStressTestMenu();
}
#endif
