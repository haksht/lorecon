# Session Handoff

## Session Date: 2026-03-04

## What Was Done

### Heltec V4 Radio Fix — Root Cause: External FEM Not Enabled

V4 (`heltec_v4` env, COM12) had `isrCount:0` — radio initialized (startReceive succeeded,
`lastRxError:0`) but received zero packets. Swapping antennas and removing USB had no effect.

**Diagnostic added: IRQ polling fallback**
- `pollIrqStatus()` reads SX1262 IRQ status register directly via SPI every main loop tick
- If `RxDone` bit set, marks packet available and increments `irqPollCount`
- `irqPollCount:0` confirmed: SX1262 itself never saw any RF — not a DIO1/GPIO issue
- Both counters exposed in `/api/status`

**Root cause: Heltec V4 external FEM not enabled**

Heltec V4 (unlike V3) has an external FEM (GC1109 on V4.2, KCT8103L on V4.3) between
the SX1262 and the antenna. The FEM requires:
- **GPIO 2 HIGH** — chip enable (powers the entire FEM; without this, LNA is off)
- **GPIO 5 LOW** — KCT8103L PA_CTX, selects LNA/RX mode (V4.3)
- **GPIO 46** — GC1109 PA_TX_EN, strapping pin defaults LOW = RX mode (V4.2 — leave as INPUT)
- **`setDio2AsRfSwitch(true)`** — DIO2 controls the internal RF path switch (confirmed by Meshtastic)

Our firmware never drove GPIO 2 HIGH → LNA always unpowered → zero RF reaching SX1262.

**Fix:**
- Added `-DBOARD_HELTEC_V4` to `heltec_v4` env `build_flags` in `platformio.ini`
- Added `#if defined(BOARD_HELTEC_V4)` block in `radio_controller.cpp` (Heltec `#else` branch):
  `pinMode(2, OUTPUT); digitalWrite(2, HIGH);` — enable FEM
  `pinMode(5, OUTPUT); digitalWrite(5, LOW);`  — LNA/RX mode
  `radio->setDio2AsRfSwitch(true);` — RF switch via DIO2
- Same block added to `runDiagnostics()` recovery path

**Result:** `isrCount:1, devices:1, totalPackets:1` — V4 confirmed receiving LoRa packets.

### Other Session Changes

**Serial menu exit command (`e`/`E`)**
- `e`/`E` exits interactive menu and resumes prior mode (targeting or recon) without clearing NVS

**Logger: configurable output stream**
- `SerialLogger` now uses `Stream* _stream` (default `&Serial`) with `setStream()` method
- Allows runtime redirect of log output (e.g., to USBSerial)

**Radio diagnostics in `/api/status`**
- `isrCount` — total DIO1 ISR firings since boot
- `lastRxError` — RadioLib error code from last `startReceive()` call (0=success)
- `irqPollCount` — packets detected via SPI IRQ poll (not DIO1 interrupt)

## Current State

- **Branch**: `main`, all changes committed and pushed
- **V4 status**: WORKING — FEM fix confirmed, receives LoRa packets
- **V3 status**: Working (no DIO2 or FEM for V3 — unchanged)
- **T3-S3, T-Beam Supreme**: Unchanged, working

## Next Steps

1. Field test V4 GPS outdoors for a fix (needs clear sky)
2. Issue #6 — Recon mode scan settings
3. Issue #8 — 20-byte capture threshold (waiting on The-Foe's decision)

## Key Parameters / Settings

- Heltec V3: COM3 (CP210x, VID:PID=10C4:EA60)
- Heltec V4: COM12 (native USB, no serial monitor — webapp only, USB PHY kills radio)
- T3-S3: COM9 (native USB, 303A:1001)
- T-Beam Supreme: running = COM10, download mode = COM11
- API token: `f71056e1899557b5d191b18ce42ce5be` (in NVS, survives reboot)
- PlatformIO: `c:/Users/tim/.platformio/penv/Scripts/pio.exe`

## Files Changed This Session

- `platformio.ini` — added `-DBOARD_HELTEC_V4` to heltec_v4 env
- `firmware/src/radio_controller.h` — added `irqPollCount` atomic, `pollIrqStatus()`,
  `getIrqPollCount()`; also `isrCount`, `getISRCount()`, `lastRxError`, `getLastRxError()`
- `firmware/src/radio_controller.cpp` — V4 FEM init (GPIO 2 HIGH, GPIO 5 LOW, DIO2 switch);
  `pollIrqStatus()` implementation; `startReceive()` logs result and stores `lastRxError`
- `firmware/src/lora_recon_tool.cpp` — `pollIrqStatus()` call in main loop
- `firmware/src/json_builders.cpp` — `isrCount`, `irqPollCount`, `lastRxError` in `/api/status`
- `firmware/src/api_controller.cpp` — `isrCount` added to config endpoint
- `firmware/src/command_handler.h/.cpp` — `cmdExitMenu` (`e`/`E` key)
- `firmware/src/logger.h/.cpp` — configurable `Stream* _stream`
