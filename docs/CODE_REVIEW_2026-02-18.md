# Code Review — 2026-02-18

**Scope:** Full repo — documentation, code, structure, suitability for purpose
**Reviewer:** Tim Arney
**Branch:** `main` (commit `df928df`)

---

## Overall Assessment

The project is well-conceived and substantially more sophisticated than a typical
hobbyist LoRa sniffer. The architecture is intentional: the config namespace system,
StaticRingBuffer for packet queuing, explicit FSPI instance, NVS write-rate fix, and
FreeRTOS mutex pattern are all sound engineering decisions. The unit test
infrastructure (native env, Unity framework, 45+ tests) is notably above average for
an embedded hobby project.

The issues below are real but fixable without structural surgery — except for one
silent dead-code bug in `applyProtocolParameters()` that deserves immediate attention.

---

## Issues by Priority

### Fix Immediately

#### 1. `data/wifi_creds.json` tracked in git — `.gitignore` gap

`data/wifi_creds.json` is committed to the repo. The `.gitignore` excludes
`wifi_creds.json` (root only), not `data/wifi_creds.json`. Content is currently just
a placeholder (`{"ssid":"YOUR_HOTSPOT_SSID","password":"YOUR_HOTSPOT_PASSWORD"}`) but
any user who puts real credentials there and runs `git add data/` will leak them.

The code already migrates these credentials to NVS. The file should be removed from
the repo and the `.gitignore` entry corrected to `data/wifi_creds.json`.

#### 2. `applyProtocolParameters()` — all protocol-specific paths are dead code

`firmware/src/radio_controller.cpp` lines 225-237:

```cpp
if (strstr(protocol, "Meshtastic_MSG") != nullptr) { ... }
else if (strstr(protocol, "Meshtastic_MF") != nullptr) { ... }
else { ... }  // <-- always executes
```

`identifyProtocol()` returns the string `"Meshtastic"`, not `"Meshtastic_MSG"` or
`"Meshtastic_MF"`. Neither `strstr` ever matches. Every packet falls through to the
`else` branch. The coding rate and preamble length customisations per protocol type
have never executed. Intended behaviour is unclear — fix the strings or remove the
branches.

#### 3. Diagnostics recovery re-initialises the wrong SPI bus

`firmware/src/radio_controller.cpp` lines 153-155 (recovery path in `runDiagnostics()`):

```cpp
SPI.end();
SPI.begin(Config::Hardware::SPI_SCK, Config::Hardware::SPI_MISO,
          Config::Hardware::SPI_MOSI);
```

`initialize()` uses an explicit `SPIClass loraSPI(FSPI)` — the radio was never on
the default `SPI` object. Re-initting `SPI` on T3-S3 does nothing for the radio.
The recovery path should call `loraSPI.begin(...)` or be removed if it never worked.
Also: the recovery hardcodes `tcxoVoltage=1.8` which is T3-S3-specific but runs
unconditionally on Heltec V3 too.

Additionally, `runDiagnostics()` hardcodes US frequencies (906.875 MHz, 902.125 MHz)
for all boards. These calls will fail or produce misleading logs for EU deployments.

---

### Fix Soon

#### 4. Recursive menu navigation — stack risk on embedded

`firmware/src/lora_recon_tool.cpp` — `showReplayMenu()` calls itself recursively on
invalid input, bad slot number, and after clearing slots. `replayPacket()` calls
`showReplayMenu()` at the end. Each frame carries local String objects and char
buffers. A few rounds of bad input will exhaust the stack. Replace with a loop.

#### 5. Race condition on `scanState` counters (dual-core ESP32-S3)

`droppedPackets`, `totalPackets`, and `peakQueueSize` in `ScanState` are
read/written from both the main loop (via `queuePacket()`) and the AsyncWebServer
task without atomics or locks. The FreeRTOS mutex in `ReconState` only protects
`packetStore_` and `deviceRepo_`, not `scanState`. On the dual-core S3 this is a
real race. Use `std::atomic<uint32_t>` or extend the mutex scope.

#### 6. README broken links and AP SSID mismatch

- `GETTING_STARTED.md` (root) — referenced line 7, does not exist.
- `API_REFERENCE.md` (root) — referenced line 7, does not exist.
- Quick Start (line 87) points to a GitHub release for v2.2.0; current code is
  2.3.0-dev. Release may not exist.
- Line 106 says "wait for `ESP32-LoRa-Sniffer` WiFi AP" but `config.h`
  `AP_SSID_PREFIX = "LoRa-"` produces `"LoRa-XXYYZZ"`. Different name entirely.
- Line 109: "7 tabs" listed, but only 6 are described in the bullets below.

---

### Clean Up

#### 7. `[env:default]` in `platformio.ini` is a byte-for-byte duplicate of `[env:heltec_v3]`

Lines 100-127 add a full environment definition that is identical to `heltec_v3`.
`default_envs = heltec_v3` means it will never be built by default anyway. Remove it.

#### 8. `tools/visualization/attack_dashboard.html`

README line 23 states "all legacy offensive/stress docs and code paths have been
removed." This file remains and its name is inconsistent with the stated passive-
reconnaissance framing. Rename or remove.

#### 9. `repositories/node_tracker.cpp` excluded in both environments

`build_src_filter` in both `heltec_v3` and `t3_s3` explicitly excludes this file.
Either delete it or document why it exists as dead/WIP code.

#### 10. No single version constant

Version appears as a log string `"v2.0"` in `lora_recon_tool.cpp:43` and as
`"2.3.0-dev"` in the README. Add a `VERSION` constant in `config.h` and use it in
both places.

---

### Technical Debt (longer term)

#### 11. `lora_recon_tool.cpp` is 879 lines and a god object

The replay logic alone is ~300 lines. The class has grown beyond "orchestrator" into
a monolith. Extract replay into a `PacketReplay` class.

#### 12. `CrashContext` namespace embedded in `main.cpp`

~100 lines of NVS persistence logic lives in `main.cpp`. It already needed an extern
function declaration (`crashContextPrintResetInfo()`). Move it to `crash_context.h/cpp`.

#### 13. `extern ReconState reconState` global

`recon_state.h:147` has the comment "eventually we'll pass this around instead of
using globals." The global is causing concrete problems: `packet_processor.cpp`
accesses `reconState.scanState` without holding the mutex because `scanState` is not
covered by it. The mutex needs to either cover `scanState` or the global needs to go.

#### 14. `ScanState.lastPacket` typed as `char` but holds raw binary

`data_structures.h:111` — `char lastPacket[256]` stores raw LoRa packet bytes.
`char` is implementation-defined for sign; use `uint8_t`.

#### 15. OLED redrawn every loop in targeted capture mode

`handleTargetedCaptureMode()` calls `showTargetingMode()` on every loop iteration
(~every 10 ms) even when nothing changed. Gate the update on a state-change flag to
reduce I2C traffic and OLED flicker.

#### 16. `setOutputPower(0)` labelled "receive-only mode" — misleading

`lora_recon_tool.cpp` lines 711/738: after replay, power is set to 0 dBm with the
comment "Back to receive-only mode." 0 dBm is 1 mW transmit, not receive mode.
Receive mode is the `startReceive()` call on line 765. The intermediate
`setOutputPower(0)` call is misleading and can be removed.

#### 17. Replay code reaches through `RadioController` abstraction

`replayPacket()` calls `radioController->getRadio().setOutputPower(10)` directly on
the underlying RadioLib object, bypassing the `RadioController` interface. Add
`setOutputPower(int)` to `RadioController` if transmit power control is needed.

---

## What Is Working Well

- **Config namespace system** (`config.h`) — comprehensive, well-documented, compile-
  time assertions, no magic numbers scattered through code.
- **StaticRingBuffer** — correct choice for interrupt-driven queue on long-running
  embedded system. Zero heap allocation.
- **FSPI explicit instance** — the `SPIClass loraSPI(FSPI)` fix is correct and the
  reasoning is documented.
- **NVS write rate limiting** — 5-minute interval with the root-cause analysis
  preserved in comments is excellent.
- **FreeRTOS mutex + ScopedLock RAII** — the right pattern for the
  main-loop/AsyncWebServer cross-task access.
- **Unit tests** — native env with Unity, 45+ assertions covering ring buffer,
  protocol analyzer, channel hash, and security scorer. Notably above average for
  this class of project.
- **Hardware reset before SPI init** in `radio_controller.cpp` — correct fix for
  the post-flash-erase CHIP_NOT_FOUND failure.
- **Anomaly detection + security scoring** — multi-factor approach (RSSI variance,
  hop count, packet rate, firmware version) is thoughtful.
- **Board abstraction** — all pin definitions in `config.h` under `BOARD_T3_S3` /
  `BOARD_HELTEC_V3` guards; `#error` if neither defined. Clean.

---

## Suggested Fix Order

1. Remove `data/wifi_creds.json` from git; fix `.gitignore`
2. Fix or remove `applyProtocolParameters()` dead branches
3. Fix `runDiagnostics()` recovery to use `loraSPI`, not `SPI`
4. Replace recursive menu navigation with loops
5. Add atomics or extend mutex to cover `scanState` counters
6. Fix README links and AP SSID
7. Remove `[env:default]` from `platformio.ini`
8. Rename/remove `attack_dashboard.html`
9. Delete or document `node_tracker.cpp`
10. Add `VERSION` constant to `config.h`
