# Session Handoff

## Session Date: 2026-03-02

## What Was Done

### Bug Fixes: T-Beam Supreme Battery + Shutdown (Issues #7, #9)

**#7 — Battery showing 0% in webapp**
- Root cause: `json_builders.cpp` used `analogReadMilliVolts(VBAT_ADC_PIN)` for all boards. On T-Beam Supreme, `VBAT_ADC_PIN = PIN_UNUSED = 0xFF` → reads garbage → 0V → 0%.
- Fix: Branch on `HAS_AXP2101` in `json_builders.cpp`. T-Beam Supreme now reads voltage and percent from `PMUController::getBatteryVoltage()` / `PMUController::getBatteryPercent()` (AXP2101 coulomb counter). Heltec/T3-S3 unchanged (ADC path).

**#9 — T-Beam Supreme not fully powering down**
- Root cause: Button long-press only called `esp_deep_sleep_start()`, which suspends the CPU but leaves AXP2101 running with all rails live (blue LED visible after shutdown).
- Fix: Added `PMUController::shutdown()` which calls `pmu.shutdown()` to cut all PMIC rails. Extracted shutdown sequence into `LoRaReconTool::performShutdown()`. On AXP2101 boards, PMIC cuts power before deep sleep fallback.

### Feature: Web UI Power Off (Issue #10)

- New "Power Off" button in Quick Tools card (Info tab) alongside Reboot.
- Confirmation dialog before acting.
- Posts `command: 's'` to `/api/command`.
- All boards supported: T-Beam Supreme = hard PMIC power-off; Heltec V3/V4 + T3-S3 = deep sleep.
- New `cmd == "s"` handler in `api_handlers.cpp` calls `g_reconTool->performShutdown()`.

### Feature: Sniffer GPS Position in Webapp (Issue #11)

- `buildStatusJson()` in `json_builders.cpp` now includes a `gps` object when `HAS_GPS` is defined: `hasFix`, `satellites`, and (when fixed) `lat`/`lon`/`alt`.
- Info tab System Status card gains a "📍 GPS" row: hidden on non-GPS boards, shows coordinates + satellite count when fixed, "No fix (N sats)" while acquiring.
- Updates every status poll cycle (10 s) via existing `handleStatusUpdate()`.

### Feature: Heltec V4 GPS Support (Issue #12)

- New `heltec_v4` PlatformIO environment for Heltec WiFi LoRa 32 V4 with the optional L76K GNSS module.
- GPS pins from Meshtastic firmware (authoritative): RX=38, TX=39, EN=34 (active LOW — differs from T-Beam Supreme which is active HIGH).
- New `Config::Hardware::GPS_EN_LEVEL` constant (LOW for Heltec V4, HIGH for T-Beam Supreme). `gps_controller.cpp` uses this instead of hardcoded polarity.
- `heltec_v4` env adds `HAS_GPS` flag + TinyGPSPlus library. All other config is identical to `heltec_v3`.
- Use `heltec_v3` for V3 or V4 without the GPS module; use `heltec_v4` only when the L76K module is physically attached.
- CI matrix updated to build `heltec_v4` on every push.

### UX Fix: Captured Packets Subtitle (Issue #8)

- Issue #8 ("packets seen but not captured") is working as designed — the webapp shows a 10-slot circular replay buffer, not all captured packets. Full history is on the SD card CSV log.
- Clarified the subtitle text in the Captured Packets card to explain replay buffer behavior and SD card logging.

## Current State

- **All four environments**: build clean (heltec_v3, heltec_v4, t3_s3, tbeam_supreme all SUCCESS)
- **Branch**: `main`, all changes committed and pushed
- **T3-S3 flash**: 89.1% — monitor before adding features
- **Outstanding issues**: #6 (recon mode scan settings)

## What's Blocked

Nothing.

## Next Steps (if desired)

1. **Flash boards** with new firmware as needed
2. **Issue #6** — Recon mode scan settings: allow user to configure frequency/SF/BW/CR from webapp (large feature, touches radio controller + UI)
3. **Token UX for AP clients**: Replace browser `prompt()` with a nicer HTML login page
4. **GPS track in webapp**: WebSocket `packet` events include `lat`/`lon` when `HAS_GPS` + fix — JS does not yet consume these to plot sniffer track on map

## Key Parameters / Settings

- Heltec V3/V4: COM3 (CP210x, VID:PID=10C4:EA60) / COM12
- T3-S3: COM9 (native USB, 303A:1001)
- T-Beam Supreme: running = COM10, download mode = COM11
- API token: `f71056e1899557b5d191b18ce42ce5be` (in NVS, survives reboot)
- T-Beam Supreme WiFi: SSID `LoRa-5A8248` / Password `recon-5A8248`
- PlatformIO CI version: 6.1.19 (pinned)

## Files Changed This Session

**Firmware:**
- `firmware/src/utils/pmu_controller.h` — added `PMUController::shutdown()`
- `firmware/src/lora_recon_tool.cpp` — extracted `performShutdown()`, button handler now calls it
- `firmware/src/lora_recon_tool.h` — added `performShutdown()` public method
- `firmware/src/json_builders.cpp` — `HAS_AXP2101` branch for battery reading; `gps` object in `/api/status`
- `firmware/src/api_handlers.cpp` — `cmd == "s"` shutdown handler
- `firmware/src/config.h` — GPS pins + `GPS_EN_LEVEL` for Heltec V4 (`BOARD_HELTEC_V3` block); `GPS_EN_LEVEL=HIGH` for T-Beam Supreme
- `firmware/src/gps_controller.cpp` — use `Config::Hardware::GPS_EN_LEVEL` instead of hardcoded `HIGH`

**Build:**
- `platformio.ini` — new `[env:heltec_v4]` environment with `HAS_GPS` + TinyGPSPlus
- `.github/workflows/build.yml` — added `heltec_v4` to CI matrix

**Web UI:**
- `data/webapp/index.html` — Power Off button in Quick Tools card; GPS row in System Status card; Captured Packets subtitle clarified
- `data/webapp/js/app.js` — `'shutdown'` action, `actionShutdown()` method; GPS row update in `handleStatusUpdate()`

**Documentation:**
- `CHANGELOG.md` — added [2.3.1], [2.3.2], [2.3.3] entries
- `HANDOFF.md` — this file
- `API_REFERENCE.md` — `/api/command` full section, battery fields and `gps` object in `/api/status`
- `docs/user-guides/FEATURES.md` — button description, Settings tab description
- `docs/user-guides/BUILD_GUIDE.md` — `heltec_v4` environment added to supported hardware list
