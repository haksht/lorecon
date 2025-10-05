/**
 * Hardware Stress Testing Module for ESP32 Security Research
 * Phase 3+: Device Resilience and Vulnerability Assessment
 * 
 * Safe stress testing framework for IoT device hardening research
 * and mesh network security assessment applications.
 */

#pragma once

#include <Arduino.h>
#include <RadioLib.h>

// Safety configuration - CRITICAL LIMITS (Enhanced with T-Deck Intelligence)
#define MAX_SAFE_POWER_DBM 30          // Maximum safe power output (full range)
#define MIN_SAFE_FREQUENCY 900.0       // Minimum safe frequency (full ISM band)
#define MAX_SAFE_FREQUENCY 930.0       // Maximum safe frequency (full ISM band)
#define TDECK_TARGET_FREQUENCY 902.125 // T-Deck operational frequency (MHz) - for targeting
#define TDECK_FREQUENCY_TOLERANCE 0.025 // ±25kHz tolerance around target
#define TDECK_OPERATIONAL_POWER 22     // T-Deck confirmed safe power level
#define STRESS_TEST_DURATION_MS 5000   // Maximum test duration per cycle
#define COOLDOWN_PERIOD_MS 30000       // Mandatory cooldown between tests (30 seconds)
#define INTER_TEST_DELAY_MS 5000       // Additional delay between test steps
#define MAX_STRESS_CYCLES 10           // Maximum stress test cycles

// Stress test types
enum StressTestType {
    FREQUENCY_SWEEP_TEST,
    POWER_RAMP_TEST,
    PARAMETER_BOUNDARY_TEST,
    RAPID_CONFIG_CHANGE_TEST,
    THERMAL_STRESS_TEST,
    MEMORY_STRESS_TEST,
    TDECK_TARGETED_TEST        // Optional T-Deck specific vulnerability assessment
};

// Test result structure
struct StressTestResult {
    StressTestType testType;
    bool testPassed;
    bool hardwareStable;
    float temperatureReading;
    unsigned long testDuration;
    String errorMessages;
    String stabilityMetrics;
    bool tdeckTargeted;        // Was this a T-Deck specific test
    float targetFrequency;     // Target frequency for vulnerability assessment
    float powerUsed;          // Actual power level used in test
};

// Safety monitoring structure
struct SafetyMonitor {
    float maxTemperature;
    float currentTemperature;
    bool thermalShutdown;
    bool hardwareFailure;
    unsigned long lastSafetyCheck;
    int failureCount;
};

class HardwareStressTester {
private:
    SX1262* radio;
    SafetyMonitor safety;
    StressTestResult lastResult;
    bool testingEnabled;
    unsigned long lastCooldown;
    
    // Safety functions
    bool checkSafetyLimits();
    bool performSafetyCheck();
    void emergencyShutdown();
    bool waitForCooldown();
    
    // Individual stress tests
    bool frequencySweepTest();
    bool powerRampTest(); 
    bool parameterBoundaryTest();
    bool rapidConfigChangeTest();
    bool thermalStressTest();
    bool memoryStressTest();
    bool tdeckTargetedTest();  // T-Deck specific vulnerability assessment
    
    // Monitoring functions
    float getCurrentTemperature();
    bool checkHardwareStability();
    bool validateRadioConfiguration();
    void logTestMetrics(const String& testName, bool result);
    
public:
    HardwareStressTester(SX1262* radioModule);
    
    // Core testing interface
    bool initializeStressTesting();
    bool runStressTest(StressTestType testType);
    bool runFullStressSuite();
    
    // Safety and monitoring
    bool isTestingSafe();
    void enableSafetyLimits(bool enable = true);
    SafetyMonitor getSafetyStatus();
    StressTestResult getLastTestResult();
    
    // Configuration and limits
    void setSafetyLimits(float maxTemp, int maxCycles);
    void setTestDuration(unsigned long duration);
    
    // Reporting and analysis
    void printStressTestReport();
    void exportTestResults(const String& filename);
    
    // Emergency controls
    void abortAllTesting();
    void resetHardware();
};

// Utility functions for stress testing integration
bool initializeHardwareStressTesting(SX1262* radio);
void performPeriodicSafetyCheck();
bool runQuickStabilityTest();
void generateStressTestReport();

// Safety validation functions
bool validateFrequencyRange(float frequency);
bool validatePowerLevel(int powerDbm);
bool validateTestDuration(unsigned long duration);

// Hardware monitoring utilities
float getSystemTemperature();
bool checkMemoryIntegrity();

// Research and analysis functions
void analyzeHardwareFailurePatterns();
void documentVulnerabilityFindings();
void generateSecurityAssessmentReport();

// Device compatibility and profiles
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

// Device-specific analysis functions
bool isTDeckCompatibleFrequency(float frequency);
bool validateTDeckPowerLimits(int powerDbm);
void performTDeckVulnerabilityAssessment();
void analyzeTDeckPowerDependency();

// Multi-device support functions
DeviceType detectConnectedDevice();
DeviceProfile getDeviceProfile(DeviceType device);
bool validateDeviceForTesting(DeviceType device);
void showSupportedDevices();