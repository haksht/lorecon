# T3-S3 V1.3 Port - Handoff Document

**Date:** 2025-02-10 (updated 2026-02-10)
**Branch:** `feature/t3-s3-port`
**COM Port:** COM9
**Status:** RESOLVED

## Summary

Porting ESP32 LoRa Sniffer to LilyGO T3-S3 V1.3 board. OLED, WiFi, and **LoRa radio all working**.

### Resolution (2026-02-10)

**Root cause**: `packet_logger.h` had `#define SD_CS_PIN 5` (hardcoded for Heltec V3). On the T3-S3, GPIO 5 is the LoRa SPI clock (`SPI_SCK`). When `SD.begin(5)` was called during init, it reconfigured GPIO 5 from SPI clock to a regular GPIO output, permanently killing the LoRa SPI bus. The radio appeared to die "after WiFi" because SD init happened before WiFi but nobody checked the radio until after WiFi started.

**Fix**: Removed hardcoded `SD_CS_PIN`, use `Config::Hardware::SD_CS` (=13 on T3-S3). T3-S3 SD card now uses a dedicated SPI bus (HSPI/SPI3 on pins SCK=14, MISO=2, MOSI=11, CS=13), completely isolated from LoRa radio (FSPI/SPI2 on pins SCK=5, MISO=3, MOSI=6, CS=7).

**Verified**: After fix, `standby(): 0` and `setFrequency(906.875): 0` — both return success after full WiFi init.

---

## Original Problem (resolved)

## CRITICAL FINDING: Error -2 Is CHIP_NOT_FOUND

The previous session misidentified error -2 as "PLL lock failure." In RadioLib 6.6.0:

```
#define RADIOLIB_ERR_CHIP_NOT_FOUND  (-2)   // TypeDef.h line 118
```

This is returned by `SPIparseStatus()` when the SPI status byte is `0x00` or `0xFF` — meaning **no chip is responding on the SPI bus**. The SX1262 is not being found.

### Diagnostic Evidence (from this session)

After WiFi setup completes (~60 seconds into boot), USB-CDC is active and diagnostics run:

```
[INFO] === RADIO DIAGNOSTICS ===
[INFO] Radio pointer: OK
[INFO] standby(): -2              ← Even standby() can't talk to chip
[INFO] setFrequency(906.875): -2
[INFO] Frequency failed, attempting full re-init...
[INFO] Re-init begin(906.875, tcxo=1.8): -2   ← Full re-init also fails
[INFO] === END DIAGNOSTICS ===
```

Key insight: `standby()` is just an SPI command (`SetStandby`). It doesn't validate frequency. Returning -2 means **SPI reads 0x00 or 0xFF — the chip is not responding**.

### The Mystery

- `radio->begin()` succeeds during early boot (before WiFi starts)
- The initial `applyConfig()` also succeeds (system doesn't halt)
- After WiFi init (~45 seconds of WiFi scanning/connection attempts), radio is gone
- Even a full hardware reset + `begin()` re-init cannot recover the radio

This suggests **WiFi startup is breaking the SPI bus** or **the pin assignments are conflicting with WiFi/other peripherals**.

## Hardware Identification

- **Confirmed SX1262** (user verified chip markings)
- MCU: ESP32-S3FH4R2
- Flash: 4MB (not 16MB — uses `default.csv` partition table, 1.2MB app)
- Radio: SX1262 (868/915 MHz variant)
- Date code on board: 20250110
- OLED: SSD1306 128x64 (I2C)
- USB: Native USB-CDC

## What Works

| Component | Status | Notes |
|-----------|--------|-------|
| Flash/Partition | OK | `default.csv` (1.2MB app) |
| OLED Display | OK | SDA=17, SCL=18, Addr=0x3C |
| WiFi AP | OK | SSID: `LoRa-809260`, Pass: `recon-809260` |
| Web Server | OK | http://192.168.4.1 |
| Native USB | OK | Serial monitor works via USB-CDC |
| SPI to Radio (early boot) | OK | `begin()` returns 0 before WiFi starts |
| SPI to Radio (after WiFi) | BROKEN | All commands return -2 (CHIP_NOT_FOUND) |

## Current Pin Configuration

```cpp
// In config.h under BOARD_T3_S3:
// SPI Bus
constexpr uint8_t SPI_SCK  = 5;
constexpr uint8_t SPI_MISO = 3;
constexpr uint8_t SPI_MOSI = 6;

// LoRa Module
constexpr uint8_t LORA_NSS  = 7;
constexpr uint8_t LORA_DIO1 = 33;
constexpr uint8_t LORA_RST  = 8;
constexpr uint8_t LORA_BUSY = 34;

// OLED (I2C) — these work
constexpr uint8_t OLED_SDA = 18;
constexpr uint8_t OLED_SCL = 17;
```

**These pins need verification against the V1.3 schematic.** The current values came from LilyGO docs but may be for a different version. The fact that `begin()` works at boot but fails after WiFi suggests either:
1. Pin conflict with WiFi/BLE (ESP32-S3 remaps some GPIOs when WiFi is active)
2. Wrong SPI controller assignment (ESP32-S3 has SPI2/SPI3)
3. Power issue (WiFi TX causes voltage drop, TCXO loses lock)

## Changes Made This Session

### radio_controller.cpp
1. **TCXO voltage passed to `begin()`** — ensures TCXO is configured before calibration (Meshtastic pattern):
   ```cpp
   radio->begin(868.0, 125.0, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10, 8, 1.8, false);
   ```
2. **`standby()` added before `applyConfig()`** — SX1262 requires STDBY before frequency change
3. **`runDiagnostics()` method added** — called after WiFi setup when USB-CDC is active
4. Removed manual hard-reset and manual TCXO config (handled by `begin()`)

### radio_controller.h
- Added `void runDiagnostics();` declaration

### main.cpp
- Added `reconTool.getRadioController()->runDiagnostics()` call after WiFi init

### platformio.ini
- No changes this session (was already correct from previous session)

## Current Code State

### radio_controller.cpp — T3-S3 init path
```cpp
SPI.begin(SCK, MISO, MOSI);  // No SS param
radio = new SX1262(new Module(NSS, DIO1, RST, BUSY));

#if defined(BOARD_T3_S3)
    int state = radio->begin(868.0, 125.0, 9, 7,
        RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10, 8,
        1.8,    // tcxoVoltage
        false); // useRegulatorLDO (DCDC)
    // Returns 0 at boot ✅, returns -2 after WiFi starts ❌

    radio->setDio2AsRfSwitch(true);
#else
    int state = radio->begin();  // Heltec V3 path
#endif
```

### radio_controller.cpp — applyConfig
```cpp
bool RadioController::applyConfig(const ScanConfig& config) {
    radio->standby();  // Added this session
    int state = radio->setFrequency(config.frequency);
    // Returns -2 (CHIP_NOT_FOUND) after WiFi ❌
    ...
}
```

## Next Steps (Priority Order)

### 1. Verify SPI Pins Against V1.3 Schematic (CRITICAL)
Download the actual T3-S3 V1.3 schematic from LilyGO GitHub and verify every pin:
- https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series
- Check if V1.3 uses different SPI pins than V1.2
- Check Meshtastic variant.h for `tlora_t3s3_v1` pins
- **Specific concern:** Do any LoRa SPI pins (3, 5, 6, 7, 33, 34) conflict with WiFi on ESP32-S3?

### 2. Test SPI Bus Isolation
Try initializing the radio AFTER WiFi starts (move radio init after WiFi in main.cpp). If `begin()` fails when called after WiFi, the SPI bus is being stolen/corrupted by WiFi.

### 3. Try Explicit SPI Bus Assignment
ESP32-S3 has SPI2 (FSPI) and SPI3 (HSPI). Try explicitly using SPI3:
```cpp
SPIClass spiLora(HSPI);  // or SPI3
spiLora.begin(SCK, MISO, MOSI);
radio = new SX1262(new Module(NSS, DIO1, RST, BUSY, spiLora));
```

### 4. Check GPIO Conflict Table
ESP32-S3 has restrictions on which GPIOs can be used when WiFi/BLE is active. GPIO 33-34 (DIO1/BUSY) are in the high-numbered GPIO range that may have special behavior. Check the ESP32-S3 Technical Reference Manual.

### 5. Minimal SPI Test After WiFi
Write a bare-minimum sketch that:
1. Starts WiFi AP
2. Does raw SPI read of SX1262 register (e.g., read device version register)
3. Reports what comes back (should be non-zero/non-FF if chip is there)

## What We Ruled Out

| Theory | Status | Evidence |
|--------|--------|----------|
| Wrong radio chip (SX1276) | Ruled out | User confirmed SX1262 on board |
| TCXO voltage wrong | Not the issue | Error is CHIP_NOT_FOUND, not calibration |
| TCXO set after calibration | Fixed but not the root cause | Proper init order implemented |
| Missing standby before freq change | Fixed but not the root cause | standby() itself returns -2 |
| Frequency out of range | Ruled out | All scan freqs are 902-925 MHz (valid) |
| PLL lock failure | Misdiagnosis | -2 is CHIP_NOT_FOUND, not INVALID_FREQUENCY |

## Files Modified (Uncommitted)

```
firmware/src/radio_controller.h   - Added runDiagnostics()
firmware/src/radio_controller.cpp - TCXO in begin(), standby in applyConfig, diagnostics
firmware/src/main.cpp             - Calls runDiagnostics() after WiFi
firmware/src/oled_display.cpp     - T3-S3 I2C pins (previous session)
firmware/src/config.h             - T3-S3 pin definitions (previous session)
platformio.ini                    - [env:t3_s3] section (previous session)
```

## Build Commands

```powershell
# Upload firmware (may need BOOT+RST for USB-CDC)
C:\Users\tim\.platformio\penv\Scripts\pio.exe run -e t3_s3 -t upload --upload-port COM9

# Must use powershell (bash path escaping breaks on Windows)
powershell -Command "& 'C:\Users\tim\.platformio\penv\Scripts\pio.exe' run -e t3_s3 -t upload --upload-port COM9"

# Monitor serial output
powershell -Command "& 'C:\Users\tim\.platformio\penv\Scripts\pio.exe' device monitor --port COM9 --baud 115200"

# Upload web UI to LittleFS
powershell -Command "& 'C:\Users\tim\.platformio\penv\Scripts\pio.exe' run -e t3_s3 -t uploadfs --upload-port COM9"
```

## RadioLib Error Code Reference (v6.6.0)

```
 0  RADIOLIB_ERR_NONE
-1  RADIOLIB_ERR_UNKNOWN
-2  RADIOLIB_ERR_CHIP_NOT_FOUND        ← OUR ERROR (SPI reads 0x00 or 0xFF)
-3  RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED
-4  RADIOLIB_ERR_PACKET_TOO_LONG
-5  RADIOLIB_ERR_TX_TIMEOUT
-705 RADIOLIB_ERR_SPI_CMD_TIMEOUT
-706 RADIOLIB_ERR_SPI_CMD_INVALID
-707 RADIOLIB_ERR_SPI_CMD_FAILED
```

## Reference Links

- [LilyGO LoRa Series GitHub](https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series)
- [RadioLib SX126x docs](https://jgromes.github.io/RadioLib/class_s_x1262.html)
- [T3-S3 Schematic](https://github.com/Xinyuan-LilyGO/T3-S3/blob/main/schematic/T3-S3_V1.3.pdf)
- [Meshtastic T3-S3 variant](https://github.com/meshtastic/firmware/tree/master/variants/esp32s3/tlora_t3s3_v1)
- [ESP32-S3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
