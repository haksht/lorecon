# Current Recommendations – ESP32 LoRa Sniffer

_Last updated: 2025‑11‑18_

## 1. High‑level assessment

Overall this is a strong, professional project:

- Clear mission and scope: a receive‑focused LoRa recon tool with replay and analysis, with offensive bits explicitly removed.
- Architecture is well thought out and documented: `RadioController` → `PacketProcessor` → `ReconState` → `LoRaReconTool` orchestrator, plus clean UI and API layers.
- Documentation quality is excellent and unusually complete for a hobby / research tool.
- Implementation in the core firmware is careful about watchdogs, blocking calls, and field reliability, with good attention to error handling.

Most of the feedback below is about polish: security hardening, tightening memory usage / heap fragmentation on the ESP32, and keeping docs fully in sync with the actual codebase.

---

## 2. Architecture & design

### Strengths

- **Layered design with clear responsibilities**
  - `LoRaReconTool` orchestrates modes (recon, targeted capture, replay, menu), watchdog, SD logging, OLED, and command handler.
  - `RadioController` encapsulates SX1262 + ISR concerns; `PacketProcessor` is responsible for decoding, PSK decryption, stats, and callbacks.
  - `ReconService` acts as an application service layer exposing well‑shaped JSON for the REST/API layer.
  - `WiFiManager` and `WebServer` encapsulate network and HTTP/WebSocket concerns.

- **Single source of truth for state**
  - `reconState` is the canonical store for configs, RF activity, targetable devices, replay slots, and diagnostics.
  - Both the serial UI and web UI read from the same state and services, which avoids duplicated business logic.

- **Mode‑driven main loop**
  - `LoRaReconTool::update()` switches behavior based on `reconState.scanState.mode` with dedicated handlers for recon vs targeted capture, and delegates replay/menu into the command system.

- **Future‑oriented docs**
  - The dual‑board and PC‑integration docs show a coherent long‑term story: single‑board recon now, expandable to multi‑board / PC‑assisted analysis later.

### Design suggestions

- **Global state and singletons**  
  Several global/singleton‑like entities (`g_reconTool`, `g_webServer`, global `reconState`, and a global logger) are pragmatic on Arduino/ESP32, but they make unit testing and reuse slightly harder. If you move toward more testing or multiple tools on one MCU, consider:
  - Passing dependencies explicitly into constructors where convenient, and
  - Limiting access to globals to a few “composition” units (e.g., `main.cpp` and a small number of service classes).

- **Explicit boundaries around ISR & realtime paths**  
  From the design docs and `handlePacketReception()` it looks like ISRs are kept minimal and packets are queued for processing in the main loop. That’s good; I would keep pushing in this direction by ensuring **no dynamic allocation and no `String` usage** in any path that runs per packet in tight bursts (especially inside `PacketProcessor::processQueue` and `protocol_analyzer`).

- **JSON & API surface coupling**  
  `ReconService` and `WebServer`/`APIController` do a lot of JSON assembly with ArduinoJson, and REST + WebSocket messages mirror each other. That’s good for consistency, but it means JSON schema changes ripple through many call sites. A thin “schema” header with shared helpers/structs for status, activity, device list, positions, etc. would reduce duplication and make future API evolution safer.

---

## 3. Firmware implementation quality

### Strengths

- **Main loop is robust and watchdog‑aware**
  - Hardware WDT configured (`esp_task_wdt_init` / `esp_task_wdt_add`) with regular `esp_task_wdt_reset()` calls, including during interactive serial flows and long‑running replay loops.
  - `delay(1)` in `loop()` keeps responsiveness while avoiding WDT issues.
  - Blocking user interactions (menus, replay prompts) all have explicit timeouts, which is critical for unattended deployments.

- **Radio & recon control logic**
  - Recon mode:
    - Dwell‑based config cycling (`Config::Scanning::DWELL_TIME_MS`) across `reconState.getNumConfigs()`.
    - Applies new `ScanConfig` then restarts receive, with periodic cycle summary logging.
  - Targeted modes:
    - `startTargetedCapture` and `startFrequencyTargeting` both lock to specific scan configs and print clear context (frequency, protocol, SF/BW, known devices).
  - Unified `handlePacketReception()`:
    - Short buffer on stack (`uint8_t tempBuffer[256]`).
    - Quick queue into `PacketProcessor` then immediate restart of RX, followed by queued processing.

- **Replay system is carefully written**
  - Respects watchdog and uses safe prompts with timeouts, mutable TX buffer copied from stored packet data, and clear validation of counts and delays.
  - Provides useful replay summary stats (success/failure counts).

- **Button handling & graceful shutdown**
  - Short vs long press semantics are clearly separated.
  - Long press path: shows shutdown on OLED, puts radio into standby/sleep before deep sleep, and waits `SHUTDOWN_WARNING_MS` while flushing serial.
  - Short press toggles display with guard rails validating config index and frequency before use.

### Risks / possible improvements

- **Heap fragmentation via `String` and dynamic JSON**
  - Many paths in `ReconService` & `WebServer` use Arduino `String` heavily (building JSON, KML, status messages).
  - Over long uptimes with heavy API traffic, this can cause fragmentation on the ESP32 heap.
  - Consider:
    - Prefer `StaticJsonDocument<N>` or `DynamicJsonDocument(capacity)` with fixed capacities instead of bare `JsonDocument doc;`.
    - Where JSON shapes are known and relatively small (e.g. `/api/status`, `/api/devices`), pre‑size documents and minimize intermediate `String` allocations.
    - For KML or large exports, stream to the HTTP client rather than building giant `String`s when/if data volume grows.

- **ArduinoJson usage correctness**
  - Some patterns (e.g. in `broadcastStatusUpdate()`) deserialize JSON into sub‑objects. It would be worth double‑checking all ArduinoJson usage (document capacities, deserialization targets) with host‑side tests to avoid subtle runtime failures on constrained hardware.

- **Dynamic allocation of controllers**
  - `radioController`, `packetProcessor`, `commandHandler`, and `oledDisplay` are allocated with `new` in `LoRaReconTool::initialize()` and never freed.
  - Since these are one‑time, never‑freed objects, they’re effectively global singletons. It’s slightly safer to allocate them statically (as member objects or globals) to avoid heap failures at startup and simplify lifetime management.

---

## 4. Web/API layer

### Strengths

- **API surface is well documented**
  - `API_REFERENCE.md` clearly describes endpoints, schemas, examples, and language‑specific snippets.
  - API design is clean and consistent: `status`, `devices`, `device`, `capture` control, `positions`, `export/*`, `statistics`, `activity`, `scan` control, etc.
  - WebSocket messages (`packet`, `deviceUpdate`, `status`) align with the REST data model.

- **AsyncWebServer usage is stability‑aware**
  - Explicit comments and guards around `cleanupClients()` causing stack overflows; you’ve disabled it in periodic update and rely on library internals instead.
  - WebSocket event handling is careful about use‑after‑free:
    - `disconnectInProgress` flag prevents late `WS_EVT_DATA` from touching freed clients.
    - Avoids accessing client details in disconnect paths.
  - Packet events are aggregated and throttled with `BROADCAST_INTERVAL_MS` to avoid spamming clients.

- **CORS & development ergonomics**
  - CORS is enabled for `*`, which is appropriate for a captive AP dev/test device.
  - Simple `/test` and `/apitest` routes are useful for manual verification.

### Suggestions

- **Authentication & CSRF**
  - Currently no HTTP auth; this is acceptable only for isolated WiFi APs and is documented as such.
  - For any semi‑real deployment:
    - Implement at least basic auth (as your docs suggest), or a shared bearer token header.
    - Consider using a per‑build random default password or requiring password configuration at first boot rather than shipping a shared default.

- **CORS scope**
  - CORS `*` is fine in AP mode, but once STA/client or bridge modes exist, consider narrowing:
    - Limit origins to `http://192.168.4.1` and `http://esp32-lora.local` (and any configurable host).
    - Or enforce same‑origin and rely on the captive UI to embed advanced tooling.

- **Error consistency**
  - Some handlers respond with `{"status":"error","error":"..."}`, others with `message` + `code` (as in `API_REFERENCE.md`).
  - Standardizing on a single error schema would keep external tools simpler.

---

## 5. Documentation & onboarding

### Strengths

- **Top‑level docs are excellent**
  - `README.md` provides a clear story: purpose, hardware, build flow, phone workflow, serial commands, repo layout, legal use, and contribution guidelines.
  - `QUICKSTART.md` is actionable and reflects the v2.0 tool’s capabilities.
  - `FEATURES.md` doubles as a product/architecture spec plus roadmap.

- **Deeper design docs**
  - `DUAL_BOARD_ARCHITECTURE.md` and `PC_INTEGRATION_ARCHITECTURE.md` outline a credible path to multi‑board, triangulation‑capable, and PC‑integrated deployments, with realistic BOM and mechanical details.

### Gaps / cleanups

- **Stale references**
  - `QUICKSTART.md` references `docs/deepdive/UNDERSTANDING.md` while the repo contains `UNDERSTANDING_v2.md`, and mentions `docs/architecture.md` and `conference/` which are not present.
  - Either:
    - Repoint these to current equivalents (e.g. `UNDERSTANDING_v2.md`, architecture sections of `FEATURES.md`), or
    - Add stubs that clearly say “replaced by X” to avoid confusion.

- **Versioning alignment**
  - Some docs mention v1.7 / DefCon prep; others say v2.0 production.
  - Helpful steps:
    - Add a short “version compatibility” note in each doc (e.g., “Applies to firmware v2.0+” / “Historical doc for v1.x”).
    - Optionally archive historical docs under a `legacy/` subfolder.

- **Phone UI / webapp docs**
  - `README` mentions a new LittleFS‑hosted minimal UI. Ensure `PHONE_APP_GUIDE.md` clearly:
    - Explains UX and constraints.
    - Shows example REST calls & WebSocket behavior for power users.
    - Explicitly documents parity with serial UI (same underlying handlers).

---

## 6. Security & safety posture

### Positives

- Strong, repeated legal/ethical warnings across README and QUICKSTART.
- v2.0 explicitly removed offensive/stress testing modules and build flags.
- PSK testing is framed as a security research feature with a realistic explanation of what is and isn’t decryptable post‑PKC.

### Actionable improvements

- **WiFi defaults**
  - AP password in `main.cpp` is hardcoded as `"recon123"`. This is better than an open AP but still weak.
  - Recommendations:
    - Add a very visible note in README/BUILD_GUIDE that this must be changed for any non‑lab use.
    - Move SSID/password into `config.h` with a build‑time guard that warns or fails if `WIFI_AP_PASSWORD` is still the default.

- **API access surface**
  - Anyone on the AP can currently control capture, replay, and export operations.
  - For more production‑like use:
    - Gate destructive operations (`capture/start`, `replay`, `clear`, `scan/stop`) behind auth.
    - Keep read‑only endpoints (e.g., `status`, `devices`, `positions`) open as desired.

---

## 7. Prioritized recommendations

### High priority (before using as a "product" in the field)

1. **Harden WiFi defaults**  
   Move AP SSID/password to configurable settings and enforce a non‑default password at build or first‑boot time.

2. **Add minimal API auth**  
   Introduce at least basic auth or a shared bearer token for endpoints that control capture, replay, and scanning.

3. **Review JSON memory usage**  
   Standardize on `StaticJsonDocument` / `DynamicJsonDocument(capacity)` with explicit capacities, and reduce round‑trip stringification between layers.

### Medium priority (robustness and maintainability)

4. **Reduce `String` usage in hot paths**  
   Replace repeated `String` concatenations in telemetry/heavy endpoints with preallocated char buffers and direct writes where possible.

5. **Tighten WebSocket JSON handling**  
   Add focused tests for WebSocket broadcast paths at high packet rates to validate heap/stack safety and the protection against stale `WS_EVT_DATA`.

6. **Align docs with repo reality**  
   Fix stale links in QUICKSTART and clarify which deep‑dive docs are current vs historical.

### Nice to have (polish and future evolution)

7. **Introduce a lightweight “schema” or DTO layer**  
   Define small structs + helper functions for serializing `Status`, `Device`, `RFActivity`, etc., shared by both REST and WebSocket producers.

8. **Static/global allocation of long‑lived components**  
   Transition `radioController`, `packetProcessor`, and other never‑freed components to static objects to avoid any startup allocation surprises.

9. **Add a small test harness for the API/JSON layer**  
   Even a couple of host‑side tests (e.g., for `ReconService` JSON construction using ArduinoJson in a desktop build) would help catch regressions in API contracts.
