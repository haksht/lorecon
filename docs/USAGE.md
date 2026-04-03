# Usage Guide

Using the sniffer after it's running: web interface, Python analysis tools, and field deployment.

If you haven't flashed the device yet, start with [SETUP.md](SETUP.md).

---

## Web interface

Browse to the device IP or `http://lora-xxyyzz.local` (replace `xxyyzz` with your device ID).

### 6 tabs + stats sidebar

**Info** — System overview: uptime, free memory, GPS fix status, security assessment summary, quick-action buttons.

**Devices** — All discovered LoRa devices sorted by vulnerability score. Click any device to target it for focused capture.

**Packets** — Live Wireshark-style packet stream with protocol badges, RSSI indicators, and encryption status. Controls: pause, resume, clear, auto-scroll. Replay slots below.

**Frequencies** — All 26 scan configurations with activity indicators. Click any config to lock the radio to that frequency and modulation.

**Dashboard** — War-room stats (protocol breakdown, live packet counts), interactive canvas network map (nodes placed by signal strength), and GPS positions. Updates every 2 seconds.

**Settings** — WiFi credentials, system diagnostics, reboot, power off. On T-Beam Supreme, power off cuts PMIC rails completely.

**Stats sidebar** — Device count, packet count, current mode, and battery level, plus Download CSV/PCAP and Export KML/GeoJSON buttons. On desktop the sidebar is always visible alongside the tabs. On mobile it collapses to a hamburger (≡) button in the header; tap it to open an overlay panel.

### Audio feedback

The web UI plays protocol-specific tones as packets arrive — a Geiger counter effect. Toggle it with the speaker button in the header or in Settings. Tones: Meshtastic 800 Hz, LoRaWAN 600 Hz, Helium 500 Hz.

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

The `tools/` directory has a Python toolkit for live monitoring and offline analysis. See [TOOLS.md](TOOLS.md) for the full reference including when to use each tool and all command options.

### Install

```bash
python -m venv venv
.\venv\Scripts\activate.ps1   # Windows (or ./venv/bin/activate on Linux/macOS)
pip install -r tools/requirements.txt
```

### Quick start

`sniffer.py` is a single entry point for all tools — run `help` to see what's available:

```bash
python tools/sniffer.py help
```

Common workflows:

```bash
# Live dashboard — 5-panel view with GPS map
python tools/sniffer.py visualize --host 192.168.4.1
python tools/sniffer.py demo                             # no hardware needed

# Live monitor — headless, with PSK decryption
python tools/sniffer.py monitor --host 192.168.4.1 --decrypt
python tools/sniffer.py monitor --host 192.168.4.1 --messages  # text messages only

# Full pipeline — capture + audit + report in one command
python tools/sniffer.py assess --host 192.168.4.1 --duration 10m

# Offline: analyze SD card captures
python tools/sniffer.py report capture.csv --format html -o report.html
python tools/sniffer.py analyze capture.pcap --wireshark
python tools/sniffer.py audit capture.csv
```

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

Check serial output on next connection for reset reason (or press `i` in the serial console):
```
[INFO] Reset reason: Power-on (code 1)    <- normal
[WARN] Reset reason: Task watchdog        <- indicates hang during test
```

On **Heltec V4** (no serial), the Info tab in the web UI shows reset reason and health info.

Stats (total packets, devices, dropped packets) are in the Info tab and `/api/status`.

---

## Deep dives

- [TOOLS.md](TOOLS.md) — full Python tools reference
- [reference/HOW_IT_WORKS.md](reference/HOW_IT_WORKS.md) — security scoring algorithm, router detection, firmware estimation
- [reference/ENCRYPTION.md](reference/ENCRYPTION.md) — what the PSK decryption can and cannot decrypt
- [reference/NETWORK_HUNTING.md](reference/NETWORK_HUNTING.md) — hunting strategies by network type
- [developers/API.md](developers/API.md) — full REST and WebSocket reference

