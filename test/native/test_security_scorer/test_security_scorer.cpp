/**
 * Native unit tests for SecurityScorer.
 *
 * Tests the unified security assessment logic that scores devices
 * based on risk factors. Penalties (current implementation):
 *   Physical proximity (avgRSSI > -50)        -15
 *   MeshCore public channel                   -40
 *   MeshCore hashtag channel (#room)          -20
 *   Router device                             -10
 *   Chatty (packetCount > 100)               -15
 *   Intermittent (packetCount < 5)            -5
 *   Outdated confirmed firmware (v1.x/v2.0)  -20
 *
 * Ratings: score >= 80 → "secure", >= 50 → "moderate", < 50 → "vulnerable"
 *
 * Note: proximity check uses avgRSSI (sustained), not bestRSSI (historical peak).
 * Firmware version penalty only fires on confirmed versions (no "(est)" or "Unknown").
 */
#include <unity.h>
#include "utils/security_scorer.h"

void setUp(void) {}
void tearDown(void) {}

// Helper: baseline device with no risk factors
static TargetableDevice makeDevice() {
    TargetableDevice d = {};
    d.nodeId = 0x12345678;
    d.avgRSSI  = -80.0f;   // not close
    d.bestRSSI = -80.0f;
    d.packetCount = 20;    // not chatty, not intermittent
    d.isRouter = false;
    strncpy(d.firmwareVersion, "Unknown", sizeof(d.firmwareVersion) - 1);
    strncpy(d.protocol, "Meshtastic", sizeof(d.protocol) - 1);
    d.meshCoreChannel[0] = '\0';
    return d;
}

// ── Baseline ──────────────────────────────────────────────────────────────────

void test_normal_device_is_secure() {
    auto d = makeDevice();
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_EQUAL(100, a.score);
    TEST_ASSERT_EQUAL_STRING("secure", a.rating);
    TEST_ASSERT_FALSE(a.physicalProximity);
    TEST_ASSERT_FALSE(a.isRouter);
    TEST_ASSERT_FALSE(a.chatty);
    TEST_ASSERT_FALSE(a.intermittent);
    TEST_ASSERT_FALSE(a.outdatedFirmware);
    TEST_ASSERT_FALSE(a.meshCorePublic);
    TEST_ASSERT_FALSE(a.meshCoreHashtag);
}

void test_no_findings_on_clean_device() {
    auto d = makeDevice();
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_FALSE(SecurityScorer::hasFindings(a));
    TEST_ASSERT_EQUAL(0, SecurityScorer::getFindingCount(a));
}

// ── Proximity (avgRSSI > -50) — penalty -15 ───────────────────────────────────

void test_proximity_triggers_on_avg_rssi_above_threshold() {
    auto d = makeDevice();
    d.avgRSSI = -49.0f;  // just above -50
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_TRUE(a.physicalProximity);
    TEST_ASSERT_EQUAL(85, a.score);
}

void test_proximity_does_not_trigger_at_threshold() {
    auto d = makeDevice();
    d.avgRSSI = -50.0f;  // exactly -50, not above
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_FALSE(a.physicalProximity);
    TEST_ASSERT_EQUAL(100, a.score);
}

void test_proximity_uses_avg_not_best_rssi() {
    // bestRSSI is very close but avgRSSI is not — no proximity finding
    auto d = makeDevice();
    d.bestRSSI = -30.0f;
    d.avgRSSI  = -80.0f;
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_FALSE(a.physicalProximity);
}

// ── Router — penalty -10 ──────────────────────────────────────────────────────

void test_router_penalty() {
    auto d = makeDevice();
    d.isRouter = true;
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_EQUAL(90, a.score);
    TEST_ASSERT_TRUE(a.isRouter);
    TEST_ASSERT_EQUAL_STRING("secure", a.rating);
}

// ── Chatty (packetCount > 100) — penalty -15 ──────────────────────────────────

void test_chatty_penalty() {
    auto d = makeDevice();
    d.packetCount = 101;
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_EQUAL(85, a.score);
    TEST_ASSERT_TRUE(a.chatty);
}

void test_chatty_does_not_trigger_at_100() {
    auto d = makeDevice();
    d.packetCount = 100;
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_FALSE(a.chatty);
}

// ── Intermittent (packetCount < 5) — penalty -5 ───────────────────────────────

void test_intermittent_penalty() {
    auto d = makeDevice();
    d.packetCount = 4;
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_EQUAL(95, a.score);
    TEST_ASSERT_TRUE(a.intermittent);
}

void test_intermittent_does_not_trigger_at_5() {
    auto d = makeDevice();
    d.packetCount = 5;
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_FALSE(a.intermittent);
}

// ── Outdated firmware — penalty -20 ───────────────────────────────────────────
// Only fires on confirmed versions (no "(est)" or "Unknown" in string)

void test_outdated_firmware_v1x_confirmed() {
    auto d = makeDevice();
    strncpy(d.firmwareVersion, "MeshCore v0", sizeof(d.firmwareVersion) - 1);
    // "v0" doesn't match "v1.x" or "v2.0" — should NOT fire
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_FALSE(a.outdatedFirmware);
}

void test_outdated_firmware_v1x_string() {
    auto d = makeDevice();
    strncpy(d.firmwareVersion, "v1.x", sizeof(d.firmwareVersion) - 1);
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_TRUE(a.outdatedFirmware);
    TEST_ASSERT_EQUAL(80, a.score);
}

void test_outdated_firmware_v20_string() {
    auto d = makeDevice();
    strncpy(d.firmwareVersion, "v2.0.1", sizeof(d.firmwareVersion) - 1);
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_TRUE(a.outdatedFirmware);
    TEST_ASSERT_EQUAL(80, a.score);
}

void test_estimated_firmware_not_penalized() {
    // Heuristic estimates contain "(est)" — should not trigger penalty
    auto d = makeDevice();
    strncpy(d.firmwareVersion, "~v1.x or beacon (est)", sizeof(d.firmwareVersion) - 1);
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_FALSE(a.outdatedFirmware);
    TEST_ASSERT_EQUAL(100, a.score);
}

void test_unknown_firmware_not_penalized() {
    auto d = makeDevice();
    strncpy(d.firmwareVersion, "Unknown", sizeof(d.firmwareVersion) - 1);
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_FALSE(a.outdatedFirmware);
}

// ── MeshCore public channel — penalty -40 ────────────────────────────────────

void test_meshcore_public_channel_penalty() {
    auto d = makeDevice();
    strncpy(d.protocol, "MeshCore", sizeof(d.protocol) - 1);
    strncpy(d.meshCoreChannel, "public", sizeof(d.meshCoreChannel) - 1);
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_TRUE(a.meshCorePublic);
    TEST_ASSERT_EQUAL(60, a.score);
    TEST_ASSERT_EQUAL_STRING("moderate", a.rating);
}

// ── MeshCore hashtag channel — penalty -20 ───────────────────────────────────

void test_meshcore_hashtag_channel_penalty() {
    auto d = makeDevice();
    strncpy(d.protocol, "MeshCore", sizeof(d.protocol) - 1);
    strncpy(d.meshCoreChannel, "#general", sizeof(d.meshCoreChannel) - 1);
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_TRUE(a.meshCoreHashtag);
    TEST_ASSERT_EQUAL(80, a.score);
    TEST_ASSERT_EQUAL_STRING("secure", a.rating);
}

void test_meshcore_unknown_channel_no_penalty() {
    auto d = makeDevice();
    strncpy(d.protocol, "MeshCore", sizeof(d.protocol) - 1);
    strncpy(d.meshCoreChannel, "unknown", sizeof(d.meshCoreChannel) - 1);
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_FALSE(a.meshCorePublic);
    TEST_ASSERT_FALSE(a.meshCoreHashtag);
    TEST_ASSERT_EQUAL(100, a.score);
}

void test_meshcore_channel_ignored_for_non_meshcore() {
    // "public" channel string on a Meshtastic device — should not trigger MeshCore check
    auto d = makeDevice();
    strncpy(d.protocol, "Meshtastic", sizeof(d.protocol) - 1);
    strncpy(d.meshCoreChannel, "public", sizeof(d.meshCoreChannel) - 1);
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_FALSE(a.meshCorePublic);
}

// ── Rating thresholds ─────────────────────────────────────────────────────────

void test_rating_secure_at_80() {
    // Router (-10) + hashtag (-20) = 70, still moderate
    // Just router = 90 → secure
    auto d = makeDevice();
    d.isRouter = true;
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_EQUAL(90, a.score);
    TEST_ASSERT_EQUAL_STRING("secure", a.rating);
}

void test_rating_moderate_below_80() {
    // proximity (-15) + router (-10) = 75 → moderate
    auto d = makeDevice();
    d.avgRSSI = -40.0f;
    d.isRouter = true;
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_EQUAL(75, a.score);
    TEST_ASSERT_EQUAL_STRING("moderate", a.rating);
}

void test_rating_vulnerable_below_50() {
    // MeshCore public (-40) + router (-10) = 50 → moderate (boundary)
    // MeshCore public (-40) + router (-10) + chatty (-15) = 35 → vulnerable
    auto d = makeDevice();
    strncpy(d.protocol, "MeshCore", sizeof(d.protocol) - 1);
    strncpy(d.meshCoreChannel, "public", sizeof(d.meshCoreChannel) - 1);
    d.isRouter = true;
    d.packetCount = 150;
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_EQUAL(35, a.score);
    TEST_ASSERT_EQUAL_STRING("vulnerable", a.rating);
}

// ── Combined / accumulation ───────────────────────────────────────────────────

void test_multiple_risks_accumulate() {
    // proximity (-15) + router (-10) + chatty (-15) = 60 → moderate
    auto d = makeDevice();
    d.avgRSSI = -40.0f;
    d.isRouter = true;
    d.packetCount = 150;
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_EQUAL(60, a.score);
    TEST_ASSERT_EQUAL_STRING("moderate", a.rating);
}

void test_score_never_goes_negative() {
    auto d = makeDevice();
    d.avgRSSI = -40.0f;     // -15
    d.isRouter = true;       // -10
    d.packetCount = 150;     // -15
    strncpy(d.protocol, "MeshCore", sizeof(d.protocol) - 1);
    strncpy(d.meshCoreChannel, "public", sizeof(d.meshCoreChannel) - 1);  // -40
    strncpy(d.firmwareVersion, "v1.x", sizeof(d.firmwareVersion) - 1);    // -20
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_GREATER_OR_EQUAL(0, (int)a.score);
}

void test_finding_count_correct() {
    auto d = makeDevice();
    d.avgRSSI = -40.0f;
    d.isRouter = true;
    auto a = SecurityScorer::assess(d);
    // physicalProximity + isRouter = 2
    TEST_ASSERT_EQUAL(2, SecurityScorer::getFindingCount(a));
}

void test_has_findings_true_with_router() {
    auto d = makeDevice();
    d.isRouter = true;
    auto a = SecurityScorer::assess(d);
    TEST_ASSERT_TRUE(SecurityScorer::hasFindings(a));
}

int main() {
    UNITY_BEGIN();

    // Baseline
    RUN_TEST(test_normal_device_is_secure);
    RUN_TEST(test_no_findings_on_clean_device);

    // Proximity
    RUN_TEST(test_proximity_triggers_on_avg_rssi_above_threshold);
    RUN_TEST(test_proximity_does_not_trigger_at_threshold);
    RUN_TEST(test_proximity_uses_avg_not_best_rssi);

    // Router
    RUN_TEST(test_router_penalty);

    // Chatty
    RUN_TEST(test_chatty_penalty);
    RUN_TEST(test_chatty_does_not_trigger_at_100);

    // Intermittent
    RUN_TEST(test_intermittent_penalty);
    RUN_TEST(test_intermittent_does_not_trigger_at_5);

    // Firmware
    RUN_TEST(test_outdated_firmware_v1x_confirmed);
    RUN_TEST(test_outdated_firmware_v1x_string);
    RUN_TEST(test_outdated_firmware_v20_string);
    RUN_TEST(test_estimated_firmware_not_penalized);
    RUN_TEST(test_unknown_firmware_not_penalized);

    // MeshCore channels
    RUN_TEST(test_meshcore_public_channel_penalty);
    RUN_TEST(test_meshcore_hashtag_channel_penalty);
    RUN_TEST(test_meshcore_unknown_channel_no_penalty);
    RUN_TEST(test_meshcore_channel_ignored_for_non_meshcore);

    // Ratings
    RUN_TEST(test_rating_secure_at_80);
    RUN_TEST(test_rating_moderate_below_80);
    RUN_TEST(test_rating_vulnerable_below_50);

    // Combined
    RUN_TEST(test_multiple_risks_accumulate);
    RUN_TEST(test_score_never_goes_negative);
    RUN_TEST(test_finding_count_correct);
    RUN_TEST(test_has_findings_true_with_router);

    return UNITY_END();
}
