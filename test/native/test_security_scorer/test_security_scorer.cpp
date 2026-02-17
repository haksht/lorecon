/**
 * Native unit tests for SecurityScorer.
 *
 * Tests the unified security assessment logic that scores devices
 * based on risk factors (proximity, firmware, routing, traffic patterns).
 */
#include <unity.h>
#include "utils/security_scorer.h"

void setUp(void) {}
void tearDown(void) {}

// Helper: create a "normal" device with no risk factors
static TargetableDevice makeDevice() {
    TargetableDevice d = {};
    d.nodeId = 0x12345678;
    d.bestRSSI = -80.0f;
    d.avgRSSI = -85.0f;
    d.packetCount = 20;
    d.isRouter = false;
    d.powerClass = 1;
    strncpy(d.firmwareVersion, "~v2.2+ (est: encryption flag)",
            sizeof(d.firmwareVersion) - 1);
    strncpy(d.protocol, "Meshtastic", sizeof(d.protocol) - 1);
    return d;
}

// --- Baseline ---

void test_normal_device_is_secure() {
    auto d = makeDevice();
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_EQUAL(100, a.score);
    TEST_ASSERT_EQUAL_STRING("secure", a.rating);
    TEST_ASSERT_FALSE(a.physicalProximity);
    TEST_ASSERT_FALSE(a.isRouter);
    TEST_ASSERT_FALSE(a.chatty);
    TEST_ASSERT_FALSE(a.outdatedFirmware);
}

// --- Individual risk factors ---

void test_close_proximity_penalty() {
    auto d = makeDevice();
    d.bestRSSI = -40.0f;  // Very close
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_EQUAL(85, a.score);
    TEST_ASSERT_TRUE(a.physicalProximity);
    TEST_ASSERT_EQUAL_STRING("secure", a.rating);  // 85 >= 80
}

void test_router_penalty() {
    auto d = makeDevice();
    d.isRouter = true;
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_EQUAL(90, a.score);
    TEST_ASSERT_TRUE(a.isRouter);
}

void test_chatty_device_penalty() {
    auto d = makeDevice();
    d.packetCount = 150;
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_EQUAL(85, a.score);
    TEST_ASSERT_TRUE(a.chatty);
}

void test_intermittent_device_penalty() {
    auto d = makeDevice();
    d.packetCount = 2;
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_EQUAL(95, a.score);
    TEST_ASSERT_TRUE(a.intermittent);
}

void test_outdated_firmware_v1x() {
    auto d = makeDevice();
    strncpy(d.firmwareVersion, "~v1.x or beacon (est)",
            sizeof(d.firmwareVersion) - 1);
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_EQUAL(80, a.score);
    TEST_ASSERT_TRUE(a.outdatedFirmware);
}

void test_outdated_firmware_v20() {
    auto d = makeDevice();
    strncpy(d.firmwareVersion, "~v2.0.x (est: flag pattern)",
            sizeof(d.firmwareVersion) - 1);
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_EQUAL(80, a.score);
    TEST_ASSERT_TRUE(a.outdatedFirmware);
}

// --- Combined risk factors ---

void test_multiple_risks_accumulate() {
    auto d = makeDevice();
    d.bestRSSI = -40.0f;   // -15
    d.isRouter = true;      // -10
    d.packetCount = 150;    // -15
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_EQUAL(60, a.score);
    TEST_ASSERT_EQUAL_STRING("moderate", a.rating);
}

void test_all_risks_vulnerable() {
    auto d = makeDevice();
    d.bestRSSI = -40.0f;   // -15
    d.isRouter = true;      // -10
    d.packetCount = 150;    // -15
    strncpy(d.firmwareVersion, "~v2.0.x (est: flag pattern)",
            sizeof(d.firmwareVersion) - 1);  // -20
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_EQUAL(40, a.score);
    TEST_ASSERT_EQUAL_STRING("vulnerable", a.rating);
}

// --- Utility functions ---

void test_has_findings_none() {
    auto d = makeDevice();
    auto a = SecurityScorer::assess(d);
    // possibleUnencrypted is set for Meshtastic devices with >10 packets
    // but it doesn't affect score. hasFindings checks all flags.
    // Our device has packetCount=20 and protocol="Meshtastic" -> possibleUnencrypted=true
    TEST_ASSERT_TRUE(SecurityScorer::hasFindings(a));
}

void test_finding_count() {
    auto d = makeDevice();
    d.bestRSSI = -40.0f;
    d.isRouter = true;
    auto a = SecurityScorer::assess(d);
    // physicalProximity + isRouter + possibleUnencrypted (packetCount>10 + Meshtastic)
    TEST_ASSERT_EQUAL(3, SecurityScorer::getFindingCount(a));
}

void test_score_does_not_go_negative() {
    auto d = makeDevice();
    d.bestRSSI = -40.0f;
    d.isRouter = true;
    d.packetCount = 150;
    strncpy(d.firmwareVersion, "~v1.x or beacon (est)",
            sizeof(d.firmwareVersion) - 1);
    auto a = SecurityScorer::assess(d);
    // -15 -10 -15 -20 = -60 from 100 = 40
    TEST_ASSERT_EQUAL(40, a.score);
    TEST_ASSERT_TRUE(a.score >= 0);  // Must never underflow
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_normal_device_is_secure);
    RUN_TEST(test_close_proximity_penalty);
    RUN_TEST(test_router_penalty);
    RUN_TEST(test_chatty_device_penalty);
    RUN_TEST(test_intermittent_device_penalty);
    RUN_TEST(test_outdated_firmware_v1x);
    RUN_TEST(test_outdated_firmware_v20);
    RUN_TEST(test_multiple_risks_accumulate);
    RUN_TEST(test_all_risks_vulnerable);
    RUN_TEST(test_has_findings_none);
    RUN_TEST(test_finding_count);
    RUN_TEST(test_score_does_not_go_negative);
    return UNITY_END();
}
