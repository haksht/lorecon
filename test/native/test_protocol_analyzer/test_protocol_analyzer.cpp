/**
 * Native unit tests for ProtocolAnalyzer.
 *
 * Tests identifyProtocol() — the core packet classifier — including
 * the fixed LoRaWAN detection that previously had a tautological check
 * (improvement plan #1).
 *
 * Also tests estimateFirmwareVersion() heuristics.
 */
#include <unity.h>
#include "protocol_analyzer.h"

void setUp(void) {}
void tearDown(void) {}

static ProtocolAnalyzer analyzer;

// ==========================================================================
// identifyProtocol() — Protocol classification
// ==========================================================================

void test_short_packet() {
    uint8_t pkt[] = {0x01, 0x02, 0x03};
    TEST_ASSERT_EQUAL_STRING("Short", analyzer.identifyProtocol(pkt, sizeof(pkt)));
}

void test_meshtastic_magic_header() {
    uint8_t pkt[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    TEST_ASSERT_EQUAL_STRING("Meshtastic", analyzer.identifyProtocol(pkt, sizeof(pkt)));
}

void test_beacon_small_packet() {
    // 5 bytes, not Meshtastic magic -> Beacon (length <= 8)
    uint8_t pkt[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    TEST_ASSERT_EQUAL_STRING("Beacon", analyzer.identifyProtocol(pkt, sizeof(pkt)));
}

void test_unknown_large_packet() {
    // 20 bytes of non-matching data
    uint8_t pkt[20] = {0x12, 0x34, 0x56, 0x78};
    TEST_ASSERT_EQUAL_STRING("Unknown", analyzer.identifyProtocol(pkt, sizeof(pkt)));
}

// --- LoRaWAN detection (regression tests for improvement plan #1) ---

void test_lorawan_join_request() {
    // MHDR: mtype=0x00 (join request), major=0x00 -> byte 0 = 0x00
    // Must be exactly 23 bytes
    uint8_t pkt[23] = {0x00};
    TEST_ASSERT_EQUAL_STRING("LoRaWAN", analyzer.identifyProtocol(pkt, 23));
}

void test_lorawan_join_request_wrong_length() {
    // Same MHDR but length 22 — must NOT match
    uint8_t pkt[22] = {0x00};
    TEST_ASSERT_EQUAL_STRING("Unknown", analyzer.identifyProtocol(pkt, 22));
}

void test_lorawan_join_accept_17() {
    // MHDR: mtype=0x01 -> (0x01 << 5) = 0x20, major=0x00
    uint8_t pkt[17] = {0x20};
    TEST_ASSERT_EQUAL_STRING("LoRaWAN", analyzer.identifyProtocol(pkt, 17));
}

void test_lorawan_join_accept_33() {
    // Join Accept with CFList (33 bytes)
    uint8_t pkt[33] = {0x20};
    TEST_ASSERT_EQUAL_STRING("LoRaWAN", analyzer.identifyProtocol(pkt, 33));
}

void test_lorawan_data_unconfirmed_up() {
    // MHDR: mtype=0x02 -> (0x02 << 5) = 0x40, major=0x00
    // Minimum 12 bytes, FCtrl at byte 5 with foptsLen=0
    uint8_t pkt[12] = {0x40, 0x01, 0x02, 0x03, 0x04,  // MHDR + DevAddr
                        0x00,                            // FCtrl (foptsLen=0)
                        0x01, 0x00,                      // FCnt
                        0x00, 0x00, 0x00, 0x00};         // MIC
    TEST_ASSERT_EQUAL_STRING("LoRaWAN", analyzer.identifyProtocol(pkt, 12));
}

void test_lorawan_data_with_fopts() {
    // mtype=0x04 (confirmed up) -> (0x04 << 5) = 0x80
    // FCtrl with foptsLen=3 -> need at least 8+3+4=15 bytes
    uint8_t pkt[15] = {0x80, 0x01, 0x02, 0x03, 0x04,  // MHDR + DevAddr
                        0x03,                            // FCtrl (foptsLen=3)
                        0x01, 0x00,                      // FCnt
                        0xAA, 0xBB, 0xCC,                // FOpts (3 bytes)
                        0x00, 0x00, 0x00, 0x00};         // MIC
    TEST_ASSERT_EQUAL_STRING("LoRaWAN", analyzer.identifyProtocol(pkt, 15));
}

void test_lorawan_bad_major_version_rejected() {
    // MHDR with major=0x01 (not LoRaWAN R1) -> must NOT match
    // mtype=0x00, major=0x01 -> byte 0 = 0x01
    uint8_t pkt[23] = {0x01};
    TEST_ASSERT_EQUAL_STRING("Unknown", analyzer.identifyProtocol(pkt, 23));
}

void test_lorawan_fopts_too_long_rejected() {
    // mtype=0x02 (unconfirmed up) = 0x40, foptsLen=10 but packet only 14 bytes
    // Need 8+10+4=22 bytes, but only have 14 -> should NOT match
    uint8_t pkt[14] = {0x40, 0x01, 0x02, 0x03, 0x04,
                        0x0A,  // FCtrl: foptsLen=10
                        0x01, 0x00};
    TEST_ASSERT_EQUAL_STRING("Unknown", analyzer.identifyProtocol(pkt, 14));
}

void test_lorawan_random_12_byte_not_matched() {
    // 12 bytes that look random — major bits != 0 should prevent false positive
    // byte 0 = 0x73: mtype=3, major=3 -> rejected (major != 0)
    uint8_t pkt[12] = {0x73, 0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78,
                        0x9A, 0xBC, 0xDE, 0xF0};
    TEST_ASSERT_EQUAL_STRING("Unknown", analyzer.identifyProtocol(pkt, 12));
}

// ==========================================================================
// estimateFirmwareVersion() — Heuristic version detection
// ==========================================================================

void test_firmware_meshtastic_encryption_flag() {
    // Meshtastic header + data[8] bit 7 set = v2.2+
    uint8_t pkt[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0, 0x80};
    const char* result = analyzer.estimateFirmwareVersion(pkt, sizeof(pkt), "Meshtastic");
    TEST_ASSERT_NOT_NULL(strstr(result, "v2.2+"));
}

void test_firmware_meshtastic_extended_headers() {
    // >50 bytes without encryption flag = v2.1+
    uint8_t pkt[60] = {0xFF, 0xFF, 0xFF, 0xFF};
    pkt[8] = 0x00;  // No encryption flag
    const char* result = analyzer.estimateFirmwareVersion(pkt, sizeof(pkt), "Meshtastic");
    TEST_ASSERT_NOT_NULL(strstr(result, "v2.1+"));
}

void test_firmware_lorawan() {
    uint8_t pkt[23] = {0x00};  // Join request
    const char* result = analyzer.estimateFirmwareVersion(pkt, sizeof(pkt), "LoRaWAN");
    TEST_ASSERT_NOT_NULL(strstr(result, "LoRaWAN"));
}

void test_firmware_unknown_protocol() {
    uint8_t pkt[8] = {0};
    TEST_ASSERT_EQUAL_STRING("Unknown",
        analyzer.estimateFirmwareVersion(pkt, sizeof(pkt), "Beacon"));
}

int main() {
    UNITY_BEGIN();

    // identifyProtocol
    RUN_TEST(test_short_packet);
    RUN_TEST(test_meshtastic_magic_header);
    RUN_TEST(test_beacon_small_packet);
    RUN_TEST(test_unknown_large_packet);

    // LoRaWAN detection (improvement plan #1 regression tests)
    RUN_TEST(test_lorawan_join_request);
    RUN_TEST(test_lorawan_join_request_wrong_length);
    RUN_TEST(test_lorawan_join_accept_17);
    RUN_TEST(test_lorawan_join_accept_33);
    RUN_TEST(test_lorawan_data_unconfirmed_up);
    RUN_TEST(test_lorawan_data_with_fopts);
    RUN_TEST(test_lorawan_bad_major_version_rejected);
    RUN_TEST(test_lorawan_fopts_too_long_rejected);
    RUN_TEST(test_lorawan_random_12_byte_not_matched);

    // estimateFirmwareVersion
    RUN_TEST(test_firmware_meshtastic_encryption_flag);
    RUN_TEST(test_firmware_meshtastic_extended_headers);
    RUN_TEST(test_firmware_lorawan);
    RUN_TEST(test_firmware_unknown_protocol);

    return UNITY_END();
}
