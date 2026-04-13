# 5-Minute Conference Demo Script

**ESP32 LoRa Sniffer - Live Demonstration**

Use this script for reproducible live demos at security conferences, workshops, or meetups.

---

## Pre-Demo Checklist (Day Before)

- [ ] Flash latest firmware: `pio run -e heltec_v3 --target upload` (replace `heltec_v3` with your board env)
- [ ] Upload web UI: `pio run -e heltec_v3 --target uploadfs`
- [ ] Verify antenna connected (915MHz band)
- [ ] Charge battery or confirm USB power source
- [ ] Test WiFi AP comes up (`LoRa-XXXXXX`)
- [ ] Note your API token from serial output
- [ ] **Optional**: Bring a second Meshtastic device as guaranteed traffic source
- [ ] **Optional**: Pre-load SD card for PCAP export demo

---

## Demo Setup (5 Minutes Before)

1. **Power on ESP32** - Wait for OLED to show "Scanning..."
2. **Connect your phone** to `LoRa-XXXXXX` WiFi
3. **Open browser** to `http://192.168.4.1`
4. **Verify web UI loads** - You should see the Info tab

**Backup plan**: If WiFi flaky, use serial monitor (`pio device monitor`)  14 not available on Heltec V4 (no serial port by design; use web UI only)

---

## The 5-Minute Demo

### Minute 0-1: Introduction & Hook

**Say:** *"In the next 5 minutes, I'm going to show you every Meshtastic device in this room, decrypt their broadcasts, and plot their GPS coordinates - using $30 of hardware."*

**Show:** Hold up the Heltec board. Point out:
- Tiny form factor
- Built-in OLED showing live scan
- No laptop required (phone as display)

### Minute 1-2: Live Device Discovery

**Action:** Navigate to **Devices** tab in web UI

**Say:** *"The device is scanning 29 LoRa configurations covering Meshtastic, LoRaWAN, and Helium networks. Every ~6 minutes it completes a full cycle."*

**Point out:**
- Device count growing in real-time
- Node IDs appearing (hex format)
- RSSI values (signal strength)
- Protocol identification

**If no devices appear:**
- Activate your backup Meshtastic device
- Or show pre-captured data: *"At DEF CON we found 47 devices in 10 minutes"*

### Minute 2-3: PSK Decryption Demo

**Action:** Click on a device showing "Meshtastic" protocol

**Say:** *"Here's where it gets interesting. Meshtastic encrypts traffic, but most people use default keys. We test 23 known default PSKs including admin channel defaults from older firmware."*

**Show:**
- Navigate to **Info** tab → scroll to PSK Stats section
- Or use serial command `m` to show decryption hits

**Key talking points:**
- *"Key #1 is the official default - most networks use it"*
- *"Key #15 is the admin channel default from pre-2.5 firmware"*
- *"If we get a hit, we can read position broadcasts, telemetry, and channel messages"**

### Minute 3-4: GPS Extraction & Mapping

**Action:** Navigate to **Network** tab

**Say:** *"Every time we decrypt a position packet, we extract GPS coordinates. Here's where those devices physically are."*

**Show:**
- Interactive network map with device positions
- Color coding (red = vulnerable, green = secure)
- RSSI-based proximity estimation

**If no GPS data:**
- Show KML export feature: *"Export to Google Earth for post-analysis"*
- Or pre-prepared KML file from previous capture

### Minute 4-5: Packet Replay & Implications

**Action:** Navigate to **Packets** tab (Replay slots)

**Say:** *"We automatically capture packets for replay. This lets you test how mesh networks handle duplicate traffic - a key resilience question."*

**Show:**
- Captured packets with timestamps
- Decrypted text (if any messages captured)
- Replay controls (but DON'T actually transmit without permission)

**Closing statement:** *"All of this runs on a $30 board you can build in an afternoon. The takeaway: if you're deploying Meshtastic, change your default PSK. And if you're researching LoRa security, this is your new best friend."*

---

## Audience Participation Option

**Before demo starts, ask:**
*"Anyone here running Meshtastic on their phone or a device? Keep it on - let's see if we can find you."*

**During device discovery:**
*"I see 5 Meshtastic devices. Who wants to claim one?"*

**This creates engagement and proves the tool works live.**

---

## Handling Demo Failures

### No devices found
- *"The RF environment here is quiet - let me show you data from a busier venue"*
- Switch to pre-recorded screenshots or video
- Activate backup Meshtastic device

### WiFi connection issues
- Switch to serial monitor (always works)
- *"The web UI is convenient but the serial interface gives you more detail"*

### Web UI doesn't load
- Run `pio run -e heltec_v3 --target uploadfs` (adjust env for your board) to re-upload
- Use serial commands as backup: `m`, `a`, `g` (not available on Heltec V4)

### Decryption not working
- *"This network uses a custom PSK - that's actually good security practice"*
- Show the PSK stats: attempts vs successes

---

## Common Audience Questions

### "Why not just use a HackRF?"

**Short answer:** *"HackRF is a great tool, but it needs a laptop and setup time. This runs standalone, fits in your pocket, and is on from the moment you power it up."*

**Talking points:**
- HackRF is a general-purpose SDR — powerful, but requires a PC + SDR software (GNU Radio, SDR#, etc.) and meaningful setup per session
- This device is purpose-built for LoRa: powers on and immediately sniffs, no configuration needed
- The SX1262 radio demodulates LoRa in hardware — more sensitive and reliable than software-defined approaches on HackRF
- **Cost**: Heltec V3 ~$25, T3-S3 ~$20 vs HackRF ~$350+
- **Power**: HackRF draws ~350 mA and needs USB host; this runs for hours on a small LiPo
- **Form factor**: Fits in a shirt pocket, deployable anywhere — no laptop bag required
- HackRF can do LoRa, but you'd spend 30 minutes configuring GNU Radio before your first packet. This gives you your first packet in 5 minutes out of the box.

**Honest concession:** *"If you already own a HackRF and know GNU Radio, you can do everything this does and more. This project's value is accessibility and deployability, not raw capability."*

---

### "Can't a Flipper Zero do this?"

**Short answer:** *"Flipper can receive LoRa with an add-on module, but it has no LoRa-specific analysis — no PSK decryption, no device tracking, no web UI. It's a great multi-tool, not a dedicated LoRa recon platform."*

**Talking points:**
- Flipper LoRa support is via community add-on modules (e.g., LoRa-E5 board) — not first-class and not in stock firmware
- Flipper can capture raw bytes on a fixed frequency; it cannot scan 29 channel configurations, decode Meshtastic protocol, or test PSKs
- No SD logging, no PCAP export, no GPS extraction, no network map
- Flipper is excellent for SubGHz replay attacks on simpler protocols (garage doors, etc.) — LoRa's spread-spectrum modulation is a different problem
- Think of Flipper as a Swiss Army knife and this as a scalpel

---

### "Is this legal?"

**Short answer:** *"Passive RF monitoring of unlicensed spectrum is legal in the US under FCC Part 15. We don't transmit, we don't intercept private communications — we observe the same signals any LoRa gateway would receive."*

**Talking points:**
- LoRa operates on ISM bands (915 MHz in US, 868 MHz in EU) — unlicensed, publicly shared spectrum
- Passive monitoring (receive-only) has no legal restriction in the US; it's the same as listening to FM radio
- Transmission (replay) requires authorization — that's why the demo script says don't replay without permission
- Decrypting packets using publicly known default keys is legally analogous to logging into a router with the default password — not a CFAA violation
- If someone in the audience challenges this, redirect: *"Great question for the legal panel — but we're in the same category as a WiFi packet sniffer on an open network"*

---

### "Could you use this against me right now?"

**Short answer:** *"Only if your Meshtastic device is using a default PSK, which most out-of-the-box installations are. Change your PSK and you're invisible to this tool."*

**Talking points:**
- The tool discovers any LoRa device transmitting in range — that part is unavoidable (it's RF)
- Decryption only works against known default PSKs — custom keys are opaque to us
- Position data is only extracted if decryption succeeds
- The whole point of the demo is to show what passive observers can see — and motivate people to harden their configs
- *"This is the threat model you should be designing against"*

---

### "Why build this instead of contributing to an existing tool?"

**Short answer:** *"Nothing else combined standalone hardware demodulation, PSK testing, and GPS extraction in a $25 embedded package. We looked."*

**Talking points:**
- Wireshark LoRa dissectors exist but require a PC + SDR frontend
- ChirpStack and TTN gateways are infrastructure tools, not recon tools
- Meshtastic's own app can only see traffic on its own network with the right key — it's not a passive sniffer
- The embedded form factor is the unique contribution: deployable, low-power, no dependencies

---

## Post-Demo Resources

**Slide:** Show QR code linking to:
- GitHub repo
- This demo script
- Hardware BOM

**Handout/Slide content:**
```
ESP32 LoRa Sniffer
GitHub: github.com/tiarno/esp32-sniffer
Hardware: Heltec WiFi LoRa 32 V3 (~$25)
Time to first packet: 5 minutes
```

---

## Extended Demo Options (If Time Permits)

### 10-Minute Version
Add:
- Show ARCHITECTURE.md diagram
- Explain ISR → Queue → Processing pipeline
- Demonstrate quiet mode for high-traffic capture

### 15-Minute Version
Add:
- Live PCAP export to Wireshark
- Python live TUI (`lorarecon dev monitor --host <ip> --tui`)
- Security assessment scoring explanation

### Workshop Version (1 Hour)
- Audience builds their own devices
- Flash firmware together
- Capture and analyze real traffic
- Discussion of ethical considerations

---

## Legal Reminder

Before demonstrating at any venue:
1. Confirm passive RF monitoring is permitted
2. Don't transmit (replay) without explicit authorization
3. Don't identify individuals by their device traffic
4. Remind audience this is for research/education

*"What we're doing is the LoRa equivalent of passive WiFi monitoring - observing RF spectrum activity for research purposes."*
