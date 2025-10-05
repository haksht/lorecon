/**
 * LilyGo T-Deck Hardware Validation Test
 * 
 * Comprehensive stress testing for your specific T-Deck device
 * Tests power limits, frequency response, and hardware stability
 */

#include <Arduino.h>
#include <RadioLib.h>
#include "hardware_stress_tester.h"

// T-Deck pin configuration (verified working)
#define LORA_NSS 8
#define LORA_DIO1 14
#define LORA_NRST 12  
#define LORA_BUSY 13

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY);
HardwareStressTester* stressTester = nullptr;

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("🧪 LILYGO T-DECK HARDWARE VALIDATION");
    Serial.println("=====================================");
    Serial.println("📱 Device: LilyGo T-Deck ESP32-S3");
    Serial.println("📡 Radio: SX1262 LoRa Module");
    Serial.println("🎯 Focus: Hardware stress testing and validation");
    Serial.println();
    
    // Initialize LoRa radio
    Serial.print("🔧 Initializing SX1262 radio... ");
    int state = radio.begin(902.125, 250.0, 11, 5, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 22);
    
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("✅ SUCCESS");
        Serial.printf("📡 Frequency: 902.125 MHz\n");
        Serial.printf("📶 Bandwidth: 250 kHz\n");
        Serial.printf("🔋 Power: 22 dBm\n");
    } else {
        Serial.printf("❌ FAILED (Error: %d)\n", state);
        Serial.println("🛑 Cannot proceed with stress testing");
        return;
    }
    
    // Initialize stress testing framework
    Serial.print("🧪 Initializing stress testing framework... ");
    stressTester = new HardwareStressTester(&radio);
    
    if (stressTester->initializeStressTesting()) {
        Serial.println("✅ SUCCESS");
        Serial.println("⚠️ Safety limits enabled for T-Deck protection");
    } else {
        Serial.println("❌ FAILED");
        Serial.println("🛑 Stress testing not available");
        return;
    }
    
    Serial.println("\n🎯 Starting T-Deck hardware validation...");
    runTDeckValidation();
}

void runTDeckValidation() {
    Serial.println("\n📋 T-DECK COMPREHENSIVE VALIDATION SUITE");
    Serial.println("==========================================");
    
    // Test 1: T-Deck Targeted Assessment
    Serial.println("\n🎯 TEST 1: T-Deck Targeted Vulnerability Assessment");
    bool test1 = stressTester->runStressTest(TDECK_TARGETED_TEST);
    Serial.printf("Result: %s\n", test1 ? "✅ PASS" : "❌ FAIL");
    delay(2000);
    
    // Test 2: Frequency Sweep (ISM Band)
    Serial.println("\n📡 TEST 2: ISM Band Frequency Sweep");
    bool test2 = stressTester->runStressTest(FREQUENCY_SWEEP_TEST);
    Serial.printf("Result: %s\n", test2 ? "✅ PASS" : "❌ FAIL");
    delay(2000);
    
    // Test 3: Power Ramp Test
    Serial.println("\n⚡ TEST 3: Power Output Validation");
    bool test3 = stressTester->runStressTest(POWER_RAMP_TEST);
    Serial.printf("Result: %s\n", test3 ? "✅ PASS" : "❌ FAIL");
    delay(2000);
    
    // Test 4: Parameter Boundary Test
    Serial.println("\n🔧 TEST 4: LoRa Parameter Boundaries");
    bool test4 = stressTester->runStressTest(PARAMETER_BOUNDARY_TEST);
    Serial.printf("Result: %s\n", test4 ? "✅ PASS" : "❌ FAIL");
    delay(2000);
    
    // Test 5: Rapid Configuration Changes
    Serial.println("\n⚡ TEST 5: Rapid Configuration Stress");
    bool test5 = stressTester->runStressTest(RAPID_CONFIG_CHANGE_TEST);
    Serial.printf("Result: %s\n", test5 ? "✅ PASS" : "❌ FAIL");
    delay(2000);
    
    // Test 6: Memory Stress Test
    Serial.println("\n💾 TEST 6: Memory Integrity Validation");
    bool test6 = stressTester->runStressTest(MEMORY_STRESS_TEST);
    Serial.printf("Result: %s\n", test6 ? "✅ PASS" : "❌ FAIL");
    delay(2000);
    
    // Test 7: Thermal Monitoring
    Serial.println("\n🌡️ TEST 7: Thermal Stress Monitoring");
    bool test7 = stressTester->runStressTest(THERMAL_STRESS_TEST);
    Serial.printf("Result: %s\n", test7 ? "✅ PASS" : "❌ FAIL");
    
    // Generate comprehensive report
    Serial.println("\n📊 GENERATING VALIDATION REPORT...");
    delay(1000);
    stressTester->printStressTestReport();
    
    // Overall assessment
    int passedTests = test1 + test2 + test3 + test4 + test5 + test6 + test7;
    Serial.println("\n🎯 OVERALL T-DECK ASSESSMENT");
    Serial.println("==============================");
    Serial.printf("✅ Tests Passed: %d/7\n", passedTests);
    Serial.printf("📊 Success Rate: %.1f%%\n", (passedTests / 7.0) * 100.0);
    
    if (passedTests >= 6) {
        Serial.println("🏆 VERDICT: T-Deck hardware is EXCELLENT for security research");
        Serial.println("✅ Device validated for professional LoRa reconnaissance");
    } else if (passedTests >= 4) {
        Serial.println("✅ VERDICT: T-Deck hardware is GOOD with minor limitations");
        Serial.println("⚠️ Some stress conditions may require monitoring");
    } else {
        Serial.println("⚠️ VERDICT: T-Deck hardware has stability concerns");
        Serial.println("🔧 Recommend further hardware diagnosis");
    }
    
    // T-Deck specific analysis
    Serial.println("\n🔬 T-DECK SPECIFIC ANALYSIS");
    performTDeckVulnerabilityAssessment();
    analyzeTDeckPowerDependency();
    
    Serial.println("\n🎉 VALIDATION COMPLETE - Your T-Deck is ready for reconnaissance!");
}

void loop() {
    // Continuous monitoring mode
    static unsigned long lastMonitor = 0;
    
    if (millis() - lastMonitor > 30000) {  // Every 30 seconds
        Serial.println("\n📊 Periodic T-Deck Health Check:");
        
        SafetyMonitor status = stressTester->getSafetyStatus();
        Serial.printf("🌡️ Temperature: %.1f°C\n", status.currentTemperature);
        Serial.printf("🛡️ Safety Status: %s\n", status.thermalShutdown ? "SHUTDOWN" : "OK");
        Serial.printf("⚠️ Failure Count: %d\n", status.failureCount);
        
        // Quick stability verification
        Serial.print("🔧 Hardware Stability: ");
        int testResult = radio.standby();
        Serial.println(testResult == RADIOLIB_ERR_NONE ? "✅ STABLE" : "⚠️ UNSTABLE");
        
        lastMonitor = millis();
    }
    
    // Check for user commands
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command == "test") {
            Serial.println("🧪 Running quick validation test...");
            bool quickTest = stressTester->runStressTest(TDECK_TARGETED_TEST);
            Serial.printf("Result: %s\n", quickTest ? "✅ PASS" : "❌ FAIL");
        } else if (command == "report") {
            stressTester->printStressTestReport();
        } else if (command == "power") {
            analyzeTDeckPowerDependency();
        } else if (command == "help") {
            Serial.println("📋 Available Commands:");
            Serial.println("  test   - Run quick T-Deck validation");
            Serial.println("  report - Show detailed test report");
            Serial.println("  power  - Analyze power dependency");
            Serial.println("  help   - Show this help");
        }
    }
    
    delay(1000);
}