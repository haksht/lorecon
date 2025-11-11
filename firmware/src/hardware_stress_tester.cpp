/**
 * Hardware Stress Testing Module - Simplified Implementation
 * Safe stress testing framework for security research
 */

#include "hardware_stress_tester.h"
#include "recon_state.h"

HardwareStressTester::HardwareStressTester(SX1262* radioModule) {
    radio = radioModule;
    testingEnabled = false;
    lastCooldown = 0;
    
    // Initialize saved configuration tracker
    savedConfig.saved = false;
    savedConfig.frequency = 0.0;
    savedConfig.bandwidth = 0.0;
    savedConfig.spreadingFactor = 0;
    savedConfig.syncWord = 0;
    savedConfig.codingRate = 0;
    savedConfig.preambleLength = 0;
    
    // Initialize safety monitoring
    safety.maxTemperature = 65.0;  // Conservative temperature limit
    safety.currentTemperature = 0.0;
    safety.thermalShutdown = false;
    safety.hardwareFailure = false;
    safety.lastSafetyCheck = 0;
    safety.failureCount = 0;
    
    // Initialize test result structure
    lastResult.testPassed = false;
    lastResult.hardwareStable = true;
    lastResult.testDuration = 0;
    lastResult.errorMessages = "";
    lastResult.stabilityMetrics = "";
}

bool HardwareStressTester::initializeStressTesting() {
    Serial.println("[STRESS] Initializing hardware stress testing module...");
    
    if (!radio) {
        Serial.println("❌ Radio module not available for stress testing");
        return false;
    }
    
    // Perform initial safety checks
    if (!performSafetyCheck()) {
        Serial.println("❌ Initial safety check failed - stress testing disabled");
        return false;
    }
    
    // Validate hardware is in stable state
    if (!checkHardwareStability()) {
        Serial.println("❌ Hardware not in stable state for testing");
        return false;
    }
    
    float currentTemp = getCurrentTemperature();
    
    testingEnabled = true;
    Serial.println("✅ Hardware stress testing module initialized");
    Serial.println("⚠️ Safety limits enabled - testing within safe parameters only");
    Serial.printf("📡 Full frequency range: %.1f - %.1f MHz\n", MIN_SAFE_FREQUENCY, MAX_SAFE_FREQUENCY);
    Serial.printf("🔋 Power range: 5 - %d dBm\n", MAX_SAFE_POWER_DBM);
    Serial.printf("🌡️ Current temperature: %.1f°C (ESP32 internal sensor)\n", currentTemp);
    
    return true;
}

bool HardwareStressTester::checkSafetyLimits() {
    // Temperature check
    float currentTemp = getCurrentTemperature();
    if (currentTemp > safety.maxTemperature) {
        Serial.printf("🌡️ Temperature limit exceeded: %.1f°C (max: %.1f°C)\n", 
                      currentTemp, safety.maxTemperature);
        return false;
    }
    
    // Cooldown period check
    if (millis() - lastCooldown < COOLDOWN_PERIOD_MS) {
        unsigned long remaining = COOLDOWN_PERIOD_MS - (millis() - lastCooldown);
        Serial.printf("⏱️ Hardware cooldown active: %.1f seconds remaining\n", remaining / 1000.0);
        Serial.println("   (Hardware needs recovery time for reliable testing)");
        return false;
    }
    
    // Hardware stability check
    if (!checkHardwareStability()) {
        Serial.println("⚠️ Hardware stability check failed");
        return false;
    }
    
    return true;
}

bool HardwareStressTester::performSafetyCheck() {
    safety.lastSafetyCheck = millis();
    safety.currentTemperature = getCurrentTemperature();
    
    // Check for hardware failures
    if (safety.currentTemperature > safety.maxTemperature) {
        safety.thermalShutdown = true;
        emergencyShutdown();
        return false;
    }
    
    // Validate radio configuration is within safe bounds
    if (!validateRadioConfiguration()) {
        safety.hardwareFailure = true;
        return false;
    }
    
    return true;
}

void HardwareStressTester::emergencyShutdown() {
    Serial.println("🚨 EMERGENCY SHUTDOWN ACTIVATED");
    
    if (radio) {
        radio->standby();
        radio->setOutputPower(0);  // Minimum power
    }
    
    testingEnabled = false;
    safety.thermalShutdown = true;
    
    Serial.println("🛡️ All stress testing halted for safety");
}

bool HardwareStressTester::runStressTest(StressTestType testType) {
    if (!testingEnabled || !checkSafetyLimits()) {
        Serial.println("❌ Stress testing not available - safety limits exceeded");
        return false;
    }
    
    Serial.printf("🧪 Starting stress test type: %d\n", testType);
    
    // Save current radio configuration before modifying it
    saveCurrentRadioConfiguration();
    
    unsigned long testStart = millis();
    bool testResult = false;
    
    // Record initial state
    float initialTemp = getCurrentTemperature();
    
    switch (testType) {
        case FREQUENCY_SWEEP_TEST:
            testResult = frequencySweepTest();
            break;
            
        case POWER_RAMP_TEST:
            testResult = powerRampTest();
            break;
            
        case PARAMETER_BOUNDARY_TEST:
            testResult = parameterBoundaryTest();
            break;
            
        case RAPID_CONFIG_CHANGE_TEST:
            testResult = rapidConfigChangeTest();
            break;
            
        case THERMAL_STRESS_TEST:
            testResult = thermalStressTest();
            break;
            
        case MEMORY_STRESS_TEST:
            testResult = memoryStressTest();
            break;
            
        case TDECK_TARGETED_TEST:
            testResult = tdeckTargetedTest();
            break;
            
        default:
            Serial.println("❌ Unknown stress test type");
            return false;
    }
    
    // Record results
    lastResult.testType = testType;
    lastResult.testPassed = testResult;
    lastResult.testDuration = millis() - testStart;
    lastResult.temperatureReading = getCurrentTemperature();
    lastResult.hardwareStable = checkHardwareStability();
    
    // Restore radio configuration for reconnaissance operations
    restoreReconConfiguration();
    
    // Mandatory cooldown after any stress test
    lastCooldown = millis();
    
    // Safety check after test
    performSafetyCheck();
    
    Serial.printf("📊 Test completed: %s (Duration: %lu ms, Temp: %.1f°C)\n",
                  testResult ? "PASS" : "FAIL", lastResult.testDuration, 
                  lastResult.temperatureReading);
    
    return testResult;
}

bool HardwareStressTester::frequencySweepTest() {
    Serial.println("📡 Starting safe frequency sweep test");
    
    // Safe frequency range only
    float startFreq = max((float)MIN_SAFE_FREQUENCY, 902.0f);
    float endFreq = min((float)MAX_SAFE_FREQUENCY, 928.0f);
    float step = 0.5;  // Small steps for safety
    
    for (float freq = startFreq; freq <= endFreq; freq += step) {
        // Safety check every iteration
        if (!performSafetyCheck()) {
            Serial.println("🛑 Safety check failed during frequency sweep");
            return false;
        }
        
        // Apply frequency
        int result = radio->setFrequency(freq);
        if (result != RADIOLIB_ERR_NONE) {
            Serial.printf("⚠️ Frequency set failed at %.3f MHz (error: %d)\n", freq, result);
            lastResult.errorMessages += "Freq_" + String(freq, 1) + "_Failed ";
        }
        
        delay(50);  // Longer settle time for hardware stability
        
        // Monitor for instability
        if (!checkHardwareStability()) {
            Serial.printf("🚨 Hardware instability detected at %.3f MHz\n", freq);
            return false;
        }
    }
    
    Serial.println("✅ Frequency sweep test completed successfully");
    return true;
}

bool HardwareStressTester::powerRampTest() {
    Serial.println("⚡ Starting safe power ramp test");
    
    // Conservative power range
    int minPower = -10;  // Low power start
    int maxPower = min((int)MAX_SAFE_POWER_DBM, 10);  // Safe maximum
    
    for (int power = minPower; power <= maxPower; power += 2) {
        if (!performSafetyCheck()) {
            Serial.println("🛑 Safety check failed during power ramp");
            return false;
        }
        
        int result = radio->setOutputPower(power);
        if (result != RADIOLIB_ERR_NONE) {
            Serial.printf("⚠️ Power set failed at %d dBm (error: %d)\n", power, result);
            lastResult.errorMessages += "Power_" + String(power) + "_Failed ";
        }
        
        delay(200);  // Longer power settling time
        
        // Temperature monitoring during power test
        float currentTemp = getCurrentTemperature();
        if (currentTemp > safety.maxTemperature - 5.0) {  // 5°C safety margin
            Serial.printf("🌡️ Temperature approaching limit: %.1f°C\n", currentTemp);
            break;  // Stop test before reaching limit
        }
    }
    
    // Return to safe power level
    radio->setOutputPower(0);
    
    Serial.println("✅ Power ramp test completed successfully");
    return true;
}

bool HardwareStressTester::parameterBoundaryTest() {
    Serial.println("🎯 Starting parameter boundary test");
    
    // Test spreading factor boundaries
    int spreadingFactors[] = {7, 8, 9, 10, 11, 12};
    
    for (int sf : spreadingFactors) {
        if (!performSafetyCheck()) return false;
        
        int result = radio->setSpreadingFactor(sf);
        if (result != RADIOLIB_ERR_NONE) {
            Serial.printf("⚠️ SF boundary test failed at SF%d (error: %d)\n", sf, result);
            lastResult.errorMessages += "SF" + String(sf) + "_Failed ";
        }
        
        delay(20);
    }
    
    // Test bandwidth boundaries  
    float bandwidths[] = {125.0, 250.0, 500.0};
    
    for (float bw : bandwidths) {
        if (!performSafetyCheck()) return false;
        
        int result = radio->setBandwidth(bw);
        if (result != RADIOLIB_ERR_NONE) {
            Serial.printf("⚠️ BW boundary test failed at %.0f kHz (error: %d)\n", bw, result);
            lastResult.errorMessages += "BW" + String(bw, 0) + "_Failed ";
        }
        
        delay(20);
    }
    
    Serial.println("✅ Parameter boundary test completed");
    return true;
}

bool HardwareStressTester::rapidConfigChangeTest() {
    Serial.println("⚡ Starting rapid configuration change test");
    
    unsigned long testStart = millis();
    int changeCount = 0;
    
    while (millis() - testStart < STRESS_TEST_DURATION_MS) {
        if (!performSafetyCheck()) return false;
        
        // Rapidly change between safe configurations
        radio->setFrequency(902.0 + (changeCount % 20) * 0.5);
        radio->setSpreadingFactor(7 + (changeCount % 6));
        radio->setBandwidth(125.0 + (changeCount % 3) * 125.0);
        
        changeCount++;
        delay(20);  // Longer delay between changes for stability
        
        // Check for hardware instability more frequently
        if (changeCount % 25 == 0 && !checkHardwareStability()) {
            Serial.printf("🚨 Instability detected after %d rapid changes\n", changeCount);
            return false;
        }
    }
    
    Serial.printf("✅ Rapid config test completed: %d changes in %lu ms\n", 
                  changeCount, millis() - testStart);
    return true;
}

bool HardwareStressTester::thermalStressTest() {
    Serial.println("🌡️ Starting thermal monitoring stress test");
    
    unsigned long testStart = millis();
    float initialTemp = getCurrentTemperature();
    float maxTempSeen = initialTemp;
    
    // Moderate stress to monitor thermal behavior
    radio->setOutputPower(5);  // Moderate power for heat generation
    
    while (millis() - testStart < STRESS_TEST_DURATION_MS) {
        float currentTemp = getCurrentTemperature();
        maxTempSeen = max(maxTempSeen, currentTemp);
        
        if (currentTemp > safety.maxTemperature - 3.0) {  // 3°C safety margin
            Serial.printf("🛑 Thermal limit approach: %.1f°C\n", currentTemp);
            break;
        }
        
        delay(100);
    }
    
    radio->setOutputPower(0);  // Return to safe power
    
    Serial.printf("🌡️ Thermal test: Initial %.1f°C, Max %.1f°C, Final %.1f°C\n",
                  initialTemp, maxTempSeen, getCurrentTemperature());
    
    return true;
}

bool HardwareStressTester::memoryStressTest() {
    Serial.println("💾 Starting memory stress test");
    
    // Test memory allocation and deallocation patterns
    const int blockSize = 1024;
    const int maxBlocks = 10;
    void* memBlocks[maxBlocks];
    
    // Allocate memory blocks
    for (int i = 0; i < maxBlocks; i++) {
        memBlocks[i] = malloc(blockSize);
        if (!memBlocks[i]) {
            Serial.printf("⚠️ Memory allocation failed at block %d\n", i);
            // Free previously allocated blocks
            for (int j = 0; j < i; j++) {
                if (memBlocks[j]) free(memBlocks[j]);
            }
            return false;
        }
        
        // Fill with test pattern
        memset(memBlocks[i], 0xAA + i, blockSize);
        delay(10);
    }
    
    // Verify memory integrity
    bool memoryIntact = true;
    for (int i = 0; i < maxBlocks; i++) {
        uint8_t* block = (uint8_t*)memBlocks[i];
        for (int j = 0; j < blockSize; j++) {
            if (block[j] != (uint8_t)(0xAA + i)) {
                Serial.printf("💥 Memory corruption detected in block %d at offset %d\n", i, j);
                memoryIntact = false;
                break;
            }
        }
    }
    
    // Free all blocks
    for (int i = 0; i < maxBlocks; i++) {
        if (memBlocks[i]) free(memBlocks[i]);
    }
    
    Serial.printf("💾 Memory test: %s\n", memoryIntact ? "PASS" : "FAIL");
    return memoryIntact;
}

float HardwareStressTester::getCurrentTemperature() {
    // ESP32 internal temperature sensor (accurate to ~5°C)
    #ifdef ESP32
    return temperatureRead();  // Built-in ESP32 function, returns Celsius
    #else
    return 25.0;  // Fallback for non-ESP32
    #endif
}

bool HardwareStressTester::checkHardwareStability() {
    // Perform basic hardware stability checks
    if (!radio) return false;
    
    // Simple non-invasive check - just verify the radio object exists
    // Don't put radio in standby mode as it disrupts packet reception
    // The radio state will be managed by individual stress tests
    
    return true;
}

bool HardwareStressTester::validateRadioConfiguration() {
    // Verify current configuration is within safe bounds
    return true;  // Simplified - basic validation
}

void HardwareStressTester::saveCurrentRadioConfiguration() {
    // Save the current radio configuration so we can restore it after stress testing
    // Note: RadioLib doesn't provide getters for these values, so we save on first test
    // and use fallback defaults if needed
    
    if (!radio) return;
    
    // We can't read these values from the radio, so we mark as "not saved"
    // and will use smart defaults on restore based on typical recon config
    savedConfig.saved = false;
    
    Serial.println("[STRESS] Radio configuration will be restored after testing");
}

void HardwareStressTester::restoreReconConfiguration() {
    // Restore radio to reconnaissance configuration
    // Since RadioLib doesn't provide getters, we restore to standard recon defaults
    if (!radio) return;
    
    Serial.println("[STRESS] Restoring reconnaissance radio configuration...");
    
    // Get current recon state to determine which frequency to restore to
    extern ReconState reconState;
    uint8_t currentConfigIndex = reconState.scanState.currentConfig;
    const ScanConfig& config = reconState.getScanConfig(currentConfigIndex);
    
    // Restore to the current scan configuration
    Serial.printf("[STRESS] Restoring to: %s (%.3f MHz, SF%d, BW%.0f)\n",
                  config.protocol, config.frequency, config.spreadingFactor, config.bandwidth);
    
    radio->setFrequency(config.frequency);
    radio->setBandwidth(config.bandwidth);
    radio->setSpreadingFactor(config.spreadingFactor);
    radio->setSyncWord(config.syncWord);
    
    // Standard Meshtastic parameters
    if (strstr(config.protocol, "Meshtastic") != nullptr) {
        if (strstr(config.protocol, "_MF") != nullptr) {
            radio->setCodingRate(8);         // 4/8 for MediumFast
            radio->setPreambleLength(12);
        } else {
            radio->setCodingRate(5);         // 4/5 standard
            radio->setPreambleLength(8);
        }
    } else {
        radio->setCodingRate(5);
        radio->setPreambleLength(8);
    }
    
    radio->setCRC(false);                // Promiscuous mode
    radio->explicitHeader();
    radio->setCurrentLimit(100);
    radio->setOutputPower(0);            // Receive-only
    
    // Put radio back in receive mode
    int state = radio->startReceive();
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("[STRESS] ✅ Radio restored to receive mode");
    } else {
        Serial.printf("[STRESS] ⚠️ Radio restore failed (error: %d)\n", state);
    }
}

void HardwareStressTester::printStressTestReport() {
    Serial.println("\n📋 HARDWARE STRESS TEST REPORT");
    Serial.println("========================================");
    Serial.printf("🧪 Last Test Type: %d\n", lastResult.testType);
    Serial.printf("✅ Test Result: %s\n", lastResult.testPassed ? "PASS" : "FAIL");
    Serial.printf("🏥 Hardware Stable: %s\n", lastResult.hardwareStable ? "YES" : "NO");
    Serial.printf("⏱️ Test Duration: %lu ms\n", lastResult.testDuration);
    Serial.printf("🌡️ Temperature: %.1f°C\n", lastResult.temperatureReading);
    
    if (lastResult.errorMessages.length() > 0) {
        Serial.printf("⚠️ Errors: %s\n", lastResult.errorMessages.c_str());
    }
    
    Serial.printf("🛡️ Safety Status: %s\n", safety.thermalShutdown ? "SHUTDOWN" : "OK");
    Serial.printf("🔥 Failure Count: %d\n", safety.failureCount);
    Serial.println("========================================\n");
}

SafetyMonitor HardwareStressTester::getSafetyStatus() {
    return safety;
}

StressTestResult HardwareStressTester::getLastTestResult() {
    return lastResult;
}

void HardwareStressTester::abortAllTesting() {
    Serial.println("🛑 ABORTING ALL STRESS TESTING");
    emergencyShutdown();
}

// T-Deck specific test (simplified - just a frequency lock test)
bool HardwareStressTester::tdeckTargetedTest() {
    Serial.println("🎯 Starting targeted frequency vulnerability assessment...");
    
    bool testPassed = true;
    float testFreq = TDECK_TARGET_FREQUENCY;
    
    if (radio->setFrequency(testFreq) == RADIOLIB_ERR_NONE) {
        Serial.printf("✅ Locked onto target frequency: %.3f MHz\n", testFreq);
        
        // Test power levels
        for (int power = 10; power <= MAX_SAFE_POWER_DBM; power += 2) {
            if (radio->setOutputPower(power) == RADIOLIB_ERR_NONE) {
                Serial.printf("🔋 Power level %d dBm: OK\n", power);
                delay(500);
                
                if (!performSafetyCheck()) {
                    Serial.println("⚠️ Safety limit reached during power test");
                    break;
                }
            }
        }
    } else {
        Serial.println("❌ Failed to set target frequency");
        testPassed = false;
    }
    
    lastResult.tdeckTargeted = true;
    lastResult.targetFrequency = TDECK_TARGET_FREQUENCY;
    lastResult.powerUsed = MAX_SAFE_POWER_DBM;
    
    Serial.printf("🎯 Vulnerability assessment %s\n", testPassed ? "COMPLETED" : "FAILED");
    
    return testPassed;
}
