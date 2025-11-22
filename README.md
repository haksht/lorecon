# ESP32 LoRa Packet Sniffer & Reconnaissance Tool

Passive LoRa reconnaissance firmware for ESP32-S3 + SX1262 hardware. The ESP32 handles radio control, packet capture, replay slots, PSK testing, GPS extraction, and WiFi access-point hosting so that a phone—or a serial console—can drive the entire workflow in the field.

## Quick Links

- `QUICKSTART.md` – build/upload/monitor steps.
- `PHONE_APP_GUIDE.md` – WiFi setup and phone workflow (mirrors the serial menu).
- `docs/BUILD_GUIDE.md` – supported hardware + PlatformIO notes.
- `docs/FEATURES.md` – feature inventory and configuration knobs.
- `docs/TROUBLESHOOTING_MESHTASTIC.md` & `docs/ENCRYPTION_REALITY.md` – protocol gotchas.
- `API_REFERENCE.md` – HTTP + WebSocket surface area for custom tooling.

## Current Status

- **Branch:** `main`
- **Hardware:** **Heltec WiFi LoRa 32 V3** (ESP32-S3 + SX1262 + OLED). Optional SD card.
  - ✅ **Fully Supported:** Heltec WiFi LoRa 32 V3 (tested, production-ready)
  - ⚠️ **Not Supported:** T-Deck variants (hardware incompatibilities - see `docs/TDECK_PLUS_INVESTIGATION.md` for details)
  - ℹ️ **Note:** Codebase is Heltec-specific. Other boards would require porting.
- **Focus:** Passive reconnaissance (scan, target, capture, replay, export). All legacy offensive/stress docs and code paths have been removed.
- **Web UI:** A new lightweight interface served from LittleFS. It exposes every serial command as a button or action so the phone experience equals the USB menu.

## Feature Snapshot

| Area | Highlights |
| --- | --- |
| Radio Control | `RadioController` wraps RadioLib with atomic ISR flagging and cached RSSI/SNR reads. The recon state cycles through 26 LoRa configs including Meshtastic, LoRaWAN/TTN, Helium Network, and ISM band (5 min cycle). |
| Packet Processing | `PacketProcessor` queues interrupt captures, runs protocol analysis, PSK decryption, diagnostics, replay capture, and emits events to the web server with audio feedback. |
| Recon State | `ReconState` tracks RF activity, targetable nodes, replay slots, quiet mode, and device intelligence across all 26 frequency configurations. |
| UI Surfaces | 1) Serial menu (command handler with dispatch table). 2) OLED quick-look cards. 3) **WiFi Web UI** with interactive network map, live packet stream, protocol statistics, and audio feedback. |
| Storage / Export | Optional SD logging (`packet_logger`), KML/GeoJSON exports, diagnostics report, JSON APIs for scripting. |

## Build & Flash

```powershell
# from repo root
pio run --target upload
pio device monitor
```

LittleFS is still used purely for serving the HTML/JS bundle. If you prefer to embed assets in PROGMEM or host them elsewhere, remove the LittleFS mount in `main.cpp` and adjust `web_server.cpp` to serve strings via `send_P`.

## Phone Workflow (Enhanced Visualization UI)

1. Power the ESP32 and wait for `ESP32-LoRa-Sniffer` WiFi AP.
2. Connect a phone/laptop, browse to `http://192.168.4.1`.
3. Use the interactive web UI with **8 tabs**:
   - **Status:** System overview, uptime, packet counts, quick actions
   - **Live Stream:** Real-time packet feed with syntax highlighting and audio feedback (Geiger counter effect)
   - **Stats:** Protocol breakdown with pie/bar charts (Meshtastic/LoRaWAN/Helium)
   - **Network:** Interactive topology map with RSSI-based positioning and signal strength visualization
   - **Devices:** Discovered devices with targeting controls
   - **Packets:** Replay menu for captured packets
   - **Frequency:** 26-config targeting menu
   - **GPS:** Geographic positions with export options
4. **Audio Feedback:** Different tones for different protocols (Meshtastic: high, LoRaWAN: medium, Helium: low)
5. WebSocket updates stream real-time data to all visualization components

All buttons call the same handlers used by serial commands, so parity is guaranteed (no duplicated business logic in JavaScript).

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
  BUILD_GUIDE.md, FEATURES.md, TROUBLESHOOTING_MESHTASTIC.md,
  ENCRYPTION_REALITY.md, deepdive references

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
