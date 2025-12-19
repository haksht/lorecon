# CackalackyCon 2026 - Call for Papers Submission

**Submission URL:** https://sessionize.com/cackalackycon-2026  
**Deadline:** February 15, 2026  
**Conference Date:** May 15-17, 2026  
**Location:** DoubleTree by Hilton, Durham, NC (Research Triangle Park)

---

## Talk Title

**LoRa Under the Radar: Building a $30 Passive Reconnaissance Tool for Mesh Network Security Research**

### Alternative Titles
- *Meshtastic Mayhem: Exposing Encryption Gaps in Consumer Mesh Networks*
- *When Default Keys Aren't: Hunting Leaked PSKs in LoRa Mesh Traffic*

---

## Abstract (300 words)

Meshtastic, Helium, and LoRaWAN have quietly built massive mesh networks across cities worldwide—often using default encryption keys that offer little more than security theater. This talk introduces an open-source ESP32-based passive reconnaissance tool that scans 26 LoRa configurations, captures packets across the 902-928 MHz ISM band, and automatically decrypts traffic using 23 known PSKs—including leaked admin channel keys from 2023 security incidents.

We'll walk through the architecture of a production-grade embedded firmware that transforms a $30 Heltec WiFi LoRa 32 V3 into a field-deployable LoRa sniffer with:

- **Real-time packet capture** with interrupt-driven reception and 100-packet queuing
- **Multi-protocol detection** (Meshtastic, LoRaWAN/TTN, Helium Network)
- **Automated PSK testing** against default keys, including "AQ==" and leaked admin keys like "PKdTs51e4EB0BoOevIN0Dw=="
- **GPS coordinate extraction** from position broadcasts
- **Mobile-friendly web UI** with threat-level network visualization
- **PCAP export** compatible with Wireshark and custom LoRa dissectors

Live demo: We'll power up the sniffer at CackalackyCon and see what's transmitting in the Research Triangle—likely discovering Meshtastic nodes, IoT sensors, and potentially vulnerable devices broadcasting with default encryption.

**Key takeaways:**
1. Why default PSKs in consumer mesh networks are a research goldmine
2. How to identify vulnerable devices using RSSI, firmware fingerprinting, and traffic patterns
3. Defense strategies: proper key rotation, firmware updates, and network segmentation
4. The ethics of passive RF reconnaissance (receive-only, legal considerations)

All code is open source (MIT license). Attendees will leave with a shopping list, flash instructions, and the knowledge to build their own reconnaissance platform.

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
**Secondary:** IoT developers, ham radio operators, embedded systems engineers  
**Skill Level:** Intermediate (some familiarity with wireless concepts helpful)

---

## Speaker Bio

*[Your name and bio here]*

Security researcher focused on wireless protocols and embedded systems. Background in [relevant experience]. Previous talks at [conferences if any]. Active contributor to open-source security tooling.

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

## Why CackalackyCon?

This talk is a perfect fit for CackalackyCon because:

1. **Local relevance:** Research Triangle has significant Meshtastic/IoT adoption
2. **Hands-on focus:** Working demo hardware, not just slides
3. **Actionable takeaways:** Attendees can build their own tool for ~$30
4. **Responsible disclosure:** Focuses on defense, not exploitation
5. **Open source:** All code available, community can contribute

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
