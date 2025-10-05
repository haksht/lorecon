/**
 * Unit Tests for ESP32 LoRa Security Research Platform
 * 
 * Production-grade testing framework for validating core functionality.
 * Can be run on device or in CI/CD pipeline.
 */

#ifndef UNIT_TESTS_H
#define UNIT_TESTS_H

#include <Arduino.h>

// Test result structure
struct TestResult {
    const char* testName;
    bool passed;
    const char* message;
    uint32_t executionTime;
};

/**
 * Unit Test Framework
 * 
 * Simple but effective testing framework for embedded systems.
 * Tests core functionality without requiring external hardware.
 */
class UnitTests {
public:
    static void runAllTests();
    static void printResults();
    
private:
    static const uint8_t MAX_TESTS = 50;
    static TestResult results[MAX_TESTS];
    static uint8_t testCount;
    static uint8_t passedTests;
    static uint8_t failedTests;
    
    // Test helper
    static void recordTest(const char* name, bool passed, const char* message = nullptr);
    
    // Core functionality tests
    static void testReconStateInitialization();
    static void testScanConfigAccess();
    static void testRFActivityTracking();
    static void testTargetableDeviceManagement();
    static void testNodeTracking();
    static void testPacketReplayStorage();
    
    // Protocol analyzer tests
    static void testMeshtasticDetection();
    static void testLoRaWANDetection();
    static void testNodeIDExtraction();
    static void testFirmwareFingerprinting();
    
    // Data structure tests
    static void testCapturedPacketStorage();
    static void testScanStateManagement();
    static void testArrayBounds();
    
    // Error handling tests
    static void testErrorLogging();
    static void testErrorRecovery();
    static void testMemoryTracking();
    
    // Command handler tests
    static void testCommandDispatch();
    static void testInvalidCommands();
    
    // Memory safety tests
    static void testBufferOverflows();
    static void testNullPointers();
    static void testMemoryLeaks();
    
    // Integration tests
    static void testFullReconCycle();
    static void testModeTransitions();
    static void testInterruptSafety();
};

// Assertion macros
#define ASSERT_TRUE(condition, message) \
    if (!(condition)) { \
        Serial.printf("[TEST FAIL] %s: %s\n", __FUNCTION__, message); \
        return false; \
    }

#define ASSERT_FALSE(condition, message) \
    ASSERT_TRUE(!(condition), message)

#define ASSERT_EQUAL(expected, actual, message) \
    if ((expected) != (actual)) { \
        Serial.printf("[TEST FAIL] %s: %s (expected: %d, actual: %d)\n", \
                      __FUNCTION__, message, (int)(expected), (int)(actual)); \
        return false; \
    }

#define ASSERT_NOT_NULL(ptr, message) \
    ASSERT_TRUE((ptr) != nullptr, message)

#define ASSERT_NULL(ptr, message) \
    ASSERT_TRUE((ptr) == nullptr, message)

#endif // UNIT_TESTS_H
