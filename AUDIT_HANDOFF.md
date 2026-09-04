# Post-v2.5.0 Code Audit — Handoff

Date produced: 2026-05-24
Audit scope: single Explore pass, brief: "top 5 things most likely to bite us in the next 6 months."
Methodology: one general-purpose agent doing read-only deep dive of load-bearing modules
(`packet_processor.cpp`, `recon_state.cpp`, `web_server.cpp`, `api_handlers.cpp`,
`api_security.cpp`, `json_builders.cpp`, `psk_decryption_simple.cpp`, `packet_logger.cpp`,
`wifi_manager.cpp`, repositories/, `oled_display.cpp`, plus build/partition config).

Verification status (as of this writing): only **#1 was independently verified**. The
others are agent claims — concrete enough to act on but worth checking before code changes.

| #   | Issue                                                                    | Verified | Severity (prob × impact) |
|-----|--------------------------------------------------------------------------|----------|--------------------------|
| 1   | T-Beam Supreme partition table has no app1 → web OTA fails               | ✅       | high × med               |
| 2   | `streamDevicesJson` holds repo mutex during async TCP send               | ⏳       | high × med               |
| 3   | Unauthenticated read endpoints have no rate limit                        | ⏳       | med × med                |
| 4   | `numAnomalies` is `uint16_t` → wrap corrupts ack indexing                | ⏳       | med × low-med            |
| 5   | `lastPositionExtracted_` is an unguarded `static` (cross-core landmine)  | ⏳       | low-today × med          |

---

## 1. T-Beam Supreme firmware OTA flow is broken (not bricking, but broken) ✅ VERIFIED

**Files:**
- `firmware/partitions_8MB.csv` — partition table for `tbeam_supreme` env only
- `firmware/src/web_server.cpp:255` — `Update.begin(UPDATE_SIZE_UNKNOWN)` (U_FLASH default)
- `platformio.ini` `[env:tbeam_supreme]` — references `board_build.partitions = firmware/partitions_8MB.csv`

**What's wrong:** `partitions_8MB.csv` defines only `app0` (0x10000, size 0x2F0000). No `app1`.
`Update.begin()` calls `esp_ota_get_next_update_partition(NULL)` which returns NULL when
there's no second OTA slot → `begin()` returns false → handler logs "OTA begin failed" and bails.

**Agent claimed:** worst case writes garbage into `spiffs` partition, bricking LittleFS.
**Reality after verification:** Arduino-ESP32's `Update` with `U_FLASH` does not write into
a `spiffs`-subtype partition — it just refuses to start. So the actual outcome is "OTA
button silently fails" rather than "device bricked." Filesystem OTA (`U_SPIFFS` at
`web_server.cpp:328`) does work because it explicitly targets the `spiffs` partition.

**Who's affected:** T-Beam Supreme only. Heltec V3/V4 use the framework's `default.csv`
(dual-OTA at 8MB region). T3-S3 explicitly uses `default.csv` (dual-OTA at 4MB).

**Why it bites in 6 months:** users who deploy a T-Beam Supreme in the field have no way to
update it without physical USB access. The web installer (browser-serial) is unaffected
because that path doesn't go through `Update.begin()` — but it requires the user to plug
the device back in.

**Fix sketch:** repartition `partitions_8MB.csv` as dual-OTA. ~2.5MB per app slot + ~2MB
LittleFS fits (web app is ~600KB):
```
nvs,      data, nvs,     0x9000,   0x5000,
otadata,  data, ota,     0xe000,   0x2000,
app0,     app,  ota_0,   0x10000,  0x280000,
app1,     app,  ota_1,   0x290000, 0x280000,
spiffs,   data, spiffs,  0x510000, 0x2F0000,
```
Smoke-test: build, USB-flash, then OTA upload a slightly different build, confirm reboot
into the new build. Also confirm `uploadfs` still fits the smaller LittleFS partition
(should — current data/ is ~600KB compressed).

**Watch-out:** existing T-Beam Supreme units in the field will lose their LittleFS contents
when they next get USB-flashed with the new partition table (partition layout change wipes
flash). Document this in the release notes.

---

## 2. `streamDevicesJson` holds the global state mutex across the entire async TCP send ⏳

**Files:**
- `firmware/src/json_builders.cpp:248-267` — lock taken at 249, held through `out.print()`
  for every device until 266
- `firmware/src/recon_state.cpp:215` — `addTargetableDevice()` uses `lock(50)` (50ms timeout)
- `firmware/src/api_handlers.cpp:148` — heap probe-malloc guard precedes the stream

**Bite scenario:** A slow Wi-Fi client requesting `/api/devices` keeps AsyncTCP back-pressured.
`out.print()` blocks waiting for TCP ACKs while `repoMutex_` is still held. Packet processor
on the other core tries to record an incoming device update, hits the 50ms timeout, drops
the update silently. Visible symptoms: device counts stop changing, RSSI freezes, anomaly
detection skipped. Single watcher tab on weak Wi-Fi is enough.

**Why it bites in 6 months:** the conference demo was on a tight LAN. Field use puts these
on flaky Wi-Fi where the back-pressure window is much larger. Likely silent — users notice
"feels stuck" but don't connect it to network conditions.

**Fix sketch:** snapshot the device array into a local heap allocation under the lock,
release the lock, stream from the snapshot. ~5KB for 50 devices. The probe-malloc gate at
`api_handlers.cpp:148` already protects the allocation.

**Verify first:** confirm `streamDevicesJson` is actually holding the mutex across the
async writes (not just during snapshot/build) and that `addTargetableDevice` really uses
50ms. If both true, this is the highest-likelihood bug on the list.

---

## 3. Unauthenticated read endpoints have no rate limit ⏳

**Files:**
- `firmware/src/api_security.cpp:180-226` — `checkRateLimit` implementation
- `firmware/src/api_security.h:115-120` — `REQUIRE_AUTH` macro (only path that invokes rate limit)
- `firmware/src/api_handlers.cpp` — read endpoints (`/api/dashboard`, `/api/recon/security`,
  `/api/temporal`, `/api/anomalies`, `/api/replay/slots`, `/api/recon/summary`) call
  neither `REQUIRE_AUTH` nor `checkRateLimit`

**Bite scenario:** repo is public, the talk is public, the AP password may end up known.
Anyone on the AP can hammer `while true; do curl /api/dashboard; done` from one device.
The 65KB JSON builder fires every request. Heap guard correctly rejects with 503, but
the loop still queues 503 responses through AsyncTCP and starves the WebSocket broadcast.
Combined with #2 → trivial DoS until reboot.

**Why it bites in 6 months:** likelihood depends on adversarial interest. If anyone uses
this defensively (monitoring their own mesh) and gets noticed by an actor, this is the
softest target on the device.

**Fix sketch:** add per-IP token-bucket middleware that runs before every handler, not
gated on auth. 10 req/sec/IP for read endpoints is enough — legitimate clients poll at
~1 req/sec. Existing `checkRateLimit` infrastructure can probably be reused.

**Verify first:** confirm the listed endpoints actually skip rate-limit. If even one uses
a middleware path the agent missed, the picture changes.

---

## 4. `numAnomalies` overflow corrupts ack indexing ⏳

**Files:**
- `firmware/src/recon_state.h:50` — `numAnomalies` declared as `uint16_t`
- `firmware/src/recon_state.cpp:480-498` — anomaly recording, increments `numAnomalies`
- `firmware/src/recon_state.cpp:566-575` — `getUnacknowledgedAnomalies`
- `firmware/src/api_handlers.cpp:362-375` — `acknowledgeAnomaly` indexed by user-submitted slot

**Bite scenario:** ~65K anomaly events triggers wraparound. Circular buffer keeps working,
but the UI's "ack" call indexes the buffer by slot number — after wrap, the UI fetched a
snapshot showing anomaly X at slot N; user clicks ack; backend now has a different anomaly
at slot N. Silent ack of the wrong event. For a security-research tool, this corrupts the
audit trail.

**Why it bites in 6 months:** 65K seems large, but on a noisy ISM band with rate-violation
detection enabled, that's days, not months. Likely to trigger on any long unattended capture.

**Fix sketch:** ack by `(nodeId, timestamp)` tuple instead of slot index. Or include a
session-id in the API response that the client must echo on ack, and reject mismatches.

**Verify first:** check whether `getUnacknowledgedAnomalies` already does any
slot-stability validation. Also confirm the UI actually indexes by slot (vs. some
stable id).

---

## 5. `lastPositionExtracted_` is an unguarded `static` (cross-core landmine) ⏳

**Files:**
- `firmware/src/psk_decryption_simple.cpp:27` — `static bool lastPositionExtracted_`
- `firmware/src/psk_decryption_simple.cpp:441, 554, 698` — set/cleared inside `testDefaultPSKs()`
- `firmware/src/packet_processor.cpp:305` — read via `PSKDecryption::wasLastDecryptionPosition()`

**Bite scenario:** today the read and write are both on the main loop core, so it works.
But any future refactor that calls `testDefaultPSKs()` from a serial command handler, a
replay flow, or a web handler (AsyncTCP core) will start tagging random subsequent packets
as position packets in the SD log. Operator opens their CSV after a long capture and finds
GPS coords next to nodes that never broadcast a position. Already adjacent to the landmine
fixed in commit `0e439ee` (Meshtastic position extraction returning false).

**Why it bites in 6 months:** low probability of triggering with current code, but the
existence of the static is a trap waiting for a maintainer who doesn't know it's load-
bearing on call site. Silent data corruption in the field-collected dataset is the worst
class of bug for a tool that exists to produce trustworthy data.

**Fix sketch:** `testDefaultPSKs()` already returns `bool`. Add an output parameter or
return a small struct that carries the position-extracted flag, then drop the static
entirely. ~10 line change.

**Verify first:** quick — just check the call sites are limited to the main loop.

---

## Recommended next actions

1. **#1 first** (verified, concrete fix, ships before any user tries OTA on T-Beam Supreme)
2. **#5 second** (cheap to fix, removes a landmine)
3. **#2 third** (verify the mutex hold first; if confirmed, fix before next field deploy)
4. **#3 and #4** can wait but should become tracked issues
