# Feature Documentation

**ESP32 LoRa Reconnaissance Tool v2.2.1**

## 🎯 Tool Identity

**Core Mission**: Recon → Sniff → Capture → Replay → Analyze

A production-ready LoRa packet capture and analysis platform for security research and RF experimentation. Features clean architecture (RadioController, PacketProcessor, IReconTool), GPS parsing, PSK decryption, and OLED display for standalone operation.

**Architecture**: v2.0 - Refactored with clean component separation and dependency inversion

---

## ✅ Implemented Features

### 📺 OLED Display & Button Control (v1.8+)
**Status**: ✅ Production Ready  
**Architecture**: UserInterface + OLEDDisplay components  
**Files**: `oled_display.cpp/.h`, `user_interface.cpp/.h`, `ui_components.cpp/.h`

#### Hardware Requirements
- **Heltec WiFi LoRa 32 V3** with 128x64 SSD1306 OLED (I2C address 0x3C)
- **User button** on GPIO 0 (PRG button, active low with internal pullup)

#### Pin Configuration
- **OLED I2C**: SDA=GPIO 17, SCL=GPIO 18, RST=GPIO 21
- **Power Control**: Vext=GPIO 36 (active LOW for V3)
- **Button**: GPIO 0
- **SD Card** (optional): CS=GPIO 5 (SPI uses default pins: SCK=9, MISO=11, MOSI=10)

#### Features
- **128x64 SSD1306 display** (I2C 0x3C)
- **6 display modes** (Welcome, Idle, Device Info, Signal Info, Stats, Summary)
- **Button control**:
  - **Short press** (< 3 seconds): Toggle display mode
  - **Long press** (≥ 3 seconds): Initiate shutdown sequence
- **Auto-cycle timer** (configurable display rotation)
- **Robust initialization** (reset pulse + retry logic)
- **Runtime recovery** (reinitialize() method for transient failures)
- **Graceful degradation** (continues without display on boards lacking OLED)

**What it does**: Provides standalone visual feedback and control without requiring serial connection. Display shows scanning status, packet info, device counts, signal quality, and statistics. Button allows display mode cycling and safe shutdown.

---

### 🔍 Core Reconnaissance (ESSENTIAL)
**Status**: ✅ Production Ready  
**Architecture**: LoRaReconTool (orchestrator) + ReconState (tracking)  
**Files**: `lora_recon_tool.cpp/.h`, `recon_state.cpp/.h`

- **26 scan configurations** covering comprehensive US LoRa spectrum:
  - **Meshtastic** (11 configs): ShortFast/Slow, MedFast/Slow, LongFast/Slow, VLongSlow variants across 902-907 MHz
  - **LoRaWAN/TTN** (7 configs): The Things Network uplink channels 903.9-911.9 MHz
  - **Helium Network** (6 configs): Uplink (904.3, 904.5 MHz) + Downlink (923-925 MHz)
  - **ISM Band** (2 configs): Generic 915/920 MHz experimental frequencies
- **Sequential frequency scanning** (~12s dwell time per config, ~5 minute full cycle)
- **RF activity monitoring** (RSSI tracking, peak detection per config)
- **Device tracking** (50 devices max, LRU eviction for long sessions)
- **Protocol detection** (Meshtastic, LoRaWAN, Helium, generic)
- **Interactive targeting** (select discovered devices or frequencies for focused capture)
- **Resume capability** ('r' command keeps discovered devices)

**What it does**: Automatically scans 26 LoRa configurations to discover active devices across Meshtastic mesh networks, The Things Network (TTN) gateways, Helium hotspots, and experimental ISM band activity. Tracks signal strength and lets you target specific devices or frequencies for deeper analysis. Resume feature allows restarting scans without losing device history.

**Network Coverage**: Optimized for US frequency plan (902-928 MHz). Includes all common Meshtastic modem presets, TTN uplink channels, and Helium Network uplink/downlink frequencies.

---

### 📡 Packet Capture & Analysis (ESSENTIAL)
**Status**: ✅ Production Ready  
**Architecture**: RadioController (hardware) + PacketProcessor (analysis)  
**Files**: `radio_controller.cpp/.h`, `packet_processor.cpp/.h`, `protocol_analyzer.cpp/.h`

#### RadioController Features
- **Thread-safe interrupt handling** (atomic flags, ISR-driven)
- **SX1262 hardware abstraction** (SPI, pins, configuration)
- **Signal metrics caching** (RSSI, SNR with 100ms cache)
- **Configuration management** (frequency, BW, SF, sync word)

#### PacketProcessor Features
- **Queue management** (100-packet queue, overflow protection with drop tracking)
- **Protocol analysis coordination** (Meshtastic, LoRaWAN)
- **PSK decryption coordination** (23 default keys including legacy admin defaults)
- **Activity tracking** (via TextPacketDiagnostic)
- **Last packet storage** (for replay capture)

#### Analysis Capabilities
- **Real-time packet capture** (interrupt-driven, <50ms latency)
- **Meshtastic header parsing** (node IDs, hop counts, routing flags)
- **LoRaWAN frame analysis** (message types, DevAddr extraction)
- **Device fingerprinting** (firmware version estimation, power class)
- **Router detection** (identifies mesh routing devices)
- **Signal analysis** (RSSI, SNR, frequency error)
- **Packet timing analysis** (gaps, burst detection)
- **Encryption status tracking** (encrypted vs plaintext)

**What it does**: Captures and analyzes LoRa packets in real-time with clean architecture separation. RadioController handles hardware, PacketProcessor manages queue and analysis, resulting in reliable, maintainable packet capture.

---

### 🔄 Packet Replay (ESSENTIAL)
**Status**: ✅ Production Ready  
**Build Flag**: Always enabled  
**Files**: `lora_recon_tool.cpp`, `recon_state.cpp`, `repositories/packet_store.cpp`

- **10 replay slots** (circular buffer - oldest replaced when full)
- **Auto-capture** (packets automatically saved during scanning)
- **FIFO behavior** (always shows 10 most recent unique packets)
- **Packet deduplication** (skips relay copies with same packetId)
- **Replay menu** (list, select, transmit captured packets)
- **Configuration preservation** (replays with original radio settings)
- **Metadata tracking** (original RSSI, timestamp, protocol, decrypted text)

**What it does**: Automatically captures packets during scanning into a circular buffer with deduplication. When slots are full, oldest packets are replaced, ensuring you always have the 10 most recent *unique* packets available for replay or analysis. Mesh relay copies are automatically filtered out.

---

### 🔓 Broadcast & Channel Message Decryption (SECURITY RESEARCH)
**Status**: ✅ Production Ready  
**Architecture**: PSK testing via PacketProcessor coordination  
**Files**: `psk_decryption_simple.cpp/.h`, `psk_tests.h`

- **23 default Meshtastic PSKs** including:
  - Standard 16-byte keys (most common: "AQ==", "1PG7OiApB1nwvP+rz05pAQ==")
  - Legacy single-byte variants (0x01-0x09, pre-2.0 firmware)
  - **Historic defaults from older firmware:**
    - Admin channel default (pre-2.5): "PKdTs51e4EB0BoOevIN0Dw=="
    - Secondary channel default: "shmLkA9H74gAeLH3eGCqsw=="
    - Debug/dev key (from firmware source): "ogDPnKVRN7wz/VF8nt6LkA=="
    - EU868 regional default: "ZQ+HdKKbbAU4dSCGt66Qqw=="
  - Channel preset derived keys (LongFast, MediumSlow, ShortFast, etc.)
  - Weak test keys (all zeros, "1234567890123456", "testtesttesttest")
- **AES-128/256 decryption** (Meshtastic encryption standard)
- **Automated test suite** (Base64 parsing, PSK loading, decryption logic)
- **Message extraction** (pulls plaintext from decrypted packets)
- **Success statistics** (tracks which keys work)

**What it decrypts**: Position broadcasts, telemetry, node info, traceroutes, and **channel/group messages**

**Supported Packet Types**:
- TEXT_MESSAGE_APP (0x01): Channel chat messages
- POSITION_APP (0x03): GPS coordinates (triggers GPS extraction)
- NODEINFO_APP (0x04): Device names, hardware info
- ADMIN_APP (0x07): Administrative messages
- TELEMETRY_APP (0x08): Battery %, voltage, channel utilization, air util
- TRACEROUTE_APP (0x42): Routing traces
- MAP_REPORT_APP (0x43): Map reports

**What it cannot decrypt**: Direct messages (DMs) - these use Public Key Cryptography (PKC) in firmware 2.5.0+

**What it does**: Tests captured Meshtastic packets against 23 known default encryption keys including legacy admin channel defaults. When a match is found, decrypts broadcasts and channel messages to reveal content, GPS data, and telemetry. Integrates with GPS extraction for POSITION_APP packets.

**Current Status**: Code complete and working. Successfully decrypts position broadcasts, telemetry, and channel messages sent with default PSKs. Direct messages require PKC decryption (not feasible without private key).

**See also**: [../technical/ENCRYPTION.md](../technical/ENCRYPTION.md) for complete explanation of what's decryptable vs. PKC-protected.

---

### � LoRaWAN Key Testing (SECURITY RESEARCH)
**Status**: ✅ Production Ready  
**Architecture**: AES-CMAC verification via lorawan_keys module  
**Files**: `lorawan_keys.cpp/.h`

- **16 default LoRaWAN AppKeys** including:
  - Test keys (all zeros, all ones)
  - TTN documentation examples
  - Manufacturer factory defaults (Dragino, RAK, Heltec)
  - Sequential pattern keys
- **Join Request detection** (identifies MHDR 0x00 frames)
- **MIC verification** (AES-CMAC over JoinEUI + DevEUI + DevNonce)
- **Statistics tracking** (Join Requests seen, decoded, keys found)

**How it works**: When a LoRaWAN Join Request is captured, the tool computes the MIC (Message Integrity Code) using each default AppKey. A matching MIC means the device uses that default key, indicating a security vulnerability.

**Access**: 
- Serial command: `w` - Print LoRaWAN stats summary
- API endpoint: `/api/recon/summary` - `lorawanStats` object

**What it detects**: Devices using factory-default AppKeys are vulnerable to replay attacks and key compromise. Production LoRaWAN devices should always use unique, randomly-generated AppKeys.

---

### �📍 Geographic Intelligence (v2.0)
**Status**: ✅ Production Ready  
**Architecture**: GeoIntelligence component with protobuf parsing  
**Files**: `geo_intelligence.cpp/.h`

- **GPS coordinate extraction** (latitude, longitude, altitude)
- **Meshtastic POSITION_APP parsing** (protobuf wire types 0 and 5)
- **Wire type 0 support** (varint encoding)
- **Wire type 5 support** (sfixed32 encoding)
- **Coordinate scaling** (1e-7 factor for degree conversion)
- **50 position history** (tracks device movements over time)
- **KML export** (Google Earth compatible, 'k' command)
- **GeoJSON export** (web mapping tools like Leaflet, Mapbox, 'j' command)
- **Node position tracking** (associates GPS with node IDs)
- **Integration with PSK decryption** (automatic extraction from decrypted POSITION_APP packets)

**Verified Working**: Extracted real coordinates (35.730228° N, 78.879128° W) from live Meshtastic traffic

**What it does**: Automatically extracts GPS coordinates from Meshtastic position packets (both encrypted and plaintext) and lets you export them for mapping in Google Earth or web-based maps. Works with both wire type encodings found in Meshtastic firmware.

---

### 🔇 Quiet Mode (FAST PACKET CAPTURE)
**Status**: ✅ Production Ready  
**Build Flag**: Always enabled  
**Command**: Press 'q' to toggle

#### Purpose
Reduces serial output by 95% to minimize packet processing time and capture gaps.

#### Features
- **Minimal output**: Only displays TEXT messages when found
- **Fast processing**: <200ms per packet (vs 2-6s in verbose mode)
- **Reduced gaps**: Eliminates 90% of timing gaps that cause missed packets
- **Statistics tracking**: All data still tracked (view with 'x' command)
- **Toggle mode**: Press 'q' to switch between quiet and verbose

#### When to Use
- Capturing text messages (reduces blind time)
- Long monitoring sessions (cleaner output)
- Performance-critical scenarios (maximizes packet capture rate)

**What it does**: Speeds up packet processing by disabling verbose diagnostic output, allowing the tool to capture more packets during rapid transmission bursts.

---

### �🛡️ Production-Grade Reliability (ESSENTIAL)
**Status**: ✅ Production Ready  
**Build Flag**: `-DPRODUCTION_BUILD`, `-DERROR_LOGGING_ENABLED`  
**Files**: `error_handler.cpp`, `command_handler.cpp`

- **Hardware watchdog timer** (30s timeout, auto-recovery)
- **Atomic interrupt handling** (`std::atomic<bool>` for thread safety)
- **Comprehensive error recovery** (automatic radio reset, memory checks)
- **Error history tracking** (last 20 errors with full context)
- **Health monitoring** (memory usage, uptime, error counts)
- **Command pattern dispatcher** (O(1) lookup, no nested if/else)
- **Input timeouts** (30s timeout on all user input, no hangs)
- **Fixed buffer allocations** (no heap fragmentation risk)

**What it does**: Ensures the tool runs reliably for extended periods, automatically recovers from errors, and provides detailed diagnostics when things go wrong.

---

### 📊 Data Export & Logging (UTILITY)
**Status**: ✅ Production Ready  
**Build Flag**: Always enabled  
**Files**: `lora_recon_tool.cpp`, `geo_intelligence.cpp`

- **JSON packet logging** (LittleFS filesystem)
- **Structured log format** (timestamp, frequency, protocol, RSSI, SNR, hex data)
- **KML export** (geographic positions for Google Earth)
- **GeoJSON export** (geographic positions for web maps)
- **Serial output** (human-readable real-time display)

**What it does**: Saves captured packets to flash memory for later analysis and exports geographic data to standard mapping formats.

#### 💾 Data Export: SD Card vs No SD Card

| Export Type | Without SD Card | With SD Card |
|-------------|-----------------|--------------|
| **Discovered Devices (JSON)** | ✅ `/api/devices` | ✅ `/api/devices` |
| **System Status (JSON)** | ✅ `/api/status` | ✅ `/api/status` |
| **Replay Packets (JSON)** | ✅ `/api/replay/slots` | ✅ `/api/replay/slots` |
| **GPS Positions (KML)** | ✅ Serial 'k' command | ✅ Serial + file export |
| **GPS Positions (GeoJSON)** | ✅ Serial 'j' command | ✅ Serial + file export |
| **PCAP Packet Capture** | ❌ Not available | ✅ `/packets.pcap` |
| **CSV Packet Log** | ❌ Not available | ✅ `/packets.csv` |
| **Persistent History** | ❌ Lost on reboot | ✅ Survives reboot |

**Without SD Card - How to Export:**
```bash
# Via browser or curl - works over WiFi (AP at 192.168.4.1 or hotspot IP)
curl http://<device-ip>/api/devices > devices.json
curl http://<device-ip>/api/status > status.json
curl http://<device-ip>/api/replay/slots > packets.json

# Via serial - KML/GeoJSON output (copy from terminal)
# Press 'k' for KML, 'j' for GeoJSON
```

**Key Limitation**: Without SD card, all captured data is in RAM only. A reboot loses everything. Export frequently if you need to preserve data.

---

### 🎨 User Interface (ESSENTIAL)
**Status**: ✅ Production Ready  
**Build Flag**: Always enabled  
**Files**: `user_interface.cpp`, `ui_components.cpp`, `data/webapp/`

#### Serial Interface
- **Interactive menu system** (press 'm' to see options)
- **Device selection** (numbered list, pick target)
- **RF activity visualization** (frequency heatmap in serial output)
- **Security assessment** (vulnerability scoring for discovered devices)
- **Device type analysis** (breakdown by device type, power class)
- **Real-time statistics** (packet counts, uptime, error rates)

#### Web Interface (v2.1 - November 2025)
**Files**: `data/webapp/index.html`, `js/app.js`, `js/network-map.js`, `js/war-room.js`, `js/toast.js`

**7-Tab Interface:**
1. **Info Tab** - System overview, GPS data, security assessment, quick actions
2. **Devices Tab** - Discovered devices with one-click targeting and vulnerability sorting
3. **Packets Tab** - Replay menu with transmission controls and packet history
4. **Frequency Tab** - 26 scan configuration targeting with activity indicators
5. **Network Tab** - Interactive topology map with RSSI-based node positioning
6. **Stats Tab** - Protocol statistics with war room dashboard (pie/bar charts)
7. **Settings Tab** - WiFi configuration, diagnostics, OTA updates, system controls

**Audio Feedback System** ("Geiger Counter for LoRa"):
- **Protocol-specific tones**: Meshtastic (800 Hz), LoRaWAN (600 Hz), Helium (500 Hz)
- **Event sounds**: Device discovery (click), capture complete (two-tone)
- **Toggle control**: Enable/disable via settings or header button
- **Web Audio API**: Browser-native, no external dependencies

**Protocol Statistics Dashboard**:
- **Visual breakdown**: Pie chart (desktop) or bar chart (mobile)
- **Color-coded**: Meshtastic (blue), LoRaWAN (orange), Helium (green)
- **Live updates**: Real-time packet counts and percentages
- **Canvas-based**: 60fps smooth animations

**Live Packet Stream** (Wireshark-style):
- **Syntax highlighting**: Color-coded fields (node ID, frequency, RSSI)
- **Protocol badges**: Visual indicators for Meshtastic/LoRaWAN/Helium
- **Encryption status**: 🔒 Encrypted / 📖 Plaintext badges
- **RSSI indicators**: Color-coded signal strength (excellent/good/fair/poor)
- **Decrypted text display**: Shows plaintext when PSK succeeds
- **Controls**: Pause/resume, clear, auto-scroll

**Interactive Network Map**:
- **RSSI positioning**: Nodes placed by signal strength (50-300px from center)
- **Canvas rendering**: 60fps smooth animations with hover effects
- **Signal bars**: Visual signal strength indicators per node
- **Click-to-details**: Node info panel with targeting button
- **Protocol colors**: Meshtastic (blue), LoRaWAN (orange), Helium (green)
- **Real-time updates**: 2-second polling for topology changes

**What it does**: Provides three interface options (serial, OLED, web) for different use cases. Web UI adds impressive visualizations perfect for conference demonstrations and field deployment with mobile devices.

---

## 🔧 Build Configuration

### Active Flags (v2.0)
```ini
# All features enabled by default in v2.0
# No build flags needed - PSK testing, GPS parsing, etc. always available
```

### Removed Flags (v2.0)
```ini
-DENABLE_STRESS_TESTING          # Removed - hardware stress testing
-DENABLE_OFFENSIVE_TESTING       # Removed - attack framework  
-DENABLE_INTELLIGENCE_STORAGE    # Never fully implemented
-DENABLE_TRAFFIC_ANALYSIS        # Redundant with existing analysis
```
-DENABLE_INTELLIGENCE_STORAGE  # REMOVED - Module deleted
-DENABLE_NETWORK_RECON         # REMOVED - Module deleted
-DENABLE_TRAFFIC_ANALYSIS      # REMOVED - Module deleted
```

---

## 📏 Feature Priority Classification

### ESSENTIAL (Cannot Demo Without)
- ✅ Core reconnaissance (device discovery)
- ✅ Packet capture (real-time sniffing)
- ✅ Packet replay (retransmission)
- ✅ Production reliability (watchdog, error recovery)
- ✅ User interface (interactive menus)

### HIGH VALUE (Makes Demo Impressive)
- ✅ Broadcast decryption (position, telemetry, channel messages with default PSK)
- ✅ Geographic intelligence (GPS mapping)
- ✅ Protocol analysis (device fingerprinting)
- ✅ SD card logging (field deployment capability)

### NICE TO HAVE (Polish)
- ✅ KML/GeoJSON export
- ✅ Security assessment scoring
- ✅ Device type analysis
- ✅ Live visualization (Python tools in `tools/`)

### OUT OF SCOPE
- ❌ Persistent session storage
- ❌ Network topology graphs
- ❌ Attack/stress testing framework (removed v2.0)

---

## 🎯 Conference Demo Focus

### What We'll Demonstrate
1. **Live Network Visualization** ⭐ - Interactive network map showing devices appearing in real-time
2. **Audio Feedback** ⭐ - "Geiger counter" effect with different tones per protocol (Meshtastic/LoRaWAN/Helium)
3. **Protocol Statistics** ⭐ - Real-time pie chart breakdown of captured traffic
4. **Live Packet Stream** ⭐ - Wireshark-style feed with syntax highlighting and encryption indicators
5. **Reconnaissance Phase** - Discover nearby LoRa devices across 26 frequency configs (~5 min cycle)
6. **Device Targeting** - Lock onto specific device for focused capture
7. **Packet Replay** - Capture and retransmit packets
8. **PSK Exploitation** - Decrypt default-key Meshtastic broadcasts and channel messages
9. **Geographic Intelligence** - Show GPS positions extracted from decrypted POSITION_APP packets
10. **Multi-Network Detection** - Demonstrate coverage of Meshtastic, TTN/LoRaWAN, and Helium Network

⭐ = New visualization features added November 2025 for maximum audience impact

### What We Won't Demonstrate
- Session management (not implemented)
- Network topology graphs (not needed)
- Cryptographic attacks (out of scope - this is reconnaissance)

---

## 📊 Code Statistics

| Component | Lines | Status | Build Flag |
|-----------|-------|--------|------------|
| Core Recon | ~800 | ✅ Production | Always |
| Packet Capture | ~400 | ✅ Production | Always |
| Packet Replay | ~200 | ✅ Production | Always |
| PSK Decryption | ~300 | ✅ Code Complete | Optional |
| Geo Intelligence | ~400 | ✅ Production | Always |
| Stress Testing | ~600 | ✅ Production | Optional |
| Error Handler | ~400 | ✅ Production | Optional |
| User Interface | ~800 | ✅ Production | Always |
| Protocol Analyzer | ~300 | ✅ Production | Always |
| **Total** | **~4,200** | **Production** | - |

---

## � Hardware Capabilities & Limitations

### ✅ What This ESP32 Platform Excels At:

1. **Passive Reconnaissance** - Multi-frequency scanning and device discovery
2. **Packet Analysis** - Real-time protocol parsing and signal metrics
3. **Targeted Monitoring** - Deep inspection of individual devices
4. **Data Collection** - Stream RF data to PC for offline analysis
5. **Opportunistic Decryption** - Testing weak/default PSKs
6. **Packet Replay** - Retransmission for network testing

### ❌ What ESP32 Cannot Do:

1. **Cryptographic Brute Force** - AES keyspace too large (2^128 combinations)
2. **Long-Term Storage** - Flash limited to 8-16 MB (use SD card or PC streaming)
3. **Complex Graph Analysis** - RAM/CPU insufficient for large-scale network mapping
4. **Full Protocol Implementation** - Can't act as mesh router/forwarder
5. **Multi-Frequency Simultaneous Operations** - Single radio limits to sequential scanning

### 🎯 Optimal Architecture:

**ESP32 as field sensor + PC for analysis:**
- ESP32 captures RF data (packets, RSSI, timing)
- Streams to laptop/server via serial or WiFi
- PC performs heavy lifting: database storage, graph analysis, pattern correlation
- Think: Remote probe, not standalone platform

---

**Last Updated**: December 21, 2025  
**Version**: 2.2.1 Production  
**Status**: Ready for security research and RF experimentation
