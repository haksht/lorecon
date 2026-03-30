# Python Analysis Tools

Full reference for the `tools/` Python toolkit — when to use each tool, what it needs to run, and complete command options.

See [USAGE.md](USAGE.md) for install instructions and a quick-start command list.

---

## SD card vs live connection

The choice of tool depends on how you're accessing data:

| Scenario | Connection needed | Tools to use |
|----------|------------------|--------------|
| No SD card, device running | WiFi or USB | Live tools (`ws_monitor`, `api_client`, `enhanced_live_visualizer`) |
| SD card, device running | WiFi or USB | Any tool — live tools still work |
| SD card, device not with you | None (read card on PC) | Offline tools (`pcap_analyzer`, `recon_report`, Meshtastic tools) |

**No SD card means no persistence.** All data lives in device RAM and is gone on reboot. Export before power cycling.

---

## Directory structure

```
tools/
├── demo/
│   ├── make_reveal.py              # Conference decrypt reveal page (from PCAP)
│   └── decrypt_reveal.html         # Example output
├── export/
│   └── wireshark_exporter.py       # Convert to LoRaTap format for Wireshark
├── meshtastic/
│   ├── meshtastic_decoder.py       # Offline packet decryption
│   ├── mesh_topology_analyzer.py   # Visualize mesh network structure
│   ├── psk_auditor.py              # Scan for PSK vulnerabilities
│   └── psk_decrypt.py              # PSK decryption library (23 built-in keys)
├── api_client.py                   # REST API CLI
├── enhanced_live_visualizer.py     # 5-panel live dashboard
├── pcap_analyzer.py                # PCAP file analysis
├── recon_report.py                 # Security assessment report generator
├── ws_monitor.py                   # Headless WebSocket monitor
├── demo_tracking.html              # Standalone signal tracking page
└── requirements.txt
```

---

## Live tools

These require the device to be reachable over WiFi or USB.

### `enhanced_live_visualizer.py` — 5-panel dashboard

The most complete live view: packet stream, device list, signal strength history, protocol breakdown, and geo map panels.

```bash
# Connect via WebSocket (WiFi)
python tools/enhanced_live_visualizer.py --host 192.168.4.1

# Connect via serial (USB)
python tools/enhanced_live_visualizer.py COM3            # Windows
python tools/enhanced_live_visualizer.py /dev/ttyUSB0   # Linux

# Demo mode — simulated traffic, no hardware needed
python tools/enhanced_live_visualizer.py --demo
```

### `ws_monitor.py` — Headless WebSocket monitor

Low-overhead monitor for unattended logging or scripting.

```bash
python tools/ws_monitor.py --host 192.168.4.1
python tools/ws_monitor.py --host 192.168.4.1 --json     # machine-readable output
```

### `api_client.py` — REST API CLI

Query the device API directly from the command line.

```bash
python tools/api_client.py --host 192.168.4.1 devices
python tools/api_client.py --host 192.168.4.1 status
python tools/api_client.py --host 192.168.4.1 replay-slots
```

---

## Offline tools

These work from SD card files with no device or WiFi required.

### `pcap_analyzer.py` — PCAP analysis

Parse and summarize a `.pcap` capture file.

```bash
python tools/pcap_analyzer.py capture.pcap
python tools/pcap_analyzer.py capture.pcap --json
python tools/pcap_analyzer.py capture.pcap --export-csv output.csv
```

### `recon_report.py` — Security assessment report

Generates a formatted report from a CSV capture or live API data.

```bash
python tools/recon_report.py capture.csv
python tools/recon_report.py capture.csv --format html --output report.html
python tools/recon_report.py --api 192.168.4.1              # pull live data
```

### `export/wireshark_exporter.py` — Wireshark export

Converts the custom PCAP pseudo-header to standard LoRaTap (DLT 270) for Wireshark.

```bash
python tools/export/wireshark_exporter.py capture.pcap -o wireshark_capture.pcap
```

---

## Meshtastic tools

All three work from CSV or PCAP files. `psk_auditor` and `mesh_topology_analyzer` also support `--demo` mode with no input file.

### `meshtastic/psk_auditor.py` — PSK vulnerability scanner

Tests captured packets against all 23 known default PSKs. Reports which devices are using default or known-weak keys.

```bash
python tools/meshtastic/psk_auditor.py capture.csv
python tools/meshtastic/psk_auditor.py capture.csv --verbose
python tools/meshtastic/psk_auditor.py --demo
```

### `meshtastic/mesh_topology_analyzer.py` — Topology map

Reconstructs the mesh network graph from hop count data in the capture.

```bash
python tools/meshtastic/mesh_topology_analyzer.py capture.csv --ascii
python tools/meshtastic/mesh_topology_analyzer.py --demo
```

### `meshtastic/meshtastic_decoder.py` — Offline decryption

Attempts decryption of all Meshtastic packets in a PCAP using the built-in key database.

```bash
python tools/meshtastic/meshtastic_decoder.py capture.pcap
```

### `meshtastic/psk_decrypt.py` — PSK library

Underlying decryption library, also usable standalone.

```bash
# List all 23 known PSKs
python tools/meshtastic/psk_decrypt.py --list-keys
```

---

## Demo tool

### `demo/make_reveal.py` — Conference decrypt reveal page

Takes a PCAP and produces a self-contained HTML page that steps through each packet: encrypted hex first, then decrypted plaintext on click/keypress. No network required after generation.

```bash
python tools/demo/make_reveal.py capture.pcap -o reveal.html
python tools/demo/make_reveal.py capture.pcap --all-ports -o reveal.html
```

Open the output in any browser. Space bar or click advances to the next packet; arrow keys navigate back.
