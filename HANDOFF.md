# Session Handoff

## Session Date: 2026-02-21

## What Was Done

### T-Beam Supreme Board Support (Primary Feature)
Added full support for the LilyGo T-Beam Supreme as the third supported board, including AXP2101 PMIC, built-in GPS, SH1106 OLED, SD card, and SX1262 radio.

**Key architectural constraints**:
- AXP2101 PMIC is **mandatory** — it powers LoRa (ALDO3), GPS (ALDO4), SD (BLDO1), display (ALDO1/2). Must init before all other peripherals.
- Separate I2C buses: display on Wire (GPIO 17/18), PMU on Wire1 (GPIO 42/41).
- SH1106 controller (not SSD1306) — requires different U8g2 driver class.
- 8MB QIO flash requires custom partition table.

**Files created**:
| File | Purpose |
|------|---------|
| `firmware/partitions_8MB.csv` | 8MB partition table (~3MB app + ~5MB LittleFS) |
| `firmware/src/utils/pmu_controller.h` | AXP2101 PMIC init (HAS_AXP2101 guard) |
| `firmware/src/gps_controller.h` | GPS interface — TinyGPS++ wrapper (HAS_GPS guard) |
| `firmware/src/gps_controller.cpp` | GPS implementation — Serial2, EN pin, HDOP check |

**Files modified**:
| File | Change |
|------|--------|
| `platformio.ini` | Added `[env:tbeam_supreme]` with XPowersLib + TinyGPSPlus deps |
| `firmware/src/config.h` | Added BOARD_TBEAM_SUPREME pin block |
| `firmware/src/utils/sd_utils.h` | Extended HSPI branch for T-Beam Supreme |
| `firmware/src/radio_controller.cpp` | Added TBEAM_SUPREME to TCXO/DIO2 init branch |
| `firmware/src/lora_recon_tool.cpp` | PMU init first; GPS init + update in loop |
| `firmware/src/oled_display.h` | DisplayDriver typedef (SH1106 vs SSD1306); GPS status fields |
| `firmware/src/oled_display.cpp` | T-Beam Supreme init path; GPS indicator in scan/target views |
| `.github/workflows/build.yml` | Added tbeam_supreme to CI matrix |

**Build results**: heltec_v3 ✅, t3_s3 ✅, tbeam_supreme ✅ (RAM: 22.2%, Flash: 39.1% of 3MB partition)

### Non-obvious Implementation Details
- **XPowersLib API**: Generic `enablePowerOutput()`/`setPowerChannelVoltage()` are protected in `XPowersAXP2101`. Must use per-channel public methods: `setALDO1Voltage(3300)`, `enableALDO1()`, etc.
- **`#define XPOWERS_CHIP_AXP2101`** must be defined before `#include <XPowersLib.h>` to get the `XPowersPMU` typedef.
- **TinyGPS++ getters are non-const** — they mutate internal `updated` flag. All GPS getter methods omit `const`.
- **TCXO**: ALDO3 powers it continuously, but `tcxoVoltage=1.8` still passed to `radio->begin()` so RadioLib configures SX1262 in TCXO oscillator mode.
- **DisplayDriver typedef**: `oled_display.h` selects `U8G2_SH1106_128X64_NONAME_F_HW_I2C` for T-Beam Supreme, `U8G2_SSD1306_128X64_NONAME_F_HW_I2C` for others.
- **OLED_RST = -1**: Guarded RST pulse with `#if OLED_RST >= 0` to avoid `digitalWrite(-1, ...)`.

### Wireless Data Access (Previous Session)
All SD card and map data is now downloadable wirelessly from the webapp sidebar.

**Problem**: CSV had no HTTP endpoint. PCAP/KML/GeoJSON had endpoints but no UI buttons.

**Solution**: 4 new sidebar buttons + supporting firmware:

| Button | Endpoint | Notes |
|--------|----------|-------|
| Download CSV | `GET /api/export/csv` | Streams current session CSV from SD |
| Download PCAP | `GET /api/export/pcap` | Was already implemented; now has a button |
| Export KML | `GET /api/export/kml` | Fixed to force-download (was `window.open`) |
| Export GeoJSON | `GET /api/export/geojson` | New action; fixed Content-Disposition header |

## Current State
- **Heltec V3**: flashed 2026-02-21 with `a0ea27e` (wireless downloads + mDNS OLED).
- **T3-S3**: flashed 2026-02-21 with `a0ea27e` (same).
- **T-Beam Supreme**: NOT yet flashed — code complete, builds clean, awaiting hardware test.
- **Branch**: `main`, commits unpushed, working tree has HANDOFF.md modified.
- **CI**: tbeam_supreme added to matrix; should be green.

## What's Blocked / Known Issues
- T-Beam Supreme hardware test pending (need physical device).
- GPS fix time: L76K/MAX-M10S typically 30-60s outdoors for first fix.

## Next Steps
- Push `main` to origin.
- Flash T-Beam Supreme; verify serial shows: `AXP2101 initialized`, `SX1262 initialized`, `GPS fix acquired`.
- Consider wiring the file browser into a UI panel (currently API-only).

## Key Parameters / Settings
- **monitor_rts = 0 / monitor_dtr = 0** in platformio.ini — do not remove, prevents reset-on-connect
- Display refresh interval: `TARGETING_REFRESH_MS` / `SCANNING_REFRESH_MS` = 2000ms
- PlatformIO CI version: 6.1.19
- Heltec V3: COM3 (CP210x, VID:PID=10C4:EA60), `monitor_rts=0`
- T3-S3: COM9 (native USB, needs BOOT+RST for bootloader if in crash loop)
- T-Beam Supreme: native USB CDC (`ARDUINO_USB_CDC_ON_BOOT=1`), needs BOOT+RST for bootloader

## Commits This Session
| Hash | Message |
|------|---------|
| (uncommitted) | T-Beam Supreme board support |
| `a0ea27e` | docs: update all project docs for 2026-02-21 session |
| `147fb88` | feat: wireless data access — CSV/PCAP/KML/GeoJSON downloads via webapp |

## T-Beam Supreme Pin Map (verified from LilyGo schematic)
- LoRa SPI (FSPI): SCK=12, MISO=13, MOSI=11, NSS=10
- LoRa control: DIO1=1, RST=5, BUSY=4; DIO2=RF switch
- SD card (HSPI): SCK=36, MISO=37, MOSI=35, CS=47
- OLED I2C: SDA=17, SCL=18 (SH1106 128×64 @ 0x3C)
- PMU I2C: SDA=42, SCL=41 (AXP2101 @ 0x34), IRQ=40
- GPS UART: RX=9, TX=8, EN=7 (pull HIGH to enable)
- User button: GPIO 0
- Chip: ESP32-S3FN8 (8MB QIO flash, 8MB Quad PSRAM) — GPIO 33-37 safe (not OPI)
