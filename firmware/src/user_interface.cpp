/**
 * User Interface Module Implementation
 * 
 * Contains all display functions, menus, and user interaction logic
 * extracted from main.cpp for better code organization.
 */

#include "user_interface.h"
#include "lora_recon_tool.h"
#include "mode_manager.h"
#include "psk_decryption_simple.h"
#include "utils/security_scorer.h"
#include <Arduino.h>
#include <esp_task_wdt.h>

// Serial timeout constant
#define SERIAL_INPUT_TIMEOUT_MS 30000  // 30 seconds

// Helper function: Wait for serial input with timeout
static bool waitForSerialInput(uint32_t timeoutMs = SERIAL_INPUT_TIMEOUT_MS) {
  uint32_t startTime = millis();
  while (!Serial.available()) {
    if (millis() - startTime > timeoutMs) {
      Serial.println("\n[TIMEOUT] No input received. Returning to menu...");
      return false;
    }
    esp_task_wdt_reset();  // Feed watchdog while waiting
    delay(10);
  }
  return true;
}

// Display device type analysis summary for targetable devices
void showDeviceTypeSummary() {
  Serial.println("\nTARGETABLE DEVICE TYPE ANALYSIS:");
  Serial.println("Type                    | Count | Avg RSSI | Routers | Power Class");
  Serial.println("------------------------|-------|----------|---------|------------");
  
  // Count device types
  struct DeviceTypeStat {
    char type[24];
    uint8_t count;
    float totalRSSI;
    uint8_t routers;
    uint8_t powerClassSum;
  } typeStats[20];
  uint8_t numTypes = 0;
  
  for (uint8_t i = 0; i < reconState.getNumTargetableDevices(); i++) {
    const TargetableDevice& dev = reconState.getTargetableDevice(i);
    
    // Find existing type or create new
    DeviceTypeStat* stat = nullptr;
    for (uint8_t j = 0; j < numTypes; j++) {
      if (strcmp(typeStats[j].type, dev.deviceType) == 0) {
        stat = &typeStats[j];
        break;
      }
    }
    
    if (!stat && numTypes < 20) {
      stat = &typeStats[numTypes++];
      strncpy(stat->type, dev.deviceType, sizeof(stat->type) - 1);
      stat->type[sizeof(stat->type) - 1] = '\0';
      stat->count = 0;
      stat->totalRSSI = 0;
      stat->routers = 0;
      stat->powerClassSum = 0;
    }
    
    if (stat) {
      stat->count++;
      stat->totalRSSI += dev.avgRSSI;
      if (dev.isRouter) stat->routers++;
      stat->powerClassSum += dev.powerClass;
    }
  }
  
  if (numTypes == 0) {
    Serial.println("(No targetable devices for analysis)");
    return;
  }
  
  // Display type statistics
  for (uint8_t i = 0; i < numTypes; i++) {
    DeviceTypeStat& stat = typeStats[i];
    float avgRSSI = stat.count > 0 ? stat.totalRSSI / stat.count : 0.0f;
    float avgPowerClass = stat.count > 0 ? (float)stat.powerClassSum / stat.count : 0.0f;
    const char* powerDesc = avgPowerClass > 1.5 ? "High" : (avgPowerClass > 0.5 ? "Medium" : "Low");
    
    Serial.printf("%-23s | %5d | %8.1f | %7d | %s\n",
                  stat.type, stat.count, avgRSSI, stat.routers, powerDesc);
  }
  
  Serial.print("\nPress any key to continue: ");
  waitForSerialInput();
  Serial.read();  // Consume the character
}

// Security vulnerability assessment
void showSecurityAssessment() {
  Serial.println("\n🛡️ SECURITY VULNERABILITY ASSESSMENT");
  Serial.println("====================================");
  
  if (reconState.getNumTargetableDevices() == 0) {
    Serial.println("No devices available for assessment.");
    Serial.print("\nPress any key to continue: ");
    waitForSerialInput();
    Serial.read();
    return;
  }
  
  Serial.println("Device                  | Security Score | Status");
  Serial.println("------------------------|----------------|---------------");
  
  // Store device assessments for detailed breakdown
  struct DeviceAssessment {
    uint8_t index;
    uint8_t score;
    const char* rating;
    String vulnerabilities;
    const TargetableDevice* dev;
  };
  DeviceAssessment assessments[Config::Tracking::MAX_DEVICES];
  
  uint8_t vulnerableCount = 0;
  uint8_t moderateCount = 0;
  
  for (uint8_t i = 0; i < reconState.getNumTargetableDevices(); i++) {
    const TargetableDevice& dev = reconState.getTargetableDevice(i);
    
    // Use shared security scorer for consistent assessment
    SecurityScorer::Assessment assessment = SecurityScorer::assess(dev);
    
    // Build vulnerability string for display
    String vulnerabilities = "";
    if (assessment.physicalProximity) vulnerabilities += "Physical ";
    if (assessment.possibleUnencrypted) vulnerabilities += "Crypto? ";
    if (assessment.isRouter) vulnerabilities += "Router ";
    if (assessment.chatty) vulnerabilities += "Chatty ";
    if (assessment.intermittent) vulnerabilities += "Weak ";
    if (assessment.outdatedFirmware) vulnerabilities += "OldFW ";
    
    if (vulnerabilities.length() == 0) {
      vulnerabilities = "None detected";
    }
    
    // Track counts
    if (strcmp(assessment.rating, "vulnerable") == 0) {
      vulnerableCount++;
    } else if (strcmp(assessment.rating, "moderate") == 0) {
      moderateCount++;
    }
    
    // Print summary line
    Serial.printf("0x%08X (%8s) | %3d/100 %s |\n",
                  dev.nodeId, dev.deviceType, assessment.score, assessment.ratingEmoji);
    
    // Store for detailed breakdown
    assessments[i] = {i, assessment.score, assessment.ratingEmoji, vulnerabilities, &dev};
  }
  
  Serial.println("------------------------|----------------|---------------");
  
  // Detailed breakdowns for vulnerable and moderate devices
  if (vulnerableCount > 0 || moderateCount > 0) {
    Serial.println("\n🔍 DETAILED VULNERABILITY ANALYSIS:");
    Serial.println("===================================");
    
    for (uint8_t i = 0; i < reconState.getNumTargetableDevices(); i++) {
      if (assessments[i].score >= 80) continue; // Skip secure devices
      
      const TargetableDevice* dev = assessments[i].dev;
      Serial.printf("\n%s Device 0x%08X (%s):\n", 
                    assessments[i].rating, dev->nodeId, dev->deviceType);
      Serial.printf("   Score: %d/100\n", assessments[i].score);
      Serial.printf("   RSSI: %.1f dBm (avg: %.1f), Packets: %d\n",
                    dev->bestRSSI, dev->avgRSSI, dev->packetCount);
      
      Serial.println("   Findings:");
      if (dev->bestRSSI > -50) {
        Serial.println("      • Very strong signal (-50+ dBm) = physical proximity risk");
      }
      if (dev->isRouter) {
        Serial.println("      • Router device = higher attack surface (message forwarding)");
      }
      if (dev->packetCount > 100) {
        Serial.println("      • High traffic volume = potential information leakage");
      }
      if (dev->packetCount < 5) {
        Serial.println("      • Low packet count = weak battery or intermittent operation");
      }
      if (strstr(dev->firmwareVersion, "v1.x") != nullptr || 
          strstr(dev->firmwareVersion, "v2.0") != nullptr) {
        Serial.printf("      • Outdated firmware: %s (known vulnerabilities)\n", dev->firmwareVersion);
      }
      
      Serial.println("   Recommended Actions:");
      if (dev->bestRSSI > -50) {
        Serial.println("      → Increase physical distance or use directional antenna");
      }
      if (dev->isRouter) {
        Serial.println("      → Monitor for injection/replay attack opportunities");
      }
      if (dev->packetCount > 100) {
        Serial.println("      → Capture and analyze traffic patterns");
      }
    }
  }
  
  // Summary
  Serial.printf("\n📊 NETWORK SUMMARY:\n");
  Serial.printf("   Total Devices: %d\n", reconState.getNumTargetableDevices());
  Serial.printf("   🚨 Vulnerable: %d\n", vulnerableCount);
  Serial.printf("   ⚠️  Moderate Risk: %d\n", moderateCount);
  Serial.printf("   ✅ Secure: %d\n", 
                reconState.getNumTargetableDevices() - vulnerableCount - moderateCount);
  
  if (vulnerableCount > 0) {
    Serial.println("\n   Status: 🚨 High-priority targets identified");
  } else if (moderateCount > 0) {
    Serial.println("\n   Status: ⚠️ Some devices warrant investigation");
  } else {
    Serial.println("\n   Status: ✅ All devices appear well-secured");
  }
  
  Serial.println("\n💡 RECOMMENDATIONS:");
  if (vulnerableCount > 0) {
    Serial.println("   • Enable encryption with strong PSK");
    Serial.println("   • Update firmware to latest version");
    Serial.println("   • Reduce transmission frequency if possible");
    Serial.println("   • Monitor for unauthorized devices");
  } else {
    Serial.println("   • Continue monitoring for new devices");
    Serial.println("   • Perform periodic security assessments");
  }
  
  Serial.print("\nPress any key to continue: ");
  waitForSerialInput();
  Serial.read();
}

// Show detailed RF activity analysis
void showActivityDetails() {
  // Check if we're in targeted capture mode
  if (reconState.scanState.mode == MODE_TARGETED_CAPTURE) {
    // Show targeted device statistics instead
    Serial.println("\n🎯 TARGETED CAPTURE STATUS:");
    Serial.println("============================");
    
    const ScanConfig& config = reconState.getScanConfig(reconState.scanState.targetConfig);
    Serial.printf("Target Frequency: %.3f MHz\n", config.frequency);
    Serial.printf("Protocol: %s\n", config.protocol);
    Serial.printf("Spreading Factor: %d\n", config.spreadingFactor);
    Serial.printf("Bandwidth: %.0f kHz\n", config.bandwidth);
    
    // Show activity on this frequency
    const RFActivity& activity = reconState.getRFActivity(reconState.scanState.targetConfig);
    Serial.printf("\nPackets Captured: %d\n", reconState.getNumCapturedPackets());
    Serial.printf("Replay Slots Available: %d/%d\n", 
                  Config::Replay::MAX_SLOTS - reconState.getNumCapturedPackets(), Config::Replay::MAX_SLOTS);
    
    if (activity.activityCount > 0) {
      uint32_t now = millis();
      uint32_t ageMs = (activity.lastActivity <= now) ? (now - activity.lastActivity) : 0;
      uint32_t ageSeconds = ageMs / 1000;
      Serial.printf("Signal Activity: %s\n", activity.activityLevel);
      Serial.printf("Packets Detected: %d\n", activity.activityCount);
      Serial.printf("Peak RSSI: %.1f dBm\n", activity.peakRSSI);
      Serial.printf("Avg RSSI: %.1f dBm\n", activity.avgRSSI);
      Serial.printf("Last Activity: %us ago\n", (unsigned int)ageSeconds);
    }
    
    // Show devices discovered on this frequency
    Serial.println("\nDEVICES ON THIS FREQUENCY:");
    Serial.println("Node ID    | Device Type              | RSSI  | Packets");
    Serial.println("-----------|--------------------------|-------|--------");
    
    uint8_t devicesOnFreq = 0;
    for (uint8_t i = 0; i < reconState.getNumTargetableDevices(); i++) {
      const TargetableDevice& dev = reconState.getTargetableDevice(i);
      if (dev.configIndex == reconState.scanState.targetConfig) {
        Serial.printf("0x%08X | %-24s | %5.1f | %6d\n",
                      dev.nodeId, dev.deviceType, dev.bestRSSI, dev.packetCount);
        devicesOnFreq++;
      }
    }
    
    if (devicesOnFreq == 0) {
      Serial.println("(No devices identified yet)");
    }
    
    Serial.println("\n💡 TIP: Use 'c' to capture next packet, 'p' for replay menu");
    
    Serial.print("\nPress any key to continue: ");
    if (!waitForSerialInput()) return;
    Serial.read();
    return;
  }
  
  // Standard reconnaissance mode - show all frequencies
  Serial.println("\nDETAILED RF ACTIVITY ANALYSIS:");
  Serial.println("Configuration            | Frequency | SF | BW  | Activity | Signals | Age");
  Serial.println("-------------------------|-----------|----|----|----------|---------|-------");
  
  bool anyActivity = false;
  for (uint8_t i = 0; i < reconState.getNumConfigs(); i++) {
    const RFActivity& activity = reconState.getRFActivity(i);
    if (activity.activityCount > 0) {
      anyActivity = true;
      uint32_t now = millis();
      uint32_t ageMs = (activity.lastActivity <= now) ? (now - activity.lastActivity) : 0;
      uint32_t ageSeconds = ageMs / 1000;
      
      Serial.printf("%-24s | %7.3f   | %2d | %3.0f | %-8s | %7d | %4us\n",
                    reconState.getScanConfig(i).protocol,
                    reconState.getScanConfig(i).frequency,
                    reconState.getScanConfig(i).spreadingFactor,
                    reconState.getScanConfig(i).bandwidth,
                    activity.activityLevel,
                    activity.activityCount,
                    (unsigned int)ageSeconds);
    }
  }
  
  if (!anyActivity) {
    Serial.println("(No RF activity detected)");
    
    // If we're in menu mode and have no activity, explain why
    if (reconState.scanState.mode == MODE_INTERACTIVE_MENU) {
      Serial.println("\nℹ️  This command shows frequency scanning activity.");
      Serial.println("   You're currently in menu/targeted mode, not scanning.");
      Serial.println("   Use 'r' to resume reconnaissance and scan all frequencies.");
    }
  }
  
  Serial.println("\n💡 INTERPRETATION:");
  Serial.println("• HIGH activity: Strong nearby devices, good targeting potential");
  Serial.println("• MEDIUM activity: Moderate signals, devices may be distant");
  Serial.println("• LOW activity: Weak signals or intermittent transmissions");
  Serial.println("\nℹ️  This shows RF activity per frequency configuration.");
  Serial.println("   Use 'm' to see the full targetable device list.");
  
  Serial.print("\nPress any key to continue: ");
  if (!waitForSerialInput()) return;
  Serial.read();  // Consume the character
}

void showReconResults() {
  Serial.println("\n" + String('=').substring(0,60));
  Serial.println("              RECONNAISSANCE COMPLETE");
  Serial.println(String('=').substring(0,60));
  
  uint32_t reconTime = reconState.getReconDuration();
  Serial.printf("Scan Duration: %u seconds\n", (unsigned int)reconTime);
  Serial.printf("Total Detections: %d\n", reconState.scanState.totalDetections);
  Serial.printf("Targetable Devices: %d\n", reconState.getNumTargetableDevices());
  Serial.println();
  
  // Show RF Activity Summary
  Serial.println("RF ACTIVITY DETECTED:");
  Serial.println("Configuration            | Frequency | Activity | Avg RSSI | Peak RSSI | Count");
  Serial.println("-------------------------|-----------|----------|----------|-----------|-------");
  
  bool anyActivity = false;
  for (uint8_t i = 0; i < reconState.getNumConfigs(); i++) {
    const RFActivity& activity = reconState.getRFActivity(i);
    if (activity.activityCount > 0) {
      anyActivity = true;
      Serial.printf("%-24s | %7.3f   | %-8s | %8.1f | %9.1f | %5d\n",
                    reconState.getScanConfig(i).protocol,
                    reconState.getScanConfig(i).frequency,
                    activity.activityLevel,
                    activity.avgRSSI,
                    activity.peakRSSI,
                    activity.activityCount);
    }
  }
  
  if (!anyActivity) {
    Serial.println("(No significant RF activity detected)");
  }
  
  Serial.println();
  
  // Show Targetable Devices
  if (reconState.getNumTargetableDevices() == 0) {
    Serial.println("TARGETABLE DEVICES: None\n");
    Serial.println("ℹ️  No devices with decoded packets found.");
    Serial.println("   RF activity detected above, but no targetable node IDs captured.");
    Serial.println("   Press 'r' to resume reconnaissance or wait longer for packet capture.");
  } else {
    Serial.println("TARGETABLE DEVICES (Confirmed Node IDs):");
    Serial.println("ID | Device Type          | Node ID    | Protocol     | RSSI  | Pkts | Router");
    Serial.println("---|----------------------|------------|--------------|-------|------|-------");
    
    for (uint8_t i = 0; i < reconState.getNumTargetableDevices(); i++) {
      const TargetableDevice& dev = reconState.getTargetableDevice(i);
      
      Serial.printf("%2d | %-20s | 0x%08X | %-12s | %5.1f | %4d | %s\n",
                    i + 1,
                    dev.deviceType,
                    (unsigned int)dev.nodeId,
                    dev.protocol,
                    dev.bestRSSI,
                    (int)dev.packetCount,
                    dev.isRouter ? " YES " : " NO  ");
    }
  }
  
  // Show full command menu regardless of whether devices were found
  Serial.println("\nCOMMANDS:");
  if (reconState.getNumTargetableDevices() > 0) {
    Serial.println("1-" + String(reconState.getNumTargetableDevices()) + " : Target device for capture");
  }
  Serial.println("a   : Show activity details");
  Serial.println("d   : Show device type analysis");
  Serial.println("f   : Frequency targeting (skip device ID)");
  Serial.println("g   : Geographic intelligence (GPS data)");
  Serial.println("k   : Export KML (Google Earth format)");
  Serial.println("j   : Export GeoJSON (web mapping)");
  Serial.println("p   : Packet replay menu");
  Serial.println("r   : Resume reconnaissance (keep devices)");
  Serial.println("b   : Reboot device (clears all data)");
  Serial.println("s   : Show summary again");
  Serial.println("v   : Security vulnerability assessment");
  Serial.print("\nSelect target (1-" + String(reconState.getNumTargetableDevices()) + ") or command: ");
}

// Print scanning statistics
void printStats() {
  Serial.println("\n=== RECONNAISSANCE SUMMARY ===");
  Serial.printf("Total packets: %d\n", reconState.scanState.totalPackets.load());
  Serial.printf("Targetable devices: %d\n", reconState.getNumTargetableDevices());
  Serial.printf("Current: %s\n", reconState.getScanConfig(reconState.scanState.currentConfig).protocol);
  
  if (reconState.getNumTargetableDevices() > 0) {
    Serial.println("\n--- Targetable Devices ---");
    for (uint8_t i = 0; i < reconState.getNumTargetableDevices(); i++) {
      const TargetableDevice& dev = reconState.getTargetableDevice(i);
      uint32_t now = millis();
      uint32_t ageMs = (dev.lastSeen <= now) ? (now - dev.lastSeen) : 0;
      Serial.printf("0x%08X (%s): %d pkts, %.1f dBm avg, %ds ago\n",
                    dev.nodeId, dev.protocol, dev.packetCount,
                    dev.avgRSSI, ageMs / 1000);
    }
  }
  
  if (pskStats.attempts > 0) {
    Serial.println("\n--- PSK Testing Stats ---");
    Serial.printf("Attempts: %d, Successes: %d (%.1f%%)\n", 
                  pskStats.attempts, pskStats.successes,
                  (float)pskStats.successes / pskStats.attempts * 100.0);
    
    for (uint8_t i = 0; i < PSKDecryption::getDefaultPSKCount(); i++) {
      if (pskStats.hitCount[i] > 0) {
        Serial.printf("  Key #%d: %d hits\n", i + 1, pskStats.hitCount[i]);
      }
    }
  }
  
  Serial.println("==============================\\n");
}

void displayWelcomeMessage() {
  Serial.println("ESP32 LoRa Reconnaissance Tool");
  Serial.println("==============================");
  Serial.println("Hardware: ESP32-S3 + SX1262 LoRa");
  Serial.printf("Scanning %d configurations\n", reconState.getNumConfigs());
  Serial.printf("PSK testing: ENABLED (%d keys)\n", PSKDecryption::getDefaultPSKCount());
  
  Serial.println();
}

void displayReconStartMessage() {
  Serial.println("=== STARTING RECONNAISSANCE PHASE ===");
  Serial.println("Scanning all frequencies to detect active devices...");
  Serial.println("This will take approximately 2-3 minutes.");
  Serial.println("Press 'm' anytime to enter menu mode.");
  Serial.println();
}

// Frequency targeting menu for direct frequency selection
void showFrequencyTargetingMenu() {
  Serial.println("\n📡 FREQUENCY TARGETING");
  Serial.println("======================");
  Serial.println("Select frequency configuration to target directly:");
  Serial.println("(Based on recon activity detections)");
  Serial.println();
  
  // Show available frequency configurations with activity indicators
  for (uint8_t i = 0; i < reconState.getNumConfigs(); i++) {
    const ScanConfig& cfg = reconState.getScanConfig(i);
    
    // Check if this frequency has detected activity
    bool hasActivity = false;
    for (uint8_t j = 0; j < reconState.getNumTargetableDevices(); j++) {
      if (reconState.getTargetableDevice(j).configIndex == i) {
        hasActivity = true;
        break;
      }
    }
    
    // Display with activity indicator
    const char* activityIcon = hasActivity ? "🔥" : "⚪";
    Serial.printf("%2d %s : %s (%.3f MHz, SF%d, BW%.0f)\n",
                  i + 1, activityIcon, cfg.protocol, cfg.frequency, 
                  cfg.spreadingFactor, cfg.bandwidth);
  }
  
  Serial.println();
  Serial.println("🔥 = Activity detected during recon");
  Serial.println("⚪ = No activity detected"); 
  Serial.println();
  Serial.println("f  : Show frequency activity summary");
  Serial.println("r  : Return to main menu");
  Serial.print("\nSelect frequency (1-" + String(reconState.getNumConfigs()) + ") or command: ");
}

// Handle frequency targeting input
void handleFrequencyTargetingInput() {
  if (!waitForSerialInput()) {
    ModeManager modeManager;
    modeManager.logModeTransition(reconState.scanState.mode, MODE_INTERACTIVE_MENU, "FreqMenu:timeout");
    reconState.scanState.mode = MODE_INTERACTIVE_MENU;
    if (g_reconTool) {
        g_reconTool->setMenuModeEntered();
    }
    showReconResults();
    return;
  }
  
  char cmd = Serial.read();
  Serial.println(cmd);
  
  if (cmd >= '1' && cmd <= '9') {
    uint8_t configIndex = cmd - '1';
    if (configIndex < reconState.getNumConfigs()) {
      startFrequencyTargeting(configIndex);
    } else {
      Serial.println("❌ Invalid frequency selection");
      showFrequencyTargetingMenu();
    }
  } else if (cmd == 'f' || cmd == 'F') {
    // Show frequency activity summary
    Serial.println("\n📊 FREQUENCY ACTIVITY SUMMARY:");
    for (uint8_t i = 0; i < reconState.getNumConfigs(); i++) {
      const ScanConfig& cfg = reconState.getScanConfig(i);
      uint8_t deviceCount = 0;
      
      for (uint8_t j = 0; j < reconState.getNumTargetableDevices(); j++) {
        if (reconState.getTargetableDevice(j).configIndex == i) {
          deviceCount++;
        }
      }
      
      if (deviceCount > 0) {
        Serial.printf("🔥 %s: %d devices detected\n", cfg.protocol, deviceCount);
      }
    }
    
    Serial.println("\nPress any key to continue...");
    if (!waitForSerialInput()) {
      ModeManager modeManager;
      modeManager.logModeTransition(reconState.scanState.mode, MODE_INTERACTIVE_MENU, "FreqSummary:timeout");
      reconState.scanState.mode = MODE_INTERACTIVE_MENU;
      if (g_reconTool) {
          g_reconTool->setMenuModeEntered();
      }
      showReconResults();
      return;
    }
    Serial.read();
    showFrequencyTargetingMenu();
  } else if (cmd == 'r' || cmd == 'R') {
    ModeManager modeManager;
    modeManager.logModeTransition(reconState.scanState.mode, MODE_INTERACTIVE_MENU, "FreqMenu:return");
    reconState.scanState.mode = MODE_INTERACTIVE_MENU;
    if (g_reconTool) {
        g_reconTool->setMenuModeEntered();
    }
    showReconResults();
  } else {
    Serial.println("❌ Invalid selection");
    showFrequencyTargetingMenu();
  }
}

// Start targeted capture on specific frequency
void startFrequencyTargeting(uint8_t configIndex) {
  // Delegate to LoRaReconTool instance
  if (g_reconTool) {
    g_reconTool->startFrequencyTargeting(configIndex);
  } else {
    Serial.println("❌ System error: Tool not initialized");
  }
}