/**
 * Native unit tests for SDUtils::initialize() HAS_SD_CARD guard.
 *
 * Regression test for the Heltec V3 radio silence bug:
 *
 *   After the loraSPI(FSPI) fix in radio_controller.cpp, the default
 *   SPI object was never begin()'d by radio init. When SD.begin() ran
 *   on Heltec V3 (no HAS_SD_CARD), it called SPI.begin() with the
 *   board's default FSPI pins (not our LoRa pins 9/10/11), reconfiguring
 *   the shared FSPI hardware and silently killing radio reception.
 *
 *   The fix: SDUtils::initialize() returns false immediately when
 *   HAS_SD_CARD is not defined, never touching the SPI bus.
 *
 * These tests run under the native env which defines BOARD_HELTEC_V3
 * and does NOT define HAS_SD_CARD — exactly the broken scenario.
 */

#include <unity.h>
#include "utils/sd_utils.h"

// Reset the SDState singleton between tests so each test starts clean.
static void reset_sd_state() {
    SDUtils::SDState& s = SDUtils::getState();
    s.initialized = false;
    s.available   = false;
    s.cardType    = 0;
}

void setUp()    { reset_sd_state(); SPIClass::begin_called = false; }
void tearDown() {}

// --- Guard behaviour ---

void test_no_sd_card_returns_false() {
    // On a board without HAS_SD_CARD, initialize() must return false.
    TEST_ASSERT_FALSE(SDUtils::initialize());
}

void test_no_sd_card_marks_initialized_not_available() {
    // After the call, state must be initialised=true, available=false
    // so subsequent calls short-circuit and never reach SD.begin().
    SDUtils::initialize();
    const SDUtils::SDState& s = SDUtils::getState();
    TEST_ASSERT_TRUE(s.initialized);
    TEST_ASSERT_FALSE(s.available);
}

void test_no_sd_card_does_not_call_spi_begin() {
    // THE CRITICAL CHECK: SPI.begin() must never be called.
    // Calling it with wrong default pins clobbers the FSPI hardware
    // configuration set up by loraSPI(FSPI) in radio_controller.cpp.
    SPIClass::begin_called = false;
    SDUtils::initialize();
    TEST_ASSERT_FALSE_MESSAGE(SPIClass::begin_called,
        "SPI.begin() must not be called when HAS_SD_CARD is undefined");
}

void test_no_sd_card_second_call_is_noop() {
    // Calling initialize() a second time must also return false without
    // touching SPI (short-circuits on initialized flag).
    SDUtils::initialize();
    SPIClass::begin_called = false;  // Reset after first call
    bool result = SDUtils::initialize();
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(SPIClass::begin_called);
}

void test_is_available_returns_false() {
    // isAvailable() must report false and must not trigger SPI.begin().
    bool avail = SDUtils::isAvailable();
    TEST_ASSERT_FALSE(avail);
    TEST_ASSERT_FALSE(SPIClass::begin_called);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_no_sd_card_returns_false);
    RUN_TEST(test_no_sd_card_marks_initialized_not_available);
    RUN_TEST(test_no_sd_card_does_not_call_spi_begin);
    RUN_TEST(test_no_sd_card_second_call_is_noop);
    RUN_TEST(test_is_available_returns_false);
    return UNITY_END();
}
