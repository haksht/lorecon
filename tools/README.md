# ESP32 LoRa Sniffer - Analysis Tools

Professional-grade visualization and analysis tools for ESP32 LoRa reconnaissance data.

## 📁 Directory Structure

```
tools/
├── compliance/                    # Regulatory compliance analysis
│   └── duty_cycle_monitor.py     # Duty cycle violation detection
├── export/                        # Format conversion tools
│   └── wireshark_exporter.py     # LoRaTap format for Wireshark
├── lorawan/                       # LoRaWAN-specific analysis
│   ├── abp_detector.py           # ABP vs OTAA device detection
│   └── lorawan_join_analyzer.py  # Join sequence security analysis
├── meshtastic/                    # Meshtastic-specific analysis
│   ├── meshtastic_decoder.py     # Offline packet decryption
│   ├── mesh_topology_analyzer.py # Network structure mapping
│   ├── psk_auditor.py            # PSK vulnerability scanning
│   └── psk_decrypt.py            # PSK decryption library (23 keys)
├── visualization/                 # Dashboards and visualization
│   ├── attack_dashboard.html     # Live attack surface dashboard
│   ├── live_map.html             # Real-time GPS mapping
│   ├── network_topology.html     # D3.js force-directed graph
│   ├── MAP_GUIDE.md              # Map customization guide
│   └── VISUALIZATION_GUIDE.md    # Visualization tips
├── api_client.py                  # REST API CLI client
├── enhanced_live_visualizer.py    # 5-panel conference demo
├── packet_differ.py               # Side-by-side packet diff
├── pcap_analyzer.py               # PCAP file analysis
├── position_tracker.py            # GPS tracking & mapping
├── recon_report.py                # Security assessment reports
├── session_analyzer.py            # SD card log analysis
├── timeline_replay.py             # DVR-style session replay
├── ws_monitor.py                  # Headless WebSocket monitor
├── README.md                      # This file
└── requirements.txt               # Python dependencies
```

---

## 🎯 Quick Start for Conference Demos

### Attack Dashboard (Standalone HTML) 🔥

**Zero-dependency dashboard for live presentations:**
```bash
# Just open in your browser - no Python needed!
start visualization/attack_dashboard.html   # Windows
open visualization/attack_dashboard.html    # macOS
```

Features:
- ⚔️ **Attack Surface Stats** - Devices found, networks cracked, replay slots ready
- 🔓 **PSK Decryption Panel** - Live success rate, per-key hit counts
- 📡 **Replay Attack Slots** - One-click packet replay with decrypted text
- 📍 **Live GPS Map** - Real Leaflet/OpenStreetMap with movement trails
- ⚠️ **Anomaly Feed** - Real-time security alerts overlay

Enter your ESP32's IP (e.g., `172.20.10.3`) and click Connect.

---

### 🎮 Demo Mode Quick Reference (No Hardware Needed!)

**Perfect for conference presentations without live ESP32:**

```bash
# 5-panel live dashboard with simulated traffic
python enhanced_live_visualizer.py --demo

# GPS tracking map with simulated devices
python position_tracker.py --demo
python position_tracker.py --demo --my-location 36.13,-115.15  # Custom location (Vegas)

# Mesh network topology visualization
python meshtastic/mesh_topology_analyzer.py --demo
python meshtastic/mesh_topology_analyzer.py --demo --format graphviz -o mesh.dot

# PSK vulnerability audit with simulated networks
python meshtastic/psk_auditor.py --demo

# List all 23 known Meshtastic PSKs
python meshtastic/psk_decrypt.py --list-keys
```

---

### Python-Based Tools

```bash
# Install all dependencies
pip install -r requirements.txt

# Quick demo with auto-detect (opens everything)
python enhanced_live_visualizer.py --auto-detect --web --audio

# Offline PSK vulnerability audit
python meshtastic/psk_auditor.py capture.csv --verbose

# Generate security assessment report
python recon_report.py capture.csv --html report.html

# GPS tracking with animated trails
python position_tracker.py capture.csv --map tracking.html

# Network topology visualization (opens in browser)
start visualization/network_topology.html    # Windows
open visualization/network_topology.html     # macOS

# Mesh topology analysis
python meshtastic/mesh_topology_analyzer.py capture.csv --ascii

# LoRaWAN join sequence analysis  
python lorawan/lorawan_join_analyzer.py capture.csv

# Side-by-side packet diff
python packet_differ.py --hex "AABBCC" "AABBDD"

# Timeline replay (DVR mode) at 10x speed
python timeline_replay.py capture.csv --speed 10 --interactive
```

---

## 📊 Tool Overview

| Tool | Purpose | Live/Offline | Demo Mode | Output |
|------|---------|--------------|-----------|--------|
| `visualization/attack_dashboard.html` | Full attack surface dashboard | Live (WiFi) | N/A | Browser UI |
| `visualization/network_topology.html` | D3.js force-directed graph | Both | N/A | Browser UI, SVG |
| `timeline_replay.py` | DVR-style session replay | Offline | ❌ | Terminal UI |
| `enhanced_live_visualizer.py` | 5-panel real-time dashboard | Both | ✅ `--demo` | PNG, HTML map |
| `position_tracker.py` | GPS tracking & mapping | Both | ✅ `--demo` | HTML, PNG, GIF |
| `meshtastic/psk_auditor.py` | PSK vulnerability scanner | Both | ✅ `--demo` | Console, JSON |
| `meshtastic/mesh_topology_analyzer.py` | Mesh structure analysis | Both | ✅ `--demo` | ASCII, JSON, DOT |
| `meshtastic/psk_decrypt.py` | PSK decryption library | Offline | N/A | Console |
| `meshtastic/meshtastic_decoder.py` | Offline packet decryption | Offline | `--test` | Console, CSV |
| `lorawan/abp_detector.py` | ABP vs OTAA detection | Both | Console, JSON |
| `lorawan/lorawan_join_analyzer.py` | Join sequence analysis | Both | Console, JSON |
| `compliance/duty_cycle_monitor.py` | Regulatory compliance | Offline | Console, JSON |
| `export/wireshark_exporter.py` | LoRaTap conversion | Offline | PCAP |
| `packet_differ.py` | Side-by-side packet diff | Offline | Console |
| `recon_report.py` | Security assessment reports | Offline | Markdown, HTML, JSON |
| `position_tracker.py` | GPS tracking & mapping | Both | HTML, PNG, GIF |
| `ws_monitor.py` | Headless WebSocket monitor | Live (WiFi) | Console, JSON |
| `api_client.py` | REST API command-line client | Live (WiFi) | Console |
| `session_analyzer.py` | SD card log analysis | Offline | Dashboard, HTML map |
| `pcap_analyzer.py` | PCAP file analysis | Offline | CSV, JSON |

---

## 🕸️ Network Topology Visualizer ⭐ NEW

**D3.js force-directed graph** showing device relationships and network structure.

### Features

- **Interactive Graph** - Drag nodes, zoom, pan
- **Live Updates** - Connects via WebSocket for real-time node discovery
- **Risk Coloring** - Nodes colored by vulnerability assessment
- **CSV Import** - Load offline captures for analysis
- **SVG Export** - Save high-quality graphics for presentations

### Usage

```bash
# Just open in browser
start visualization/network_topology.html   # Windows
open visualization/network_topology.html    # macOS

# Connect to ESP32 IP and watch the network build!
```

---

## 🗺️ Mesh Topology Analyzer ⭐ NEW

**Parse traceroute and neighborinfo packets** to build mesh network structure.

### Features

- **Network Graph** - Nodes and edges with hop counts
- **Node Classification** - Gateway, relay, and edge node detection  
- **Mesh Metrics** - Density, average neighbors, path analysis
- **Multiple Output Formats** - ASCII, JSON, Graphviz DOT, NetworkX

### Usage

```bash
# Demo mode - no input needed
python meshtastic/mesh_topology_analyzer.py --demo

# ASCII visualization
python meshtastic/mesh_topology_analyzer.py capture.csv --ascii

# JSON export for web apps
python meshtastic/mesh_topology_analyzer.py capture.csv --format json -o topology.json

# Graphviz DOT for diagrams
python meshtastic/mesh_topology_analyzer.py capture.csv --format graphviz -o mesh.dot
dot -Tpng -o mesh.png mesh.dot

# Live monitoring
python meshtastic/mesh_topology_analyzer.py --live 192.168.4.1 --watch
```

**Example ASCII Output:**
```
MESH TOPOLOGY ANALYSIS
============================================================
Total Nodes: 12
Total Links: 18
Mesh Density: 27.27%
Avg Neighbors: 3.0

NODE CLASSIFICATION:
  Gateway Nodes:  2
  Relay Nodes:    7
  Edge Nodes:     3

TRACEROUTE PATHS:
  !abc123 → !def456 → !789abc (2 hops)
```

---

## 🔐 LoRaWAN Join Analyzer ⭐ NEW

**Analyze LoRaWAN join sequences** for security vulnerabilities.

### Security Issues Detected

| Issue | Severity | Description |
|-------|----------|-------------|
| DevNonce Reuse | 🔴 Critical | Enables replay attacks |
| Sequential Nonces | 🟠 High | Predictable values |
| Join Storms | 🟡 Medium | Possible attack or misconfiguration |
| Counter Resets | 🟡 Medium | Device instability |

### Usage

```bash
# Analyze capture file
python lorawan/lorawan_join_analyzer.py capture.csv

# Verbose output showing each packet
python lorawan/lorawan_join_analyzer.py capture.pcap --verbose

# JSON export
python lorawan/lorawan_join_analyzer.py capture.csv --json -o report.json

# Live monitoring
python lorawan/lorawan_join_analyzer.py --live 192.168.4.1
```

**Example Output:**
```
LORAWAN JOIN SEQUENCE ANALYSIS
======================================================================
SUMMARY:
  Join Requests:             47
  Join Accepts:              42
  Join Success Rate:         89.4%

SECURITY ASSESSMENT:
  🔴 HIGH RISK Devices:      1
  🟢 LOW RISK Devices:       5

  ⚠️  CRITICAL: 3 DevNonce reuse event(s) detected!
```

---

## 🔍 Packet Differ ⭐ NEW

**Side-by-side hex diff** for reverse engineering and protocol analysis.

### Features

- **Byte-level Comparison** - Highlights exact differences
- **Color Coding** - Red for packet A, green for packet B
- **Multiple Input Formats** - Files, hex strings, stdin, CSV
- **ASCII View** - Parallel hex and ASCII display

### Usage

```bash
# Compare two hex strings
python packet_differ.py --hex "AABBCC" "AABBDD"

# Compare binary files  
python packet_differ.py packet1.bin packet2.bin

# Compare packets from CSV by index
python packet_differ.py capture.csv --packets 0,5

# Pipe from stdin
echo "AABBCC" | python packet_differ.py --stdin --hex "AABBDD"

# Compact inline format
python packet_differ.py --hex "AABBCC" "AABBDD" --compact
```

**Example Output:**
```
======================================================================
PACKET DIFF ANALYSIS
======================================================================
Packet A: 3 bytes
Packet B: 3 bytes

0000  A: AA BB CC  |...| 
      B: AA BB DD  |...|

⚠ 1 byte(s) differ (66.7% match)
Diff positions: 0x02
======================================================================
```

---

## 🎬 Timeline Replay (DVR Mode) ⭐ NEW

**Replay captured sessions** at configurable speeds for demos without live hardware.

### Features

- **Speed Control** - 1x, 2x, 5x, 10x, 50x playback
- **Rich Terminal UI** - Beautiful progress bars, tables, live stats
- **Auto-Pause** - Stops on significant events (PSK cracks, GPS)
- **WebSocket Broadcast** - Sync with visualizers for coordinated demos
- **Event Markers** - Highlights interesting moments

### Usage

```bash
# Simple playback at 10x speed
python timeline_replay.py capture.csv --speed 10

# Interactive mode with rich UI
python timeline_replay.py capture.csv -i --speed 5

# Broadcast to network topology visualizer
python timeline_replay.py capture.csv --broadcast 192.168.4.1 --speed 10
```

---

## Enhanced Live Visualizer ⭐

**Conference demo edition** with 5-panel dashboard, audio feedback, and map export.

### Features

- **5-Panel Dashboard**:
  1. 📡 **Signal Strength** - RSSI over time per device
  2. 🎯 **Device List** - Top active devices with stats
  3. 📊 **Protocol Pie Chart** - Visual protocol distribution
  4. 📦 **Packet Histogram** - Capture rates by device
  5. 🗺️ **GPS Map** - Geographic positions and movement trails
  
- **Audio Feedback** - "Geiger counter" effect with protocol-specific tones
- **Auto-Screenshot** - Saves images at milestones (10, 50, 100, 500 packets)
- **Interactive Map Export** - Auto-generates HTML map on exit
- **Auto-Detection** - Finds ESP32 serial port automatically
- **Web UI Launch** - Opens browser to ESP32's web interface

### Usage

```bash
# Demo mode - no hardware needed!
python enhanced_live_visualizer.py --demo

# Full demo mode with all features
python enhanced_live_visualizer.py COM3 --audio --record --web

# Auto-detect ESP32 port
python enhanced_live_visualizer.py --auto-detect --audio

# List available serial ports
python enhanced_live_visualizer.py --list-ports

# Timed demo (auto-exit after 5 minutes)
python enhanced_live_visualizer.py COM3 --duration 300 --record

# Custom web UI host
python enhanced_live_visualizer.py COM3 --web --web-ip 192.168.4.1
```

**Outputs:**
- Live matplotlib dashboard (5 panels)
- `screenshots/milestone_*.png` - Auto-captured screenshots
- `screenshots/lora_map.html` - Interactive map (on exit, if GPS data)

---

## 🔓 PSK Auditor ⭐

**Vulnerability scanner with risk classification** - Tests captured packets against 23 known Meshtastic PSKs.

### Risk Levels

| Level | Description | Example Keys |
|-------|-------------|--------------|
| 🔴 CRITICAL | Leaked production keys | Admin key, Debug key (2023 leak) |
| 🟠 HIGH | Default channel keys | Default, LongFast, ShortFast |
| 🟡 MEDIUM | Regional defaults | EU868, US915 factory keys |
| 🟢 LOW | Common test keys | Test, Development keys |

### Usage

```bash
# Audit CSV capture file
python meshtastic/psk_auditor.py capture.csv

# Verbose mode with decrypted content
python meshtastic/psk_auditor.py capture.csv --verbose

# Live monitoring via WebSocket
python meshtastic/psk_auditor.py --live 192.168.4.1

# Live monitoring via REST API
python meshtastic/psk_auditor.py --api 192.168.4.1

# Export results to JSON
python meshtastic/psk_auditor.py capture.csv --output audit_results.json
```

**Example Output:**
```
🔴 CRITICAL: Leaked Admin Key
   Device: !401acd4e
   Decrypted: "GPS coordinates..."
   Risk: Administrative access compromised
```

---

## � PSK Decryption Library ⭐ NEW

**Standalone PSK decryption module** - Tests Meshtastic packets against 23 known keys.

### Features

- **23 Known Keys** - Includes 2023 leaked admin/debug keys
- **AES-CTR Decryption** - Proper nonce construction from packet metadata
- **Key Expansion** - Handles 1-byte, 8-byte, and 16-byte keys
- **Validation** - UTF-8 and protobuf structure validation
- **Library + CLI** - Use as import or standalone tool

### Usage

```bash
# List all known PSKs with risk levels
python meshtastic/psk_decrypt.py --list-keys

# Decrypt a hex packet (requires packet_id and source node)
python meshtastic/psk_decrypt.py --hex "encrypted_payload_hex"

# Use in Python code
from psk_decrypt import MeshtasticDecryptor
decryptor = MeshtasticDecryptor()
result = decryptor.try_decrypt(payload, packet_id, source_node_id)
if result:
    key_name, plaintext = result
```

**Known Keys (23 total):**
```
🔴 CRITICAL: admin_key, debug_key_2023, leaked_master
🟠 HIGH:     default, longfast, shortfast, mediumfast
🟡 MEDIUM:   eu868_default, us915_default, cn470_default
🟢 LOW:      test, development, demo
```

---

## �📋 Security Report Generator ⭐ NEW

**Generates professional security assessment reports** from capture data.

### Report Contents

- Executive Summary with threat level assessment
- Vulnerability analysis (default PSKs, leaked keys, GPS exposure)
- Device inventory with security scores
- Protocol distribution statistics
- Recommendations for network hardening

### Usage

```bash
# Generate Markdown report
python recon_report.py capture.csv

# Generate HTML report (styled)
python recon_report.py capture.csv --html report.html

# Generate JSON report (for automation)
python recon_report.py capture.csv --json report.json

# Load from live ESP32 API
python recon_report.py --api 192.168.4.1 --html live_report.html

# Custom report title
python recon_report.py capture.csv --title "DEF CON Site Survey"
```

**Report includes:**
- 🎯 Overall security score (0-100)
- 🔴 Critical findings highlighted
- 📊 Device breakdown by vulnerability type
- 📍 GPS exposure analysis
- 📝 Remediation recommendations

---

## 📍 Position Tracker ⭐ NEW

**Dedicated GPS tracking and visualization** with animated timelines and movement trails.

### Features

- **Interactive Maps** - Folium/Leaflet with OpenStreetMap
- **Animated Trails** - AntPath showing device movement
- **Timeline Animation** - GIF export showing position changes
- **Live Monitoring** - Real-time map updates from ESP32
- **Multi-format Export** - HTML, PNG, GIF outputs

### Usage

```bash
# Demo mode - simulated GPS tracks (no input needed)
python position_tracker.py --demo
python position_tracker.py --demo --my-location 36.13,-115.15  # Center on Vegas

# Generate interactive HTML map from CSV
python position_tracker.py capture.csv --map tracking.html

# Generate static PNG map
python position_tracker.py capture.csv --static tracking.png

# Generate animated GIF
python position_tracker.py capture.csv --animate tracking.gif --fps 2

# Live monitoring (updates map every 5 seconds)
python position_tracker.py --live 192.168.4.1 --map live_map.html

# Custom update interval
python position_tracker.py --live 192.168.4.1 --update-interval 10
```

**Interactive Map Features:**
- 🟢 Green markers = Start position
- 🔴 Red markers = Last known position
- 🔵 Blue markers = Intermediate positions
- 🐜 Animated "ant trail" showing movement direction

---

## 🔐 Meshtastic Decoder

**Offline packet decryption and protocol analysis** - Process captures without live ESP32.

### Features

- Tests 23 known Meshtastic PSKs (including 2023 leaked keys)
- Extracts text messages and GPS positions
- Parses Meshtastic protobuf headers
- Processes PCAP and CSV capture files

### Usage

```bash
# Decode CSV capture
python meshtastic/meshtastic_decoder.py capture.csv

# Decode PCAP file
python meshtastic/meshtastic_decoder.py capture.pcap

# Verbose output with hex dumps
python meshtastic/meshtastic_decoder.py capture.csv --verbose

# Run self-test with known data
python meshtastic/meshtastic_decoder.py --test

# Only show messages containing keyword
python meshtastic/meshtastic_decoder.py capture.csv | grep -i "password"
```

**Decrypted Output:**
```
[!401acd4e] TEXT: "Meeting at 3pm"
[!401acd4e] GPS: 37.7749, -122.4194
```

---

## 🌐 WebSocket Monitor

**Headless terminal monitor** - Perfect for SSH sessions and logging.

### Usage

```bash
# Basic monitoring
python ws_monitor.py

# Custom host
python ws_monitor.py --host 192.168.4.1

# Filter by protocol
python ws_monitor.py --filter meshtastic

# JSON output (for piping)
python ws_monitor.py --json | jq '.nodeId'

# Quiet mode (packets only)
python ws_monitor.py --quiet

# No colors (for log files)
python ws_monitor.py --no-color > packets.log
```

---

## 🔧 REST API Client

**Command-line access to all ESP32 API endpoints** - No browser needed.

### Usage

```bash
# System status
python api_client.py status

# List discovered devices
python api_client.py devices

# Target specific device
python api_client.py capture 0x401ACD4E

# Download PCAP
python api_client.py download-pcap -o capture.pcap

# Send command
python api_client.py command r   # Resume recon

# Custom host
python api_client.py --host 192.168.4.1 status
```

---

## 📈 Session Analyzer

**Offline dashboard for SD card captures** - Replay field sessions.

### Usage

```bash
# Standard analysis
python session_analyzer.py logs/snf_123456.csv --bin-seconds 60 --top-n 8

# With map export
python session_analyzer.py logs/snf_123456.csv --export-map

# Custom bin size for timeline
python session_analyzer.py capture.csv --bin-seconds 30
```

**Dashboard Panels:**
1. Timeline - Packet capture rate over time
2. Top Devices - Most active nodes
3. Frequency Usage - Channel distribution
4. GPS Scatter - Geographic positions

---

## 📦 PCAP Analyzer

**Parse and analyze PCAP files** with custom LoRa pseudo-header.

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

# Show raw hex
python pcap_analyzer.py capture.pcap --raw
```

---

## 📋 Requirements

### Core Dependencies
```bash
pip install pyserial matplotlib requests websocket-client
```

### Full Installation (all features)
```bash
pip install -r requirements.txt
```

### Optional Dependencies

| Package | Used By | Feature |
|---------|---------|---------|
| `folium` | position_tracker, visualizer | Interactive HTML maps |
| `sounddevice` | visualizer | Audio feedback |
| `cryptography` | psk_auditor, meshtastic_decoder | AES decryption |
| `numpy` | visualizer | Audio generation |
| `markdown` | recon_report | HTML report styling |

All tools gracefully degrade if optional dependencies are missing.

---

## 🎪 Conference Demo Workflow

### Preparation (Before Talk)

1. **Test hardware**: Verify ESP32 flashed and capturing
2. **Pre-capture data**: Run overnight to build device database
3. **Generate baseline report**: `python recon_report.py overnight.csv --html baseline.html`

### Live Demo Sequence

```bash
# 1. Start visualizer with all features
python enhanced_live_visualizer.py --auto-detect --web --audio --record

# 2. In another terminal, monitor PSK vulnerabilities
python psk_auditor.py --live 192.168.4.1 --verbose

# 3. After demo, generate report
python recon_report.py capture.csv --html findings.html

# 4. Generate GPS tracking visualization
python position_tracker.py capture.csv --map tracking.html --animate tracking.gif
```

### Offline Demo (No Live RF)

```bash
# Use pre-captured data
python session_analyzer.py captured_data.csv --export-map
python psk_auditor.py captured_data.csv --verbose
python meshtastic_decoder.py captured_data.csv
```

---

## 📁 Output Files

| Tool | Output | Description |
|------|--------|-------------|
| `enhanced_live_visualizer.py` | `screenshots/milestone_*.png` | Auto-captured at packet milestones |
| `enhanced_live_visualizer.py` | `screenshots/lora_map.html` | Interactive map on exit |
| `position_tracker.py` | `*_tracking.html` | Interactive position map |
| `position_tracker.py` | `*.gif` | Animated position timeline |
| `recon_report.py` | `*_report.md/html/json` | Security assessment report |
| `psk_auditor.py` | `audit_results.json` | Vulnerability scan results |
| `session_analyzer.py` | `session_map.html` | Interactive session map |

---

## 🔗 Related Documentation

- [visualization/VISUALIZATION_GUIDE.md](visualization/VISUALIZATION_GUIDE.md) - Detailed visualization tips
- [visualization/MAP_GUIDE.md](visualization/MAP_GUIDE.md) - Interactive map customization
- [API_REFERENCE.md](../API_REFERENCE.md) - Complete REST API documentation
- [FEATURES.md](../docs/user-guides/FEATURES.md) - Firmware feature guide
