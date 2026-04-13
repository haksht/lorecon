# ESP32 LoRa Sniffer - AI Agent Instructions

## Project Overview
ESP32-S3 passive LoRa reconnaissance firmware for security research. Supports four boards: **Heltec WiFi LoRa 32 V3/V4**, **LilyGO T3-S3**, and **LilyGO T-Beam Supreme** — all ESP32-S3 + SX1262. T-Deck variants are explicitly unsupported.

## Critical Architecture (v2.0 Refactor)

### Component Boundaries
- **`RadioController`**: SX1262 hardware abstraction. Atomic ISR flags (`std::atomic<bool> packetAvailable`), cached RSSI/SNR to avoid SPI spam. Global instance (`g_radioController`) required for ISR access.
- **`PacketProcessor`**: Queue-based analysis. Processes interrupt-captured packets, runs protocol analysis, PSK decryption (23 default keys incl. legacy admin defaults), GPS extraction. Optional callback for WebSocket streaming.
- **`LoRaReconTool`**: Application orchestrator implementing `IReconTool` interface. Coordinates components, manages modes (recon/targeted), handles ~6 min scan cycle through 29 LoRa configs.
- **`IReconTool`**: Interface pattern breaks circular dependency with `CommandHandler`. Use this, not concrete class, for dependencies.
- **`CommandHandler`**: Dispatch table (not if/else chains). Static command functions take `IReconTool*`.
- **`ReconState`**: Device tracking, RF stats, replay slots. Singleton pattern with 50-device fixed array (LRU eviction when full).

### Data Flow
```
ISR (radioISR) → RadioController.markPacketReceived() → 
LoRaReconTool.handlePacketReception() → PacketProcessor.queuePacket() → 
PacketProcessor.processQueue() → Protocol analysis + PSK testing → 
ReconState updates + WebSocket broadcast
```

## Build & Flash Workflow (PlatformIO)

```powershell
# Flash firmware to ESP32 (use full path on Windows)
pio run --target upload

# Upload web UI files to LittleFS (required for browser interface)
pio run --target uploadfs

# Monitor serial output (115200 baud)
pio device monitor

# All-in-one: flash + upload filesystem + monitor
pio run --target upload --target uploadfs --target monitor
```

**Key:** `uploadfs` uploads `data/webapp/` to LittleFS. Run once or when web assets change. SD card optional but needed for PCAP export.

## Project-Specific Patterns

### 1. Interrupt-Driven Reception (Thread-Safe)
RadioLib ISR sets atomic flag, main loop reads packet. Never blocking operations in ISR context.
```cpp
// In ISR context (IRAM_ATTR)
void IRAM_ATTR radioISR() {
    g_radioController->markPacketReceived();
}

// In main loop
if (radioController->hasPacket()) {
    int len = radioController->readPacket(buffer, sizeof(buffer));
    packetProcessor->queuePacket(buffer, len, rssi, snr, configIdx, freq);
}
```

### 2. Config Namespace Organization (`config.h`)
All magic numbers in `Config::` namespaces: `Config::Hardware::LORA_NSS`, `Config::Scanning::DWELL_TIME_MS`, `Config::PacketProcessing::QUEUE_SIZE`. Never hardcode pin numbers or timeouts.

### 3. Web Server Integration
`WebServer` uses `PacketProcessor::setPacketCallback()` for real-time updates. Aggregates stats every 100ms to avoid WebSocket spam. Uses `IReconTool*` interface, not concrete class.

### 4. PSK Testing
23 Meshtastic keys tested on encrypted packets (includes legacy admin channel defaults). `PSKDecryption::testDefaultPSKs()` called from `PacketProcessor`, not UI layer. `PSKTests::runAll()` runs at boot for validation.

### 5. LoRaWAN Key Testing
16 default AppKeys tested against LoRaWAN Join Request packets. Uses AES-CMAC MIC verification to detect devices using factory default or well-known keys. Stats exposed via `/api/recon/summary` and serial command 'w'. See `lorawan_keys.h/cpp`.

### 6. Channel Hash Mapping
Meshtastic channel hash (byte 13 of packet header) mapped to known channel names (LongFast, admin, ncmesh, etc.). Used in replay slots and packet analysis. See `utils/channel_hash.h/cpp`.

### 7. Error Handling
No exceptions (embedded C++). Return `bool` for success/failure. Use `LOG_ERROR()`, `LOG_WARN()`, `LOG_INFO()` macros from `logger.h`. Never `printf()` directly.

## Critical Files for Understanding

- **`docs/developers/ARCHITECTURE.md`**: deep dive into v2.0 design, ISR patterns, memory safety
- **`firmware/src/main.cpp`**: 108-line entry point showing WiFi/LittleFS/PSK test initialization sequence
- **`firmware/src/irecon_tool.h`**: Interface contract that breaks circular dependencies
- **`firmware/src/data_structures.h`**: All shared structs (16 definitions): `CapturedPacket`, `QueuedPacket`, `ScanConfig`, etc.
- **`firmware/src/utils/`**: Shared utilities:
  - `format_utils.h`: Node ID formatting (`formatNodeId`, `formatNodeIdJson`, `formatNodeIdPadded`, `estimatePowerClass`)
  - `protobuf_utils.h`: Varint decoding utilities
  - `security_scorer.h`: Unified security assessment scoring
  - `json_utils.h`: Standardized JSON response helpers (`JsonUtils::success()`, `JsonUtils::error()`, `JsonUtils::successWithData()`)
  - `channel_hash.h`: Maps Meshtastic channel hash (byte 13) to channel names (LongFast, admin, ncmesh, etc.)
- **`firmware/src/lorawan_keys.h/cpp`**: LoRaWAN Join Request parsing, AES-CMAC MIC verification, 16 default AppKeys testing
- **`firmware/src/repositories/`**: Storage layer (delegates from ReconState):
  - `device_repository.h/cpp`: Targetable device storage with Welford's RSSI stats (50 devices max, LRU eviction)
  - `packet_store.h/cpp`: Replay slot management with FIFO circular buffer and packet deduplication
- **`firmware/src/json_builders.h/cpp`**: Extracted JSON response builders from ReconService (13 functions)
- **`platformio.ini`**: Build flags include `-DBOARD_HELTEC_V3`, `-DHAS_OLED_DISPLAY`. Filters exclude test files with `build_src_filter`.

## Common Gotchas

1. **Hardware Specificity**: Pin layout is board-specific — always use `Config::Hardware::*` constants, never hardcode GPIO numbers. `BOARD_HELTEC_V3` covers both V3 and V4 (identical pinout). Don't suggest generic ESP32 solutions.
2. **Global Instances for ISR**: `g_radioController` and `g_reconTool` required for interrupt handler access. Not a design mistake.
3. **Memory Constraints**: T3-S3 has 2MB PSRAM (Quad), T-Beam Supreme has 8MB PSRAM (Quad), Heltec V3/V4 has no external PSRAM (320KB SRAM only). Avoid heap fragmentation. `std::vector` with `reserve()` preferred over dynamic allocation in loops.
4. **LittleFS vs SD**: Web app in LittleFS (built-in), PCAP/CSV logs on SD card (optional). Don't confuse the two.
5. **Frequency Configs**: 29 configs define Meshtastic, LoRaWAN, Helium presets. Cycle time = 29 × 12s = ~5.8 min. Don't suggest random frequency additions.
6. **No Arduino String**: Use `std::string` or C-strings. Arduino `String` class avoided for memory fragmentation reasons.
7. **Web UI Script Loading**: Scripts load in order: `toast.js`, `war-room.js`, `network-map.js`, `app.js`. If "NetworkMap class not found" error occurs, re-run `uploadfs` to ensure all files uploaded correctly to LittleFS.
8. **PSK Key Count**: Always use `Config::PSK::NUM_DEFAULT_KEYS` constant (23) or `PSKDecryption::getDefaultPSKCount()` function. Never hardcode key counts.
9. **Queue Overflow**: 100-packet queue can overflow in high-traffic (50+ devices). Drops are tracked and displayed. Trade-off: continuous coverage vs perfect capture. See TROUBLESHOOTING.md for mitigation strategies.
10. **strcpy Safety**: Use `strncpy` with explicit null termination for buffer safety, even when copying string literals.
11. **API Security (v2.2.0+)**: Protected endpoints require `X-API-Token` header. Use `APISecurity::isAuthenticated(request)` guard on destructive endpoints. Token displayed at boot in serial output.
12. **WiFi Credentials**: Stored in NVS (`Preferences` API), not JSON files. Use `WiFiManager` methods, never direct file access.
13. **Device-Unique AP Password**: Format is `recon-XXYYZZ` where XXYYZZ matches the SSID suffix. Use `Config::WiFi::DEFAULT_AP_PASSWORD_PREFIX`.
14. **Shared Utilities**: Use `utils/format_utils.h` for node ID formatting, `utils/security_scorer.h` for security assessment, `utils/json_utils.h` for API responses. Don't duplicate this logic in new code.
15. **API Response Format**: Use `JsonUtils::success()`, `JsonUtils::error()`, `JsonUtils::successWithData()` for consistent JSON responses. Don't manually build `{"status":"success"}` patterns.
16. **Arduino String with printf**: When passing Arduino `String` to `LOG_INFO`/`Serial.printf` with `%s`, always use `.c_str()`. The temporary String is destroyed before printf executes otherwise, causing garbage output.
17. **Serial Activation**: Serial commands are ignored until user presses Enter TWICE within 1.5 seconds. This prevents phantom commands from USB electrical noise. Press Enter twice quickly to activate serial console, then commands work normally.
18. **AP_STA Mode**: Device runs both AP (192.168.4.1) and STA (hotspot) simultaneously. Always reachable via AP even if hotspot connection drops.

## Testing Approach

No unit test framework (embedded constraints). Validation via:
- **Boot-time PSK tests**: `PSKTests::runAll()` validates decryption with known data
- **Real hardware testing**: Serial monitor + web UI + live Python monitor (`lorarecon dev monitor --host <ip> --tui`)
- **Compile-time checks**: `-Werror=return-type`, `-Werror=format` enforce correctness
- `build_src_filter` excludes `test_*.cpp`, `unit_tests.cpp` from production builds

## Python Tooling (`tools/`)

Unified entry point: `lorarecon <cmd>`. Three headline outputs and four dev utilities:

- **`report`**: Security assessment HTML report (main offline analysis)
- **`map`**: GPS positions → interactive folium HTML map
- **`topology`**: Mesh graph → PNG (Meshtastic traceroute + MeshCore)
- **`dev monitor`**: Live WebSocket packet stream (headless or `--tui`, supports `--demo`)
- **`dev wireshark`**: Convert ESP32 PCAP → LoRaTap (DLT 270) for Wireshark
- **`dev merge`**: Cross-capture identity linker (2+ CSVs)
- **`dev api`**: REST API client (~30 subcommands)
- **Requirements**: `pip install -r tools/requirements.txt` (cryptography, websocket-client, rich, folium, matplotlib, networkx, requests)

## When Modifying Code

1. **Keep module line counts low**: RadioController ~200 lines, PacketProcessor ~350 lines, LoRaReconTool ~850 lines. Extract new components if growing beyond ~400 lines.
2. **Update relevant docs**: `docs/developers/API.md` for HTTP endpoints, `docs/developers/ARCHITECTURE.md` for major design changes.
3. **Test on hardware**: No simulator. Flash real Heltec V3 board.
4. **Preserve web UI simplicity**: `data/webapp/` is vanilla HTML+CSS+JS. No build step. Keep it under 100KB total.
5. **Document PSK key additions**: If adding keys to `psk_decryption_simple.cpp`, document source/rationale.

## References

- Hardware details: `docs/HARDWARE.md` — board comparison, T-Deck unsupported section
- API surface: `docs/developers/API.md` — all HTTP/WebSocket endpoints
- User workflows: `docs/SETUP.md` (flash/connect), `docs/USAGE.md` (web UI/tools)
- Encryption: `docs/reference/ENCRYPTION.md` for Meshtastic PSK details
