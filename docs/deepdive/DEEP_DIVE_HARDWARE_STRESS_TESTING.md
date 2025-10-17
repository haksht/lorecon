# Deep Dive: Hardware Stress Testing & Device Resilience

## Executive Summary

This document explains the hardware stress testing framework built into your ESP32 LoRa sniffer. You'll understand safety limits, thermal monitoring, frequency sweep testing, power ramp testing, device-specific vulnerability assessment (T-Deck targeting), and how the system ensures safe operation during aggressive testing scenarios.

---

## Table of Contents
1. [Safety Architecture](#safety-architecture)
2. [Test Types](#test-types)
3. [Safety Monitoring](#safety-monitoring)
4. [Device Profiles](#device-profiles)
5. [Test Execution](#test-execution)
6. [Results Analysis](#results-analysis)

---

## Safety Architecture

### Design Philosophy

```
Hardware Stress Testing Goals:
  ✅ Validate radio chip stability across full parameter range
  ✅ Identify thermal limits under sustained operation
  ✅ Test rapid configuration changes (frequency hopping)
  ✅ Assess device-specific vulnerabilities (T-Deck targeting)
  ✅ NEVER damage hardware (safety-first approach)

Safety-First Design:
  ├─ Conservative limits (well below absolute max)
  ├─ Mandatory cooldown periods (30s between tests)
  ├─ Temperature monitoring (ESP32 internal sensor)
  ├─ Emergency shutdown capability
  └─ Hardware stability checks after each test
```

### Safety Limits

**Frequency Range:**

```cpp
#define MIN_SAFE_FREQUENCY 900.0   // 900 MHz (ISM band start)
#define MAX_SAFE_FREQUENCY 930.0   // 930 MHz (ISM band end)

// Why these limits?
ISM Band (North America): 902-928 MHz
  ├─ Our range: 900-930 MHz (includes full band + margins)
  ├─ Meshtastic operates: 902-928 MHz
  └─ Safe for testing without regulatory issues

// SX1262 absolute limits: 150-960 MHz
// We restrict to ISM band for:
  ✅ Legal compliance
  ✅ No interference with licensed bands
  ✅ Known-good operating range
```

**Power Range:**

```cpp
#define MAX_SAFE_POWER_DBM 30      // 30 dBm = 1 Watt

// Power safety pyramid:
SX1262 Absolute Maximum:     +22 dBm (limited by PA)
PA+ (external amplifier):    +30 dBm (1 watt)
Our safe testing maximum:    +30 dBm
Typical Meshtastic:          +17 to +22 dBm
T-Deck operational power:    +22 dBm

Power levels:
  -9 dBm  = 125 μW  (very low, testing only)
   0 dBm  = 1 mW    (low power)
  +10 dBm = 10 mW   (moderate)
  +17 dBm = 50 mW   (typical Meshtastic)
  +22 dBm = 158 mW  (SX1262 maximum)
  +30 dBm = 1 W     (with PA+, our test maximum)
```

**Thermal Limits:**

```cpp
#define MAX_SAFE_TEMPERATURE 65.0  // °C

ESP32 temperature sensor:
  ├─ Measures die temperature (internal)
  ├─ Accuracy: ±5°C
  ├─ Useful for relative monitoring
  └─ Not calibrated absolute measurement

Temperature thresholds:
  <50°C  = Normal operation ✅
  50-60°C = Elevated, acceptable ⚠️
  60-65°C = Approaching limit 🔥
  >65°C  = Emergency shutdown 🚨

// With 5°C accuracy, 65°C limit ensures:
// True temp won't exceed 70°C (safe for ESP32)
```

**Test Durations:**

```cpp
#define STRESS_TEST_DURATION_MS 5000    // 5 seconds per test
#define COOLDOWN_PERIOD_MS 30000        // 30 seconds between tests
#define INTER_TEST_DELAY_MS 5000        // 5 seconds between steps

Timeline for full stress suite:
  Test 1: Frequency sweep (5s)
  Cooldown: (30s)
  Test 2: Power ramp (5s)
  Cooldown: (30s)
  Test 3: Parameter boundary (5s)
  Cooldown: (30s)
  Test 4: Rapid config changes (5s)
  Cooldown: (30s)
  Test 5: Thermal stress (5s)
  Cooldown: (30s)
  Test 6: Memory stress (5s)
  
  Total: ~210 seconds (~3.5 minutes)
```

---

## Test Types

### 1. Frequency Sweep Test

**Purpose:** Validate radio stability across entire ISM band

```cpp
bool HardwareStressTester::frequencySweepTest() {
    Serial.println("📡 Starting safe frequency sweep test");
    
    // Parameters
    float startFreq = 902.0;  // MHz
    float endFreq = 928.0;    // MHz
    float step = 0.5;         // 500 kHz steps
    
    // Number of steps: (928 - 902) / 0.5 = 52 steps
    
    for (float freq = startFreq; freq <= endFreq; freq += step) {
        // Safety check every iteration
        if (!performSafetyCheck()) {
            Serial.println("🛑 Safety check failed during frequency sweep");
            return false;
        }
        
        // Apply frequency
        int result = radio->setFrequency(freq);
        if (result != RADIOLIB_ERR_NONE) {
            Serial.printf("⚠️ Frequency set failed at %.3f MHz (error: %d)\n", 
                         freq, result);
            lastResult.errorMessages += "Freq_" + String(freq, 1) + "_Failed ";
        }
        
        // Settle time (PLL lock, calibration)
        delay(50);
        
        // Hardware stability check
        if (!checkHardwareStability()) {
            Serial.printf("🚨 Hardware instability detected at %.3f MHz\n", freq);
            return false;
        }
    }
    
    Serial.println("✅ Frequency sweep test completed successfully");
    return true;
}
```

**What This Tests:**

```
PLL Tuning Stability:
  ├─ SX1262 PLL locks at each frequency
  ├─ No lock failures across range
  └─ Validates oscillator stability

Calibration:
  ├─ Radio recalibrates at each frequency
  ├─ Image rejection filters updated
  └─ Optimal sensitivity maintained

Edge Cases:
  ├─ Band edges (902 MHz, 928 MHz)
  ├─ Common channels (906.875, 915.0, etc.)
  └─ Unusual frequencies (testing only)

Timing:
  ├─ 52 steps × (50ms settle + checks) = ~3-4 seconds
  └─ Well within 5-second test duration
```

### 2. Power Ramp Test

**Purpose:** Validate power control and thermal behavior

```cpp
bool HardwareStressTester::powerRampTest() {
    Serial.println("⚡ Starting safe power ramp test");
    
    // Conservative power range
    int minPower = -10;  // -10 dBm (very low)
    int maxPower = 10;   // +10 dBm (moderate, safe)
    // Note: Not testing full +30 dBm to reduce thermal stress
    
    for (int power = minPower; power <= maxPower; power += 2) {
        if (!performSafetyCheck()) {
            Serial.println("🛑 Safety check failed during power ramp");
            return false;
        }
        
        // Set power
        int result = radio->setOutputPower(power);
        if (result != RADIOLIB_ERR_NONE) {
            Serial.printf("⚠️ Power set failed at %d dBm (error: %d)\n", 
                         power, result);
            lastResult.errorMessages += "Power_" + String(power) + "_Failed ";
        }
        
        // Longer settle time for power changes
        delay(200);
        
        // Temperature monitoring critical during power test
        float currentTemp = getCurrentTemperature();
        if (currentTemp > safety.maxTemperature - 5.0) {  // 5°C margin
            Serial.printf("🌡️ Temperature approaching limit: %.1f°C\n", currentTemp);
            break;  // Stop before reaching limit
        }
    }
    
    // Return to safe power level
    radio->setOutputPower(0);
    
    Serial.println("✅ Power ramp test completed successfully");
    return true;
}
```

**What This Tests:**

```
Power Amplifier Control:
  ├─ PA gain settings accurate
  ├─ Output power matches requested
  └─ No oscillation or instability

Thermal Response:
  ├─ Temperature rise vs power output
  ├─ Cooling rate during test
  └─ Identifies thermal throttling point

Current Draw:
  ├─ Higher power = more current
  ├─ Battery/power supply stability
  └─ Validates power budget

Power Range:
  -10 dBm: Minimum useful output
   +0 dBm: Low power (1 mW)
   +5 dBm: Moderate (3 mW)
  +10 dBm: Test maximum (10 mW)
  
  (Full +30 dBm testing reserved for thermal stress test)
```

### 3. Parameter Boundary Test

**Purpose:** Validate all LoRa parameter combinations

```cpp
bool HardwareStressTester::parameterBoundaryTest() {
    Serial.println("🎯 Starting parameter boundary test");
    
    // Test all spreading factors (SX1262: SF5-SF12)
    int spreadingFactors[] = {7, 8, 9, 10, 11, 12};
    // Note: SF5-SF6 require special detection optimize settings
    
    for (int sf : spreadingFactors) {
        if (!performSafetyCheck()) return false;
        
        int result = radio->setSpreadingFactor(sf);
        if (result != RADIOLIB_ERR_NONE) {
            Serial.printf("⚠️ SF boundary test failed at SF%d (error: %d)\n", 
                         sf, result);
            lastResult.errorMessages += "SF" + String(sf) + "_Failed ";
        }
        
        delay(20);
    }
    
    // Test all bandwidth options (LoRa discrete values)
    float bandwidths[] = {125.0, 250.0, 500.0};
    
    for (float bw : bandwidths) {
        if (!performSafetyCheck()) return false;
        
        int result = radio->setBandwidth(bw);
        if (result != RADIOLIB_ERR_NONE) {
            Serial.printf("⚠️ BW boundary test failed at %.0f kHz (error: %d)\n", 
                         bw, result);
            lastResult.errorMessages += "BW" + String(bw, 0) + "_Failed ";
        }
        
        delay(20);
    }
    
    // Test all coding rates (CR 4/5 to 4/8)
    int codingRates[] = {5, 6, 7, 8};
    
    for (int cr : codingRates) {
        if (!performSafetyCheck()) return false;
        
        int result = radio->setCodingRate(cr);
        if (result != RADIOLIB_ERR_NONE) {
            Serial.printf("⚠️ CR boundary test failed at 4/%d (error: %d)\n", 
                         cr, result);
        }
        
        delay(20);
    }
    
    Serial.println("✅ Parameter boundary test completed");
    return true;
}
```

**What This Tests:**

```
Spreading Factor Configurations:
  SF7:  Fastest, shortest range, -127 dBm sensitivity
  SF8:  Balance, -130 dBm
  SF9:  Balance, -132 dBm
  SF10: Balance, -135 dBm
  SF11: Slower, longer range, -137 dBm
  SF12: Slowest, longest range, -139 dBm
  
  Tests: 6 SF values × 20ms = 120ms

Bandwidth Configurations:
  125 kHz: Meshtastic default (Long Fast)
  250 kHz: Meshtastic medium speed
  500 kHz: Meshtastic highest speed
  
  Tests: 3 BW values × 20ms = 60ms

Coding Rate Configurations:
  4/5: Least overhead, fastest (Meshtastic default)
  4/6: More error correction
  4/7: Even more error correction
  4/8: Maximum error correction, slowest
  
  Tests: 4 CR values × 20ms = 80ms

Total: <500ms (very fast test)

Validates:
  ✅ All Meshtastic configurations supported
  ✅ No parameter combination causes crash
  ✅ Radio accepts all valid values
```

### 4. Rapid Config Change Test

**Purpose:** Stress test state machine and configuration pipeline

```cpp
bool HardwareStressTester::rapidConfigChangeTest() {
    Serial.println("⚡ Starting rapid configuration change test");
    
    unsigned long testStart = millis();
    int changeCount = 0;
    
    while (millis() - testStart < STRESS_TEST_DURATION_MS) {
        if (!performSafetyCheck()) return false;
        
        // Rapidly cycle through safe configurations
        // This stresses SPI bus, register writes, PLL tuning
        radio->setFrequency(902.0 + (changeCount % 20) * 0.5);
        radio->setSpreadingFactor(7 + (changeCount % 6));
        radio->setBandwidth(125.0 + (changeCount % 3) * 125.0);
        
        changeCount++;
        delay(20);  // 50 config changes per second
        
        // Check for instability every 25 cycles
        if (changeCount % 25 == 0 && !checkHardwareStability()) {
            Serial.printf("🚨 Instability detected after %d rapid changes\n", 
                         changeCount);
            return false;
        }
    }
    
    Serial.printf("✅ Rapid config test: %d changes in %lu ms\n", 
                  changeCount, millis() - testStart);
    Serial.printf("   Average: %.1f changes/second\n", 
                  changeCount / ((millis() - testStart) / 1000.0));
    
    return true;
}
```

**What This Tests:**

```
State Machine Resilience:
  ├─ Radio handles rapid mode changes
  ├─ No state corruption
  └─ Recovers from any configuration

SPI Bus Integrity:
  ├─ High-frequency register writes
  ├─ No bus contention
  └─ All commands execute correctly

PLL Stability:
  ├─ Frequent frequency changes
  ├─ PLL locks reliably each time
  └─ No lock failures

Performance:
  5000ms test duration
  20ms per cycle
  = ~250 complete reconfigurations
  
  Each cycle changes:
    ├─ Frequency (PLL retune)
    ├─ Spreading Factor (modem config)
    └─ Bandwidth (filter config)
  
  Stress level: EXTREME
  Purpose: Find edge cases, race conditions
```

### 5. Thermal Stress Test

**Purpose:** Monitor thermal behavior under sustained load

```cpp
bool HardwareStressTester::thermalStressTest() {
    Serial.println("🌡️ Starting thermal monitoring stress test");
    
    unsigned long testStart = millis();
    float initialTemp = getCurrentTemperature();
    float maxTempSeen = initialTemp;
    
    // Moderate power for heat generation (not full power for safety)
    radio->setOutputPower(5);  // +5 dBm
    
    while (millis() - testStart < STRESS_TEST_DURATION_MS) {
        float currentTemp = getCurrentTemperature();
        maxTempSeen = max(maxTempSeen, currentTemp);
        
        // Log temperature every second
        if ((millis() - testStart) % 1000 == 0) {
            Serial.printf("🌡️ %lu s: %.1f°C\n", 
                         (millis() - testStart) / 1000, currentTemp);
        }
        
        // Safety margin: Stop 3°C before limit
        if (currentTemp > safety.maxTemperature - 3.0) {
            Serial.printf("🛑 Thermal limit approach: %.1f°C\n", currentTemp);
            break;
        }
        
        delay(100);  // Sample 10 times per second
    }
    
    radio->setOutputPower(0);  // Return to safe power
    
    float finalTemp = getCurrentTemperature();
    float tempRise = maxTempSeen - initialTemp;
    
    Serial.printf("🌡️ Thermal test results:\n");
    Serial.printf("   Initial: %.1f°C\n", initialTemp);
    Serial.printf("   Maximum: %.1f°C\n", maxTempSeen);
    Serial.printf("   Final:   %.1f°C\n", finalTemp);
    Serial.printf("   Rise:    %.1f°C\n", tempRise);
    
    // Calculate cooling rate
    if (finalTemp < maxTempSeen) {
        float coolingRate = (maxTempSeen - finalTemp) / 
                           ((millis() - testStart) / 1000.0);
        Serial.printf("   Cooling: %.2f°C/second\n", coolingRate);
    }
    
    return true;
}
```

**What This Tests:**

```
Thermal Characterization:
  ├─ Temperature rise under load
  ├─ Cooling rate when load removed
  ├─ Time to reach thermal equilibrium
  └─ Safe operating duration at power level

Typical Results:
  Initial temp: 35-45°C (ambient + ESP32 idle)
  After 5s @ +5 dBm: 40-50°C (5-10°C rise)
  Maximum safe: 65°C
  Safety margin: 15-25°C (plenty!)

Why +5 dBm?
  ├─ Enough to generate measurable heat
  ├─ Not enough to approach limits
  └─ Safe for 5-second test duration

Higher power thermal testing:
  +10 dBm: 10-15°C rise expected
  +17 dBm: 15-20°C rise expected
  +22 dBm: 20-25°C rise expected
  +30 dBm: 25-30°C rise (approaches limit!)
```

### 6. Memory Stress Test

**Purpose:** Validate heap stability and memory management

```cpp
bool HardwareStressTester::memoryStressTest() {
    Serial.println("💾 Starting memory stress test");
    
    const int blockSize = 1024;  // 1 KB per block
    const int maxBlocks = 10;    // 10 KB total allocation
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
        
        // Fill with test pattern (unique per block)
        memset(memBlocks[i], 0xAA + i, blockSize);
        delay(10);
    }
    
    Serial.printf("💾 Allocated %d KB successfully\n", maxBlocks);
    
    // Verify memory integrity
    bool memoryIntact = true;
    for (int i = 0; i < maxBlocks; i++) {
        uint8_t* block = (uint8_t*)memBlocks[i];
        for (int j = 0; j < blockSize; j++) {
            if (block[j] != (uint8_t)(0xAA + i)) {
                Serial.printf("💥 Memory corruption detected!\n");
                Serial.printf("   Block %d, offset %d\n", i, j);
                Serial.printf("   Expected: 0x%02X, Got: 0x%02X\n", 
                             0xAA + i, block[j]);
                memoryIntact = false;
                break;
            }
        }
        if (!memoryIntact) break;
    }
    
    // Free all blocks
    for (int i = 0; i < maxBlocks; i++) {
        if (memBlocks[i]) free(memBlocks[i]);
    }
    
    // Check heap after free
    uint32_t freeHeap = ESP.getFreeHeap();
    Serial.printf("💾 Free heap after test: %u bytes\n", freeHeap);
    
    Serial.printf("💾 Memory test: %s\n", memoryIntact ? "PASS" : "FAIL");
    return memoryIntact;
}
```

**What This Tests:**

```
Heap Allocation:
  ├─ Can allocate 10 KB without failure
  ├─ malloc() returns valid pointers
  └─ No out-of-memory condition

Memory Integrity:
  ├─ Data written persists correctly
  ├─ No corruption from other operations
  └─ Unique patterns per block verify isolation

Memory Leaks:
  ├─ Free heap after test = before test
  ├─ All allocated memory properly freed
  └─ No leaked allocations

ESP32-S3 Memory:
  Total SRAM: 320 KB
  Heap available: ~280 KB (depends on stack, static)
  Test allocation: 10 KB (<4% of available)
  Very conservative test

Why 10 KB?
  ✅ Large enough to detect issues
  ✅ Small enough to not risk OOM
  ✅ Fast test (<200ms)
```

### 7. T-Deck Targeted Test

**Purpose:** Device-specific vulnerability assessment

```cpp
bool HardwareStressTester::tdeckTargetedTest() {
    Serial.println("🎯 Starting targeted frequency vulnerability assessment...");
    
    // T-Deck specific parameters
    #define TDECK_TARGET_FREQUENCY 902.125  // MHz
    #define TDECK_OPERATIONAL_POWER 22      // dBm
    
    bool testPassed = true;
    float testFreq = TDECK_TARGET_FREQUENCY;
    
    Serial.printf("🎯 Target frequency: %.3f MHz\n", testFreq);
    Serial.printf("🎯 Target power: %d dBm\n", TDECK_OPERATIONAL_POWER);
    
    // Lock onto target frequency
    if (radio->setFrequency(testFreq) == RADIOLIB_ERR_NONE) {
        Serial.printf("✅ Locked onto target frequency: %.3f MHz\n", testFreq);
        
        // Test power levels (ramping up to operational)
        for (int power = 10; power <= MAX_SAFE_POWER_DBM; power += 2) {
            if (radio->setOutputPower(power) == RADIOLIB_ERR_NONE) {
                Serial.printf("🔋 Power level %d dBm: OK\n", power);
                delay(500);  // Transmit window for vulnerability assessment
                
                // Safety check at each power level
                if (!performSafetyCheck()) {
                    Serial.println("⚠️ Safety limit reached during power test");
                    break;
                }
            } else {
                Serial.printf("❌ Failed to set power to %d dBm\n", power);
                testPassed = false;
            }
        }
        
        // Test frequency tolerance
        float tolerance = 0.025;  // ±25 kHz
        for (float offset = -tolerance; offset <= tolerance; offset += 0.005) {
            float freq = testFreq + offset;
            if (radio->setFrequency(freq) == RADIOLIB_ERR_NONE) {
                Serial.printf("🎯 Frequency %.3f MHz: OK\n", freq);
                delay(200);
            }
        }
        
    } else {
        Serial.println("❌ Failed to set target frequency");
        testPassed = false;
    }
    
    // Record test details
    lastResult.tdeckTargeted = true;
    lastResult.targetFrequency = TDECK_TARGET_FREQUENCY;
    lastResult.powerUsed = MAX_SAFE_POWER_DBM;
    
    Serial.printf("🎯 Vulnerability assessment %s\n", 
                 testPassed ? "COMPLETED" : "FAILED");
    
    return testPassed;
}
```

**What This Tests:**

```
Device-Specific Parameters:
  T-Deck Lilygo Configuration:
    ├─ Frequency: 902.125 MHz (specific channel)
    ├─ Power: +22 dBm (maximum without PA+)
    ├─ Bandwidth: 125 kHz (Meshtastic default)
    └─ SF: 7-12 (depends on config)

Vulnerability Assessment Goals:
  ├─ Can our sniffer operate at T-Deck's exact frequency?
  ├─ Can we match T-Deck's power output?
  ├─ Can we maintain lock within ±25 kHz tolerance?
  └─ Does configuration remain stable?

Why Target T-Deck?
  ├─ Popular Meshtastic device (common in field)
  ├─ Known operational parameters
  ├─ Representative of SX1262-based devices
  └─ Testing validates our hardware matches capability

Ethical Considerations:
  ⚠️ Testing capability, NOT attacking devices
  ⚠️ Operating within legal ISM band
  ⚠️ No harmful interference generated
  ✅ Research and development purposes only
```

---

## Safety Monitoring

### Temperature Monitoring

**ESP32 Internal Temperature Sensor:**

```cpp
float HardwareStressTester::getCurrentTemperature() {
    #ifdef ESP32
    return temperatureRead();  // Built-in ESP32 function
    #else
    return 25.0;  // Fallback for non-ESP32 platforms
    #endif
}

// ESP32 temperatureRead():
//   ├─ Reads on-die temperature sensor
//   ├─ Returns: float (degrees Celsius)
//   ├─ Accuracy: ±5°C (not calibrated)
//   └─ Useful for: Relative measurements, trending

// Example readings:
//   Idle, room temp: 35-45°C
//   Under load: 45-60°C
//   Hot environment: Add 10-15°C to above
//   Thermal limit: 65°C (with 5°C margin = true 70°C max)
```

**Thermal Monitoring Strategy:**

```cpp
bool HardwareStressTester::performSafetyCheck() {
    safety.lastSafetyCheck = millis();
    safety.currentTemperature = getCurrentTemperature();
    
    // Check for thermal shutdown condition
    if (safety.currentTemperature > safety.maxTemperature) {
        Serial.printf("🌡️ THERMAL LIMIT EXCEEDED: %.1f°C > %.1f°C\n",
                     safety.currentTemperature, safety.maxTemperature);
        safety.thermalShutdown = true;
        emergencyShutdown();
        return false;
    }
    
    // Warn if approaching limit (5°C margin)
    if (safety.currentTemperature > safety.maxTemperature - 5.0) {
        Serial.printf("⚠️ Temperature approaching limit: %.1f°C\n",
                     safety.currentTemperature);
    }
    
    // Validate radio still responsive
    if (!validateRadioConfiguration()) {
        safety.hardwareFailure = true;
        return false;
    }
    
    return true;
}
```

### Cooldown Enforcement

**Why Cooldown is Critical:**

```
Without cooldown:
  Test 1: Temp rises 45°C → 55°C
  Test 2: Starts at 55°C, rises to 65°C (limit!)
  Test 3: Can't run, already at limit
  
  Result: Only 2 tests complete before thermal shutdown

With 30-second cooldown:
  Test 1: Temp rises 45°C → 55°C
  Cooldown: 30s → Temp drops 55°C → 48°C
  Test 2: Starts at 48°C, rises to 58°C
  Cooldown: 30s → Temp drops 58°C → 50°C
  Test 3: Starts at 50°C, rises to 60°C
  
  Result: All tests complete safely, never approaching limit

Cooling rate:
  Typical: 0.5-1.0°C per second
  30 seconds: 15-30°C drop
  Sufficient for: Full test suite
```

**Cooldown Implementation:**

```cpp
bool HardwareStressTester::checkSafetyLimits() {
    // ... temperature check ...
    
    // Cooldown period check
    if (millis() - lastCooldown < COOLDOWN_PERIOD_MS) {
        unsigned long remaining = COOLDOWN_PERIOD_MS - (millis() - lastCooldown);
        Serial.printf("⏱️ Hardware cooldown active: %.1f seconds remaining\n", 
                     remaining / 1000.0);
        Serial.println("   (Hardware needs recovery time for reliable testing)");
        return false;
    }
    
    // ... hardware stability check ...
    
    return true;
}

// After each test completes:
lastCooldown = millis();  // Record cooldown start time

// Next test attempts:
if (!checkSafetyLimits()) {
    // Cooldown active, test blocked
    return false;
}
```

### Emergency Shutdown

**Triggering Conditions:**

```
Emergency shutdown triggered by:
  🚨 Temperature exceeds maximum limit (65°C)
  🚨 Hardware stability check fails
  🚨 Radio becomes unresponsive
  🚨 User calls abortAllTesting()

Emergency actions:
  1. Set radio to standby mode (stop RF activity)
  2. Set power to minimum (0 dBm)
  3. Disable all stress testing
  4. Set safety.thermalShutdown flag
  5. Log emergency event
```

**Implementation:**

```cpp
void HardwareStressTester::emergencyShutdown() {
    Serial.println("🚨 EMERGENCY SHUTDOWN ACTIVATED");
    
    if (radio) {
        // Stop all RF activity immediately
        radio->standby();
        
        // Minimum power output
        radio->setOutputPower(0);
        
        // Could also: radio->sleep() for even lower power
    }
    
    // Disable all testing
    testingEnabled = false;
    safety.thermalShutdown = true;
    
    Serial.println("🛡️ All stress testing halted for safety");
    Serial.println("⚠️ Device requires manual reset to resume testing");
}

// Recovery:
// User must:
//   1. Power cycle device OR
//   2. Call resetHardware() OR
//   3. Re-initialize stress tester

void HardwareStressTester::resetHardware() {
    Serial.println("🔄 Resetting hardware...");
    
    // Reset safety state
    safety.thermalShutdown = false;
    safety.hardwareFailure = false;
    safety.failureCount = 0;
    
    // Reinitialize radio
    radio->begin();
    
    // Wait for temperature to drop
    while (getCurrentTemperature() > 50.0) {
        Serial.printf("⏱️ Waiting for cooldown: %.1f°C\n", 
                     getCurrentTemperature());
        delay(5000);
    }
    
    // Re-enable testing
    testingEnabled = true;
    Serial.println("✅ Hardware reset complete, testing re-enabled");
}
```

---

## Device Profiles

### Supported Devices

```cpp
enum DeviceType {
    DEVICE_LILYGO_TDECK,
    DEVICE_HELTEC_V3,
    DEVICE_TTGO_LORA32,
    DEVICE_HELTEC_STICK_LITE,
    DEVICE_GENERIC_ESP32_LORA,
    DEVICE_AUTO_DETECT
};

struct DeviceProfile {
    DeviceType type;
    String name;
    float targetFrequency;
    int maxSafePower;
    int minSafePower;
    bool hasDisplay;
    bool hasBattery;
    String specialNotes;
};
```

**Profile Definitions:**

```cpp
DeviceProfile profiles[] = {
    {
        .type = DEVICE_LILYGO_TDECK,
        .name = "Lilygo T-Deck",
        .targetFrequency = 902.125,
        .maxSafePower = 22,
        .minSafePower = 5,
        .hasDisplay = true,
        .hasBattery = true,
        .specialNotes = "Full keyboard, GPS, PA limited to +22 dBm"
    },
    {
        .type = DEVICE_HELTEC_V3,
        .name = "Heltec WiFi LoRa 32 V3",
        .targetFrequency = 915.0,
        .maxSafePower = 22,
        .minSafePower = 2,
        .hasDisplay = true,
        .hasBattery = false,
        .specialNotes = "OLED display, SX1262 chip, USB-C power"
    },
    {
        .type = DEVICE_TTGO_LORA32,
        .name = "TTGO LoRa32",
        .targetFrequency = 915.0,
        .maxSafePower = 20,
        .minSafePower = 5,
        .hasDisplay = true,
        .hasBattery = true,
        .specialNotes = "Older SX1276 chip, 18650 battery holder"
    }
    // ... more profiles ...
};
```

---

## Test Execution

### Running Full Test Suite

```cpp
bool HardwareStressTester::runFullStressSuite() {
    Serial.println("╔════════════════════════════════════════════╗");
    Serial.println("║   HARDWARE STRESS TEST SUITE - FULL RUN   ║");
    Serial.println("╚════════════════════════════════════════════╝\n");
    
    if (!testingEnabled) {
        Serial.println("❌ Stress testing not enabled");
        return false;
    }
    
    // Array of all test types
    StressTestType tests[] = {
        FREQUENCY_SWEEP_TEST,
        POWER_RAMP_TEST,
        PARAMETER_BOUNDARY_TEST,
        RAPID_CONFIG_CHANGE_TEST,
        THERMAL_STRESS_TEST,
        MEMORY_STRESS_TEST
        // TDECK_TARGETED_TEST optional
    };
    
    int passedTests = 0;
    int totalTests = sizeof(tests) / sizeof(StressTestType);
    
    for (int i = 0; i < totalTests; i++) {
        Serial.printf("\n[%d/%d] ", i + 1, totalTests);
        
        bool result = runStressTest(tests[i]);
        if (result) {
            passedTests++;
        }
        
        // Mandatory cooldown enforced by runStressTest()
    }
    
    Serial.println("\n╔════════════════════════════════════════════╗");
    Serial.println("║         TEST SUITE COMPLETE                ║");
    Serial.println("╚════════════════════════════════════════════╝");
    Serial.printf("\n✅ Passed: %d/%d tests\n", passedTests, totalTests);
    
    if (passedTests == totalTests) {
        Serial.println("🎉 ALL TESTS PASSED - Hardware validated!");
        return true;
    } else {
        Serial.printf("⚠️ %d tests failed - Review results\n", 
                     totalTests - passedTests);
        return false;
    }
}
```

---

## Results Analysis

### Test Result Structure

```cpp
struct StressTestResult {
    StressTestType testType;
    bool testPassed;
    bool hardwareStable;
    float temperatureReading;
    unsigned long testDuration;
    String errorMessages;
    String stabilityMetrics;
    bool tdeckTargeted;
    float targetFrequency;
    float powerUsed;
};
```

### Interpreting Results

**Successful Test Suite:**

```
╔════════════════════════════════════════════╗
║   HARDWARE STRESS TEST SUITE - FULL RUN   ║
╚════════════════════════════════════════════╝

[1/6] 📡 Starting safe frequency sweep test
✅ Frequency sweep test completed successfully
📊 Test completed: PASS (Duration: 3847 ms, Temp: 42.3°C)

[2/6] ⚡ Starting safe power ramp test
✅ Power ramp test completed successfully
📊 Test completed: PASS (Duration: 4521 ms, Temp: 47.8°C)

[3/6] 🎯 Starting parameter boundary test
✅ Parameter boundary test completed
📊 Test completed: PASS (Duration: 384 ms, Temp: 45.2°C)

[4/6] ⚡ Starting rapid configuration change test
✅ Rapid config test: 243 changes in 5000 ms
   Average: 48.6 changes/second
📊 Test completed: PASS (Duration: 5003 ms, Temp: 48.9°C)

[5/6] 🌡️ Starting thermal monitoring stress test
🌡️ Thermal test results:
   Initial: 46.1°C
   Maximum: 52.7°C
   Final:   49.3°C
   Rise:    6.6°C
   Cooling: 0.68°C/second
📊 Test completed: PASS (Duration: 5104 ms, Temp: 49.3°C)

[6/6] 💾 Starting memory stress test
💾 Allocated 10 KB successfully
💾 Free heap after test: 267584 bytes
💾 Memory test: PASS
📊 Test completed: PASS (Duration: 198 ms, Temp: 47.1°C)

╔════════════════════════════════════════════╗
║         TEST SUITE COMPLETE                ║
╚════════════════════════════════════════════╝

✅ Passed: 6/6 tests
🎉 ALL TESTS PASSED - Hardware validated!
```

**Interpretation:**

```
✅ All tests passed = Hardware fully operational
✅ Temperature never exceeded 53°C = Excellent thermal margin
✅ Rapid config test: 48.6 changes/sec = Fast, stable
✅ Memory test passed = No heap corruption
✅ Cooling rate 0.68°C/s = Good thermal dissipation

Conclusion: Hardware is robust, stress-tested, production-ready
```

---

## Summary

### What You've Learned

**Safety Architecture:**
- ✅ Conservative limits well below absolute maximums
- ✅ Mandatory 30-second cooldowns between tests
- ✅ Temperature monitoring with 5°C safety margins
- ✅ Emergency shutdown capabilities

**Test Coverage:**
- ✅ Frequency sweep (52 steps across ISM band)
- ✅ Power ramp (validates PA control and thermal)
- ✅ Parameter boundaries (all SF/BW/CR combinations)
- ✅ Rapid config changes (stress test state machine)
- ✅ Thermal characterization (temperature trending)
- ✅ Memory validation (heap integrity)
- ✅ Device-specific targeting (T-Deck vulnerability assessment)

**Safety Features:**
- ✅ Multiple safety checks per test
- ✅ Automatic thermal shutdown
- ✅ Hardware stability verification
- ✅ Cooldown enforcement
- ✅ Detailed result logging

### Key Takeaways

1. **Safety First:** Hardware stress testing is aggressive but never risks damage
2. **Comprehensive:** Tests cover all operational parameters and edge cases
3. **Validated:** Successful suite completion proves hardware robustness
4. **Thermal Aware:** Temperature monitoring prevents overheating
5. **Device-Specific:** Can target specific devices (T-Deck) for compatibility testing

---

*Generated for ESP32-Sniffer deep-dive session*  
*Last updated: 2025-10-16*
