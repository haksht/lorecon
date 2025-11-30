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
- **Version:** 2.1.0 (PCAP Export + Enhanced Visualization)
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

## Build & Flash

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
3. Use the interactive web UI with **6 tabs**:
   - **Info:** System status, GPS data, security assessment, frequency analysis
   - **Devices:** Discovered devices with targeting controls and packet counts
   - **Packets:** Captured packets with formatted timestamps and replay controls
   - **Frequencies:** 26-config targeting menu with activity indicators
   - **Network:** Interactive canvas map with:
     - Threat-level color coding (red/orange/green based on vulnerability score)
     - Signal strength heatmaps with radial gradients
     - RSSI-based device positioning
     - Touch-friendly controls for mobile
     - Real-time device discovery animation
   - **Stats:** Protocol breakdown with war room dashboard
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
  assets/                   -> inline CSS/JS bundle (small footprint)

docs/
  user-guides/        -> BUILD_GUIDE.md, FEATURES.md, TROUBLESHOOTING.md
  technical/          -> ARCHITECTURE.md, ENCRYPTION.md, NETWORK_HUNTING_GUIDE.md
  hardware/           -> TDECK_STATUS.md, TDECK_PLUS_INVESTIGATION.md
  api/                -> recon_service.md

tools/
  live_visualizer.py, pc_analyzer.py, analyze_logs.py
```

## Legal Use

The firmware is receive-only and intended for networks you own or have permission to observe. Respect local RF regulations, never transmit without authorization, and avoid intercepting private traffic. Default PSKs are tested only to highlight poor channel hygiene.

## Contributing

1. Keep modules focused; avoid reintroducing large one-off docs into the root.
2. Prefer short, well-commented C++ patches with unit-test hooks where possible.
3. Keep the web UI lightweight (plain HTML + minimal JS). If functionality expands, consider a build step that still outputs vanilla assets under `data/webapp`.

Issues and PRs are welcome—please include hardware details, firmware commit, and repro steps when filing bugs.
