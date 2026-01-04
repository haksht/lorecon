# 5-Minute Conference Demo Script

**ESP32 LoRa Sniffer - Live Demonstration**

Use this script for reproducible live demos at security conferences, workshops, or meetups.

---

## Pre-Demo Checklist (Day Before)

- [ ] Flash latest firmware: `pio run --target upload`
- [ ] Upload web UI: `pio run --target uploadfs`
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

**Backup plan**: If WiFi flaky, use serial monitor (`pio device monitor`)

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

**Say:** *"The device is scanning 26 LoRa configurations covering Meshtastic, LoRaWAN, and Helium networks. Every 5 minutes it completes a full cycle."*

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

**Say:** *"Here's where it gets interesting. Meshtastic encrypts traffic, but most people use default keys. We test 23 known PSKs - including keys leaked in 2023 security incidents."*

**Show:** 
- Navigate to **Info** tab → scroll to PSK Stats section
- Or use serial command `m` to show decryption hits

**Key talking points:**
- *"Key #1 is the official default - over 80% of networks use it"*
- *"Key #15 is the leaked admin channel key from pre-2.2 firmware"*
- *"If we get a hit, we can read position broadcasts, telemetry, and channel messages"*

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
- Run `pio run --target uploadfs` to re-upload
- Use serial commands as backup: `m`, `a`, `g`

### Decryption not working
- *"This network uses a custom PSK - that's actually good security practice"*
- Show the PSK stats: attempts vs successes

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
- Python visualizer (`tools/enhanced_live_visualizer.py`)
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
