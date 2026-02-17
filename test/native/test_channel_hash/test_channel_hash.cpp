/**
 * Native unit tests for ChannelHash utility.
 *
 * Tests XOR hash calculation and known-channel lookup table.
 * Zero Arduino dependencies — cleanest test target.
 */
#include <unity.h>
#include "utils/channel_hash.h"

void setUp(void) {}
void tearDown(void) {}

// --- xorBytes ---

void test_xor_bytes_simple() {
    uint8_t data[] = {0x0A, 0x0B};
    TEST_ASSERT_EQUAL_HEX8(0x01, ChannelHash::xorBytes(data, 2));
}

void test_xor_bytes_single() {
    uint8_t data[] = {0x42};
    TEST_ASSERT_EQUAL_HEX8(0x42, ChannelHash::xorBytes(data, 1));
}

void test_xor_bytes_zero_length() {
    uint8_t data[] = {0xFF};
    TEST_ASSERT_EQUAL_HEX8(0x00, ChannelHash::xorBytes(data, 0));
}

// --- getChannelName ---

void test_lookup_longfast() {
    const char* name = ChannelHash::getChannelName(0x08);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_STRING("LongFast", name);
}

void test_lookup_admin() {
    const char* name = ChannelHash::getChannelName(0x6d);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_STRING("admin", name);
}

void test_lookup_unknown_returns_null() {
    TEST_ASSERT_NULL(ChannelHash::getChannelName(0xFF));
}

// --- calculateHash ---

void test_hash_longfast_default_psk() {
    const uint8_t default_psk[] = {
        0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
        0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01
    };
    uint8_t hash = ChannelHash::calculateHash("LongFast", default_psk, 16);
    TEST_ASSERT_EQUAL_HEX8(0x08, hash);
}

void test_hash_empty_name_default_psk() {
    const uint8_t default_psk[] = {
        0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
        0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01
    };
    // XOR("") = 0x00, XOR(psk) = 0x02 -> 0x02
    uint8_t hash = ChannelHash::calculateHash("", default_psk, 16);
    TEST_ASSERT_EQUAL_HEX8(0x02, hash);
}

void test_hash_null_psk() {
    // No PSK: hash = XOR(name) ^ 0 = XOR(name)
    uint8_t hash = ChannelHash::calculateHash("AB", nullptr, 0);
    // 'A'=0x41, 'B'=0x42, XOR = 0x03
    TEST_ASSERT_EQUAL_HEX8(0x03, hash);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_xor_bytes_simple);
    RUN_TEST(test_xor_bytes_single);
    RUN_TEST(test_xor_bytes_zero_length);
    RUN_TEST(test_lookup_longfast);
    RUN_TEST(test_lookup_admin);
    RUN_TEST(test_lookup_unknown_returns_null);
    RUN_TEST(test_hash_longfast_default_psk);
    RUN_TEST(test_hash_empty_name_default_psk);
    RUN_TEST(test_hash_null_psk);
    return UNITY_END();
}
