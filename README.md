# ESP32 LoRa Packet Sniffer & Reconnaissance Tool

Passive LoRa reconnaissance firmware for ESP32-S3 + SX1262 hardware. The ESP32 handles radio control, packet capture, replay slots, PSK testing, GPS extraction, PCAP export, and WiFi access-point hosting so that a phone—or a serial console—can drive the entire workflow in the field.

## Quick Links

- `GETTING_STARTED.md` – complete setup guide with serial, OLED, and web interfaces.
- `docs/user-guides/BUILD_GUIDE.md` – supported hardware + PlatformIO notes.
- `docs/user-guides/FEATURES.md` – feature inventory and configuration knobs.
- `docs/user-guides/TROUBLESHOOTING.md` & `docs/technical/ENCRYPTION.md` – protocol gotchas.
- `API_REFERENCE.md` – HTTP + WebSocket surface area for custom tooling.

## Current Status

- **Branch:** `main`
- **Version:** 2.2.1 (Serial Noise Hardening + Long-Duration Operation)
- **Hardware:** **Heltec WiFi LoRa 32 V3** (ESP32-S3 + SX1262 + OLED). Optional SD card.
  - ✅ **Fully Supported:** Heltec WiFi LoRa 32 V3 (tested, production-ready)
  - ⚠️ **Not Supported:** T-Deck variants (hardware incompatibilities - see `docs/hardware/TDECK_PLUS_INVESTIGATION.md` for details)
  - ℹ️ **Note:** Codebase is Heltec-specific. Other boards would require porting.
- **Focus:** Passive reconnaissance (scan, target, capture, replay, export). All legacy offensive/stress docs and code paths have been removed.
- **Web UI:** Lightweight interface served from LittleFS with interactive network visualization, threat assessment, and real-time packet analysis.

## Feature Snapshot

| Area | Highlights |
| --- | --- |
| Radio Control | `RadioController` wraps RadioLib with atomic ISR flagging and cached RSSI/SNR reads. The recon state cycles through 26 LoRa configs including Meshtastic, LoRaWAN/TTN, Helium Network, and ISM band (5 min cycle). |
| Packet Processing | `PacketProcessor` queues interrupt captures, runs protocol analysis, PSK decryption, diagnostics, and replay capture with dual logging (CSV + PCAP). |
| Recon State | `ReconState` tracks RF activity, targetable nodes, replay slots, quiet mode, and device intelligence with multi-factor security scoring. |
| UI Surfaces | 1) Serial menu (command handler with dispatch table). 2) OLED quick-look cards. 3) **WiFi Web UI** with threat-colored network map, signal heatmaps, vulnerability assessment, and live packet visualization. |
| Storage / Export | Optional SD logging (`packet_logger` with CSV + PCAP), KML/GeoJSON exports, Wireshark-compatible PCAP with LoRa metadata, security assessment reports, JSON APIs for scripting. |
| Security | Token-based API authentication for sensitive endpoints, device-unique AP passwords, NVS credential storage, XSS prevention, input validation with bounds checking. |

## Why This Over Alternatives?

| Tool | Cost | LoRa Decode | PSK Decrypt | Standalone | Web UI |
|------|------|-------------|-------------|------------|--------|
| **This project** | **$30** | ✅ Full | ✅ 23 keys | ✅ | ✅ |
| Flipper Zero + LoRa | $190-210 | Basic | ❌ | ✅ | ❌ |
| HackRF + GNU Radio | $300+ | ✅ | Manual | ❌ (laptop) | ❌ |

**vs. Flipper**: Flipper is a multi-tool; this is a specialist. 1/7th the cost, deeper Meshtastic analysis, includes leaked 2023 admin keys, mobile-friendly web UI.

**vs. SDR**: SDR requires a laptop and GNU Radio expertise. This is pocket-sized, flash-and-go, with built-in replay and phone-accessible results.

## Bill of Materials (~$35-50 USD)

| Component | Price | Notes | Link |
|-----------|-------|-------|------|
| **Heltec WiFi LoRa 32 V3** | $22-28 | ESP32-S3 + SX1262 + OLED | [Amazon](https://www.amazon.com/dp/B0B697NLJ5) / [AliExpress](https://www.aliexpress.com/item/1005005967763162.html) |
| 915MHz Antenna (RP-SMA) | $0-8 | Often included with board | [Amazon](https://www.amazon.com/dp/B09K3WMCVN) |
| USB-C Cable | $0 | You have one | — |
| **Optional:** 3D Printed Case | $5-10 | Search "Heltec V3 case" | [Printables](https://www.printables.com/search/models?q=heltec%20v3) |
| **Optional:** 18650 Battery | $8-12 | For portable operation | [Amazon](https://www.amazon.com/dp/B0BG8XJMQX) |
| **Optional:** SD Card | $5-8 | For PCAP logging | Any microSD |

**Total**: ~$30 basic / ~$50 with case + battery

**Time to first packet**: Under 5 minutes (flash → power → scanning)

## Quick Start (Pre-compiled Binary)

**Don't want to compile?** Download the pre-built firmware:

1. Go to [Releases](https://github.com/tiarno/esp32-sniffer/releases/latest)
2. Download `esp32-lora-sniffer-v2.2.0-binaries.zip`
3. Extract and run `flash.bat` (Windows) or `flash.sh` (Linux/Mac)
4. Requires: `pip install esptool`

See `FLASH_INSTRUCTIONS.md` in the zip for detailed steps.

## Build & Flash (From Source)

```powershell
# from repo root
pio run --target upload          # Flash firmware
pio run --target uploadfs        # Upload web UI files
pio device monitor               # View serial output
```

LittleFS serves the web UI assets. SD card is optional but required for PCAP export and long-term logging.

## Web Interface Features

1. Power the ESP32 and wait for `ESP32-LoRa-Sniffer` WiFi AP.
2. Connect a phone/laptop, browse to `http://192.168.4.1`.
3. Use the interactive web UI with **7 tabs**:
   - **Info:** System status, GPS data, security assessment, frequency analysis
   - **Devices:** Discovered devices with targeting controls and packet counts
   - **Replay:** Captured packets with formatted timestamps and replay controls
   - **Frequencies:** 26-config targeting menu with activity indicators
   - **Network:** Interactive canvas map with:
     - Threat-level color coding (red/orange/green based on vulnerability score)
     - Signal strength heatmaps with radial gradients
     - RSSI-based device positioning
     - Touch-friendly controls for mobile
     - Real-time device discovery animation
   - **Stats:** Protocol breakdown with war room dashboard
   - **Settings:** WiFi configuration, diagnostics, system controls
4. **Security Assessment:**
   - Multi-factor vulnerability scoring (0-100 scale)
   - Considers: RSSI proximity, encryption status, router role, traffic patterns, firmware version
   - Consistent scoring across Info tab and Security modal
5. **PCAP Export:**
   - Wireshark-compatible packet captures with custom LoRa pseudo-header
   - Preserves RSSI, SNR, frequency, timestamp metadata
   - Download via Quick Actions menu
   - Automatic error handling with user feedback

WebSocket streams real-time updates to all connected clients with automatic reconnection.

## Serial Command Reference (still available over USB)

| Command | Description |
| --- | --- |
| `m` | Menu with recon results and device list |
| `f` | Frequency targeting (manual config selection) |
| `a` | RF activity & diagnostics |
| `d` | Device type summary |
| `v` | Security assessment summary |
| `g`,`k`,`j` | Geo intelligence / KML / GeoJSON |
| `r` | Resume reconnaissance |
| `b` | Confirmed reboot (clears state) |
| `c` | Capture last targeted packet into replay slot |
| `p` | Replay menu |
| `q` | Quiet/verbose toggle |
| `x` | Text-packet diagnostic report |

The web UI issues the same requests via `/api/command` so you can switch between USB and WiFi at any time.

## Repository Layout

```
firmware/src/
  main.cpp                  -> entry point, WiFi/LittleFS bring-up
  lora_recon_tool.*         -> orchestrator, implements IReconTool
  radio_controller.*        -> SX1262 abstraction + ISR hook
  packet_processor.*        -> queue, protocol analysis, PSK tests
  recon_state.*             -> configs, RF stats, device records
  command_handler.*         -> serial + web command routing
  wifi_manager.*, web_server.*, api_controller.*
  geo_intelligence.*, protocol_analyzer.*, text_packet_diagnostic.*
  packet_logger.*, logger.*, error_handler.*, ui components

data/webapp/
  index.html                -> minimal single-page UI
  css/style.css             -> dark theme, responsive layout
  js/                       -> app.js, network-map.js, war-room.js, toast.js

docs/
  user-guides/        -> BUILD_GUIDE.md, FEATURES.md, TROUBLESHOOTING.md
  technical/          -> ARCHITECTURE.md, ENCRYPTION.md, NETWORK_HUNTING_GUIDE.md
  hardware/           -> TDECK_STATUS.md, TDECK_PLUS_INVESTIGATION.md

tools/
  enhanced_live_visualizer.py  -> 5-panel matplotlib dashboard
  psk_auditor.py, recon_report.py, position_tracker.py
  api_client.py, ws_monitor.py, attack_dashboard.html
```

## Legal Use

The firmware is receive-only and intended for networks you own or have permission to observe. Respect local RF regulations, never transmit without authorization, and avoid intercepting private traffic. Default PSKs are tested only to highlight poor channel hygiene.

## Contributing

1. Keep modules focused; avoid reintroducing large one-off docs into the root.
2. Prefer short, well-commented C++ patches with unit-test hooks where possible.
3. Keep the web UI lightweight (plain HTML + minimal JS). If functionality expands, consider a build step that still outputs vanilla assets under `data/webapp`.

Issues and PRs are welcome—please include hardware details, firmware commit, and repro steps when filing bugs.
