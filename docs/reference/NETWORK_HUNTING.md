# LoRa Network Hunting Guide

## What networks the sniffer covers

The sniffer scans **five LoRa network types** across 29 frequency configurations:

### 1. **Meshtastic** (10 configs)
**What it is**: Peer-to-peer mesh network for off-grid text messaging
**Frequencies**: US 902-906 MHz band (see the full config table below)
**Why so common**: Exploded in popularity 2023-2024, cheap hardware ($30-40)
**You'll find it**: Hiking trails, preppers, ham radio operators, off-grid communities

### 2. **The Things Network (TTN) / LoRaWAN** (7 configs)
**What it is**: Global open-source IoT network with community-run gateways
**Frequencies**: US 903-911 MHz uplink channels (see the full config table below)
**Why important**: Powers thousands of IoT sensors worldwide
**You'll find it**: Cities with TTN gateways, universities, smart buildings

### 3. **Helium Network** (4 configs)
**What it is**: Blockchain-based LoRaWAN network where people mine crypto by running hotspots
**Frequencies**: 904 MHz uplink and 923-925 MHz downlink (see the full config table below)
**Why to hunt**: Huge deployment (millions of hotspots), active in urban/suburban areas
**You'll find it**: Residential neighborhoods, businesses with Helium miners

### 4. **MeshCore** (3 configs)
**What it is**: Lightweight LoRa mesh network with a hybrid repeater/companion architecture
**Frequencies**: 910.525 MHz (US recommended), 915.0 MHz (unconfigured default), 869.618 MHz (EU/UK)
**Bandwidth**: 62.5 kHz narrow (US/EU); 250 kHz for unconfigured default
**Sync word**: `0x12` (RadioLib private, hardcoded in firmware)
**Why important**: Growing alternative to Meshtastic, distinct physical layer; public channel and hashtag rooms are decryptable
**You'll find it**: Ham radio operators, mesh networking enthusiasts, outdoor/hiking communities
**Decryption**: Public channel PSK and hashtag room keys decrypted automatically — see [ENCRYPTION.md](ENCRYPTION.md)

### 5. **ISM Band General** (2 configs)
**What it is**: Exploring common ISM band frequencies for unknown/proprietary protocols
**Frequencies**: 915 MHz, 920 MHz
**Why scan**: Catch experimental projects, proprietary sensors, unlabeled devices

---

## The Things Network (TTN) - Deep Dive

### What Is TTN?

**The Things Network** is a **free, open-source global LoRaWAN network** built and maintained by volunteers. Think of it as "the people's IoT network."

**How it works**:
1. Community members set up **LoRaWAN gateways** (receivers) in their homes/offices
2. These gateways forward all received packets to TTN cloud servers
3. Anyone can deploy **LoRaWAN sensors** (end devices) that send data through nearby gateways
4. Device owners retrieve their sensor data via TTN's free API

**Why it matters for you**:
- If TTN gateways are nearby, there are likely **IoT sensors transmitting**
- Your sniffer can detect these sensor transmissions **before** they reach the gateway
- Great way to find LoRaWAN traffic beyond just Meshtastic

### Current TTN Coverage

**Check if TTN is near you**: https://www.thethingsnetwork.org/map

**What the map shows**:
- **Blue hexagons** = LoRaWAN gateway coverage areas
- **Darker blue** = stronger signal/better coverage
- **Multiple overlapping hexagons** = multiple gateways (better chance of traffic)

**Best hunting zones**:
- **Cities with dark blue coverage**: Amsterdam, Berlin, San Francisco, NYC, London
- **Universities**: Many have TTN gateways for research projects
- **Tech hubs**: Silicon Valley, Boston, Austin, Seattle
- **Smart city pilots**: Check your city's website for IoT projects

---

## Hunting strategy

### Step 1: Pre-location reconnaissance

Before going to a location, check these maps:

1. **The Things Network Map**: https://www.thethingsnetwork.org/map
   - Look for blue hexagons near your target location
   - More hexagons = more potential traffic

2. **Helium Explorer**: https://explorer.helium.com/
   - Shows Helium hotspot locations
   - Filter by "LoRaWAN" (not just 5G)
   - Clusters of hotspots = good hunting

3. **Meshtastic Map** (if public nodes): https://meshtastic.org/docs/introduction/
   - Less useful (most nodes don't share location publicly)
   - But check local groups on Discord/Reddit

4. **MeshCore community**: Check Discord/local groups — no public node map
   - Growing overlap with Meshtastic users at ham and hacker events

### Step 2: Strategic Location Selection

**High-probability locations**:

#### Urban Areas
- **Business districts**: Smart parking, waste management, air quality sensors
- **University campuses**: Research deployments, student projects
- **Smart buildings**: HVAC, occupancy sensors
- **Near TTN gateway owners**: Check TTN community forum for local chapter meetups

#### Suburban/Residential
- **Neighborhoods with Helium hotspots**: Check explorer first
- **Near hiking trailheads**: Meshtastic users starting/ending hikes
- **Parks with environmental monitoring**: Soil, water quality sensors

#### Events (Best Option!)
- **Maker faires**: Everyone brings projects
- **Ham radio Field Day**: Meshtastic is popular
- **Hacker cons**: Target-rich environment
- **Endurance races**: SAR teams may use Meshtastic

### Step 3: Field deployment

**Setup**:
1. Power on the sniffer
2. Reconnaissance mode is default -- it scans all 29 configurations automatically
3. Let it run for **at least one full cycle** (~6 minutes)
4. Watch the OLED display for detections
5. Check the web UI Devices tab for devices

**What you'll see first**:
- **Meshtastic**: First packet usually within a few minutes if any nodes are in range — defaults are position every ~15 min and NodeInfo every ~10 min per node, so a single device can be slow. A busy mesh with routers relaying neighbors' traffic appears faster.
- **TTN/LoRaWAN sensors**: May take 5-30 minutes (infrequent transmissions, battery-saving duty cycles)
- **Helium**: Depends on local activity

**Patience required**:
- LoRaWAN sensors transmit every 5-60 minutes (battery saving)
- If you see activity on TTN configs, stake out that area longer

### Step 4: Focus on active frequencies

Once activity is detected:

1. Note which config found it (Network tab or OLED)
2. Lock to that frequency: web UI -> Frequencies tab -> click the config
3. Focused listening captures more packets from that network
4. SD card logs will have all metadata for offline analysis

### Step 5: Post-capture analysis

**What you can learn**:
- Device types (from protocol analysis)
- Network topology (which devices route to which)
- Transmission patterns (how often devices beacon)
- Signal strength patterns (RSSI over time)
- Encryption status (properly secured or not)

**Use the Python analyzer**:
```bash
lorecon report capture.csv -o report.html
lorecon dev wireshark capture.pcap
```

---

## What Each Network Type Looks Like

### Meshtastic Characteristics
- **Periodic transmissions**: Position every ~15 min and NodeInfo every ~10 min per node (configurable). A mesh with active routers produces steady traffic from many neighbors even when any single node is quiet.
- **Strong signals**: Typically -40 to -90 dBm (handheld devices nearby)
- **Multiple devices**: Mesh networks have 3-20 nodes
- **Encrypted**: AES-256 with PSK (default key often used)

### LoRaWAN/TTN Characteristics
- **Infrequent transmissions**: Every 5-60 minutes
- **Weaker signals**: -90 to -120 dBm (distant sensors, low power)
- **Short packets**: 10-50 bytes typical
- **Properly encrypted**: AES-128, harder to decrypt

### Helium Network Characteristics
- **Similar to TTN**: Uses LoRaWAN protocol
- **More downlink traffic**: Hotspots send acknowledgments (923 MHz band)
- **Urban density**: More hotspots in cities than TTN
- **Commercial deployments**: Asset tracking, supply chain monitoring

---

## Ethical & legal considerations

Laws vary by jurisdiction. This is a general orientation, not legal advice — check your local regulations (in the US, the relevant statutes include the ECPA and Wiretap Act) before you act on any of it.

**Generally lower risk:**
- Passive reception of the RF spectrum
- Signal analysis (RSSI, frequency, timing, protocol identification)
- Decrypting with publicly known default keys for your own research, on packets you have a legitimate reason to examine

**Risky regardless of technical feasibility:**
- Attempting to decrypt communications you have no relationship to or authorization for
- Reading, storing, or sharing the contents of private messages
- Tracking specific individuals without consent
- Any commercial use of intercepted data

**Prohibited anywhere:**
- Transmitting interference or jamming
- Unauthorized access to systems protected by access controls

**Best practice:** treat it like passive WiFi monitoring — observe, analyze, learn. Decrypted content is for understanding protocol behavior, not for interception. If you're doing a paid assessment or public demo, get written scope and authorization first.

---

## Scan configuration summary

The sniffer covers **29 frequency configurations**:

| Config | Frequency | Protocol | Notes |
|--------|-----------|----------|-------|
| 1-4    | 902-906 MHz | Meshtastic LF/MF/SF | Primary routing configs |
| 5-10   | 902 MHz variants | Meshtastic 902 | SF8-11, various sync words |
| 11-14  | 903-904 MHz | TTN/LoRaWAN | The Things Network channels (sync 0x34) |
| 15-17  | 902-911 MHz | LoRaWAN US915 | Commercial uplink channels (sync 0x34) |
| 18     | 906 MHz | Meshtastic LongSlow | Maximum range preset |
| 19-20  | 904 MHz | Helium Uplink | Sensor transmissions (sync 0x34) |
| 21-24  | 923-925 MHz | Helium Downlink | Hotspot transmissions (sync 0x34) |
| 25-26  | 915-920 MHz | ISM General | Private LoRa / RadioHead (sync 0x12) |
| 27     | 910.525 MHz | MeshCore US | BW 62.5 kHz, SF7 — US/Canada recommended (sync 0x12) |
| 28     | 915.0 MHz | MeshCore US Default | BW 250 kHz, SF10 — unconfigured firmware default (sync 0x12) |
| 29     | 869.618 MHz | MeshCore EU | BW 62.5 kHz, SF8 — EU/UK narrow (sync 0x12) |

**Total scan cycle**: ~6 minutes (29 configs × 12 seconds each)

---

## Getting started

1. Check local coverage: TTN map and Helium Explorer for your area
2. Pick a location with confirmed gateway coverage
3. Bring a backup Meshtastic device for guaranteed activity during demos
4. Use the Python tools for post-capture analysis: `lorecon report capture.csv`, `lorecon dev wireshark capture.pcap`

---

## Resources

- The Things Network: https://www.thethingsnetwork.org/
- Helium Explorer: https://explorer.helium.com/
- Meshtastic docs: https://meshtastic.org/docs/
- LoRaWAN specification: https://lora-alliance.org/
- RadioLib: https://github.com/jgromes/RadioLib
