# Usage Guide

Using the sniffer after it's running: web interface, Python analysis tools, and field deployment.

If you haven't flashed the device yet, start with [SETUP.md](SETUP.md).

---

## Web interface

Browse to the device IP or `http://lora-xxyyzz.local` (replace `xxyyzz` with your device ID).

### 6 tabs + stats sidebar

**Info** — System overview: uptime, free memory, GPS fix status, security assessment summary, quick-action buttons.

**Devices** — All discovered LoRa devices sorted by vulnerability score. RSSI column shows Last / Avg / Best dBm; hover for standard deviation. Click any device to target it for focused capture.

**Packets** — Live Wireshark-style packet stream with protocol badges, RSSI indicators, and encryption status. Controls: pause, resume, clear, auto-scroll. Replay slots below.

**Frequencies** — All 29 scan configurations with activity indicators. Click any config to lock the radio to that frequency and modulation.

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
| Device list | Devices tab |
| GPS positions (KML) | Info tab → Export KML |
| GPS positions (GeoJSON) | Info tab → Export GeoJSON |

### With SD card (T3-S3, T-Beam Supreme)

Packets log to `/logs/` automatically on boot. Data survives reboots.

| Export | How |
|--------|-----|
| CSV packet log | Webapp sidebar → Download CSV |
| PCAP (Wireshark) | Webapp sidebar → Download PCAP |
| GPS positions | Info tab → Export KML / GeoJSON |

SD cards up to 32 GB work. Use FAT32. The T3-S3 works best with ≤4 GB cards.

---

## Python analysis tools

The `tools/` directory has a Python toolkit for live monitoring and offline analysis. See [TOOLS.md](TOOLS.md) for install steps, a quick-start command list, and the full per-tool reference.

---

## Field deployment

### Recommended setup for unattended / long-duration use

- Use a wall adapter, not a laptop USB port
- Connect the antenna before powering on
- Set the mode you want (recon or targeted) before leaving the device
- Serial console auto-deactivates after 5 minutes — or just don't connect one
- Use an SD card if you need persistent logs

### Modes

**Reconnaissance mode** (default): scans all 29 configurations in a ~6-minute cycle, discovers new devices automatically. Mode persists across reboots via NVS.

**Targeted mode**: lock to a specific device or frequency. Press `f` in serial or click a configuration in the Frequencies tab. Also persists across reboots.

### Monitoring remotely

Check the Devices tab on any browser connected to the same network. Stats (total packets, devices, dropped packets) are in the Info tab.

---

## Deep dives

- [TOOLS.md](TOOLS.md) — full Python tools reference
- [reference/HOW_IT_WORKS.md](reference/HOW_IT_WORKS.md) — security scoring algorithm, router detection, firmware estimation
- [reference/ENCRYPTION.md](reference/ENCRYPTION.md) — what the PSK decryption can and cannot decrypt
- [reference/NETWORK_HUNTING.md](reference/NETWORK_HUNTING.md) — hunting strategies by network type
- [developers/API.md](developers/API.md) — full REST and WebSocket reference
