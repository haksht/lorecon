# ESP32 LoRa Sniffer - Analysis Tools

## Data Access: SD Card vs Real-Time Connection

The ESP32 stores captured data **in RAM only** unless an SD card is installed.

### Without SD Card (Real-Time Connection Required)

| Connection | How to Use | Data Access |
|------------|------------|-------------|
| **Serial/USB** | `pio device monitor` | Live output, 'k'/'j' for KML/GeoJSON export |
| **WiFi (AP)** | Connect to `LoRa-XXYYZZ`, browse `192.168.4.1` | Web UI, API, WebSocket |
| **WiFi (Hotspot)** | Device joins your network | Same as AP, use assigned IP |

**Real-time tools:**
- `ws_monitor.py` - Headless WebSocket packet monitor
- `api_client.py` - REST API CLI (devices, status, replay slots)
- `enhanced_live_visualizer.py` - 5-panel live dashboard

**Data is lost on reboot.** Export via API before power cycling.

### With SD Card (Offline Analysis)

1. Run capture session (device writes PCAP/CSV to SD)
2. Remove SD card, insert into PC
3. Analyze offline — no device or WiFi needed

**Offline tools:**
- `pcap_analyzer.py` - Analyze `.pcap` capture files
- `recon_report.py` - Generate security assessment reports from CSV

Real-time tools still work with an SD card installed.

---

## Directory Structure

```
tools/
├── demo/
│   ├── make_reveal.py          # Generate decrypt reveal page from PCAP
│   └── decrypt_reveal.html     # Example reveal page
├── export/
│   └── wireshark_exporter.py   # LoRaTap format for Wireshark
├── meshtastic/
│   ├── meshtastic_decoder.py   # Offline packet decryption
│   ├── mesh_topology_analyzer.py
│   ├── psk_auditor.py          # PSK vulnerability scanning
│   └── psk_decrypt.py          # PSK decryption library (23 keys)
├── api_client.py
├── enhanced_live_visualizer.py
├── pcap_analyzer.py
├── recon_report.py
├── ws_monitor.py
├── demo_tracking.html
├── README.md
└── requirements.txt
```

---

## Quick Start

### Install dependencies

```bash
pip install -r requirements.txt
```

### Conference Demo: Decrypt Reveal Page

Generate a presentation page from a PCAP capture file:

```bash
python demo/make_reveal.py capture.pcap -o reveal.html
python demo/make_reveal.py capture.pcap --all-ports -o reveal.html  # include GPS
```

Opens in any browser, no network needed. SPACE/click steps through each packet: encrypted hex first, then decrypted plaintext full-screen. Arrow keys navigate back.

### Live 5-Panel Dashboard

```bash
python enhanced_live_visualizer.py 192.168.4.1
python enhanced_live_visualizer.py --demo  # simulated traffic, no hardware needed
```

### Headless WebSocket Monitor

```bash
python ws_monitor.py 192.168.4.1
python ws_monitor.py 192.168.4.1 --json  # machine-readable output
```

### REST API Client

```bash
python api_client.py 192.168.4.1 devices
python api_client.py 192.168.4.1 status
python api_client.py 192.168.4.1 replay
```

### Offline PCAP Analysis

```bash
python pcap_analyzer.py capture.pcap
python pcap_analyzer.py capture.pcap --json
python pcap_analyzer.py capture.pcap --export-csv output.csv
```

### Security Assessment Report

```bash
python recon_report.py capture.csv
python recon_report.py capture.csv --format html --output report.html
python recon_report.py --api 192.168.4.1  # pull live data
```

### Meshtastic PSK Analysis

```bash
# Audit PSK vulnerabilities
python meshtastic/psk_auditor.py capture.csv --verbose
python meshtastic/psk_auditor.py --demo

# Offline packet decryption
python meshtastic/meshtastic_decoder.py capture.pcap

# Mesh topology
python meshtastic/mesh_topology_analyzer.py capture.csv --ascii
python meshtastic/mesh_topology_analyzer.py --demo

# List all 23 known PSKs
python meshtastic/psk_decrypt.py --list-keys
```

### Wireshark Export

```bash
python export/wireshark_exporter.py capture.pcap -o wireshark_capture.pcap
```

---

## Tool Summary

| Tool | Purpose | Live/Offline |
|------|---------|--------------|
| `ws_monitor.py` | Headless WebSocket monitor | Live |
| `api_client.py` | REST API CLI client | Live |
| `enhanced_live_visualizer.py` | 5-panel dashboard | Both |
| `pcap_analyzer.py` | PCAP file analysis | Offline |
| `recon_report.py` | Security assessment reports | Both |
| `demo/make_reveal.py` | Conference decrypt reveal page | Offline |
| `meshtastic/psk_auditor.py` | PSK vulnerability scanner | Both |
| `meshtastic/mesh_topology_analyzer.py` | Mesh structure analysis | Both |
| `meshtastic/meshtastic_decoder.py` | Offline packet decryption | Offline |
| `meshtastic/psk_decrypt.py` | PSK decryption library | Offline |
| `export/wireshark_exporter.py` | LoRaTap conversion for Wireshark | Offline |
