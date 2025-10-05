# Feature Documentation

**ESP32 LoRa Reconnaissance Tool v1.7**

## 🎯 Tool Identity

**Core Mission**: Recon → Sniff → Capture → Replay

A focused LoRa packet capture and analysis tool for security research and RF experimentation. Designed for DefCon-quality demonstrations of LoRa security assessment techniques.

---

## ✅ Implemented Features

### 🔍 Core Reconnaissance (ESSENTIAL)
**Status**: ✅ Production Ready  
**Build Flag**: Always enabled  
**Files**: `lora_recon_tool.cpp`, `recon_state.cpp`

- **16 scan configurations** (Meshtastic, LoRaWAN, ISM band)
- **Sequential frequency scanning** (12s dwell time per config)
- **RF activity monitoring** (RSSI tracking, peak detection)
- **Device enumeration** (node IDs, signal strength, device types)
- **Protocol detection** (Meshtastic, LoRaWAN, generic)
- **Interactive targeting** (select discovered devices for focused capture)

**What it does**: Automatically scans 16 LoRa configurations to discover active devices in your area, tracks signal strength, and lets you target specific devices for deeper analysis.

---

### 📡 Packet Capture & Analysis (ESSENTIAL)
**Status**: ✅ Production Ready  
**Build Flag**: Always enabled  
**Files**: `protocol_analyzer.cpp`, `lora_recon_tool.cpp`

- **Real-time packet capture** (interrupt-driven, <50ms latency)
- **Meshtastic header parsing** (node IDs, hop counts, routing flags)
- **LoRaWAN frame analysis** (message types, DevAddr extraction)
- **Device fingerprinting** (firmware version estimation, power class)
- **Router detection** (identifies mesh routing devices)
- **Signal analysis** (RSSI, SNR, frequency error)

**What it does**: Captures and analyzes LoRa packets in real-time, extracting intelligence like node IDs, device types, firmware versions, and network topology information.

---

### 🔄 Packet Replay (ESSENTIAL)
**Status**: ✅ Production Ready  
**Build Flag**: Always enabled  
**Files**: `lora_recon_tool.cpp`, `recon_state.cpp`

- **10 replay slots** (256 bytes each)
- **Interactive capture** (press 'c' during targeted mode)
- **Replay menu** (list, select, transmit captured packets)
- **Configuration preservation** (replays with original radio settings)
- **Metadata tracking** (original RSSI, timestamp, protocol)

**What it does**: Lets you save interesting packets during capture and retransmit them later for testing or demonstration purposes.

---

### 🔓 PSK Decryption Testing (SECURITY RESEARCH)
**Status**: ✅ Code Complete, ⏳ Awaiting Live Traffic Validation  
**Build Flag**: `-DENABLE_PSK_TESTING`  
**Files**: `psk_decryption_simple.cpp`, `psk_tests.h`

- **5 default Meshtastic PSKs** (AQ==, 1PG...HivQY=, common defaults)
- **AES-256 decryption** (Meshtastic encryption standard)
- **Automated test suite** (Base64 parsing, PSK loading, decryption logic)
- **Message extraction** (pulls plaintext from decrypted packets)
- **Success statistics** (tracks which keys work)

**What it does**: Tests captured Meshtastic packets against 5 common default encryption keys. When a match is found, decrypts the packet to reveal message content and GPS data.

**Current Status**: Code implemented and tested with synthetic data. Waiting for multi-node mesh with encrypted traffic to validate against real-world packets.

---

### 📍 Geographic Intelligence (BONUS)
**Status**: ✅ Production Ready  
**Build Flag**: Always enabled  
**Files**: `geo_intelligence.cpp`

- **GPS coordinate extraction** (latitude, longitude, altitude)
- **Meshtastic position packet parsing** (protobuf decoding)
- **50 position history** (tracks device movements over time)
- **KML export** (Google Earth compatible)
- **GeoJSON export** (web mapping tools like Leaflet, Mapbox)
- **Node position tracking** (associates GPS with node IDs)

**What it does**: Automatically extracts GPS coordinates from Meshtastic position packets and lets you export them for mapping in Google Earth or web-based maps.

---

### ⚡ Hardware Stress Testing (ATTACK SURFACE ANALYSIS)
**Status**: ✅ Production Ready  
**Build Flag**: `-DENABLE_STRESS_TESTING`  
**Files**: `hardware_stress_tester.cpp`

- **7 stress test suites**:
  1. T-Deck targeted assessment
  2. Frequency sweep validation
  3. Power ramp testing
  4. Parameter boundary testing
  5. Rapid configuration changes
  6. Memory integrity validation
  7. Thermal monitoring
- **Real ESP32 temperature monitoring**
- **Safety limits** (thermal shutdown, power limits)
- **Automated test reports**
- **Hardware vulnerability research**

**What it does**: Tests the radio hardware's limits to identify potential attack vectors and vulnerabilities. Useful for understanding where hardware might fail under stress or malicious configuration.

**DefCon Value**: Demonstrates professional hardware security assessment methodology.

---

### 🛡️ Production-Grade Reliability (ESSENTIAL)
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

---

### 🎨 User Interface (ESSENTIAL)
**Status**: ✅ Production Ready  
**Build Flag**: Always enabled  
**Files**: `user_interface.cpp`, `ui_components.cpp`

- **Interactive menu system** (press 'm' to see options)
- **Device selection** (numbered list, pick target)
- **RF activity visualization** (frequency heatmap in serial output)
- **Security assessment** (vulnerability scoring for discovered devices)
- **Device type analysis** (breakdown by device type, power class)
- **Real-time statistics** (packet counts, uptime, error rates)

**What it does**: Provides a clean, interactive interface via serial terminal for controlling the tool and viewing results.

---

## ❌ Features NOT Included (By Design)

### Intelligence Storage / Session Management
**Why Not**: Adds complexity without clear benefit for a focused recon tool. Current LittleFS packet logging is sufficient.

**What We Have Instead**: Real-time packet logging, 10 in-memory replay slots, KML/GeoJSON export.

### Network Topology Mapping
**Why Not**: Out of scope. Tool is about packet capture, not network graphing.

**What We Have Instead**: Device enumeration, fingerprinting, and router detection gives you enough network intelligence.

### Traffic Analysis Module
**Why Not**: Overlaps with existing packet capture and protocol analysis. Adds bloat.

**What We Have Instead**: Protocol analyzer provides all needed packet intelligence extraction.

### Web Interface
**Why Not**: Deferred to post-conference. Serial interface is sufficient for demo and most use cases.

**Future Consideration**: If there's community demand after DefCon talk, could add web UI.

---

## 🔧 Build Flag Reference

### Active Flags (Default Build)
```ini
-DENABLE_PSK_TESTING           # AES-256 decryption with 5 default keys
-DENABLE_STRESS_TESTING        # Hardware validation framework
-DPRODUCTION_BUILD            # Production error handling
-DERROR_LOGGING_ENABLED       # Comprehensive error recovery
```

### Legacy/Removed Flags
```ini
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
- ✅ PSK decryption (once validated with live traffic)
- ✅ Geographic intelligence (GPS mapping)
- ✅ Protocol analysis (device fingerprinting)
- ✅ Hardware stress testing (attack surface analysis)

### NICE TO HAVE (Polish)
- ✅ KML/GeoJSON export
- ✅ Security assessment scoring
- ✅ Device type analysis
- ⏳ Live visualization (Python tool in development)

### OUT OF SCOPE
- ❌ Persistent session storage
- ❌ Network topology graphs
- ❌ Web interface (deferred)
- ❌ SD card logging (use LittleFS)

---

## 🎯 Conference Demo Focus

### What We'll Demonstrate
1. **Reconnaissance Phase** - Discover nearby LoRa devices (2-3 minutes)
2. **Device Targeting** - Lock onto specific device for capture
3. **Packet Replay** - Capture and retransmit packets
4. **PSK Exploitation** - Decrypt default-key Meshtastic networks
5. **Geographic Intelligence** - Show GPS positions on map
6. **Hardware Assessment** - Run stress tests to find vulnerabilities

### What We Won't Demonstrate
- Session management (not implemented)
- Network topology graphs (removed)
- Web interface (deferred)
- Advanced traffic analysis (out of scope)

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

## 🚀 Roadmap (DefCon Timeline)

### Week 1-2: PSK Validation ⏳
- [ ] Get multi-node mesh operational
- [ ] Capture encrypted user messages
- [ ] Validate PSK decryption with live traffic
- [ ] Document successful decryption for demo

### Week 3-4: Demo Polish ✅
- [ ] Create Python live visualizer
- [ ] Write demo script with backup plans
- [ ] Test full demo flow 10+ times
- [ ] Record backup video

### Week 5-6: Documentation 📚
- [ ] Write security assessment methodology
- [ ] Create defensive hardening guide
- [ ] Document vulnerabilities found
- [ ] Prepare slide deck

### Week 7-8: Conference Prep 🎤
- [ ] Submit abstract/proposal
- [ ] Build demo hardware (2x setups)
- [ ] Print handouts
- [ ] Final rehearsals

---

**Last Updated**: October 4, 2025  
**Version**: 1.7 Production  
**Status**: Ready for DefCon preparation phase
