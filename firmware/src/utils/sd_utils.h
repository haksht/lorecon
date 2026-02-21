/**
 * SD Card Utilities
 *
 * Shared SD card initialization and status checking.
 * Centralizes SD init to avoid duplicate code across pcap_logger and packet_logger.
 *
 * T3-S3 NOTE: The SD card has dedicated SPI pins (SCK=14, MISO=2, MOSI=11, CS=13)
 * separate from the LoRa radio SPI (SCK=5, MISO=3, MOSI=6). We MUST use a second
 * SPIClass instance (HSPI/SPI3) to avoid stealing GPIO 5 from the LoRa SPI clock.
 * The old hardcoded SD_CS_PIN=5 in packet_logger.h caused exactly this bug.
 */

#ifndef SD_UTILS_H
#define SD_UTILS_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "../config.h"

namespace SDUtils {

/**
 * SD card initialization state (singleton pattern)
 */
struct SDState {
    bool initialized = false;
    bool available = false;
    uint8_t cardType = CARD_NONE;
    uint64_t cardSizeMB = 0;
};

/**
 * Get singleton SD state
 */
inline SDState& getState() {
    static SDState state;
    return state;
}

#if defined(BOARD_T3_S3) || defined(BOARD_TBEAM_SUPREME)
/**
 * Get the dedicated SD SPI bus instance (T3-S3 and T-Beam Supreme).
 * Both boards use HSPI (SPI3) for SD so it doesn't conflict with LoRa on FSPI (SPI2).
 * Pin assignments differ per board but are resolved via Config::Hardware.
 */
inline SPIClass& getSDSPI() {
    static SPIClass sdSPI(HSPI);
    static bool begun = false;
    if (!begun) {
        sdSPI.begin(Config::Hardware::SD_SCK, Config::Hardware::SD_MISO,
                     Config::Hardware::SD_MOSI, Config::Hardware::SD_CS);
        begun = true;
    }
    return sdSPI;
}
#endif

/**
 * Initialize SD card (safe to call multiple times)
 * @return true if SD card is available
 */
inline bool initialize(int csPin = Config::Hardware::SD_CS) {
    SDState& state = getState();

    // Already initialized successfully
    if (state.initialized && state.available) {
        return true;
    }

#if !defined(HAS_SD_CARD)
    // Board has no SD card support. Return immediately WITHOUT calling SD.begin().
    // On Heltec V3 (and similar boards), SD.begin() invokes SPI.begin() with
    // board-default pins that differ from the LoRa SPI pins, reconfiguring the
    // shared FSPI hardware and breaking radio communication after init.
    // (Root cause: loraSPI(FSPI) in radio_controller.cpp never calls begin() on
    // the default SPI object, so SD.begin() finds it uninitialized and re-inits
    // FSPI on wrong GPIO pins.)
    state.initialized = true;
    state.available = false;
    return false;
#endif

    // Try to initialize
#if defined(BOARD_T3_S3) || defined(BOARD_TBEAM_SUPREME)
    // T3-S3 / T-Beam Supreme: SD card on dedicated HSPI bus to avoid LoRa SPI conflict
    if (!SD.begin(Config::Hardware::SD_CS, getSDSPI())) {
#else
    // Heltec V3: SD shares LoRa SPI bus (if external module connected)
    if (!SD.begin(csPin)) {
#endif
        state.initialized = true;
        state.available = false;
        return false;
    }

    // Check card type
    state.cardType = SD.cardType();
    if (state.cardType == CARD_NONE) {
        state.initialized = true;
        state.available = false;
        return false;
    }

    state.cardSizeMB = SD.cardSize() / (1024 * 1024);
    state.initialized = true;
    state.available = true;

    return true;
}

/**
 * Check if SD card is available (tries init if not done)
 */
inline bool isAvailable() {
    SDState& state = getState();
    if (!state.initialized) {
        initialize();
    }
    return state.available;
}

/**
 * Get card type string
 */
inline const char* getCardTypeString() {
    switch (getState().cardType) {
        case CARD_MMC:  return "MMC";
        case CARD_SD:   return "SDSC";
        case CARD_SDHC: return "SDHC";
        default:        return "None";
    }
}

/**
 * Get card size in MB
 */
inline uint64_t getCardSizeMB() {
    return getState().cardSizeMB;
}

/**
 * Ensure directory exists (creates recursively if needed)
 */
inline bool ensureDirectory(const char* path) {
    if (!isAvailable()) return false;
    if (SD.exists(path)) return true;
    return SD.mkdir(path);
}

} // namespace SDUtils

#endif // SD_UTILS_H
