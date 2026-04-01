# Changelog

All notable changes to the ESP32 LoRa Sniffer project.

## [2.4.1] - 2026-03-29

### Fixed
- **`/api/auth/token` for auto-trusted LAN clients**: Endpoint was rejecting requests from clients on the local LAN (STA mode, non-AP subnet) that are auto-trusted. Now issues a token correctly without requiring a pre-existing token.
- **esptool `write_flash` syntax**: Flash scripts used `write-flash` (hyphen); corrected to `write_flash` (underscore) for esptool v5+ compatibility.
- **OLED boot progress rendering**: `showBootProgress()`, `showWelcome()`, and `showShutdown()` never rendered because `needsRedraw_` was not set. Fixed.

## [2.4.0] - 2026-03-28

### Added
- **Filesystem OTA** (`POST /api/filesystem/upload`): Upload a new `webapp.bin` LittleFS image over WiFi — no serial cable required for webapp updates.
- **AP password config** (`POST /api/wifi/ap-password`): Change the device access point password via API; persisted in NVS.
- **Auto token retrieval** (`GET /api/auth/token`): Returns the API token for clients that cannot read serial output (primarily Heltec V4 in no-serial-monitor mode). Only available to auto-trusted LAN clients.

### Improved
- **Settings tab**: AP password change UI added; token display improved for no-serial-monitor boards.
- **OLED boot sequence**: Progress bar and welcome screen display correctly on all boards.

## [2.3.3] - 2026-03-28

### Fixed
- **LoRaWAN sync word**: All TTN, LoRaWAN US915, and Helium scan configs were using sync word `0x12` (RadioLib private) instead of `0x34` (LoRaWAN public). The SX1262 only demodulates packets matching the configured sync word, so the device was physically incapable of receiving real LoRaWAN/TTN/Helium traffic on those 11 configs. Fixed.

### Added
- **RadioHead RH_RF95 protocol detection**: Common Arduino/RadioLib sensor library uses a 4-byte MAC header `[TO][FROM][ID][FLAGS]` with reserved lower 5 bits of FLAGS always zero. Detected after LoRaWAN check, before falling through to Unknown. FROM address (byte 1, 0–255) used as node ID. Device types: RadioHead Broadcast / RadioHead ACK / RadioHead Node.

### Changed
- **Tools directory**: Removed 12 low-value files — `session_analyzer.py`, `timeline_replay.py`, `packet_differ.py`, `position_tracker.py`, `compliance/`, `visualization/`. Rewrote README to match the remaining tools. (`lorawan/` was rebuilt with `join_parser.py` and `uplink_parser.py` — not removed.)

## [2.3.1] - 2026-03-22

### Fixed
- **PCAP pseudo-header size**: Meshtastic tools were using 16-byte header instead of the correct 20-byte pseudo-header, causing payload misalignment
- **Message truncation**: Decrypt reveal page truncated long messages

### Added
- **Decrypt reveal page generator**: `tools/demo/make_reveal.py` auto-generates a dramatic presentation page from a PCAP capture file with encrypted-to-decrypted reveal animations
- **Wireshark exporter documentation**: Dedicated section in tools README documenting the LoRaTap conversion workflow

### Documentation
- Added demo tools (make_reveal, decrypt_reveal) to README and tools documentation
- Conference-readiness cleanup for v2.3.0 release

## [2.3.0] - 2026-03-22

### Fixed
- **Meshtastic device type detection**: `identifyDeviceType` and `estimateFirmwareVersion` were reading byte 8 (packet ID) instead of byte 12 (flags). Caused false "Meshtastic Router" labels based on random packet ID bits.
- **Consolidated report version**: `buildConsolidatedReportJson` had hardcoded `"2.2.1"` instead of using `Config::VERSION`.
- **MAX_PACKET_SIZE off-by-one**: Was 256 (accepted 256-byte packets that immediately triggered anomaly). LoRa PHY max is 255 bytes; corrected constant and static_assert.
- **Scan config static_assert**: Used `<=` which allowed adding configs without updating `NUM_CONFIGURATIONS`; changed to `==`.
- **PMU controller header-only ODR bug**: Anonymous namespace in `pmu_controller.h` gave each translation unit its own `pmu` and `_initialized`. Battery always read 0V/0% on T-Beam Supreme. Split into header + `.cpp` file.
- **T-Beam Supreme battery reading**: `/api/status` was reporting 0% because `batteryPercent`/`batteryVoltage` used the ADC path with `VBAT_ADC_PIN = PIN_UNUSED`. Now branches on `HAS_AXP2101` and reads from the AXP2101 coulomb counter via `PMUController`.
- **T-Beam Supreme incomplete shutdown**: `esp_deep_sleep_start()` left AXP2101 running with all power rails live. Now calls `PMUController::shutdown()` to cut all PMIC rails.

### Added
- **Heltec V4 GPS support** (issue #12): New `heltec_v4` PlatformIO environment with L76K GNSS module (RX=38, TX=39, EN=34 active-LOW). Use `heltec_v3` env if your V4 has no GPS module.
- **`GPS_EN_LEVEL` board constant**: Encodes correct enable polarity per board (T-Beam Supreme = `HIGH`, Heltec V4 = `LOW`).
- **Sniffer GPS in webapp** (issue #11): `/api/status` includes `gps` object when `HAS_GPS` is defined. Status card shows coordinates + satellite count.
- **Web UI Power Off**: New button posts `command: 's'` to `/api/command`. T-Beam Supreme = hard PMIC power-off; others = deep sleep.
- **`/api/command` shutdown command**: `s` command for safe device shutdown (radio standby, display off, PMIC power-off or deep sleep).
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
- **Captured Packets subtitle** (issue #8): Clarified that the packet list is a 10-slot replay buffer (oldest entries overwritten) and that the full history is in the SD card CSV log.
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

For detailed API documentation, see [docs/developers/API.md](docs/developers/API.md)
For architecture details, see [docs/developers/ARCHITECTURE.md](docs/developers/ARCHITECTURE.md)
