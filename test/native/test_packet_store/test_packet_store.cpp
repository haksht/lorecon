/**
 * Native unit tests for PacketStore.
 *
 * Covers:
 *   - basic capture and retrieval
 *   - field storage: protocol, decryptedText, meshCoreChannel, all header fields
 *   - circular buffer overflow: oldest packet is replaced, count stays at MAX_SLOTS
 *   - deduplication: same packetId not captured twice
 *   - null / zero-length data rejected
 *   - clear and reuse
 *   - out-of-bounds getPacket returns empty packet
 */
#include <unity.h>
#include "repositories/packet_store.h"

void setUp(void) {}
void tearDown(void) {}

// ── helpers ───────────────────────────────────────────────────────────────────

static const uint8_t PKTDATA[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
static constexpr size_t PKTLEN  = sizeof(PKTDATA);

// Minimum-viable capture call — all optional fields at defaults
static bool capture(PacketStore& s, uint32_t nodeId = 0x1234,
                    uint32_t packetId = 0,
                    const char* protocol = "Meshtastic",
                    const char* decryptedText = nullptr,
                    const char* meshCoreChannel = nullptr) {
    return s.capturePacket(PKTDATA, PKTLEN,
                           /*configIndex=*/0, /*rssi=*/-80, /*snr=*/5.0f,
                           nodeId, packetId,
                           /*hopCount=*/3, /*destId=*/0xFFFFFFFF,
                           /*channel=*/0, /*wantAck=*/false,
                           /*viaMqtt=*/false, /*priority=*/0,
                           protocol, decryptedText, meshCoreChannel);
}

// ── basic state ───────────────────────────────────────────────────────────────

void test_empty_on_init() {
    PacketStore s;
    TEST_ASSERT_EQUAL(0, s.count());
    TEST_ASSERT_TRUE(s.isEmpty());
    TEST_ASSERT_FALSE(s.isFull());
}

void test_capture_returns_true() {
    PacketStore s;
    TEST_ASSERT_TRUE(capture(s));
}

void test_count_increments() {
    PacketStore s;
    capture(s, 0x01, 1);
    capture(s, 0x02, 2);
    TEST_ASSERT_EQUAL(2, s.count());
}

void test_is_full_at_capacity() {
    PacketStore s;
    for (uint32_t i = 1; i <= PacketStore::MAX_SLOTS; i++) {
        capture(s, i, i);
    }
    TEST_ASSERT_TRUE(s.isFull());
    TEST_ASSERT_EQUAL(PacketStore::MAX_SLOTS, s.count());
}

// ── field storage ─────────────────────────────────────────────────────────────

void test_packet_data_stored_correctly() {
    PacketStore s;
    capture(s);
    const auto& p = s.getPacket(0);
    TEST_ASSERT_EQUAL(PKTLEN, p.length);
    TEST_ASSERT_EQUAL_MEMORY(PKTDATA, p.data, PKTLEN);
}

void test_protocol_stored() {
    PacketStore s;
    capture(s, 0x01, 1, "MeshCore");
    TEST_ASSERT_EQUAL_STRING("MeshCore", s.getPacket(0).protocol);
}

void test_decrypted_text_stored() {
    PacketStore s;
    capture(s, 0x01, 1, "Meshtastic", "hello world");
    TEST_ASSERT_EQUAL_STRING("hello world", s.getPacket(0).decryptedText);
}

void test_decrypted_text_empty_when_null() {
    PacketStore s;
    capture(s, 0x01, 1, "Meshtastic", nullptr);
    TEST_ASSERT_EQUAL('\0', s.getPacket(0).decryptedText[0]);
}

void test_meshcore_channel_stored() {
    PacketStore s;
    capture(s, 0x02, 2, "MeshCore", "hi", "public");
    TEST_ASSERT_EQUAL_STRING("public", s.getPacket(0).meshCoreChannel);
}

void test_meshcore_channel_empty_when_null() {
    PacketStore s;
    capture(s, 0x02, 2, "MeshCore", nullptr, nullptr);
    TEST_ASSERT_EQUAL('\0', s.getPacket(0).meshCoreChannel[0]);
}

void test_meshcore_channel_room_name() {
    PacketStore s;
    capture(s, 0x03, 3, "MeshCore", "hey", "#general");
    TEST_ASSERT_EQUAL_STRING("#general", s.getPacket(0).meshCoreChannel);
}

void test_header_fields_stored() {
    PacketStore s;
    s.capturePacket(PKTDATA, PKTLEN,
                    /*configIndex=*/2, /*rssi=*/-65, /*snr=*/7.5f,
                    /*nodeId=*/0xDEAD, /*packetId=*/0xBEEF,
                    /*hopCount=*/1, /*destId=*/0x12345678,
                    /*channel=*/3, /*wantAck=*/true,
                    /*viaMqtt=*/false, /*priority=*/2,
                    "Meshtastic", nullptr, nullptr);
    const auto& p = s.getPacket(0);
    TEST_ASSERT_EQUAL_HEX32(0xDEAD,     p.nodeId);
    TEST_ASSERT_EQUAL_HEX32(0xBEEF,     p.packetId);
    TEST_ASSERT_EQUAL_HEX32(0x12345678, p.destId);
    TEST_ASSERT_EQUAL(1,                p.hopCount);
    TEST_ASSERT_EQUAL(3,                p.channel);
    TEST_ASSERT_TRUE(p.wantAck);
    TEST_ASSERT_FALSE(p.viaMqtt);
    TEST_ASSERT_EQUAL(2,                p.priority);
    TEST_ASSERT_EQUAL(2,                p.configIndex);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 7.5f, p.snr);
}

void test_valid_flag_set() {
    PacketStore s;
    capture(s);
    TEST_ASSERT_TRUE(s.getPacket(0).valid);
}

// ── circular buffer overflow ──────────────────────────────────────────────────

void test_overflow_does_not_exceed_max_slots() {
    PacketStore s;
    // Fill past capacity — count must stay at MAX_SLOTS
    for (uint32_t i = 1; i <= PacketStore::MAX_SLOTS + 5; i++) {
        capture(s, i, i);
    }
    TEST_ASSERT_EQUAL(PacketStore::MAX_SLOTS, s.count());
}

void test_overflow_capture_returns_true() {
    PacketStore s;
    for (uint32_t i = 1; i <= PacketStore::MAX_SLOTS; i++) {
        capture(s, i, i);
    }
    // One more — should overwrite oldest, return true
    TEST_ASSERT_TRUE(capture(s, 0xFF, 0xFF));
}

void test_overflow_newest_packet_accessible() {
    // After filling the buffer and wrapping, the most recently captured
    // packet should be retrievable (at slot MAX_SLOTS-1 after wrap).
    PacketStore s;
    for (uint32_t i = 1; i <= PacketStore::MAX_SLOTS; i++) {
        capture(s, i, i, "Meshtastic");
    }
    // Overwrite slot 0 with a MeshCore packet that has a unique channel
    s.capturePacket(PKTDATA, PKTLEN, 0, -70, 6.0f,
                    0xEEEE, 0xEEEE, 0, 0xFFFFFFFF,
                    0, false, false, 0,
                    "MeshCore", "test msg", "#wrapped");
    // The wrapped packet must be somewhere in the store
    bool found = false;
    for (uint8_t i = 0; i < s.count(); i++) {
        if (strcmp(s.getPacket(i).meshCoreChannel, "#wrapped") == 0) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "Wrapped packet not found in circular buffer");
}

// ── deduplication ─────────────────────────────────────────────────────────────

void test_duplicate_packet_id_rejected() {
    PacketStore s;
    capture(s, 0x01, 0xABCD);
    bool second = capture(s, 0x02, 0xABCD);  // same packetId
    TEST_ASSERT_FALSE(second);
    TEST_ASSERT_EQUAL(1, s.count());
}

void test_zero_packet_id_not_deduplicated() {
    // packetId==0 means "unknown" — never deduplicated
    PacketStore s;
    capture(s, 0x01, 0);
    capture(s, 0x02, 0);
    TEST_ASSERT_EQUAL(2, s.count());
}

void test_different_packet_ids_both_stored() {
    PacketStore s;
    capture(s, 0x01, 0x0001);
    capture(s, 0x01, 0x0002);
    TEST_ASSERT_EQUAL(2, s.count());
}

// ── invalid input guards ──────────────────────────────────────────────────────

void test_null_data_rejected() {
    PacketStore s;
    bool result = s.capturePacket(nullptr, 5, 0, -80, 5.0f,
                                  0x01, 0, 3);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(0, s.count());
}

void test_zero_length_rejected() {
    PacketStore s;
    bool result = s.capturePacket(PKTDATA, 0, 0, -80, 5.0f,
                                  0x01, 0, 3);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(0, s.count());
}

// ── out-of-bounds access ──────────────────────────────────────────────────────

void test_get_out_of_bounds_returns_empty() {
    PacketStore s;
    const auto& p = s.getPacket(0);
    TEST_ASSERT_FALSE(p.valid);
    TEST_ASSERT_EQUAL(0, p.length);
}

void test_get_beyond_count_returns_empty() {
    PacketStore s;
    capture(s, 0x01, 1);
    const auto& p = s.getPacket(5);  // only 1 slot filled
    TEST_ASSERT_FALSE(p.valid);
}

// ── clear ─────────────────────────────────────────────────────────────────────

void test_clear_resets_count() {
    PacketStore s;
    capture(s, 0x01, 1);
    capture(s, 0x02, 2);
    s.clear();
    TEST_ASSERT_EQUAL(0, s.count());
    TEST_ASSERT_TRUE(s.isEmpty());
}

void test_clear_allows_recapture_of_same_packet_id() {
    PacketStore s;
    capture(s, 0x01, 0x5555);
    s.clear();
    // After clear, same packetId should be accepted again
    TEST_ASSERT_TRUE(capture(s, 0x01, 0x5555));
    TEST_ASSERT_EQUAL(1, s.count());
}

int main() {
    UNITY_BEGIN();

    // Basic state
    RUN_TEST(test_empty_on_init);
    RUN_TEST(test_capture_returns_true);
    RUN_TEST(test_count_increments);
    RUN_TEST(test_is_full_at_capacity);

    // Field storage
    RUN_TEST(test_packet_data_stored_correctly);
    RUN_TEST(test_protocol_stored);
    RUN_TEST(test_decrypted_text_stored);
    RUN_TEST(test_decrypted_text_empty_when_null);
    RUN_TEST(test_meshcore_channel_stored);
    RUN_TEST(test_meshcore_channel_empty_when_null);
    RUN_TEST(test_meshcore_channel_room_name);
    RUN_TEST(test_header_fields_stored);
    RUN_TEST(test_valid_flag_set);

    // Circular buffer overflow
    RUN_TEST(test_overflow_does_not_exceed_max_slots);
    RUN_TEST(test_overflow_capture_returns_true);
    RUN_TEST(test_overflow_newest_packet_accessible);

    // Deduplication
    RUN_TEST(test_duplicate_packet_id_rejected);
    RUN_TEST(test_zero_packet_id_not_deduplicated);
    RUN_TEST(test_different_packet_ids_both_stored);

    // Invalid input
    RUN_TEST(test_null_data_rejected);
    RUN_TEST(test_zero_length_rejected);

    // Out-of-bounds
    RUN_TEST(test_get_out_of_bounds_returns_empty);
    RUN_TEST(test_get_beyond_count_returns_empty);

    // Clear
    RUN_TEST(test_clear_resets_count);
    RUN_TEST(test_clear_allows_recapture_of_same_packet_id);

    return UNITY_END();
}
