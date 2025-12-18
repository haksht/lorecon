# ESP32 LoRa Sniffer - AI Agent Instructions

## Project Overview
ESP32-S3 passive LoRa reconnaissance firmware for security research. **Hardware-specific to Heltec WiFi LoRa 32 V3** (SX1262 radio + OLED). Not a generic project—T-Deck variants are explicitly unsupported.

## Critical Architecture (v2.0 Refactor)

### Component Boundaries
- **`RadioController`**: SX1262 hardware abstraction. Atomic ISR flags (`std::atomic<bool> packetAvailable`), cached RSSI/SNR to avoid SPI spam. Global instance (`g_radioController`) required for ISR access.
- **`PacketProcessor`**: Queue-based analysis. Processes interrupt-captured packets, runs protocol analysis, PSK decryption (23 keys incl. leaked 2023 admin keys), GPS extraction. Optional callback for WebSocket streaming.
- **`LoRaReconTool`**: Application orchestrator implementing `IReconTool` interface. Coordinates components, manages modes (recon/targeted), handles 5min scan cycle through 26 LoRa configs.
- **`IReconTool`**: Interface pattern breaks circular dependency with `CommandHandler`. Use this, not concrete class, for dependencies.
- **`CommandHandler`**: Dispatch table (not if/else chains). Static command functions take `IReconTool*`.
- **`ReconState`**: Device tracking, RF stats, replay slots. Singleton pattern with 80-device circular buffer.

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
C:\Users\tim\.platformio\penv\Scripts\pio.exe run --target upload

# Upload web UI files to LittleFS (required for browser interface)
C:\Users\tim\.platformio\penv\Scripts\pio.exe run --target uploadfs

# Monitor serial output (115200 baud)
C:\Users\tim\.platformio\penv\Scripts\pio.exe device monitor

# All-in-one: flash + upload filesystem + monitor
C:\Users\tim\.platformio\penv\Scripts\pio.exe run --target upload --target uploadfs --target monitor
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
23 Meshtastic keys tested on encrypted packets (includes leaked 2023 admin/debug keys). `PSKDecryption::testDefaultPSKs()` called from `PacketProcessor`, not UI layer. `PSKTests::runAll()` runs at boot for validation.

### 5. Error Handling
No exceptions (embedded C++). Return `bool` for success/failure. Use `LOG_ERROR()`, `LOG_WARN()`, `LOG_INFO()` macros from `logger.h`. Never `printf()` directly.

## Critical Files for Understanding

- **`docs/technical/ARCHITECTURE.md`**: 1400-line deep dive into v2.0 design, ISR patterns, memory safety
- **`firmware/src/main.cpp`**: 108-line entry point showing WiFi/LittleFS/PSK test initialization sequence
- **`firmware/src/irecon_tool.h`**: Interface contract that breaks circular dependencies
- **`firmware/src/data_structures.h`**: All shared structs (17 definitions): `CapturedPacket`, `QueuedPacket`, `ScanConfig`, etc.
- **`firmware/src/utils/`**: Shared utilities (`format_utils.h`, `protobuf_utils.h`, `security_scorer.h`)
- **`platformio.ini`**: Build flags include `-DBOARD_HELTEC_V3`, `-DHAS_OLED_DISPLAY`. Filters exclude test files with `build_src_filter`.

## Common Gotchas

1. **Hardware Specificity**: Code assumes Heltec V3 pin layout (`Config::Hardware::*`). Don't suggest generic ESP32 solutions.
2. **Global Instances for ISR**: `g_radioController` and `g_reconTool` required for interrupt handler access. Not a design mistake.
3. **Memory Constraints**: ESP32-S3 has 8MB PSRAM but avoid heap fragmentation. `std::vector` with `reserve()` preferred over dynamic allocation in loops.
4. **LittleFS vs SD**: Web app in LittleFS (built-in), PCAP/CSV logs on SD card (optional). Don't confuse the two.
5. **Frequency Configs**: 26 configs define Meshtastic, LoRaWAN, Helium presets. Cycle time = 26 × 12s = 5min. Don't suggest random frequency additions.
6. **No Arduino String**: Use `std::string` or C-strings. Arduino `String` class avoided for memory fragmentation reasons.
7. **Web UI Script Loading**: Scripts load in order: `toast.js`, `war-room.js`, `network-map.js`, `app.js`. If "NetworkMap class not found" error occurs, re-run `uploadfs` to ensure all files uploaded correctly to LittleFS.
8. **PSK Key Count**: Always use `Config::PSK::NUM_DEFAULT_KEYS` constant (23) or `PSKDecryption::getDefaultPSKCount()` function. Never hardcode key counts.
9. **Queue Overflow**: 100-packet queue can overflow in high-traffic (50+ devices). Drops are tracked and displayed. Trade-off: continuous coverage vs perfect capture. See TROUBLESHOOTING.md for mitigation strategies.
10. **strcpy Safety**: Use `strncpy` with explicit null termination for buffer safety, even when copying string literals.
11. **API Security (v2.2.0+)**: Protected endpoints require `X-API-Token` header. Use `APISecurity::isAuthenticated(request)` guard on destructive endpoints. Token displayed at boot in serial output.
12. **WiFi Credentials**: Stored in NVS (`Preferences` API), not JSON files. Use `WiFiManager` methods, never direct file access.
13. **Device-Unique AP Password**: Format is `recon-XXYYZZ` where XXYYZZ matches the SSID suffix. Use `Config::WiFi::DEFAULT_AP_PASSWORD_PREFIX`.
14. **Shared Utilities**: Use `utils/format_utils.h` for node ID formatting, `utils/security_scorer.h` for security assessment. Don't duplicate this logic in new code.

## Testing Approach

No unit test framework (embedded constraints). Validation via:
- **Boot-time PSK tests**: `PSKTests::runAll()` validates decryption with known data
- **Real hardware testing**: Serial monitor + web UI + Python visualizer (`tools/demo.py`)
- **Compile-time checks**: `-Werror=return-type`, `-Werror=format` enforce correctness
- `build_src_filter` excludes `test_*.cpp`, `unit_tests.cpp` from production builds

## Python Tooling (`tools/`)

- **`demo.py`**: Auto-detect ESP32 port, launch visualizer, open web UI, enable audio feedback
- **`enhanced_live_visualizer.py`**: 5-panel matplotlib dashboard for conference demos
- **`pc_analyzer.py`**: Parse serial output for offline analysis
- **Requirements**: `pip install -r tools/requirements.txt` (matplotlib, pyserial, websocket-client)

## When Modifying Code

1. **Keep module line counts low**: RadioController ~200 lines, PacketProcessor ~180 lines. Extract new components if growing beyond ~300 lines.
2. **Update relevant docs**: `API_REFERENCE.md` for HTTP endpoints, `docs/technical/ARCHITECTURE.md` for major design changes.
3. **Test on hardware**: No simulator. Flash real Heltec V3 board.
4. **Preserve web UI simplicity**: `data/webapp/` is vanilla HTML+CSS+JS. No build step. Keep it under 100KB total.
5. **Document PSK key additions**: If adding keys to `psk_decryption_simple.cpp`, document source/rationale.

## References

- Hardware details: `docs/hardware/TDECK_PLUS_INVESTIGATION.md` explains why T-Deck unsupported
- API surface: `API_REFERENCE.md` (1000+ lines) documents all HTTP/WebSocket endpoints
- User workflows: `GETTING_STARTED.md` for serial/OLED/web interfaces
- Encryption: `docs/technical/ENCRYPTION.md` for Meshtastic PSK details
