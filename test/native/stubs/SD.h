/**
 * Minimal SD stub for native (host-side) unit tests.
 *
 * Always reports no card present. Used by test_sd_guard to verify that
 * SDUtils::initialize() exits before reaching SD.begin() on boards
 * without HAS_SD_CARD defined.
 */

#ifndef SD_H_STUB
#define SD_H_STUB

#include <cstdint>

// Card type constants (mirror ESP32 SD library)
#define CARD_NONE 0
#define CARD_MMC  1
#define CARD_SD   2
#define CARD_SDHC 3

class SPIClass;

class SDClass {
public:
    // begin() overloads used by sd_utils.h
    bool begin(uint8_t /*cs*/)                                   { return false; }
    bool begin(uint8_t /*cs*/, SPIClass& /*spi*/,
               uint32_t /*freq*/ = 4000000,
               const char* /*mp*/ = "/sd",
               uint8_t /*max*/ = 5,
               bool /*fmt*/ = false)                             { return false; }

    uint8_t  cardType()              { return CARD_NONE; }
    uint64_t cardSize()              { return 0; }
    bool     exists(const char*)     { return false; }
    bool     mkdir(const char*)      { return false; }
};

inline SDClass SD;

#endif // SD_H_STUB
