# Changelog

All notable changes to the ESP32 LoRa Sniffer project.

## [2.3.3] - 2026-03-02

### Added
- **Heltec V4 GPS support** (issue #12): New `heltec_v4` PlatformIO environment for Heltec WiFi LoRa 32 V4 with the optional L76K GNSS module attached. Enables `HAS_GPS`, adds TinyGPSPlus, and configures UART pins (RX=38, TX=39) with active-LOW enable on GPIO 34. GPS data flows through the existing GPS pipeline: fix status, coordinates, and satellite count visible in the webapp Status card. Use `heltec_v3` if you have a V4 without the GPS module — hardware is otherwise identical.
- **`GPS_EN_LEVEL` board constant**: `Config::Hardware::GPS_EN_LEVEL` encodes the correct enable polarity per board (T-Beam Supreme = `HIGH`, Heltec V4 = `LOW`). `gps_controller.cpp` uses this constant instead of a hardcoded level.

### Improved
- **Captured Packets subtitle** (issue #8): Clarified that the packet list is a 10-slot replay buffer (oldest entries overwritten) and that the full history is in the SD card CSV log.

## [2.3.2] - 2026-03-02

### Added
- **Sniffer GPS in webapp** (issue #11): `/api/status` now includes a `gps` object (`hasFix`, `lat`, `lon`, `alt`, `satellites`) when `HAS_GPS` is defined. The Info tab System Status card shows a GPS row: coordinates + satellite count with a fix, "No fix (N sats)" while acquiring. Row hidden on boards without GPS hardware.

## [2.3.1] - 2026-03-02

### Fixed
- **T-Beam Supreme battery reading**: `/api/status` was reporting 0% for all T-Beam Supreme boards because `batteryPercent`/`batteryVoltage` used the ADC path with `VBAT_ADC_PIN = PIN_UNUSED`. Now branches on `HAS_AXP2101` and reads directly from the AXP2101 coulomb counter via `PMUController`.
- **T-Beam Supreme incomplete shutdown**: Button long-press called `esp_deep_sleep_start()` only, leaving the AXP2101 running with all power rails live (blue LED visible after shutdown). Now calls `PMUController::shutdown()` which cuts all PMIC rails before falling through to deep sleep as a safety net.

### Added
- **Web UI Power Off**: New "Power Off" button in the Quick Tools card (Info tab) alongside the existing Reboot button. Posts `command: 's'` to `/api/command` with a confirmation dialog. Works on all boards — T-Beam Supreme performs a hard PMIC power-off; Heltec V3/V4 and T3-S3 enter deep sleep.
- **`/api/command` shutdown command**: New `s` command shuts down the device safely (radio standby → display off → PMIC power-off or deep sleep).
- **`PMUController::shutdown()`**: New function in `pmu_controller.h` calls `pmu.shutdown()` to cut all AXP2101 rails. No-op stub provided for non-AXP2101 boards.

## [2.3.0] - 2026-02-22

### Added
- **T-Beam Supreme board support**: Full firmware port for LilyGO T-Beam Supreme (ESP32-S3FN8, 8MB flash/PSRAM)
  - AXP2101 PMIC driver (XPowersLib) — mandatory init before all other peripherals
  - SH1106 128×64 OLED via dedicated I2C bus (SDA=17, SCL=18)
  - SD card on HSPI (CS=47, SCK=36, MISO=37, MOSI=35) separate from LoRa FSPI bus
  - GPS (UART RX=9, TX=8, EN=7) with TinyGPS++ integration
  - TCXO 1.8V via AXP2101 ALDO3; RadioLib TCXO mode flag set in `begin()`
  - 8MB partition layout (`firmware/partitions_8MB.csv`): ~3MB app + ~5MB LittleFS
- **GPS location stamping**: Packets received with a GPS fix include `latitude`, `longitude`, `altitude` in CSV logs and WebSocket `packet` events (guarded by `HAS_GPS`)
- **Multi-board release packaging**: `tools/make_release.sh` / `tools/make_release.bat` — builds all three boards, merges binaries with esptool, publishes to GitHub Releases via `gh` CLI
- **Wireless data export**: 4 new sidebar buttons — Download CSV, Download PCAP, Export KML, Export GeoJSON
- **CSV download endpoint**: `GET /api/export/csv` streams current session log directly from SD card
- **File browser API**: `GET /api/files` lists `/logs/` directory; `GET /api/files/download?name=<file>` retrieves any past session file
- **SD flush before stream**: `packetLogger.flush()` called before every SD streaming response to expose unflushed FAT write buffer

### Fixed
- **`/api/status` board name**: Was hardcoded to `"HELTEC_V3"` for all boards; now board-conditional (`HELTEC_V3` / `T3_S3` / `TBEAM_SUPREME`)
- **`runDiagnostics()` recovery path**: T-Beam Supreme was falling into Heltec crystal-mode recovery (no TCXO, no DIO2); fixed to use shared `BOARD_T3_S3 || BOARD_TBEAM_SUPREME` branch
- **API auth documentation**: Corrected `API_REFERENCE.md` auto-trust section — AP-subnet clients (`192.168.4.x`) are NOT auto-trusted; token always required when connecting directly to the device's own hotspot
- **KML/GeoJSON download headers**: Added `Content-Disposition: attachment` so browsers download instead of rendering inline
- **GeoJSON MIME type**: Changed to `application/geo+json` (correct RFC 7946 type)
- **`actionExportKML`**: Replaced `window.open()` with blob download (was opening in browser tab)
- **`entry.name()` full-path bug**: `handleListFiles` now strips directory prefix from SD file entries; without fix, `/api/files` and `/api/files/download` were incompatible
- **File handle leak**: `handleListFiles` now calls `dir.close()` on all error paths
- **Error handling**: All four download actions (`actionExportCSV`, `actionExportPCAP`, `actionExportKML`, `actionExportGeoJSON`) wrapped in try/catch for network errors

### Improved
- **OLED scanning mode**: Shows IP address and mDNS hostname (`hostname.local`) on two footer lines instead of cramming both on one line
- **OLED targeting mode**: Now also shows mDNS hostname alongside IP address

## [2.2.3] - 2026-01-02

### Added
- **LoRaWAN Key Testing**: AES-CMAC MIC verification against 16 default AppKeys
  - Detects devices using factory default or well-known keys
  - Serial command `w` shows stats summary
  - `/api/recon/summary` includes `lorawanStats` object
- **Channel Hash Mapping**: Maps Meshtastic channel hash to known names
  - `channelName` field in replay slots (LongFast, admin, ncmesh, etc.)
  - `utils/channel_hash.h/cpp` utility module

### Improved
- **API Documentation**: Complete `/api/recon/summary` endpoint documentation
- **Replay Slots API**: Added `snr`, `channel`, `channelName`, `hopCount`, `destId`, `isBroadcast`, `wantAck`, `viaMqtt`, `priority` fields

### Documentation
- Updated all docs to match codebase
- Removed obsolete `DEBUG_HANDOFF_2024-12-30.md`
- Added LoRaWAN key testing to FEATURES.md, ARCHITECTURE.md
- Updated serial commands in GETTING_STARTED.md (added `t`, `w` commands)

## [2.2.2] - 2026-01-02

### Fixed
- **WebSocket crash prevention**: Added `queueIsFull()` check before broadcast to prevent memory exhaustion
- **API endpoint protection**: Heap check on `/api/devices` returns 503 if memory < 50KB

### Added
- **Heap monitoring**: 5-minute interval heap status logging for long-duration debugging
- **API heap logging**: `/api/devices` endpoint logs heap before/after JSON building

### Documented
- **Hardware sensitivity variance**: SX1262 radio chips vary in receive sensitivity between units (see TROUBLESHOOTING.md)

### Stability
- **15+ hour battery test**: Two devices ran overnight with zero crashes
- **Memory stable**: No leaks detected (188-189KB free after 15h)
- **Confirmed hardware variance**: Identical firmware, ~50% packet count difference between units due to radio sensitivity

## [2.2.1] - 2024-12-21

### Fixed
- **Serial noise hardening**: Phantom mode switches caused by USB electrical noise
  - Double-Enter activation required (2 keypresses within 1.5s)
  - `\r\n` debounce prevents single keypress from double-triggering (100ms window)
  - Auto-deactivation after 5 minutes of inactivity
  - Protects week-long unattended operation

### Added
- Debug logging for packet replay API (`[API] buildReplaySlotsJson`)
- `INACTIVITY_TIMEOUT_MS` constant for configurable auto-deactivation

## [2.2.0] - 2024-12-20

### Security
- **API Authentication**: Token-based auth for destructive endpoints (`X-API-Token` header)
- **Device-unique AP passwords**: Format `recon-XXYYZZ` based on MAC address
- **NVS credential storage**: WiFi passwords stored securely, not in JSON files
- **XSS prevention**: All user input escaped in web UI
- **Input validation**: Bounds checking on all API parameters

### Added
- **AP_STA dual WiFi mode**: Device accessible via AP (192.168.4.1) even when connected to hotspot
- **Serial activation**: Commands ignored until Enter pressed (prevents phantom commands from USB noise)
- **Network map visualization**: Interactive threat-colored SVG with device clustering
- **Security scoring**: Multi-factor assessment (RSSI proximity, encryption, router role, traffic patterns)
- **GeoJSON/KML export**: Geographic data export for mapping tools
- **Position tracker tool**: `tools/position_tracker.py` for real-time device triangulation
- **Conference demo tools**: `tools/enhanced_live_visualizer.py` 5-panel dashboard

### Changed
- **Build hardening**: `-Werror` enabled for source files (warnings as errors)
- **Repository pattern**: Device tracking extracted to `DeviceRepository` with Welford's RSSI stats
- **Packet deduplication**: Hash-based filtering in `PacketStore`
- **Replay slots**: Converted to FIFO circular buffer
- **Web UI modular refactor**: Split into `toast.js`, `war-room.js`, `network-map.js`, `app.js`

### Fixed
- Boot logging and PSK banner display
- API token printf format (Arduino String `.c_str()` requirement)
- Documentation accuracy (18 discrepancies fixed)
- Unsigned comparison warning in `wifi_manager.cpp`

### Technical
- 26 frequency configs (Meshtastic, LoRaWAN, Helium, ISM)
- 23 PSK keys including legacy admin channel defaults
- 50 max targetable devices with LRU eviction
- 100-packet processing queue
- 14+ hours stable in targeting mode (631 packets, 44 devices)

## [2.0.0] - 2024-11-13

### Added
- Complete v2.0 architecture refactor
- Component-based design: `RadioController`, `PacketProcessor`, `LoRaReconTool`, `ReconState`
- `IReconTool` interface pattern for dependency inversion
- Atomic ISR flags for thread-safe packet reception
- Web UI served from LittleFS
- PCAP export with LoRa metadata

### Changed
- Removed all offensive/stress testing features (passive recon only)
- Config namespace organization (`Config::Hardware::*`, `Config::Scanning::*`)
- Dispatch table command handler (replaced if/else chains)

---

For detailed API documentation, see [API_REFERENCE.md](API_REFERENCE.md)  
For architecture details, see [docs/technical/ARCHITECTURE.md](docs/technical/ARCHITECTURE.md)
