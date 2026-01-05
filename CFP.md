# CackalackyCon 2026 - Call for Papers Submission

**Submission URL:** https://sessionize.com/cackalackycon-2026  
**Deadline:** February 15, 2026  
**Conference Date:** May 15-17, 2026  
**Location:** DoubleTree by Hilton, Durham, NC (Research Triangle Park)

---

## Talk Title

**LoRa Under the Radar: Building a $30 Mesh Network Sniffer**

### Alternative Titles
- *Meshtastic Mayhem: A $30 Tool for LoRa Reconnaissance*
- *When Default Keys Aren't: Hunting Leaked PSKs in LoRa Mesh Traffic*

---

## Abstract (300 words)

Meshtastic, Helium, and LoRaWAN have quietly built massive mesh networks across cities—often using default encryption keys that offer little more than security theater. This talk introduces an open-source ESP32-based passive reconnaissance tool that scans 26 LoRa configurations, captures packets, and automatically decrypts traffic using 23 known PSKs—including leaked admin channel keys from 2023.

We'll walk through building a $30 Heltec WiFi LoRa 32 V3 into a field-deployable sniffer with:

- **Real-time packet capture** with interrupt-driven reception
- **Multi-protocol detection** (Meshtastic, LoRaWAN/TTN, Helium Network)
- **Automated PSK cracking** against default keys, including "AQ==" and leaked admin keys
- **GPS coordinate extraction** from position broadcasts
- **Mobile-friendly web UI** with threat-level network visualization
- **PCAP export** compatible with Wireshark

**Live demo:** We'll power up the sniffer at CackalackyCon and see what's transmitting in RTP—likely discovering Meshtastic nodes, IoT sensors, and devices broadcasting with default encryption.

**Key takeaways:**
1. Why default PSKs in consumer mesh networks are a research goldmine
2. How to identify vulnerable devices using RSSI and traffic patterns
3. Defense strategies: key rotation, firmware updates, network segmentation
4. Ethics of passive RF reconnaissance (receive-only, legal framework)

All code is MIT-licensed open source. Attendees leave with a shopping list (~$30), flash instructions, and the knowledge to build their own reconnaissance platform tonight.

*No prior LoRa experience required—just curiosity about what's transmitting around you.*

---

## Detailed Outline

### Introduction (5 minutes)
- The hidden world of LoRa: millions of devices on ISM bands
- Why mesh networks are security research targets
- Meshtastic adoption explosion (hiking, emergency comms, protests)
- Legal framework: receive-only passive monitoring

### The Tool: Architecture Deep Dive (15 minutes)
- **Hardware:** Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262 + OLED)
- **Component design:** RadioController → PacketProcessor → ReconState
- **26 scan configurations:** 5-minute cycle covering:
  - Meshtastic presets (ShortFast, LongSlow, MediumFast, etc.)
  - LoRaWAN/TTN uplink channels
  - Helium Network hotspot frequencies
- **Interrupt-driven capture:** <50ms latency, atomic ISR flags
- **Why clean architecture matters** in embedded security tools

### Cryptographic Weaknesses (10 minutes)
- Meshtastic encryption: AES-256-CTR with publicly documented keys
- **The "AQ==" problem:** Single-byte keys expanded to 16 bytes
- **2023 leaked keys:** Admin channel defaults, debug keys on GitHub
- Live decryption demo: position broadcasts, telemetry, channel messages
- **What we CAN'T decrypt:** Direct messages (PKC in firmware 2.5+)

### Live Demo: Scanning RTP (10 minutes)
- Power up at the conference venue
- Real-time packet capture on projector
- Device discovery: node IDs, protocols, signal strength
- GPS extraction from Meshtastic POSITION_APP packets
- Threat scoring: router detection, firmware fingerprinting

### Defense Strategies (5 minutes)
- **Key rotation:** Using unique PSKs, avoiding defaults
- **Firmware updates:** Why 2.5+ matters for DM security
- **Network segmentation:** Separate admin channels
- **Traffic analysis resistance:** Quiet mode, periodic timing

### Q&A / Tool Demo (5 minutes)
- Hands-on with the hardware
- Web UI walkthrough
- Where to get the code

---

## Session Format

- [x] **Standard Talk (50 minutes)** - Preferred
- [ ] Lightning Talk (20 minutes)
- [ ] Workshop (2-4 hours)

---

## Target Audience

**Primary:** Security researchers, penetration testers, red teamers  
**Secondary:** IoT developers, ham radio operators, embedded systems engineers, curious hackers  
**Skill Level:** Intermediate (but accessible to beginners—no prior LoRa experience required)

---

## Speaker Bio

Tim Arnold is a retired software developer, Python programmer, and statistician who co-authored the 2nd edition of *Black Hat Python* (No Starch Press). These days he spends his time building security tools, breaking things for fun, and teaching others how to do the same. When not sniffing LoRa packets in the wild, he's probably writing Python code that probably shouldn't exist.

**Contact:** [email]  
**GitHub:** [profile]  
**Twitter/Mastodon:** [handle]

---

## Technical Requirements

- Projector with HDMI input
- WiFi access for live API demo (or we use ESP32's AP)
- Power outlet for demo hardware
- **No special RF permits needed** - device is receive-only and compliant with FCC Part 15

---

## Competitive Differentiation (Anticipated Q&A)

### vs. Flipper Zero + LoRa Backpack

| Aspect | Flipper + LoRa Module | This Project |
|--------|----------------------|---------------|
| **Total Cost** | ~$170 (Flipper) + $20-40 (module) = **$190-210** | **~$30** |
| **Form Factor** | Flipper + dangling GPIO board | **Single integrated unit** (radio + display + antenna) |
| **LoRa Chip** | Usually SX1276/1278 (older) | **SX1262** (newer, better sensitivity) |
| **Meshtastic Decode** | Basic packet display (Mayhem firmware) | **Full protocol parsing** + 23 PSK decryption |
| **Web UI** | ❌ None | ✅ Mobile-friendly network map |
| **Multi-protocol scan** | Manual frequency changes | **26 configs auto-cycle** (5 min) |
| **GPS extraction** | ❌ | ✅ + KML/GeoJSON export |
| **Packet replay** | Limited | **10-slot capture + configurable replay** |
| **Leaked PSK testing** | ❌ | ✅ Including 2023 admin keys |
| **Long-duration operation** | Battery drain, no watchdog | **Week+ unattended** (hardened firmware) |

**Elevator pitch**: "Flipper is a Swiss Army knife — great if you need RFID, IR, NFC, and Sub-GHz too. This is the specialist scalpel for LoRa recon at 1/7th the cost."

### vs. SDR Approaches (LoraCraft, HackRF, etc.)

| Aspect | SDR-based Tools | This Project |
|--------|----------------|---------------|
| **Hardware Cost** | $100-300 (HackRF/LimeSDR) | **~$30** |
| **Portability** | Laptop required | **Standalone** (battery + OLED) |
| **Setup Complexity** | GNU Radio flowgraphs, drivers | **Flash and go** |
| **Real-time latency** | Processing lag | **<50ms** interrupt-driven |
| **Replay capability** | Complex TX path required | **Built-in** 10-slot replay |
| **Web UI** | None (terminal only) | **Mobile-friendly** |

**Elevator pitch**: "SDR gives you flexibility but requires a laptop, GNU Radio expertise, and custom flowgraphs. This is field-deployable — flash it, pocket it, check results on your phone."

### Unique Capabilities

- **23 PSKs including leaked 2023 admin keys** — not just "AQ=="
- **26 frequency configs** — Meshtastic + LoRaWAN + Helium + ISM in one 5-min cycle
- **Threat-scored network visualization** — color-coded vulnerability assessment
- **GPS extraction + geographic export** — KML/GeoJSON for mapping
- **Production firmware** — watchdog, auto-recovery, runs unattended for weeks
- **$30 BOM** — conference attendees can build one tonight

---

## Why CackalackyCon?

This talk is a perfect fit for CackalackyCon because:

1. **Local relevance:** RTP has significant Meshtastic/IoT adoption—we'll demo live at the venue
2. **Hands-on focus:** Working demo hardware, not just slides (Hardware Hacking Village energy)
3. **Build-it-tonight:** Attendees can order parts and build their own for ~$30
4. **Inclusive:** No gatekeeping—accessible to newcomers while deep enough for experts
5. **Responsible disclosure:** Focuses on defense strategies, not exploitation
6. **Open source:** MIT-licensed, community can contribute and extend

---

## Links & Resources

- **GitHub Repository:** https://github.com/[username]/esp32-sniffer
- **Hardware Cost:** ~$30 (Heltec WiFi LoRa 32 V3)
- **Demo Video:** [link if available]
- **Related Research:**
  - Meshtastic Protocol Documentation
  - SX1262 LoRa Transceiver Datasheet
  - LoRaWAN Security Whitepaper

---

## Submission Checklist

- [ ] Submit via https://sessionize.com/cackalackycon-2026
- [ ] Include speaker photo
- [ ] Confirm availability for May 15-17, 2026
- [ ] Review CackalackyCon Code of Conduct
- [ ] Prepare backup slides (offline demo in case of RF interference)

---

*Questions? Contact sq33k@cackalackycon.org*
