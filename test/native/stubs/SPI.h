/**
 * Minimal SPI stub for native (host-side) unit tests.
 *
 * Tracks whether begin() was called and with what pins, so tests can
 * verify that SD initialization does NOT call SPI.begin() on boards
 * without SD card support (the Heltec V3 FSPI-clobbering bug).
 */

#ifndef SPI_H_STUB
#define SPI_H_STUB

#include <cstdint>

class SPIClass {
public:
    explicit SPIClass(int /*bus*/ = 0) {}

    void begin(int sck = -1, int /*miso*/ = -1, int /*mosi*/ = -1, int /*cs*/ = -1) {
        begin_called = true;
        begin_sck    = sck;
    }

    // Test inspection — reset between tests
    static bool begin_called;
    static int  begin_sck;
};

// Definitions (inline so this header can be included in multiple TUs)
inline bool SPIClass::begin_called = false;
inline int  SPIClass::begin_sck    = -1;

inline SPIClass SPI;

#endif // SPI_H_STUB
