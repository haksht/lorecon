# T-Deck Plus Hardware Investigation

**Date:** November 2025  
**Status:** ❌ Not Recommended  
**Hardware:** LilyGO T-Deck Plus (ESP32-S3 + SX1262 with TCXO + GC9A01 240x240 round display)

## Summary

After extensive investigation, **T-Deck Plus hardware is not recommended** for this LoRa reconnaissance tool due to fundamental hardware design limitations that make sustained operation impossible.

### Why Not Recommended

The T-Deck Plus has two critical hardware design flaws that cannot be resolved through software:

1. **Shared Power Rail**: GPIO 10 (PERI_POWERON) powers BOTH the LoRa radio AND the TFT display via the same LDO rail. Any attempt to power-cycle or reset one component affects the other.

2. **Shared SPI Bus Without Arbitration**: The HSPI bus (SCK:40 MISO:38 MOSI:41) is shared between:
   - LoRa radio (SX1262, CS:9)
   - TFT display (GC9A01, CS:12)
   - SD card (CS:39)
   
   Neither RadioLib nor Arduino_GFX coordinate bus access, leading to transaction corruption and display blanking when both are active.

## Hardware Issues Identified

### Issue 1: Shared Power Rail (PERI_POWERON)

**Problem**: GPIO 10 controls a single LDO that provides power to both peripherals.

**Impact**:
- Cannot reset radio without killing display
- Cannot implement proper radio recovery mechanisms
- Power cycling breaks both components simultaneously

**Code Evidence**:
```cpp
// From config.h - T-Deck Plus pin definitions
namespace Config::Hardware {
    constexpr int PERI_POWERON = 10;  // Powers BOTH radio AND display
}
```

### Issue 2: Shared SPI Bus

**Problem**: HSPI bus shared between three devices with no hardware arbitration.

**Impact**:
- Display goes black within seconds of radio operation
- SPI transactions from radio corrupt display state
- GC9A01 controller enters display-off state and ignores `displayOn()` commands
- Arduino_GFX objects become invalid if SPI bus is reinitialized

**Pin Configuration**:
```
HSPI Bus:
  SCK: GPIO 40
  MISO: GPIO 38
  MOSI: GPIO 41

Devices:
  Radio (SX1262):  CS = GPIO 9
  Display (GC9A01): CS = GPIO 12
  SD Card:          CS = GPIO 39
```

### Issue 3: GC9A01 Display Behavior

**Problem**: GC9A01 round display exhibits persistent blanking issue.

**Symptoms**:
- Display shows content initially (test colors, welcome screen work)
- Goes completely black within 5-10 seconds of radio starting operations
- Backlight stays on but no pixels display
- `displayOn()` command has no effect once blanked
- Periodic refresh attempts fail to restore display

**Evidence**:
```cpp
// From tft_display.cpp - periodic refresh attempts
void TFTDisplay::renderScanning() {
    // Aggressively ensure display is awake
    gfx->displayOn();
    delay(10);  // Give display time to wake up
    
    // Also ensure backlight is on
    if (Config::Hardware::TDECK_TFT_BL >= 0) {
        digitalWrite(Config::Hardware::TDECK_TFT_BL, ...);
    }
    // ... rendering code executes successfully but display stays black
}
```

### Issue 4: Radio Recovery Impossible

**Problem**: SX1262 encounters errors (-701 "wrong modem", -705 "chip not found") after 3-4 configuration changes, but recovery mechanisms break everything.

**Why Recovery Fails**:
- Power cycling PERI_POWERON kills the display
- SPI bus reinitialization invalidates Arduino_GFX objects causing crashes
- Hardware reset alone insufficient for SX1262 recovery
- Any aggressive intervention breaks the fragile multi-device SPI arrangement

**Code Evidence**:
```cpp
// Radio recovery lambda that triggers after wrong modem errors
auto recoverLoRaMode = [&](const char* source) -> bool {
    // Can only call radio.begin() again - no hardware reset
    // Hardware reset would kill display on shared power rail
    int state = radio.begin(config.frequency, ...);
    // This often fails with -705 and cannot be recovered
};
```

## What Worked

Initial hardware bring-up was partially successful:

1. ✅ **Radio initialization** with TCXO configuration (1.8V on DIO3)
2. ✅ **Display initialization** showing test colors
3. ✅ **First 2-3 scan operations** before errors accumulate
4. ✅ **WiFi AP and web interface** function correctly

## What Failed

Sustained operation is impossible:

1. ❌ **Display persistence** - goes black within seconds, unrecoverable
2. ❌ **Radio recovery** - any error leads to permanent failure
3. ❌ **Sustained scanning** - breaks after 3-4 configuration changes
4. ❌ **SPI bus management** - no way to coordinate access between devices

## Technical Details

### TCXO Configuration (Required)

T-Deck Plus uses a TCXO (not crystal oscillator) for SX1262 clock:

```cpp
// Critical: SX1262 requires TCXO voltage specified
int state = radio.begin(
    915.0,      // Frequency
    125.0,      // Bandwidth
    9,          // Spreading factor
    7,          // Coding rate
    0x12,       // Sync word
    10,         // Power
    8,          // Preamble length
    1.8,        // TCXO voltage - REQUIRED for T-Deck!
    false       // Use DC-DC regulator
);
```

Without the `1.8` TCXO parameter, initialization fails with -705 (RADIOLIB_ERR_CHIP_NOT_FOUND).

### Display Initialization (Partial Success)

```cpp
// GC9A01 round display initialization works
bus = new Arduino_ESP32SPI(
    Config::Hardware::TDECK_TFT_DC,
    Config::Hardware::TDECK_TFT_CS,
    busConfig.sck,
    busConfig.mosi,
    busConfig.miso,
    spiHost,
    true  // shared SPI
);

gfx = new Arduino_GC9A01(
    bus,
    Config::Hardware::TDECK_TFT_RST,
    Config::Hardware::TDECK_TFT_ROTATION,
    true,
    240, 240  // Round display
);

gfx->begin();  // Succeeds, shows test colors
// But goes black once radio starts operating
```

## Attempted Solutions (All Failed)

### 1. Power Cycling During Recovery ❌
**Approach**: Toggle PERI_POWERON to reset radio  
**Result**: Kills display, doesn't fix radio, unsustainable

### 2. SPI Bus Reinitialization ❌
**Approach**: `SPI.end()` + `SPI.begin()` to reset bus  
**Result**: Arduino_GFX objects become invalid, causes LoadProhibited crashes

### 3. Periodic Display Refresh ❌
**Approach**: Redraw display every 2 seconds  
**Result**: Code executes successfully but display stays black

### 4. Aggressive displayOn() Calls ❌
**Approach**: Call `gfx->displayOn()` before each render  
**Result**: No effect on blank display, hardware unresponsive

### 5. Extended Reset Pulses ❌
**Approach**: Increase LORA_RST pulse duration (50ms → 100ms)  
**Result**: Doesn't prevent errors or aid recovery

### 6. Remove Hardware Reset from Recovery ❌
**Approach**: Only use `radio.begin()` without hardware intervention  
**Result**: Radio stays broken with -705 errors

### 7. Display-Only Refresh Without SPI Reinit ❌
**Approach**: Refresh display without touching bus  
**Result**: Display hardware ignores all commands once blanked

## Hardware Comparison

| Feature | T-Deck Plus | Heltec V3 | T-Deck (Standard) |
|---------|-------------|-----------|-------------------|
| **Display** | GC9A01 240x240 round | SSD1306 128x64 OLED | ST7789 320x240 rectangular |
| **Display Power** | Shared with radio | Independent (Vext) | Shared with radio |
| **SPI Bus** | Shared HSPI | Independent I2C | Shared HSPI |
| **Radio** | SX1262 with TCXO | SX1262 | SX1262 |
| **Radio Power** | Shared with display | Independent | Shared with display |
| **Button** | None | GPIO 0 | Multiple (keyboard) |
| **Keyboard** | Yes (I2C) | No | Yes (I2C) |
| **Trackball** | Yes | No | Yes |
| **SD Card** | Yes (shared SPI) | Yes (separate SPI) | Yes (shared SPI) |
| **Suitability** | ❌ Poor | ✅ Excellent | ⚠️ Acceptable |

## Conclusion

**T-Deck Plus is not recommended** for this LoRa reconnaissance tool due to:

1. Shared power rail makes radio recovery impossible
2. Shared SPI bus causes persistent display issues
3. GC9A01 display becomes unresponsive during radio operations
4. No software solution possible for these hardware-level conflicts

### Recommended Hardware

See the main [BUILD_GUIDE](../user-guides/BUILD_GUIDE.md) for the full list of supported boards:
- **Heltec WiFi LoRa 32 V3 / V4** — lowest cost, proven stable, I2C display (no SPI conflicts)
- **LilyGO T3-S3** — native SD card, separate SPI buses
- **LilyGO T-Beam Supreme** — onboard GPS + PMIC + SD

### For Future Hardware Selection

When evaluating hardware platforms for multi-peripheral projects:

✅ **Look for:**
- Independent power domains for peripherals
- Separate SPI buses or proper arbitration
- Display types with robust state management
- Hardware reset capabilities per component

❌ **Avoid:**
- Shared power rails for critical components
- Shared high-speed buses without arbitration
- Display controllers that enter unrecoverable states
- Hardware designs that prevent proper error recovery

## Investigation Timeline

1. **Initial Testing**: Radio initialization with TCXO succeeded, display showed test colors
2. **First Issues**: Display blanked after radio started, "wrong modem" errors after few scans
3. **Recovery Attempts**: Multiple approaches to hardware reset all failed or caused crashes
4. **SPI Bus Investigation**: Discovered shared bus without arbitration as root cause
5. **Display Analysis**: GC9A01 enters display-off state, unresponsive to commands
6. **Final Assessment**: Hardware design incompatible with application requirements

## Lessons Learned

1. **Hardware architecture matters**: Software cannot overcome fundamental hardware design flaws
2. **Shared resources need arbitration**: Multiple high-speed devices on shared buses require proper coordination
3. **Power domain separation is critical**: Independent power control enables proper recovery mechanisms
4. **Display controller choice matters**: Some displays are more resilient to bus conflicts than others
5. **Time-box investigations**: After multiple failed approaches, reassess hardware viability

## References

- RadioLib documentation: https://github.com/jgromes/RadioLib
- Arduino_GFX documentation: https://github.com/moononournation/Arduino_GFX
- T-Deck Plus schematic: (vendor documentation)
- SX1262 datasheet: (Semtech)
- GC9A01 datasheet: (GigaDevice)

---

**Investigation Status:** Completed November 2025
**Decision:** Do not support T-Deck Plus hardware
**Recommendation:** Use any of the supported boards (Heltec V3/V4, T3-S3, T-Beam Supreme)
