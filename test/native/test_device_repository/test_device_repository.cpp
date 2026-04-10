/**
 * Native unit tests for DeviceRepository.
 *
 * Covers:
 *   - add / update lifecycle
 *   - originated vs relayed packet counting for all three hop semantics:
 *       Meshtastic (countdown), MeshCore (upcount), LoRaWAN/ISM (0xFF sentinel)
 *   - router detection
 *   - RSSI tracking (Welford's running average / best / last)
 *   - find / remove / evict
 *   - capacity limit
 *
 * The originated/relayed tests are regression tests for the bug where
 * hopCount==0xFF left both counters at 0 indefinitely.
 */
#include <unity.h>
#include "repositories/device_repository.h"

void setUp(void) {}
void tearDown(void) {}

// ── helpers ──────────────────────────────────────────────────────────────────

static DeviceRepository makeRepo() {
    DeviceRepository r;
    r.initialize();
    return r;
}

static const uint8_t DUMMY_PKT[] = {0x01, 0x02, 0x03, 0x04, 0x05};
static constexpr size_t DUMMY_LEN = sizeof(DUMMY_PKT);

// ── add / update basics ───────────────────────────────────────────────────────

void test_empty_on_init() {
    auto r = makeRepo();
    TEST_ASSERT_EQUAL(0, r.count());
    TEST_ASSERT_TRUE(r.isEmpty());
}

void test_add_new_device() {
    auto r = makeRepo();
    auto* d = r.addOrUpdate(0xDEADBEEF, 0, -80.0f, "Meshtastic",
                            DUMMY_PKT, DUMMY_LEN, 3);
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQUAL(1, r.count());
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, d->nodeId);
}

void test_same_node_id_updates_not_adds() {
    auto r = makeRepo();
    r.addOrUpdate(0x11111111, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 3);
    r.addOrUpdate(0x11111111, 0, -75.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 3);
    TEST_ASSERT_EQUAL(1, r.count());
}

void test_packet_count_increments_on_update() {
    auto r = makeRepo();
    r.addOrUpdate(0xAAAAAAAA, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 3);
    r.addOrUpdate(0xAAAAAAAA, 0, -78.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 3);
    r.addOrUpdate(0xAAAAAAAA, 0, -76.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 3);
    const auto* d = r.findByNodeId(0xAAAAAAAA);
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQUAL(3, d->packetCount);
}

// ── RSSI tracking ─────────────────────────────────────────────────────────────

void test_best_rssi_tracked() {
    auto r = makeRepo();
    r.addOrUpdate(0x11, 0, -90.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 3);
    r.addOrUpdate(0x11, 0, -60.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 3);
    r.addOrUpdate(0x11, 0, -85.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 3);
    const auto* d = r.findByNodeId(0x11);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -60.0f, d->bestRSSI);
}

void test_last_rssi_is_most_recent() {
    auto r = makeRepo();
    r.addOrUpdate(0x22, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 3);
    r.addOrUpdate(0x22, 0, -55.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 3);
    const auto* d = r.findByNodeId(0x22);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -55.0f, d->lastRSSI);
}

void test_avg_rssi_welford() {
    // 3 packets at -80, -80, -80 → avg = -80
    auto r = makeRepo();
    r.addOrUpdate(0x33, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 3);
    r.addOrUpdate(0x33, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 3);
    r.addOrUpdate(0x33, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 3);
    const auto* d = r.findByNodeId(0x33);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, -80.0f, d->avgRSSI);
}

// ── Meshtastic hop semantics (countdown) ─────────────────────────────────────

void test_meshtastic_first_packet_counts_as_originated() {
    // hopCount == maxHopCount (3) → originated
    auto r = makeRepo();
    auto* d = r.addOrUpdate(0x01, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 3);
    TEST_ASSERT_EQUAL(1, d->originatedPackets);
    TEST_ASSERT_EQUAL(0, d->relayedPackets);
}

void test_meshtastic_lower_hop_counts_as_relayed() {
    // First packet at hopCount=3 (max), second at hopCount=1 (< max) → relayed
    auto r = makeRepo();
    r.addOrUpdate(0x01, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 3);
    r.addOrUpdate(0x01, 0, -75.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 1);
    const auto* d = r.findByNodeId(0x01);
    TEST_ASSERT_EQUAL(1, d->originatedPackets);
    TEST_ASSERT_EQUAL(1, d->relayedPackets);
}

void test_meshtastic_router_flagged_after_two_relays() {
    auto r = makeRepo();
    r.addOrUpdate(0x01, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 3);
    r.addOrUpdate(0x01, 0, -75.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 1);
    const auto* d = r.findByNodeId(0x01);
    TEST_ASSERT_FALSE(d->isRouter);  // only 1 relay so far

    r.addOrUpdate(0x01, 0, -75.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 1);
    d = r.findByNodeId(0x01);
    TEST_ASSERT_TRUE(d->isRouter);   // 2 relays → router
}

void test_meshtastic_max_hop_count_updates() {
    // If a later packet arrives with a higher hop count, maxHopCount is updated
    auto r = makeRepo();
    r.addOrUpdate(0x01, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 2);
    r.addOrUpdate(0x01, 0, -75.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN, 3);
    const auto* d = r.findByNodeId(0x01);
    TEST_ASSERT_EQUAL(3, d->maxHopCount);
}

// ── MeshCore hop semantics (upcount) ─────────────────────────────────────────

void test_meshcore_hop0_is_originated() {
    // hopCount=0 → fresh from originator
    auto r = makeRepo();
    auto* d = r.addOrUpdate(0x02, 0, -70.0f, "MeshCore", DUMMY_PKT, DUMMY_LEN, 0);
    TEST_ASSERT_EQUAL(1, d->originatedPackets);
    TEST_ASSERT_EQUAL(0, d->relayedPackets);
}

void test_meshcore_nonzero_hop_is_relayed() {
    // hopCount=2 → traversed 2 relays, not direct
    auto r = makeRepo();
    auto* d = r.addOrUpdate(0x02, 0, -70.0f, "MeshCore", DUMMY_PKT, DUMMY_LEN, 2);
    TEST_ASSERT_EQUAL(0, d->originatedPackets);
    TEST_ASSERT_EQUAL(1, d->relayedPackets);
}

void test_meshcore_router_after_two_relayed() {
    auto r = makeRepo();
    r.addOrUpdate(0x02, 0, -70.0f, "MeshCore", DUMMY_PKT, DUMMY_LEN, 0);
    r.addOrUpdate(0x02, 0, -68.0f, "MeshCore", DUMMY_PKT, DUMMY_LEN, 1);
    const auto* d = r.findByNodeId(0x02);
    TEST_ASSERT_FALSE(d->isRouter);

    r.addOrUpdate(0x02, 0, -68.0f, "MeshCore", DUMMY_PKT, DUMMY_LEN, 1);
    d = r.findByNodeId(0x02);
    TEST_ASSERT_TRUE(d->isRouter);
}

// ── LoRaWAN / ISM / RadioHead (hopCount == 0xFF, no hop semantics) ────────────

void test_lorawan_first_packet_counts_as_originated() {
    // Regression test: before fix, both counters were left at 0
    auto r = makeRepo();
    auto* d = r.addOrUpdate(0x03, 0, -85.0f, "LoRaWAN", DUMMY_PKT, DUMMY_LEN, 0xFF);
    TEST_ASSERT_EQUAL(1, d->originatedPackets);
    TEST_ASSERT_EQUAL(0, d->relayedPackets);
}

void test_lorawan_subsequent_packets_increment_originated() {
    // All packets from hop-less protocols count as originated
    auto r = makeRepo();
    r.addOrUpdate(0x03, 0, -85.0f, "LoRaWAN", DUMMY_PKT, DUMMY_LEN, 0xFF);
    r.addOrUpdate(0x03, 0, -83.0f, "LoRaWAN", DUMMY_PKT, DUMMY_LEN, 0xFF);
    r.addOrUpdate(0x03, 0, -84.0f, "LoRaWAN", DUMMY_PKT, DUMMY_LEN, 0xFF);
    const auto* d = r.findByNodeId(0x03);
    TEST_ASSERT_EQUAL(3, d->originatedPackets);
    TEST_ASSERT_EQUAL(0, d->relayedPackets);
    TEST_ASSERT_EQUAL(3, d->packetCount);
}

void test_lorawan_never_flagged_as_router() {
    auto r = makeRepo();
    for (int i = 0; i < 10; i++) {
        r.addOrUpdate(0x03, 0, -80.0f, "LoRaWAN", DUMMY_PKT, DUMMY_LEN, 0xFF);
    }
    const auto* d = r.findByNodeId(0x03);
    TEST_ASSERT_FALSE(d->isRouter);
}

void test_ism_same_as_lorawan_behavior() {
    auto r = makeRepo();
    r.addOrUpdate(0x04, 0, -90.0f, "ISM", DUMMY_PKT, DUMMY_LEN, 0xFF);
    r.addOrUpdate(0x04, 0, -88.0f, "ISM", DUMMY_PKT, DUMMY_LEN, 0xFF);
    const auto* d = r.findByNodeId(0x04);
    TEST_ASSERT_EQUAL(2, d->originatedPackets);
    TEST_ASSERT_EQUAL(0, d->relayedPackets);
    TEST_ASSERT_FALSE(d->isRouter);
}

// ── find / remove ─────────────────────────────────────────────────────────────

void test_find_by_node_id_existing() {
    auto r = makeRepo();
    r.addOrUpdate(0xBEEF, 0, -70.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN);
    TEST_ASSERT_NOT_NULL(r.findByNodeId(0xBEEF));
}

void test_find_by_node_id_missing() {
    auto r = makeRepo();
    TEST_ASSERT_NULL(r.findByNodeId(0xDEAD));
}

void test_remove_existing_device() {
    auto r = makeRepo();
    r.addOrUpdate(0xAAAA, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN);
    TEST_ASSERT_TRUE(r.removeByNodeId(0xAAAA));
    TEST_ASSERT_EQUAL(0, r.count());
    TEST_ASSERT_NULL(r.findByNodeId(0xAAAA));
}

void test_remove_missing_device_returns_false() {
    auto r = makeRepo();
    TEST_ASSERT_FALSE(r.removeByNodeId(0x9999));
}

void test_remove_middle_device_preserves_others() {
    auto r = makeRepo();
    r.addOrUpdate(0x01, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN);
    r.addOrUpdate(0x02, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN);
    r.addOrUpdate(0x03, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN);
    r.removeByNodeId(0x02);
    TEST_ASSERT_EQUAL(2, r.count());
    TEST_ASSERT_NOT_NULL(r.findByNodeId(0x01));
    TEST_ASSERT_NULL(r.findByNodeId(0x02));
    TEST_ASSERT_NOT_NULL(r.findByNodeId(0x03));
}

// ── clear ─────────────────────────────────────────────────────────────────────

void test_clear_resets_count() {
    auto r = makeRepo();
    r.addOrUpdate(0x01, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN);
    r.addOrUpdate(0x02, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN);
    r.clear();
    TEST_ASSERT_EQUAL(0, r.count());
    TEST_ASSERT_TRUE(r.isEmpty());
}

void test_clear_allows_reuse() {
    auto r = makeRepo();
    r.addOrUpdate(0x01, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN);
    r.clear();
    auto* d = r.addOrUpdate(0x01, 0, -70.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN);
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQUAL(1, r.count());
}

// ── null / zero node ID guard ─────────────────────────────────────────────────

void test_zero_node_id_rejected() {
    auto r = makeRepo();
    auto* d = r.addOrUpdate(0x00, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN);
    TEST_ASSERT_NULL(d);
    TEST_ASSERT_EQUAL(0, r.count());
}

// ── get by index ──────────────────────────────────────────────────────────────

void test_get_by_index_valid() {
    auto r = makeRepo();
    r.addOrUpdate(0xABCD, 0, -80.0f, "Meshtastic", DUMMY_PKT, DUMMY_LEN);
    const auto& d = r.getByIndex(0);
    TEST_ASSERT_EQUAL_HEX32(0xABCD, d.nodeId);
}

void test_get_by_index_out_of_bounds_returns_empty() {
    auto r = makeRepo();
    const auto& d = r.getByIndex(0);
    // Empty device has nodeId == 0
    TEST_ASSERT_EQUAL_HEX32(0x00000000, d.nodeId);
}

// ── protocol stored correctly ─────────────────────────────────────────────────

void test_protocol_stored_on_new_device() {
    auto r = makeRepo();
    auto* d = r.addOrUpdate(0xFF, 0, -80.0f, "MeshCore", DUMMY_PKT, DUMMY_LEN, 0);
    TEST_ASSERT_EQUAL_STRING("MeshCore", d->protocol);
}

int main() {
    UNITY_BEGIN();

    // Basics
    RUN_TEST(test_empty_on_init);
    RUN_TEST(test_add_new_device);
    RUN_TEST(test_same_node_id_updates_not_adds);
    RUN_TEST(test_packet_count_increments_on_update);

    // RSSI
    RUN_TEST(test_best_rssi_tracked);
    RUN_TEST(test_last_rssi_is_most_recent);
    RUN_TEST(test_avg_rssi_welford);

    // Meshtastic hop semantics (countdown)
    RUN_TEST(test_meshtastic_first_packet_counts_as_originated);
    RUN_TEST(test_meshtastic_lower_hop_counts_as_relayed);
    RUN_TEST(test_meshtastic_router_flagged_after_two_relays);
    RUN_TEST(test_meshtastic_max_hop_count_updates);

    // MeshCore hop semantics (upcount)
    RUN_TEST(test_meshcore_hop0_is_originated);
    RUN_TEST(test_meshcore_nonzero_hop_is_relayed);
    RUN_TEST(test_meshcore_router_after_two_relayed);

    // LoRaWAN / ISM (no hop field — regression tests for 0xFF bug)
    RUN_TEST(test_lorawan_first_packet_counts_as_originated);
    RUN_TEST(test_lorawan_subsequent_packets_increment_originated);
    RUN_TEST(test_lorawan_never_flagged_as_router);
    RUN_TEST(test_ism_same_as_lorawan_behavior);

    // Find / remove
    RUN_TEST(test_find_by_node_id_existing);
    RUN_TEST(test_find_by_node_id_missing);
    RUN_TEST(test_remove_existing_device);
    RUN_TEST(test_remove_missing_device_returns_false);
    RUN_TEST(test_remove_middle_device_preserves_others);

    // Clear
    RUN_TEST(test_clear_resets_count);
    RUN_TEST(test_clear_allows_reuse);

    // Guards
    RUN_TEST(test_zero_node_id_rejected);
    RUN_TEST(test_get_by_index_valid);
    RUN_TEST(test_get_by_index_out_of_bounds_returns_empty);
    RUN_TEST(test_protocol_stored_on_new_device);

    return UNITY_END();
}
