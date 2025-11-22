# T-Deck Hardware Support Status

**Last Updated:** November 2025

## Quick Summary

| Hardware | Status | Notes |
|----------|--------|-------|
| **Heltec WiFi LoRa 32 V3** | ✅ **Fully Supported** | Recommended platform - tested and production-ready |
| **T-Deck Plus** | ❌ **Not Supported** | Hardware incompatibilities prevent reliable operation |
| **T-Deck Standard** | ❓ **Untested** | May work but not verified - use at your own risk |

## T-Deck Plus Investigation Results

A comprehensive investigation into T-Deck Plus support identified fundamental hardware limitations:

- **Shared power rail** (PERI_POWERON) powers both radio and display
- **Shared SPI bus** without proper arbitration between radio, display, and SD card
- **Display blanking** - GC9A01 goes black during radio operations and cannot be recovered
- **Radio recovery impossible** - error handling mechanisms break due to hardware constraints

**Full details:** See [`docs/TDECK_PLUS_INVESTIGATION.md`](docs/TDECK_PLUS_INVESTIGATION.md)

## Recommended Hardware

### Heltec WiFi LoRa 32 V3 ✅

**Why it works well:**
- Independent power control (Vext for display)
- OLED uses I2C (no SPI conflicts with radio)
- User button for display toggle and shutdown
- Proven stable in field use
- Full documentation and support

**Specifications:**
- ESP32-S3 @ 240MHz
- SX1262 LoRa radio
- 128x64 OLED display
- User button (GPIO 0)
- Optional SD card support

## Board Abstraction Architecture

The codebase includes a board abstraction layer to potentially support multiple hardware variants:

- `firmware/src/boards/board_config.h` - Hardware definitions
- `firmware/src/boards/board_config.cpp` - Board-specific configurations
- `firmware/src/bus_manager.h/cpp` - Shared SPI bus management
- `firmware/src/display_interface.h` - Display abstraction
- `firmware/src/oled_display.h/cpp` - OLED implementation (Heltec)
- `firmware/src/tft_display.h/cpp` - TFT implementation (T-Deck variants)

**Note:** While the architecture supports multiple boards, **only Heltec V3 is recommended for production use**.

## Build Environment

Current `platformio.ini` environments:

```ini
[platformio]
default_envs = heltec-v3

[env:heltec-v3]
# Fully supported and tested

[env:tdeck]
# Standard T-Deck - untested

[env:tdeck-plus]
# T-Deck Plus - not recommended (kept for reference)
```

To build for Heltec V3:
```bash
pio run -e heltec-v3 --target upload
```

## Future Hardware Support

If you're considering porting to new hardware, review the investigation document to understand requirements:

✅ **Look for:**
- Independent power domains for peripherals
- Separate SPI buses or proper arbitration
- Proven display controller compatibility
- Hardware reset capabilities per component

❌ **Avoid:**
- Shared power rails for critical components
- Shared high-speed buses without coordination
- Display types prone to unrecoverable states

## Getting Help

- For Heltec V3 issues: Check `docs/TROUBLESHOOTING_MESHTASTIC.md` and `QUICKSTART.md`
- For T-Deck Plus questions: See `docs/TDECK_PLUS_INVESTIGATION.md`
- For build issues: See `docs/BUILD_GUIDE.md`

---

**Bottom Line:** Use Heltec WiFi LoRa 32 V3 for reliable field operation.
