# LoRa Network Hunting Guide

## What Networks Are You Scanning?

Your ESP32 sniffer is now configured to detect **three major LoRa network types**:

### 1. **Meshtastic** (10 configs)
**What it is**: Peer-to-peer mesh network for off-grid text messaging  
**Frequencies**: 902.125 MHz, 903.875 MHz, 904.375 MHz, 906.875 MHz  
**Why so common**: Exploded in popularity 2023-2024, cheap hardware ($30-40)  
**You'll find it**: Hiking trails, preppers, ham radio operators, off-grid communities

### 2. **The Things Network (TTN) / LoRaWAN** (4 configs)
**What it is**: Global open-source IoT network with community-run gateways  
**Frequencies**: 903.9 MHz, 904.1 MHz, 904.3 MHz, 904.5 MHz  
**Why important**: Powers thousands of IoT sensors worldwide  
**You'll find it**: Cities with TTN gateways, universities, smart buildings

### 3. **Helium Network** (4 configs)
**What it is**: Blockchain-based LoRaWAN network where people mine crypto by running hotspots  
**Frequencies**: 923.3 MHz, 923.9 MHz, 924.5 MHz, 925.1 MHz (downlink bands)  
**Why to hunt**: Huge deployment (millions of hotspots), active in urban/suburban areas  
**You'll find it**: Residential neighborhoods, businesses with Helium miners

### 4. **ISM Band General** (2 configs)
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

## Best Hunting Strategy

### Step 1: Reconnaissance - Is There Activity?

**Before going to a location**, check these maps:

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
- **Hacker cons**: Target-rich environment (like CackalackyCon!)
- **Endurance races**: SAR teams may use Meshtastic

### Step 3: Field Deployment

**Setup**:
1. Power on your sniffer
2. Start reconnaissance mode (scans all 26 frequencies)
3. Let it run for **at least one full cycle** (4 minutes)
4. Watch the OLED display for detections
5. Check web interface Network tab for devices

**What you'll see first**:
- **Meshtastic**: Usually appears within 30 seconds (frequent beaconing)
- **TTN/LoRaWAN sensors**: May take 5-30 minutes (infrequent transmissions)
- **Helium**: Depends on local activity

**Patience required**:
- LoRaWAN sensors transmit every 5-60 minutes (battery saving)
- If you see TTN configs with "🔥" (activity detected), stake out that area longer

### Step 4: Deep Dive on Active Frequencies

Once you detect activity:

1. **Note which config found it**: Check the Network tab or OLED
2. **Target that frequency**: Use web UI → Frequency Targeting
3. **Capture more packets**: Focused listening increases packet count
4. **Analyze later**: SD card logs will have all metadata

### Step 5: Data Analysis Back Home

**What you can learn**:
- Device types (from protocol analysis)
- Network topology (which devices route to which)
- Transmission patterns (how often devices beacon)
- Signal strength patterns (RSSI over time)
- Encryption status (properly secured or not)

**Use the Python analyzer**:
```bash
python tools/pcap_analyzer.py your_capture.pcap
python tools/recon_report.py your_capture.csv
```

---

## What Each Network Type Looks Like

### Meshtastic Characteristics
- **Frequent transmissions**: Every 15-30 seconds (position beacons)
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

## Ethical & Legal Considerations

### ✅ Legal to Monitor
- **Passive reception**: Listening to unencrypted broadcasts
- **Signal analysis**: RSSI, frequency, timing patterns
- **Protocol identification**: What type of device is transmitting

### ⚠️ Gray Area
- **Attempting decryption**: Legal if using publicly known default keys for research
- **Content inspection**: OK for research, not for interception

### ❌ Illegal
- **Transmitting interference**: Jamming or disrupting networks
- **Unauthorized access**: Decrypting private communications
- **Identifying individuals**: Tracking specific people without consent
- **Commercial exploitation**: Selling intercepted data

**Best practice**: Treat it like packet sniffing WiFi - observe, analyze, learn, but don't intercept content or disrupt service.

---

## Current Scan Configuration Summary

Your device now scans **26 frequency configurations**:

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

**Total scan cycle**: ~5 minutes (26 configs × 12 seconds each)

---

## Getting Started

1. **Check local coverage**: Visit TTN and Helium maps for your area
2. **Pick a location** with confirmed gateway coverage
3. **Bring a backup Meshtastic device** for guaranteed activity during demos
4. **Use the Python tools** for post-capture analysis: `tools/recon_report.py`, `tools/pcap_analyzer.py`

---

## Resources

- **The Things Network**: https://www.thethingsnetwork.org/
- **Helium Explorer**: https://explorer.helium.com/
- **Meshtastic Docs**: https://meshtastic.org/docs/
- **LoRaWAN Specification**: https://lora-alliance.org/
- **RadioLib (our library)**: https://github.com/jgromes/RadioLib

## Questions?

Check the project README or open an issue on GitHub!
