/**
 * User Interface Module Implementation
 * 
 * Contains all display functions, menus, and user interaction logic
 * extracted from main.cpp for better code organization.
 */

#include "user_interface.h"
#include "lora_recon_tool.h"
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

#ifdef ENABLE_STRESS_TESTING
#include "hardware_stress_tester.h"
#endif

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
  
  for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
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
      strcpy(stat->type, dev.deviceType);
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
    float avgRSSI = stat.totalRSSI / stat.count;
    float avgPowerClass = (float)stat.powerClassSum / stat.count;
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
  
  if (reconState.numTargetableDevices == 0) {
    Serial.println("No devices available for assessment.");
    Serial.print("\nPress any key to continue: ");
    waitForSerialInput();
    Serial.read();
    return;
  }
  
  Serial.println("Device                  | Security Score | Vulnerabilities");
  Serial.println("------------------------|----------------|------------------");
  
  uint16_t totalScore = 0;
  uint8_t vulnerableCount = 0;
  
  for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
    const TargetableDevice& dev = reconState.getTargetableDevice(i);
    
    // Calculate security score (100 = perfect, 0 = critical)
    uint8_t score = 100;
    String vulnerabilities = "";
    
    // Check 1: Signal strength (too strong = physical security risk)
    if (dev.bestRSSI > -50) {
      score -= 15;
      vulnerabilities += "Physical ";
    }
    
    // Check 2: Encryption status (would need PSK test results)
    // Placeholder for when PSK testing is validated
    bool maybeUnencrypted = (dev.packetCount > 10 && strcmp(dev.protocol, "Meshtastic") == 0);
    if (maybeUnencrypted) {
      // Will be updated once PSK testing works
      vulnerabilities += "Crypto? ";
    }
    
    // Check 3: Routing device (higher attack surface)
    if (dev.isRouter) {
      score -= 10;
      vulnerabilities += "Router ";
    }
    
    // Check 4: High packet count (chatty = info leakage)
    if (dev.packetCount > 100) {
      score -= 15;
      vulnerabilities += "Chatty ";
    }
    
    // Check 5: Low packet count (intermittent = weak battery?)
    if (dev.packetCount < 5) {
      score -= 5;
      vulnerabilities += "Weak ";
    }
    
    // Check 6: Firmware version check
    if (strstr(dev.firmwareVersion, "v1.x") != nullptr || 
        strstr(dev.firmwareVersion, "v2.0") != nullptr) {
      score -= 20;
      vulnerabilities += "OldFW ";
    }
    
    if (vulnerabilities.length() == 0) {
      vulnerabilities = "None detected";
    }
    
    // Rating
    const char* rating;
    if (score >= 80) {
      rating = "✅ SECURE  ";
      } else if (score >= 60) {
      rating = "⚠️ MODERATE";
    } else {
      rating = "🚨 VULNERABLE";
      vulnerableCount++;
    }
    
    Serial.printf("0x%08X (%8s) | %3d/100 %s | %s\n",
                  dev.nodeId, dev.deviceType, score, rating,
                  vulnerabilities.c_str());
    
    totalScore += score;
  }
  
  Serial.println("------------------------|----------------|------------------");
  
  // Summary
  uint8_t avgScore = totalScore / reconState.numTargetableDevices;
  Serial.printf("\n📊 OVERALL ASSESSMENT:\n");
  Serial.printf("   Average Score: %d/100\n", avgScore);
  Serial.printf("   Vulnerable Devices: %d/%d\n", vulnerableCount, reconState.numTargetableDevices);
  
  if (avgScore >= 80) {
    Serial.println("   Status: ✅ Network appears well-secured");
  } else if (avgScore >= 60) {
    Serial.println("   Status: ⚠️ Some security concerns detected");
  } else {
    Serial.println("   Status: 🚨 Multiple vulnerabilities found");
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
  Serial.println("\nDETAILED RF ACTIVITY ANALYSIS:");
  Serial.println("Configuration            | Frequency | SF | BW  | Activity | Signals | Age");
  Serial.println("-------------------------|-----------|----|----|----------|---------|-------");
  
  bool anyActivity = false;
  for (uint8_t i = 0; i < reconState.getNumConfigs(); i++) {
    const RFActivity& activity = reconState.getRFActivity(i);
    if (activity.activityCount > 0) {
      anyActivity = true;
      uint32_t ageSeconds = (millis() - activity.lastActivity) / 1000;
      
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
  }
  
  Serial.println("\n💡 INTERPRETATION:");
  Serial.println("• HIGH activity: Strong nearby devices, good targeting potential");
  Serial.println("• MEDIUM activity: Moderate signals, devices may be distant");
  Serial.println("• LOW activity: Weak signals or intermittent transmissions");
  Serial.println("\nℹ️  Only devices with decoded node IDs appear in targetable list.");
  
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
  Serial.printf("Targetable Devices: %d\n", reconState.numTargetableDevices);
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
  if (reconState.numTargetableDevices == 0) {
    Serial.println("TARGETABLE DEVICES: None\n");
    Serial.println("ℹ️  No devices with decoded packets found.");
    Serial.println("   RF activity detected above, but no targetable node IDs captured.");
    Serial.println("   Press 'r' to restart reconnaissance or wait longer for packet capture.");
  } else {
    Serial.println("TARGETABLE DEVICES (Confirmed Node IDs):");
    Serial.println("ID | Device Type          | Node ID    | Protocol     | RSSI  | Pkts | Router");
    Serial.println("---|----------------------|------------|--------------|-------|------|-------");
    
    for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
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
  if (reconState.numTargetableDevices > 0) {
    Serial.println("1-" + String(reconState.numTargetableDevices) + " : Target device for capture");
  }
  Serial.println("a   : Show activity details");
  Serial.println("d   : Show device type analysis");
  Serial.println("f   : Frequency targeting (skip device ID)");
  Serial.println("g   : Geographic intelligence (GPS data)");
  Serial.println("k   : Export KML (Google Earth format)");
  Serial.println("j   : Export GeoJSON (web mapping)");
  Serial.println("p   : Packet replay menu");
  Serial.println("r   : Restart reconnaissance");
  Serial.println("s   : Show summary again");
  Serial.println("v   : Security vulnerability assessment");
#ifdef ENABLE_STRESS_TESTING
  Serial.println("t   : Hardware stress testing");
#endif
  Serial.print("\nSelect target (1-" + String(reconState.numTargetableDevices) + ") or command: ");
}

// Print scanning statistics
void printStats() {
  Serial.println("\n=== RECONNAISSANCE SUMMARY ===");
  Serial.printf("Total packets: %d\n", reconState.scanState.totalPackets);
  Serial.printf("Targetable devices: %d\n", reconState.numTargetableDevices);
  Serial.printf("Active nodes: %d\n", reconState.nodeCount);
  Serial.printf("Current: %s\n", reconState.getScanConfig(reconState.scanState.currentConfig).protocol);
  
  if (reconState.numTargetableDevices > 0) {
    Serial.println("\n--- Targetable Devices ---");
    for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
      const TargetableDevice& dev = reconState.getTargetableDevice(i);
      uint32_t ageMs = millis() - dev.lastSeen;
      Serial.printf("0x%08X (%s): %d pkts, %.1f dBm avg, %ds ago\n",
                    dev.nodeId, dev.protocol, dev.packetCount,
                    dev.avgRSSI, ageMs / 1000);
    }
  }
  
  #ifdef ENABLE_PSK_TESTING
  if (pskStats.attempts > 0) {
    Serial.println("\n--- PSK Testing Stats ---");
    Serial.printf("Attempts: %d, Successes: %d (%.1f%%)\n", 
                  pskStats.attempts, pskStats.successes,
                  (float)pskStats.successes / pskStats.attempts * 100.0);
    
    for (uint8_t i = 0; i < 5; i++) { // NUM_DEFAULT_PSKS
      if (pskStats.hitCount[i] > 0) {
        Serial.printf("  Key #%d: %d hits\n", i + 1, pskStats.hitCount[i]);
      }
    }
  }
  #endif
  
  Serial.println("==============================\n");
}

void displayWelcomeMessage() {
  Serial.println("ESP32 LoRa Reconnaissance Tool");
  Serial.println("==============================");
  Serial.println("Hardware: ESP32-S3 + SX1262 LoRa");
  Serial.printf("Scanning %d configurations\n", reconState.getNumConfigs());
  
  #ifdef ENABLE_PSK_TESTING
  Serial.printf("PSK testing: ENABLED (%d default keys)\n", 5); // NUM_DEFAULT_PSKS
  #else
  Serial.println("PSK testing: DISABLED");
  #endif
  
  Serial.println();
}

void displayReconStartMessage() {
  Serial.println("=== STARTING RECONNAISSANCE PHASE ===");
  Serial.println("Scanning all frequencies to detect active devices...");
  Serial.println("This will take approximately 2-3 minutes.");
  Serial.println("Press 'm' anytime to enter menu mode.");
  Serial.println();
}

#ifdef ENABLE_STRESS_TESTING
// Hardware stress testing menu
void showStressTestMenu() {
  Serial.println("\n🧪 HARDWARE STRESS TESTING");
  Serial.println("============================");
  Serial.println("⚠️ Safety limits enabled - testing within safe parameters");
  Serial.println();
  
  // Initialize stress tester if not already done
  if (g_reconTool) {
    g_reconTool->ensureStressTesterInitialized();
  }
  
  // Get stress tester from tool instance  
  HardwareStressTester* stressTester = g_reconTool ? g_reconTool->getStressTester() : nullptr;
  
  if (!stressTester) {
    Serial.println("❌ Stress testing not available");
    return;
  }
  
  Serial.print("🔧 Initializing stress testing framework... ");
  if (stressTester->initializeStressTesting()) {
    Serial.println("✅ SUCCESS");
  } else {
    Serial.println("❌ FAILED");
    Serial.println("Stress testing not available - returning to main menu");
    return;
  }
  
  Serial.println("TEST OPTIONS:");
  Serial.println("1 : T-Deck targeted assessment");
  Serial.println("2 : Frequency sweep test");
  Serial.println("3 : Power ramp validation");
  Serial.println("4 : Parameter boundary test");
  Serial.println("5 : Rapid config stress test");
  Serial.println("6 : Memory integrity test");
  Serial.println("7 : Thermal monitoring test");
  Serial.println("8 : Full validation suite");
  Serial.println("9 : Show last test report");
  Serial.println("r : Return to main menu");
  Serial.print("\nSelect test (1-9) or command: ");
}

// Execute selected stress test
void runHardwareStressTest(char testChoice) {
  bool testResult = false;
  
  // Get stress tester from tool instance
  HardwareStressTester* stressTester = g_reconTool ? g_reconTool->getStressTester() : nullptr;
  
  if (!stressTester) {
    Serial.println("❌ Stress tester not initialized");
    return;
  }
  
  switch (testChoice) {
    case '1':
      Serial.println("\n🎯 Running T-Deck Targeted Assessment...");
      testResult = stressTester->runStressTest(TDECK_TARGETED_TEST);
      break;
      
    case '2':
      Serial.println("\n📡 Running Frequency Sweep Test...");
      testResult = stressTester->runStressTest(FREQUENCY_SWEEP_TEST);
      break;
      
    case '3':
      Serial.println("\n⚡ Running Power Ramp Test...");
      testResult = stressTester->runStressTest(POWER_RAMP_TEST);
      break;
      
    case '4':
      Serial.println("\n🔧 Running Parameter Boundary Test...");
      testResult = stressTester->runStressTest(PARAMETER_BOUNDARY_TEST);
      break;
      
    case '5':
      Serial.println("\n⚡ Running Rapid Config Stress Test...");
      testResult = stressTester->runStressTest(RAPID_CONFIG_CHANGE_TEST);
      break;
      
    case '6':
      Serial.println("\n💾 Running Memory Integrity Test...");
      testResult = stressTester->runStressTest(MEMORY_STRESS_TEST);
      break;
      
    case '7':
      Serial.println("\n🌡️ Running Thermal Monitoring Test...");
      testResult = stressTester->runStressTest(THERMAL_STRESS_TEST);
      break;
      
    case '8':
      {
        Serial.println("\n🧪 Running Full Validation Suite...");
        Serial.println("This will run all tests sequentially...");
        
        // Run all tests with proper cooldown management
        int passedTests = 0;
        
        // Helper function for cooldown with countdown
        auto waitForCooldown = [](int seconds) {
          Serial.printf("⏱️ Hardware cooldown: ");
          for (int i = seconds; i > 0; i--) {
            Serial.printf("%d ", i);
            Serial.flush();
            delay(1000);
          }
          Serial.println("✅ Ready");
        };
        
        // Initial cooldown to ensure hardware is ready
        Serial.println("🔧 Preparing hardware for validation suite...");
        waitForCooldown(5);
        
        Serial.println("\n1/7: T-Deck Assessment");
        if (stressTester->runStressTest(TDECK_TARGETED_TEST)) passedTests++;
        waitForCooldown(30);
        
        Serial.println("2/7: Frequency Sweep");
        if (stressTester->runStressTest(FREQUENCY_SWEEP_TEST)) passedTests++;
        waitForCooldown(30);
        
        Serial.println("3/7: Power Validation");
        if (stressTester->runStressTest(POWER_RAMP_TEST)) passedTests++;
        waitForCooldown(30);
        
        Serial.println("4/7: Parameter Boundaries");
        if (stressTester->runStressTest(PARAMETER_BOUNDARY_TEST)) passedTests++;
        waitForCooldown(30);
        
        Serial.println("5/7: Rapid Config Stress");
        if (stressTester->runStressTest(RAPID_CONFIG_CHANGE_TEST)) passedTests++;
        waitForCooldown(30);
        
        Serial.println("6/7: Memory Integrity");
        if (stressTester->runStressTest(MEMORY_STRESS_TEST)) passedTests++;
        waitForCooldown(30);
        
        Serial.println("7/7: Thermal Monitoring");
        if (stressTester->runStressTest(THERMAL_STRESS_TEST)) passedTests++;
        
        Serial.printf("\n🎯 VALIDATION COMPLETE: %d/7 tests passed (%.1f%%)\n", 
                      passedTests, (passedTests / 7.0) * 100.0);
        
        if (passedTests >= 6) {
          Serial.println("🏆 VERDICT: Hardware excellent for security research");
        } else if (passedTests >= 4) {
          Serial.println("✅ VERDICT: Hardware good with minor limitations");
        } else {
          Serial.println("⚠️ VERDICT: Hardware stability concerns detected");
        }
        
        testResult = (passedTests >= 4);
      }
      break;
      
    case '9':
      Serial.println("\n📊 Last Test Report:");
      stressTester->printStressTestReport();
      Serial.println("\nPress any key to continue...");
      if (!waitForSerialInput()) return;  // Timeout returns to menu
      Serial.read();
      // Note: Menu handling done by caller
      return;
      
    case 'r':
    case 'R':
      // Note: Mode switching handled by caller
      return;
      
    default:
      Serial.println("❌ Invalid selection");
      delay(1000);
      // Note: Menu re-display handled by caller
      return;
  }
  
  // Show test result
  Serial.printf("\n📊 Test Result: %s\n", testResult ? "✅ PASS" : "❌ FAIL");
  
  // Show safety status
  SafetyMonitor status = stressTester->getSafetyStatus();
  Serial.printf("🛡️ Safety Status: %s\n", status.thermalShutdown ? "⚠️ SHUTDOWN" : "✅ OK");
  Serial.printf("🌡️ Temperature: %.1f°C\n", status.currentTemperature);
  
  Serial.println("\nPress any key to return to stress test menu...");
  if (!waitForSerialInput()) return;  // Timeout returns to menu
  Serial.read();
  
  // Note: Menu re-display handled by caller
}
#endif





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
    for (uint8_t j = 0; j < reconState.numTargetableDevices; j++) {
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
    reconState.scanState.mode = MODE_INTERACTIVE_MENU;
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
      
      for (uint8_t j = 0; j < reconState.numTargetableDevices; j++) {
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
      reconState.scanState.mode = MODE_INTERACTIVE_MENU;
      showReconResults();
      return;
    }
    Serial.read();
    showFrequencyTargetingMenu();
  } else if (cmd == 'r' || cmd == 'R') {
    reconState.scanState.mode = MODE_INTERACTIVE_MENU;
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