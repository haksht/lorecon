# ESP32 LoRa Packet Sniffer & Reconnaissance Tool

Passive LoRa reconnaissance firmware for ESP32-S3 + SX1262 hardware. Scans 26 frequency configurations, captures and replays packets, tests 23 default Meshtastic PSKs, extracts GPS coordinates, and serves a phone-accessible web UI — all from a $30 board.

**Version:** 2.4.1 | **Status:** Production

---

## Supported hardware

- **Heltec WiFi LoRa 32 V3** — proven, lowest cost, CP210x serial
- **Heltec WiFi LoRa 32 V4** — adds optional L76K GPS; native USB (no serial monitor)
- **LilyGO T3-S3 V1.2/V1.3** — native SD card slot for PCAP/CSV logging
- **LilyGO T-Beam Supreme** — SD + built-in GPS + AXP2101 PMIC + battery management

See [docs/HARDWARE.md](docs/HARDWARE.md) for specs, pin maps, and purchase links.

---

## Getting started

**Just want to use it** (flash + web UI + tools):
→ [docs/SETUP.md](docs/SETUP.md) — download binary, flash, connect

**Want to build or modify the firmware:**
→ [docs/developers/BUILDING.md](docs/developers/BUILDING.md) — PlatformIO, serial monitor, OTA

---

## Why this over alternatives

| Tool | Cost | LoRa decode | PSK decrypt | Standalone | Web UI |
|------|------|-------------|-------------|------------|--------|
| **This project** | **$30** | Full | 23 keys | Yes | Yes |
| Flipper Zero + LoRa | $190-210 | Basic | No | Yes | No |
| HackRF + GNU Radio | $300+ | Full | Manual | No (laptop) | No |

---

## Feature snapshot

| Area | What it does |
|------|-------------|
| Reconnaissance | Cycles 26 LoRa configs (Meshtastic, LoRaWAN/TTN, Helium, ISM) on a 5-minute scan |
| Packet capture | Interrupt-driven, <50 ms latency, 100-packet queue |
| PSK decryption | Tests 23 Meshtastic default keys including legacy admin channel defaults |
| LoRaWAN testing | Verifies 16 default AppKeys against captured Join Requests |
| GPS extraction | Extracts coordinates from Meshtastic POSITION_APP packets |
| Packet replay | 10 slots, configurable repeat count and delay |
| Export | CSV, PCAP (Wireshark), KML, GeoJSON — wireless download from web UI |
| Web UI | 7-tab interface: devices, packets, network map, stats, frequency control |
| Security | Token-based API auth, device-unique AP password, NVS credential storage |

---

## Documentation

**For users:**
- [docs/SETUP.md](docs/SETUP.md) — flash + first boot + WiFi provisioning
- [docs/USAGE.md](docs/USAGE.md) — web interface, Python tools, field deployment
- [docs/TOOLS.md](docs/TOOLS.md) — Python toolkit full reference
- [docs/HARDWARE.md](docs/HARDWARE.md) — board comparison, specs, purchase links
- [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) — common problems and fixes

**For developers:**
- [docs/developers/BUILDING.md](docs/developers/BUILDING.md) — PlatformIO, build commands, serial monitor, OTA
- [docs/developers/ARCHITECTURE.md](docs/developers/ARCHITECTURE.md) — code structure and design decisions
- [docs/developers/API.md](docs/developers/API.md) — REST and WebSocket reference

**Reference:**
- [docs/reference/HOW_IT_WORKS.md](docs/reference/HOW_IT_WORKS.md) — security scoring, router detection, protocol analysis
- [docs/reference/ENCRYPTION.md](docs/reference/ENCRYPTION.md) — what PSK decryption can and cannot do
- [docs/reference/NETWORK_HUNTING.md](docs/reference/NETWORK_HUNTING.md) — hunting strategies by network type
- [docs/reference/CONFERENCE_DEMO.md](docs/reference/CONFERENCE_DEMO.md) — demo script and setup
- [docs/reference/THREAT_MODEL.md](docs/reference/THREAT_MODEL.md) — security threat model

---

## Legal and ethical use

This tool is for authorized security research, education, and testing on networks you own or have explicit permission to analyze.

**Legitimate uses:** Auditing your own Meshtastic/LoRa networks, security research with authorization, conference demonstrations, identifying devices using vulnerable default configurations.

**Not for:** Intercepting private communications without consent, tracking individuals without authorization, jamming or disrupting networks, commercial exploitation of captured data.

US context: passive RF reception is generally legal; interception of content may implicate 18 U.S.C. § 2511. Laws vary by jurisdiction. Default PSKs are tested to demonstrate the risk of using factory-default encryption keys.

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Keep modules focused, patches small, and include hardware details in bug reports.
