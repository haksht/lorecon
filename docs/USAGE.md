# Usage Guide

Using the sniffer after it's running: web interface, Python analysis tools, and field deployment.

If you haven't flashed the device yet, start with [SETUP.md](SETUP.md).

---

## Web interface

Browse to the device IP or `http://lora-xxyyzz.local` (replace `xxyyzz` with your device ID).

### 7 tabs

**Info** — System overview: uptime, free memory, GPS fix status, security assessment summary, quick-action buttons.

**Devices** — All discovered LoRa devices sorted by vulnerability score. Click any device to target it for focused capture.

**Packets** — Live Wireshark-style packet stream with protocol badges, RSSI indicators, and encryption status. Controls: pause, resume, clear, auto-scroll. Replay slots below.

**Frequency** — All 26 scan configurations with activity indicators. Click any config to lock the radio to that frequency and modulation.

**Network** — Interactive canvas map. Nodes placed by signal strength. Click a node to see details and targeting button. Updates every 2 seconds.

**Stats** — Protocol breakdown (Meshtastic / LoRaWAN / Helium) as pie chart (desktop) or bar chart (mobile). Live packet counts and percentages.

**Settings** — WiFi credentials, system diagnostics, reboot, power off. On T-Beam Supreme, power off cuts PMIC rails completely.

### Audio feedback

The web UI plays protocol-specific tones as packets arrive — a Geiger counter effect. Toggle it with the speaker button in the header or in Settings. Tones: Meshtastic 800 Hz, LoRaWAN 600 Hz, Helium 500 Hz.

---

## API access

Read-only endpoints (devices, status, packets) are public. The web UI handles authentication automatically for everything else — you won't be prompted unless accessing the API directly.

For direct API access and the full endpoint reference, see [developers/API.md](developers/API.md).

---

## Data export

### Without SD card

All data lives in RAM and is lost on reboot. Export before power cycling.

| Export | How |
|--------|-----|
| Device list | `GET /api/devices` or Devices tab |
| GPS positions (KML) | Info tab → Export KML |
| GPS positions (GeoJSON) | Info tab → Export GeoJSON |
| System status | `GET /api/status` |

### With SD card (T3-S3, T-Beam Supreme)

Packets log to `/logs/` automatically on boot. Data survives reboots.

| Export | How |
|--------|-----|
| CSV packet log | Webapp sidebar → Download CSV |
| PCAP (Wireshark) | Webapp sidebar → Download PCAP |
| Past sessions | `GET /api/files` to list, `/api/files/download?name=snf_12345.csv` to fetch |
| GPS positions | Same as above (KML / GeoJSON) |

SD cards up to 32 GB work. Use FAT32. The T3-S3 works best with ≤4 GB cards.

---

## Python analysis tools

The `tools/` directory has a Python toolkit for live monitoring and offline analysis.

### Install

```bash
pip install -r tools/requirements.txt
```

### Live tools (device must be running)

```bash
# 5-panel dashboard (packets, devices, signal, protocols)
python tools/enhanced_live_visualizer.py COM3            # Windows serial port
python tools/enhanced_live_visualizer.py /dev/ttyUSB0   # Linux
python tools/enhanced_live_visualizer.py --demo          # simulated, no hardware needed

# Headless WebSocket monitor
python tools/ws_monitor.py --host 192.168.4.1
python tools/ws_monitor.py --host 192.168.4.1 --json     # machine-readable

# REST API client
python tools/api_client.py --host 192.168.4.1 devices
python tools/api_client.py --host 192.168.4.1 status
python tools/api_client.py --host 192.168.4.1 replay-slots
```

### Offline tools (work from SD card files)

```bash
# Analyze a PCAP capture
python tools/pcap_analyzer.py capture.pcap
python tools/pcap_analyzer.py capture.pcap --export-csv output.csv

# Security assessment report
python tools/recon_report.py capture.csv
python tools/recon_report.py capture.csv --format html --output report.html
python tools/recon_report.py --api 192.168.4.1              # pull live data

# Meshtastic PSK analysis
python tools/meshtastic/psk_auditor.py capture.csv --verbose
python tools/meshtastic/mesh_topology_analyzer.py capture.csv --ascii
python tools/meshtastic/meshtastic_decoder.py capture.pcap

# Conference decrypt reveal page (browser-based walkthrough)
python tools/demo/make_reveal.py capture.pcap -o reveal.html
python tools/demo/make_reveal.py capture.pcap --all-ports -o reveal.html

# Wireshark LoRaTap conversion
python tools/export/wireshark_exporter.py capture.pcap -o wireshark_capture.pcap
```

### Tool reference

| Tool | Purpose | Mode |
|------|---------|------|
| `ws_monitor.py` | Headless WebSocket monitor | Live |
| `api_client.py` | REST API CLI | Live |
| `enhanced_live_visualizer.py` | 5-panel dashboard | Live + demo |
| `pcap_analyzer.py` | PCAP file analysis | Offline |
| `recon_report.py` | Security assessment report | Both |
| `demo/make_reveal.py` | Conference decrypt reveal page | Offline |
| `meshtastic/psk_auditor.py` | PSK vulnerability scanner | Both |
| `meshtastic/mesh_topology_analyzer.py` | Mesh topology | Both |
| `meshtastic/meshtastic_decoder.py` | Offline packet decryption | Offline |
| `export/wireshark_exporter.py` | LoRaTap format for Wireshark | Offline |

---

## Field deployment

### Recommended setup for unattended / long-duration use

- Use a wall adapter, not a laptop USB port
- Connect the antenna before powering on
- Set the mode you want (recon or targeted) before leaving the device
- Serial console auto-deactivates after 5 minutes — or just don't connect one
- Use an SD card if you need persistent logs

### Modes

**Reconnaissance mode** (default): scans all 26 configurations in a 5-minute cycle, discovers new devices automatically. Mode persists across reboots via NVS.

**Targeted mode**: lock to a specific device or frequency. Press `f` in serial or click a configuration in the Frequency tab. Also persists across reboots.

### Monitoring remotely

If the device is on your hotspot:
```bash
# Check device is alive
curl http://<device-ip>/api/status

# Device count
curl http://<device-ip>/api/devices | python -m json.tool

# Watch mode hasn't drifted
curl http://<device-ip>/api/status | python -c "import sys,json; d=json.load(sys.stdin); print(d.get('mode'))"
```

Or check the Devices tab on any browser connected to the same network.

### Stability features built in

| Feature | Behavior |
|---------|---------|
| Hardware watchdog | 30-second timeout, auto-resets on hang |
| WiFi reconnect | Auto-reconnects with 5-second throttle |
| AP fallback | `192.168.4.1` always accessible even if hotspot drops |
| Mode persistence | NVS stores current mode across reboots |
| Device buffer | Tracks 50 devices; oldest evicted when full |
| Packet queue | 100-packet queue; overflow logged in UI |

### Post-test

Check serial output on next connection for reset reason:
```
[INFO] Reset reason: Power-on (code 1)    <- normal
[WARN] Reset reason: Task watchdog        <- indicates hang during test
```

Stats (total packets, devices, dropped packets) are in the Info tab and `/api/status`.

---

## Deep dives

- [reference/HOW_IT_WORKS.md](reference/HOW_IT_WORKS.md) — security scoring algorithm, router detection, firmware estimation
- [reference/ENCRYPTION.md](reference/ENCRYPTION.md) — what the PSK decryption can and cannot decrypt
- [reference/NETWORK_HUNTING.md](reference/NETWORK_HUNTING.md) — hunting strategies by network type
- [developers/API.md](developers/API.md) — full REST and WebSocket reference
