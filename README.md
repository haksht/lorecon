# ESP32 LoRa Packet Sniffer & Reconnaissance Tool

Passive LoRa reconnaissance firmware for ESP32-S3 + SX1262 hardware. The ESP32 handles radio control, packet capture, replay slots, PSK testing, GPS extraction, PCAP export, and WiFi access-point hosting so that a phone—or a serial console—can drive the entire workflow in the field.

## Quick Links

- `GETTING_STARTED.md` – complete setup guide with serial, OLED, and web interfaces.
- `docs/user-guides/BUILD_GUIDE.md` – supported hardware + PlatformIO notes.
- `docs/user-guides/FEATURES.md` – feature inventory and configuration knobs.
- `docs/user-guides/HOW_IT_WORKS.md` – how security scoring, router detection, GPS, and packet limits work.
- `docs/user-guides/TROUBLESHOOTING.md` & `docs/technical/ENCRYPTION.md` – protocol gotchas.
- `API_REFERENCE.md` – HTTP + WebSocket surface area for custom tooling.
- `tools/README.md` – Python analysis toolkit (session analyzer, PSK auditor, position tracker, and more).

## Current Status

- **Branch:** `main`
- **Version:** 2.3.2
- **Hardware:** ESP32-S3 + SX1262 + OLED. Native or optional SD card.
  - ✅ **Fully Supported:**
    - **Heltec WiFi LoRa 32 V3 / V4** (tested, production-ready; SD requires external module; V4 with L76K GPS module uses separate `heltec_v4` build env)
    - **LilyGO T3-S3 V1.2/V1.3** (ESP32-S3 + SX1262 + OLED + native SD card slot)
    - **LilyGO T-Beam Supreme** (ESP32-S3 + SX1262 + SH1106 OLED + SD + GPS + AXP2101 PMIC)
  - ⚠️ **Not Supported:** T-Deck variants (hardware incompatibilities - see `docs/hardware/TDECK_PLUS_INVESTIGATION.md`)
  - ℹ️ **Note:** All three boards use ESP32-S3 + SX1262 architecture, differing in pin mappings and peripherals
- **Focus:** Passive reconnaissance (scan, target, capture, replay, export).
- **Web UI:** Lightweight interface served from LittleFS with interactive network visualization, threat assessment, and real-time packet analysis.

## Feature Snapshot

| Area | Highlights |
| --- | --- |
| Radio Control | `RadioController` wraps RadioLib with atomic ISR flagging and cached RSSI/SNR reads. The recon state cycles through 26 LoRa configs including Meshtastic, LoRaWAN/TTN, Helium Network, and ISM band (5 min cycle). |
| Packet Processing | `PacketProcessor` queues interrupt captures, runs protocol analysis, PSK decryption (23 Meshtastic keys), LoRaWAN key testing (16 default AppKeys), diagnostics, and replay capture with dual logging (CSV + PCAP). |
| Recon State | `ReconState` tracks RF activity, targetable nodes, replay slots, quiet mode, and device intelligence with multi-factor security scoring. |
| UI Surfaces | 1) Serial menu (command handler with dispatch table). 2) OLED quick-look cards. 3) **WiFi Web UI** with threat-colored network map, signal heatmaps, vulnerability assessment, and live packet visualization. |
| Storage / Export | Optional SD logging (`packet_logger` with CSV + PCAP), wireless download buttons for CSV/PCAP/KML/GeoJSON, file browser API for past sessions, Wireshark-compatible PCAP with LoRa metadata, security assessment reports, JSON APIs for scripting. |
| Security | Token-based API authentication for sensitive endpoints, device-unique AP passwords, NVS credential storage, XSS prevention, input validation with bounds checking. |

## Why This Over Alternatives?

| Tool | Cost | LoRa Decode | PSK Decrypt | Standalone | Web UI |
|------|------|-------------|-------------|------------|--------|
| **This project** | **$30** | ✅ Full | ✅ 23 keys | ✅ | ✅ |
| Flipper Zero + LoRa | $190-210 | Basic | ❌ | ✅ | ❌ |
| HackRF + GNU Radio | $300+ | ✅ | Manual | ❌ (laptop) | ❌ |

**vs. Flipper**: Flipper is a multi-tool; this is a specialist. Lower cost, deeper Meshtastic protocol analysis, tests 23 default PSKs, mobile-friendly web UI.

**vs. SDR**: SDR requires a laptop and GNU Radio expertise. This is pocket-sized, flash-and-go, with built-in replay and phone-accessible results.

## Bill of Materials (~$30-50 USD)

### Option 1: Heltec WiFi LoRa 32 V3 / V4 (Proven)

| Component | Price | Notes | Link |
|-----------|-------|-------|------|
| **Heltec WiFi LoRa 32 V3 or V4** | $22-32 | ESP32-S3 + SX1262 + OLED; use `heltec_v3` env (or `heltec_v4` if V4 has L76K GPS) | [Amazon (V3)](https://www.amazon.com/dp/B0B697NLJ5) / [Heltec Store](https://heltec.org/project/wifi-lora-32-v3/) |
| 915MHz Antenna (RP-SMA) | $0-8 | Often included with board | [Amazon](https://www.amazon.com/dp/B09K3WMCVN) |
| USB-C Cable | $0 | You have one | — |
| **Optional:** 3D Printed Case | $5-10 | Search "Heltec V3 case" | [Printables](https://www.printables.com/search/models?q=heltec%20v3) |
| **Optional:** 18650 Battery | $8-12 | For portable operation | [Amazon](https://www.amazon.com/dp/B0BG8XJMQX) |
| **Optional:** SD Card Module | $5-8 | Requires hardware mod | Any microSD |

**Total**: ~$30 basic / ~$50 with case + battery

### Option 2: LilyGO T3-S3 V1.2/V1.3 (Native SD Card)

| Component | Price | Notes | Link |
|-----------|-------|-------|------|
| **LilyGO T3-S3 V1.2/V1.3** | $23-30 | ESP32-S3FH4R2 (4MB flash, 2MB PSRAM) + SX1262 + OLED + **SD slot** | [Amazon](https://www.amazon.com/dp/B0BW5W9QXZ) / [Official](https://lilygo.cc/products/t3s3-v1-0) |
| 915MHz Antenna (U.FL) | $0-8 | Usually included | [Amazon](https://www.amazon.com/dp/B09K3WMCVN) |
| MicroSD Card (≤4GB) | $5-8 | **Native slot - no mod needed** | FAT32 formatted |
| USB-C Cable | $0 | You have one | — |
| **Optional:** 18650 Battery | $8-12 | For portable operation | [Amazon](https://www.amazon.com/dp/B0BG8XJMQX) |

**Total**: ~$30 basic / ~$45 with battery + SD

### Option 3: LilyGO T-Beam Supreme (GPS + PMIC)

| Component | Price | Notes | Link |
|-----------|-------|-------|------|
| **LilyGO T-Beam Supreme** | $45-55 | ESP32-S3FN8 (8MB flash, 8MB PSRAM) + SX1262 + SH1106 OLED + SD + GPS + AXP2101 | [Official](https://lilygo.cc/products/t-beam-supreme) |
| 915MHz Antenna (U.FL) | $0-8 | Usually included | — |
| MicroSD Card (≤32GB) | $5-8 | Native slot | FAT32 formatted |
| USB-C Cable | $0 | You have one | — |
| **Optional:** 18650 Battery | $8-12 | Recommended for GPS use | [Amazon](https://www.amazon.com/dp/B0BG8XJMQX) |

**Total**: ~$50 basic / ~$70 with battery + SD

**Recommendation**: Choose **T-Beam Supreme** if you want onboard GPS for location-stamped packet capture. Choose **T3-S3** for native SD without GPS. Choose **Heltec V3 or V4** for proven stability and lowest cost.

> See [docs/hardware/T3_S3_GUIDE.md](docs/hardware/T3_S3_GUIDE.md) for T3-S3 setup details.

**Time to first packet**: Under 5 minutes (flash → power → scanning)

## Quick Start (Pre-compiled Binary)

**Don't want to compile?** Download the pre-built firmware:

1. Go to [Releases](https://github.com/tiarno/esp32-sniffer/releases/latest)
2. Download the firmware zip for your board
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

1. Power the ESP32 and wait for the `LoRa-XXYYZZ` WiFi AP (where `XXYYZZ` is the last 3 bytes of the device MAC address).
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
5. **Wireless Data Download (Quick Actions sidebar):**
   - **Download CSV** — current session packet log from SD card (`lora_capture_*.csv`)
   - **Download PCAP** — Wireshark-compatible capture from SD card; preserves RSSI, SNR, frequency, timestamp metadata
   - **Export KML** — GPS positions for Google Earth (`lora-positions-*.kml`)
   - **Export GeoJSON** — GPS positions for web mapping tools (`lora-positions-*.geojson`)
   - All downloads use blob streaming with try/catch error handling and user toast feedback
   - Past session files accessible via `/api/files` + `/api/files/download` REST endpoints

WebSocket streams real-time updates to all connected clients with automatic reconnection.

## Serial Command Reference (still available over USB)

**Note:** Serial commands require activation. Press **Enter twice** within 1.5 seconds to activate, then commands work normally.

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
| `l` | Clear captured packets |
| `n` | Clear discovered devices |
| `t` | Show API token |
| `w` | LoRaWAN key testing stats |
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
  hardware/           -> T3_S3_GUIDE.md, TDECK_PLUS_INVESTIGATION.md

tools/
  demo/make_reveal.py           -> generate decrypt reveal page from PCAP
  meshtastic/meshtastic_decoder.py -> offline Meshtastic packet decryption
  meshtastic/psk_auditor.py     -> PSK vulnerability scanning
  pcap_analyzer.py              -> PCAP capture file analysis
  enhanced_live_visualizer.py   -> 5-panel matplotlib dashboard
  recon_report.py, position_tracker.py, api_client.py, ws_monitor.py
  visualization/                -> network_topology.html, live_map.html, network_dashboard.html
```

## ⚠️ Legal & Ethical Use Disclaimer

**This tool is intended for authorized security research, education, and testing on networks you own or have explicit permission to analyze.**

### ✅ Legitimate Uses
- Security auditing of your own Meshtastic/LoRa networks
- Research into LoRa protocol security with proper authorization
- Educational demonstrations at conferences and workshops
- Testing mesh network resilience (with network owner consent)
- Identifying devices using vulnerable default configurations

### ⚠️ Gray Area (Jurisdiction-Dependent)
- Passive monitoring of RF signals in public spaces
- Testing default PSKs against unidentified networks (research context)
- Signal strength mapping without content inspection

### ❌ Prohibited Uses
- Intercepting private communications without consent
- Tracking individuals without authorization
- Disrupting or jamming networks (even briefly)
- Commercial exploitation of captured data
- Using packet replay to impersonate devices maliciously

### Legal Context
- **United States**: Passive RF reception is generally legal; interception of content may violate 18 U.S.C. § 2511 (Wiretap Act)
- **European Union**: GDPR applies if tracking individuals; national laws vary
- **Other jurisdictions**: Laws vary significantly—research local regulations

**The authors provide this tool for educational purposes and assume no liability for misuse. Default PSKs are tested to demonstrate the risks of using factory-default encryption keys.**

When in doubt, treat LoRa traffic like WiFi: observe protocol behavior for research, but don't intercept content or disrupt service.

## Contributing

1. Keep modules focused; avoid reintroducing large one-off docs into the root.
2. Prefer short, well-commented C++ patches with unit-test hooks where possible.
3. Keep the web UI lightweight (plain HTML + minimal JS). If functionality expands, consider a build step that still outputs vanilla assets under `data/webapp`.

Issues and PRs are welcome—please include hardware details, firmware commit, and repro steps when filing bugs.
