/**
 * SD Card Utilities
 * 
 * Shared SD card initialization and status checking.
 * Centralizes SD init to avoid duplicate code across pcap_logger, packet_logger, device_archiver.
 */

#ifndef SD_UTILS_H
#define SD_UTILS_H

#include <Arduino.h>
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

/**
 * Initialize SD card (safe to call multiple times)
 * @param csPin Chip select pin (default: Config::Hardware::SD_CS)
 * @return true if SD card is available
 */
inline bool initialize(int csPin = Config::Hardware::SD_CS) {
    SDState& state = getState();
    
    // Already initialized successfully
    if (state.initialized && state.available) {
        return true;
    }
    
    // Try to initialize
    if (!SD.begin(csPin)) {
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
