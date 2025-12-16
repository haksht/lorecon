# ESP32 LoRa Sniffer - Analysis Tools

Professional-grade visualization and analysis tools for ESP32 LoRa reconnaissance data.

## 🎯 Quick Start for Conference Demos

### Attack Dashboard (NEW!) 🔥

**Standalone HTML dashboard for live presentations:**
```bash
# Just open in your browser - no Python needed!
start attack_dashboard.html   # Windows
open attack_dashboard.html    # macOS
```

Features:
- ⚔️ **Attack Surface Stats** - Devices found, networks cracked, replay slots ready
- 🔓 **PSK Decryption Panel** - Live success rate, per-key hit counts
- 📡 **Replay Attack Slots** - One-click packet replay with decrypted text
- 📍 **Live GPS Map** - Real Leaflet/OpenStreetMap with movement trails
- ⚠️ **Anomaly Feed** - Real-time security alerts overlay

Enter your ESP32's IP (e.g., `172.20.10.3`) and click Connect.

---

**Serial-based 5-panel dashboard with audio:**
```bash
# Install dependencies
pip install -r requirements.txt

# One-command demo launcher (auto-detects ESP32)
python demo.py --auto-detect --audio --record

# Or specify port manually
python demo.py COM3 --audio --record --web

# List available ports
python demo.py --list-ports
```

### Demo Launcher Features

The `demo.py` script provides one-command setup:
- 🔍 Auto-detects ESP32 port
- 📊 Launches enhanced visualizer  
- 🌐 Opens web UI in browser (optional)
- 🔊 Enables audio feedback (optional)
- 📸 Records screenshots at milestones (optional)
- 🗺️ **Exports interactive HTML map on exit** (with GPS data)
- ⏱️ Auto-exits after duration for scripted demos

**Example: 5-minute recorded demo with all features**
```bash
python demo.py COM3 --web --audio --record --duration 300

# Outputs:
# - screenshots/milestone_*.png
# - screenshots/lora_map.html (interactive map)
```

---

## Enhanced Live Visualizer ⭐ NEW

**Conference demo edition** with 5-panel dashboard and audio feedback.

### Features

- **5-Panel Dashboard**:
  1. 📡 **Signal Strength** - RSSI over time per device
  2. 🎯 **Device List** - Top active devices with stats
  3. 📊 **Protocol Pie Chart** - Visual protocol distribution
  4. 📦 **Packet Histogram** - Capture rates by device
  5. 🗺️ **GPS Map** - Geographic positions and movement trails
  
- **Audio Feedback** - "Geiger counter" effect with protocol-specific tones:
  - Meshtastic: 800 Hz
  - LoRaWAN: 600 Hz
  - Helium: 500 Hz
  
- **Auto-Screenshot** - Saves images at milestones (10, 50, 100, 500 packets)

- **Interactive Map Export** ⭐ NEW
  - Automatically exports `lora_map.html` on exit (when GPS data available)
  - OpenStreetMap basemap with device markers
  - Color-coded by protocol (Meshtastic=Blue, LoRaWAN=Orange, Helium=Green)
  - Popup details: Node ID, protocol, packet count, RSSI, coordinates
  - Movement trails for devices that changed position
  - Auto-opens in default browser

- **Protocol Colors** - Matches web UI color scheme:
  - Meshtastic: Blue
  - LoRaWAN: Orange
  - Helium: Green

### Usage

```bash
# Full demo mode
python enhanced_live_visualizer.py COM3 --audio --record

# JSON mode (if firmware supports structured output)
python enhanced_live_visualizer.py COM3 --json --audio

# Quiet mode (no audio)
python enhanced_live_visualizer.py COM3 --record
```

**Outputs:**
- Live matplotlib dashboard (5 panels)
- `screenshots/milestone_*.png` - Auto-captured screenshots
- `screenshots/lora_map.html` - Interactive map (on exit, if GPS data)

**Interactive Map Features:**
- 🗺️ OpenStreetMap basemap
- 📍 Device markers with popups (node ID, stats, coordinates)
- 🎨 Color-coded by protocol
- 🛤️ Movement trails for devices
- 🌐 Auto-opens in browser

### Requirements

```bash
pip install pyserial matplotlib folium sounddevice numpy
```

**Optional:** 
- `sounddevice` and `numpy` for audio feedback (gracefully degrades if missing)
- `folium` for interactive map export (gracefully degrades if missing)

---

## REST API Client ⭐ NEW

Command-line client for interacting with ESP32's REST API - no browser needed.

### Features

- Full access to all 30+ API endpoints
- Status monitoring, device listing, capture control
- PCAP/GeoJSON/KML export downloads
- Replay slot management
- Send serial-style commands remotely

### Usage

```bash
# Check system status
python api_client.py status

# List discovered devices
python api_client.py devices

# Start targeting a device
python api_client.py capture 0x401ACD4E

# Download PCAP file
python api_client.py download-pcap -o capture.pcap

# Send command (r = resume recon)
python api_client.py command r

# Use custom host
python api_client.py --host 192.168.4.1 status
```

### Requirements

```bash
pip install requests
```

---

## WebSocket Monitor ⭐ NEW

Headless terminal-based monitor for real-time packet events. Perfect for SSH sessions.

### Features

- No GUI dependencies (matplotlib not required)
- Color-coded protocol output
- RSSI signal strength visualization
- Protocol filtering
- JSON output for piping to other tools

### Usage

```bash
# Basic monitoring
python ws_monitor.py

# Filter by protocol
python ws_monitor.py --filter meshtastic

# JSON output (for piping)
python ws_monitor.py --json | jq '.nodeId'

# Quiet mode (packets only, no status)
python ws_monitor.py --quiet

# No colors (for logging)
python ws_monitor.py --no-color > packets.log
```

### Requirements

```bash
pip install websocket-client
```

---

## PCAP Analyzer ⭐ NEW

Analyze PCAP files exported from ESP32 LoRa Sniffer.

### Features

- Parse custom LoRa pseudo-header (RSSI, SNR, frequency)
- Protocol identification (Meshtastic, LoRaWAN, Helium)
- Device statistics and signal quality analysis
- Export to CSV/JSON
- Open directly in Wireshark

### Usage

```bash
# Basic analysis
python pcap_analyzer.py capture.pcap

# Export to CSV
python pcap_analyzer.py capture.pcap -o packets.csv

# JSON output
python pcap_analyzer.py capture.pcap --json

# Open in Wireshark
python pcap_analyzer.py capture.pcap --wireshark

# Show raw hex dumps
python pcap_analyzer.py capture.pcap --raw
```

### Requirements

Native PCAP parsing (no dependencies). Optional `scapy` for advanced analysis.

---

## PC Analyzer (Offline Log Analysis)

Analyze captured packet logs from SD card (supports CSV and JSONL formats).

### Features

- Device mapping and tracking
- Signal strength analysis
- Protocol distribution
- Timeline visualization
- Supports both CSV and JSONL log formats

### Usage

```bash
# Analyze CSV file
python pc_analyzer.py logs/capture.csv

# Analyze JSONL file
python pc_analyzer.py logs/packets.jsonl

# Analyze directory (finds all logs)
python pc_analyzer.py logs/

# Export as JSON
python pc_analyzer.py capture.csv --json results.json
```

---

## Session Analyzer (SD Log Dashboard)

Offline dashboard for SD-card CSV captures produced by the firmware's packet logger.

### Highlights

- **Portable story** – replay any field capture without needing live RF traffic.
- **2×2 dashboard** – timeline, top devices, frequency usage, and GPS scatter in one view.
- **Analyzer-friendly CSV** – consumes the enhanced schema written by `PacketLogger`.
- **Interactive map export** ⭐ NEW – Generate HTML map with `--export-map`

### Usage

```bash
# Standard analysis
python session_analyzer.py logs/snf_123456.csv --bin-seconds 60 --top-n 8

# With interactive map export
python session_analyzer.py logs/snf_123456.csv --export-map

# Custom map filename
python session_analyzer.py logs/snf_123456.csv --export-map --map-output mymap.html
```

### Interactive Map Features

When using `--export-map`:
- 🗺️ OpenStreetMap basemap
- 📍 Device markers with detailed popups
- 🎨 Color-coded by protocol (Meshtastic/LoRaWAN/Helium)
- 🛤️ Movement trails showing device paths
- 📊 Legend with session stats
- 🌐 Auto-opens in default browser

Perfect for:
- **Conference presentations** - Embed interactive map in slides
- **Security reports** - Professional geographic visualization
- **Team collaboration** - Share HTML file (self-contained, no server needed)

### Workflow

1. Insert SD card after a field session and copy the generated `snf_*.csv` file.
2. Run `session_analyzer.py` with the CSV path (adjust `--bin-seconds` for different timelines).
3. Add `--export-map` to generate interactive HTML map.
4. Project the matplotlib window or save a screenshot for slides.
5. Open the generated HTML map in browser or embed in presentation.

> Tip: pair the analyzer with live captures for a two-part conference demo—live RF when available, SD replay when the spectrum is quiet.

