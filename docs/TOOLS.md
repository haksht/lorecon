# Python Analysis Tools

Full reference for the `tools/` Python toolkit — when to use each tool, what it needs to run, and complete command options.

See [USAGE.md](USAGE.md) for install instructions and a quick-start command list.

---

## Unified entry point

`sniffer.py` is a single dispatcher for all tools. If you're not sure which script to run, start here.

```bash
python tools/sniffer.py help
```

```
Commands:
  monitor     Live WebSocket packet stream (headless)
  visualize   5-panel live dashboard with GPS map
  analyze     Analyze a PCAP file
  audit       PSK vulnerability audit on a CSV capture
  report      Generate security assessment report
  topology    Mesh network topology reconstruction
  decode      Batch Meshtastic decryption from PCAP
  api         REST API client (~30 subcommands)
  lorawan     LoRaWAN join request / DevEUI scanner
  uplink      LoRaWAN uplink frame parser + decryptor
  assess      End-to-end capture + audit + report pipeline
  demo        Launch visualizer in demo mode (no hardware needed)
```

All extra arguments pass through to the underlying tool:
```bash
python tools/sniffer.py monitor --host 192.168.4.1 --decrypt
python tools/sniffer.py analyze capture.pcap --wireshark
python tools/sniffer.py report capture.csv --format html -o report.html
```

---

## SD card vs live connection

| Scenario | Connection needed | Tools to use |
|----------|------------------|--------------|
| No SD card, device running | WiFi or USB | Live tools (`monitor`, `visualize`, `api`) |
| SD card, device running | WiFi or USB | Any tool — live tools still work |
| SD card, device not with you | None (read card on PC) | Offline tools (`analyze`, `audit`, `report`, LoRaWAN tools) |

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
├── lorawan/
│   ├── join_parser.py              # OTAA join request parser (DevEUI exposure, nonce reuse)
│   └── uplink_parser.py            # Uplink frame parser with optional AppSKey decryption
├── meshtastic/
│   ├── meshtastic_decoder.py       # Offline packet decryption
│   ├── mesh_topology_analyzer.py   # Visualize mesh network structure
│   ├── psk_auditor.py              # Scan for PSK vulnerabilities
│   └── psk_decrypt.py              # PSK decryption library (23 built-in keys)
├── api_client.py                   # REST API CLI
├── enhanced_live_visualizer.py     # 5-panel live dashboard
├── pcap_analyzer.py                # PCAP file analysis
├── recon_report.py                 # Security assessment report generator
├── run_assessment.py               # End-to-end capture + audit + report pipeline
├── sniffer.py                      # Unified CLI dispatcher
├── ws_monitor.py                   # Headless WebSocket monitor
├── demo_tracking.html              # Standalone signal tracking page
└── requirements.txt
```

Release build scripts are in `scripts/` (not `tools/`): `make_release.sh`, `make_release.bat`.

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

Optional dependencies (installed separately if needed):
```bash
pip install folium       # HTML map export
pip install sounddevice numpy  # audio feedback
```

### `ws_monitor.py` — Headless WebSocket monitor

Low-overhead monitor for unattended logging or scripting. Runs over WiFi only.

```bash
python tools/ws_monitor.py --host 192.168.4.1
python tools/ws_monitor.py --host 192.168.4.1 --json          # machine-readable output
python tools/ws_monitor.py --host 192.168.4.1 --filter meshtastic
python tools/ws_monitor.py --host 192.168.4.1 --filter meshcore
python tools/ws_monitor.py --host 192.168.4.1 --decrypt       # try all 23 PSKs live
python tools/ws_monitor.py --host 192.168.4.1 --messages      # decrypted text messages only
```

**`--decrypt`**: Attempts PSK decryption on every Meshtastic packet as it arrives, and channel decryption on MeshCore packets (public channel + common hashtag rooms). Prints `🔓 key#N name: <plaintext>` on success, `🔒 no default PSK matched` on failure. Requires `pip install cryptography`.

**`--messages`**: Implies `--decrypt`. Suppresses all packet headers, status updates, and non-text traffic — only prints a line when a Meshtastic packet decrypts successfully as a TEXT_MESSAGE. Useful for passive message interception without noise.

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

Parse and summarize a `.pcap` capture file. Extracts signal stats, protocol distribution, device list, and GPS coordinates from any position packets (including encrypted ones using default PSKs).

```bash
python tools/pcap_analyzer.py capture.pcap
python tools/pcap_analyzer.py capture.pcap --json
python tools/pcap_analyzer.py capture.pcap --export-csv output.csv
python tools/pcap_analyzer.py capture.pcap --wireshark   # convert to LoRaTap and open
```

**`--wireshark`**: Automatically converts the capture to standard LoRaTap format (DLT 270) and opens Wireshark. The converted file is saved alongside the original with `_loratap` appended to the name.

### `recon_report.py` — Security assessment report

Generates a formatted report from a CSV capture or live API data. Includes findings with CWE references, device inventory, GPS observations table, and an Assessment Methodology section that documents severity criteria and observation limitations.

```bash
python tools/recon_report.py capture.csv
python tools/recon_report.py capture.csv --format html --output report.html
python tools/recon_report.py --api 192.168.4.1              # pull live data
```

### `run_assessment.py` — End-to-end pipeline

Connects to the device, runs a timed capture, downloads CSV and PCAP, runs the PSK audit and LoRaWAN join scan, generates a report, and opens it in your browser — one command produces a complete deliverable.

```bash
python tools/run_assessment.py --host 192.168.4.1
python tools/run_assessment.py --host 192.168.4.1 --duration 30m --format html
python tools/run_assessment.py --demo    # full run on simulated data, no device needed
```

| Flag | Default | Description |
|------|---------|-------------|
| `--host` | `192.168.4.1` | Device IP |
| `--duration` | `10m` | Capture window (e.g. `5m`, `30m`, `1h`) |
| `--min-packets` | `50` | Stop early when this count is reached |
| `--format` | `html` | Report format: `html`, `markdown`, `json` |
| `--out` | `assessment_<timestamp>/` | Output directory |

### `export/wireshark_exporter.py` — Wireshark export

Converts the custom PCAP pseudo-header to standard LoRaTap (DLT 270) for Wireshark. Called automatically by `pcap_analyzer.py --wireshark`; use directly for scripting.

```bash
python tools/export/wireshark_exporter.py capture.pcap -o wireshark_capture.pcap
python tools/export/wireshark_exporter.py capture.pcap --open   # convert and launch
```

---

## Meshtastic tools

All tools work from CSV or PCAP files. `psk_auditor` and `mesh_topology_analyzer` also support `--demo` mode with no input file.

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
python tools/meshtastic/mesh_topology_analyzer.py capture.csv --format ascii
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

## LoRaWAN tools

These parse LoRaWAN frames from PCAP files. Both support `--demo` mode and `--json` output.

### `lorawan/join_parser.py` — OTAA join request scanner

Parses LoRaWAN Join Request frames (MType 0x00), which are transmitted in plaintext by the LoRaWAN specification. Every OTAA join attempt exposes the DevEUI and JoinEUI over the air.

```bash
python tools/lorawan/join_parser.py capture.pcap
python tools/lorawan/join_parser.py capture.pcap --json
python tools/lorawan/join_parser.py capture.pcap -o joins.csv
python tools/lorawan/join_parser.py --demo
```

Reports:
- DevEUI and JoinEUI per device
- Join attempt count and signal strength
- DevNonce reuse detection (indicates replay vulnerability, common in LoRaWAN 1.0.x devices)

### `lorawan/uplink_parser.py` — Uplink frame parser

Parses LoRaWAN uplink frames (Unconfirmed/Confirmed Data Up). Extracts DevAddr, FCnt, and FPort. Optionally decrypts FRMPayload if you have the AppSKey.

```bash
python tools/lorawan/uplink_parser.py capture.pcap
python tools/lorawan/uplink_parser.py capture.pcap --appskey <32-hex-chars>
python tools/lorawan/uplink_parser.py capture.pcap --json
python tools/lorawan/uplink_parser.py --demo
```

Reports:
- DevAddr, frame counter range, FPort usage per device
- FCnt gap detection (large gaps indicate missed traffic or packet loss)
- Decrypted payload (ASCII + hex) when AppSKey provided

---

## Demo tool

### `demo/make_reveal.py` — Conference decrypt reveal page

Takes a PCAP and produces a self-contained HTML page that steps through each packet: encrypted hex first, then decrypted plaintext on click/keypress. No network required after generation.

```bash
python tools/demo/make_reveal.py capture.pcap -o reveal.html
python tools/demo/make_reveal.py capture.pcap --all-ports -o reveal.html
```

Open the output in any browser. Space bar or click advances to the next packet; arrow keys navigate back.
