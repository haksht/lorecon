# ESP32 LoRa Sniffer - Improvement Plan

Based on full code review (2026-02-17). Prioritized by impact and risk.

---

## P0: Correctness Bugs (fix first)

### ~~1. LoRaWAN detection is a tautology~~ DONE
Fixed in previous session. Proper MHDR/length/FCtrl validation. 9 regression tests in native test suite.

### ~~2. Duplicate `Logging` namespace in config.h~~ DONE
Removed dead comment block. Single `namespace Logging` with `constexpr bool PCAP_EXPORT_ENABLED` already correct.

### ~~3. `uint8_t = -1` for unused pins~~ DONE
Already uses `PIN_UNUSED = 0xFF` sentinel throughout.

---

## P1: Reliability (affects long-running operation)

### 4. Blocking serial I/O freezes the entire system
**Files:** `firmware/src/lora_recon_tool.cpp:459-466, 504-518, 597-620, 649-673, 749-753`
**Problem:** `while (!Serial.available())` loops block for up to 30 seconds. During this time: no packets received, no WebSocket updates, no WiFi health checks. Watchdog is manually fed to mask the freeze.
**Fix:**
- Replace blocking serial menus with a state-machine approach in `update()`:
  - Add `MenuState` enum: `IDLE`, `AWAITING_SLOT_SELECT`, `AWAITING_REPEAT_COUNT`, `AWAITING_DELAY`, `AWAITING_CONFIRM`.
  - Store pending menu state in `LoRaReconTool` member variables.
  - In `update()`, check `Serial.available()` non-blockingly. If input arrives, advance the state machine. If timeout expires, cancel and return to previous mode.
- This is the largest refactor in this plan. The replay menu (~320 lines) should be extracted into its own class (e.g., `ReplayController`) with a non-blocking `update()` method.
- Remove all manual `esp_task_wdt_reset()` calls inside wait loops -- they should not be necessary if the loop is non-blocking.

### 5. String fragmentation in API response building
**Files:** `firmware/src/api_handlers.cpp`, `firmware/src/json_builders.cpp`, `firmware/src/packet_logger.cpp:84`
**Problem:** Arduino `String` concatenation in API handlers allocates/frees heap repeatedly. Over 24+ hours this fragments the heap.
**Fix:**
- Use ArduinoJson's `serializeJson()` to write directly to the `AsyncWebServerResponse` stream instead of building a `String` in memory.
- For file paths in packet_logger, use `snprintf` into a `char[64]` buffer.
- Audit all `String` usage in files called from the main loop or web server task. Replace with fixed buffers where possible.

### ~~6. Packet header extraction duplicated ~120 lines~~ DONE
Already extracted to `findAndExtractMeshtasticHeader()` with `MeshtasticHeader` struct. Both code paths call it.

---

## P2: Security (low risk given local-network threat model, but worth fixing)

### 7. Private-network auto-trust bypasses auth on device AP
**File:** `firmware/src/api_security.cpp:59-75`
**Problem:** Any RFC 1918 IP bypasses token auth. When the device runs its own AP (192.168.4.x), anyone who connects to the AP WiFi gets full API access to dangerous endpoints (replay/transmit, firmware upload, device clear).
**Fix:**
- Remove the blanket RFC 1918 bypass.
- Instead, auto-trust only the AP gateway IP (192.168.4.1 requests from the device itself) or require token auth for all write/dangerous endpoints regardless of source IP.
- Keep read-only endpoints (status, devices, config) unauthenticated for usability.

### ~~8. Token comparison leaks length via timing~~ DONE
Already uses fixed-length rejection (TOKEN_LENGTH is public) + volatile XOR constant-time comparison.

---

## P3: Architecture (technical debt, not urgent)

### 9. Extract God Object: `lora_recon_tool.cpp` (870 lines)
**Problem:** Handles init, main loop, recon mode, targeting, packet replay, button handling, shutdown, and display updates.
**Fix:** Split into focused classes:
- `ReplayController` -- all replay menu logic (~320 lines, also fixes #4)
- `ButtonHandler` -- button press/debounce/shutdown (~100 lines, already partially in `user_interface.cpp`)
- `ScanController` -- frequency scanning and mode switching (~150 lines)
- Keep `LoRaReconTool` as a thin orchestrator that delegates to these.

### 10. Replace globals with dependency injection
**Problem:** `g_reconTool`, `g_webServer`, `reconState`, `packetLogger`, `g_radioController` are all global. Makes testing impossible and creates hidden coupling.
**Fix:**
- Pass dependencies via constructor or `initialize()` method.
- `LoRaReconTool` already receives most dependencies -- make it the composition root.
- `WebServer` handlers need `reconTool` access -- pass via lambda captures or a context struct instead of `g_webServer` global.
- This is a large refactor. Do incrementally: start by removing `g_webServer` (replace with lambda captures in route setup), then `g_reconTool`.

### ~~11. Firmware version estimation is unreliable~~ DONE
Already uses `~v2.2+ (est: encryption flag)` labeling pattern throughout. Clearly marked as estimates.

---

## P4: Quality of Life

### 12. Add heap pre-checks before large allocations
**Files:** `firmware/src/lora_recon_tool.cpp:68,75,96`, `firmware/src/web_server.cpp:60-69`
**Problem:** `new RadioController()`, `new PacketProcessor()`, `new AsyncWebServer()` allocate without checking available heap first.
**Fix:** Add `if (ESP.getFreeHeap() < 40000) { LOG_ERROR("Insufficient heap"); return false; }` before each allocation block.

### ~~13. Web app cache busting is manual~~ DONE
`Cache-Control: no-cache` already set globally in web_server.cpp. Removed redundant `?v=4` params.

### ~~14. No automated tests~~ DONE
45 native tests across 4 suites (channel_hash, ring_buffer, protocol_analyzer, security_scorer). Unity framework, PlatformIO native env.

---

## Suggested Execution Order

| Phase | Items | Effort | Impact |
|-------|-------|--------|--------|
| 1. Quick wins | #1, #2, #3, #8 | 1-2 hours | Fix correctness bugs |
| 2. Reliability | #5, #6, #12 | 3-4 hours | Improve long-run stability |
| 3. Security | #7 | 1 hour | Close AP auth gap |
| 4. Big refactor | #4, #9 | 6-8 hours | Non-blocking menus, decompose God Object |
| 5. Architecture | #10 | 4-6 hours | Testability, maintainability |
| 6. Testing | #14 | 4-6 hours | Catch regressions, validate fixes |
| 7. Polish | #11, #13 | 1-2 hours | UI accuracy, developer experience |

**Total estimate: ~20-30 hours of focused work.**

Phase 1 should be done immediately -- those are bugs. Phase 2-3 before any extended field deployment. Phases 4-6 can be done incrementally.
