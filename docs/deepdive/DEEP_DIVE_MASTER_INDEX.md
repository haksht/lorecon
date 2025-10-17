# ESP32 LoRa Sniffer - Deep Dive Master Index

## Overview

This repository contains **11 comprehensive deep-dive documents** covering every aspect of the ESP32 LoRa reconnaissance tool. Each document provides conference-level technical understanding of a specific subsystem.

**Total Documentation: ~15,000 lines of technical content** 📚

---

## Core System Architecture (Documents 1-5)

### 1. [Packet Reception](DEEP_DIVE_PACKET_RECEPTION.md)
**1,110 lines** | ISR mechanics, atomic flags, FreeRTOS queues

- ✅ Interrupt Service Routine (ISR) fundamentals
- ✅ Atomic flag patterns for thread safety
- ✅ FreeRTOS queue buffering (10-packet capacity)
- ✅ Watchdog timer (30-second timeout)
- ✅ Timing analysis: ISR→Queue→Processing pipeline

**Key Concepts:** `IRAM_ATTR`, `std::atomic<bool>`, `xQueueSendFromISR()`

---

### 2. [AES-CTR Decryption](DEEP_DIVE_AES_CTR_DECRYPTION.md)
**1,200+ lines** | Encryption fundamentals, PSK testing

- ✅ AES-128/256-CTR counter mode encryption
- ✅ Nonce construction (packet ID + node ID)
- ✅ PSK (Pre-Shared Key) testing framework
- ✅ 28 keys × 3 nonces = 84 tests (96% success rate)
- ✅ Tiny-AES-C library integration

**Key Concepts:** Counter mode, IV/nonce, keystream, XOR decryption

---

### 3. [Protocol Analysis](DEEP_DIVE_PROTOCOL_ANALYSIS.md)
**1,300+ lines** | Protobuf wire format, Meshtastic packets

- ✅ Protobuf wire format (varints, field tags, wire types)
- ✅ Meshtastic packet structure (header + encrypted payload)
- ✅ GPS coordinate extraction (1e-7 degrees precision)
- ✅ Field parsing without full Protobuf library
- ✅ Text message, position, and telemetry packets

**Key Concepts:** Varint encoding, zigzag encoding, field tags (type:number)

---

### 4. [State Machine](DEEP_DIVE_STATE_MACHINE.md)
**1,100+ lines** | Operation modes, transitions, timing

- ✅ 4 operation modes: SCAN, FIXED_FREQ, CHANNEL_HOP, PSK_TEST
- ✅ State transition logic and timing (12-second dwell)
- ✅ Configuration management (frequency, SF, bandwidth)
- ✅ Error recovery and mode validation
- ✅ Scheduling strategy for frequency sweeps

**Key Concepts:** State pattern, dwell timing, transition guards

---

### 5. [Error Handling](DEEP_DIVE_ERROR_HANDLING.md)
**1,060 lines** | Watchdog, recovery, diagnostics

- ✅ Hardware watchdog timer (ESP32 Task WDT)
- ✅ 30-second timeout with automatic reset
- ✅ Error recovery strategies (graceful degradation)
- ✅ Diagnostic logging and error codes
- ✅ RadioLib error interpretation

**Key Concepts:** `esp_task_wdt_init()`, `esp_task_wdt_add()`, graceful degradation

---

## Advanced Features (Documents 6-8)

### 6. [Display System](DEEP_DIVE_DISPLAY_SYSTEM.md)
**1,240 lines** | OLED, I2C, U8g2, rendering

- ✅ SSD1306 OLED display (128×64 pixels)
- ✅ I2C protocol (100 kHz, GPIO 17/18)
- ✅ U8g2 library architecture (buffer, font rendering)
- ✅ Double-buffering for flicker-free updates
- ✅ 4 display modes (stats, signal, scan, diagnostic)

**Key Concepts:** I2C start/stop conditions, ACK/NACK, display buffer, page mode

---

### 7. [Session Key Harvesting](DEEP_DIVE_SESSION_KEY_HARVESTING.md)
**1,450 lines** | Two-layer encryption, active/passive

- ✅ Two-layer encryption (channel PSK + session keys)
- ✅ Passive key harvesting (intercept ADMIN_APP packets)
- ✅ Active key harvesting (send key request packets)
- ✅ Key storage and management (multiple sessions)
- ✅ Ethical considerations and detection risks

**Key Concepts:** Session keys, ADMIN_APP port, key rotation, replay attacks

---

### 8. [Hardware Abstraction](DEEP_DIVE_HARDWARE_ABSTRACTION.md)
**900+ lines** | SPI protocol, RadioLib, SX1262

- ✅ SPI protocol mechanics (MOSI, MISO, SCK, NSS)
- ✅ RadioLib 4-layer architecture (Module → SX1262 → App)
- ✅ SX1262 radio chip internals (registers, commands)
- ✅ Pin assignments (NSS=8, DIO1=14, RST=12, BUSY=13)
- ✅ Interrupt handling (DIO1 packet received)

**Key Concepts:** SPI Mode 0, BUSY wait, burst transfers, Module class

---

## Validation & Tools (Documents 9-11)

### 9. [Testing Strategy](DEEP_DIVE_TESTING_STRATEGY.md)
**800+ lines** | Validation, diagnostics, debugging

- ✅ PSK unit tests (84 test cases, 96% pass rate)
- ✅ Hardware stress testing framework
- ✅ Boot diagnostics and health checks
- ✅ Field testing procedures
- ✅ Debug logging (conditional compilation)

**Key Concepts:** Test pyramid, PSK validation, watchdog monitoring

---

### 10. [Hardware Stress Testing](DEEP_DIVE_HARDWARE_STRESS_TESTING.md)
**1,800+ lines** | Device resilience, thermal monitoring

- ✅ 7 stress test types (frequency sweep, power ramp, etc.)
- ✅ Safety limits (900-930 MHz, max 30 dBm, 65°C limit)
- ✅ 30-second mandatory cooldown between tests
- ✅ Temperature monitoring (ESP32 internal sensor)
- ✅ T-Deck targeted vulnerability assessment

**Key Concepts:** Thermal stress, rapid config changes, safety monitoring

---

### 11. [Geographic Intelligence & Python Tools](DEEP_DIVE_GEO_INTELLIGENCE_AND_TOOLS.md)
**1,600+ lines** | GPS extraction, log analysis, visualization

#### Part 1: Geographic Intelligence
- ✅ GPS coordinate extraction from Protobuf
- ✅ Integer coordinates (1e-7 degrees precision)
- ✅ KML export (Google Earth)
- ✅ GeoJSON export (web mapping)
- ✅ Ring buffer storage (50 positions)

#### Part 2: Python Tools
- ✅ `analyze_logs.py` - Post-capture statistical analysis
- ✅ `live_visualizer.py` - Real-time serial monitoring
- ✅ 4-panel dashboard (RSSI, devices, histogram, summary)
- ✅ Conference demonstration ready

**Key Concepts:** Zigzag encoding, varint decoding, JSONL logs, matplotlib visualization

---

## Quick Reference

### By Topic

**Hardware:**
- Packet Reception (#1), Hardware Abstraction (#8), Hardware Stress Testing (#10)

**Encryption:**
- AES-CTR Decryption (#2), Session Key Harvesting (#7)

**Protocols:**
- Protocol Analysis (#3), Geographic Intelligence (#11)

**System Design:**
- State Machine (#4), Error Handling (#5)

**User Interface:**
- Display System (#6)

**Validation:**
- Testing Strategy (#9), Hardware Stress Testing (#10)

**Tools:**
- Python Tools (#11)

---

### By Difficulty Level

**Beginner (Start Here):**
1. State Machine (#4) - Understand overall operation
2. Display System (#6) - Visual feedback system
3. Testing Strategy (#9) - Validation approach

**Intermediate:**
1. Packet Reception (#1) - ISR and queues
2. Protocol Analysis (#3) - Protobuf parsing
3. Error Handling (#5) - Watchdog and recovery

**Advanced:**
1. AES-CTR Decryption (#2) - Cryptography
2. Hardware Abstraction (#8) - SPI and RadioLib
3. Session Key Harvesting (#7) - Advanced attacks

**Expert:**
1. Hardware Stress Testing (#10) - Thermal/safety limits
2. Geographic Intelligence (#11) - GPS extraction + tools

---

## Reading Paths

### Path 1: "How Does It Work?" (Functional Understanding)

```
1. State Machine → Understand modes and operation
2. Packet Reception → See how packets are captured
3. Protocol Analysis → Learn how packets are parsed
4. Display System → Understand user feedback
```

**Time:** ~4 hours | **Level:** Beginner-Intermediate

---

### Path 2: "Security Research" (Attack Techniques)

```
1. Protocol Analysis → Understand Meshtastic packets
2. AES-CTR Decryption → Learn encryption basics
3. Session Key Harvesting → Advanced decryption
4. Geographic Intelligence → Track device locations
```

**Time:** ~5 hours | **Level:** Intermediate-Advanced

---

### Path 3: "Hardware Deep Dive" (Low-Level Understanding)

```
1. Hardware Abstraction → SPI and RadioLib
2. Packet Reception → ISR and interrupts
3. Display System → I2C protocol
4. Hardware Stress Testing → Safety and limits
```

**Time:** ~4 hours | **Level:** Advanced

---

### Path 4: "Complete System" (Conference Preparation)

```
Read all 11 documents in order.
```

**Time:** ~12-15 hours | **Level:** All levels

**Result:** Conference-level understanding of entire system

---

## Code Location Reference

### Primary Firmware Files

| File | Lines | Purpose |
|------|-------|---------|
| `main.cpp` | ~400 | Setup, main loop, ISR registration |
| `lora_recon_tool.cpp` | ~800 | Core reconnaissance logic, state machine |
| `psk_decryption_simple.cpp` | ~400 | AES-CTR decryption, PSK testing |
| `protocol_analyzer.cpp` | ~600 | Protobuf parsing, packet dissection |
| `oled_display.cpp` | ~500 | Display rendering, 4 modes |
| `error_handler.cpp` | ~300 | Watchdog, error recovery |
| `session_key_manager.cpp` | ~500 | Session key harvesting, storage |
| `geo_intelligence.cpp` | ~400 | GPS extraction, KML/GeoJSON export |
| `hardware_stress_tester.cpp` | ~700 | 7 stress tests, safety monitoring |

**Total Firmware:** ~4,600 lines of C++ code

---

### Python Tools

| File | Lines | Purpose |
|------|-------|---------|
| `tools/analyze_logs.py` | ~350 | Statistical analysis, CSV export, plots |
| `tools/live_visualizer.py` | ~450 | Real-time serial monitoring, 4-panel dashboard |

**Total Tools:** ~800 lines of Python code

---

## Documentation Statistics

| Document | Lines | Topics | Code Examples |
|----------|-------|--------|---------------|
| 1. Packet Reception | 1,110 | 8 | 25 |
| 2. AES-CTR Decryption | 1,200 | 9 | 30 |
| 3. Protocol Analysis | 1,300 | 10 | 35 |
| 4. State Machine | 1,100 | 8 | 20 |
| 5. Error Handling | 1,060 | 7 | 18 |
| 6. Display System | 1,240 | 9 | 28 |
| 7. Session Key Harvesting | 1,450 | 11 | 32 |
| 8. Hardware Abstraction | 900 | 7 | 24 |
| 9. Testing Strategy | 800 | 8 | 22 |
| 10. Hardware Stress Testing | 1,800 | 6 | 35 |
| 11. Geo Intelligence & Tools | 1,600 | 12 | 40 |
| **TOTAL** | **~14,560** | **95** | **309** |

---

## Learning Outcomes

After completing all 11 deep dives, you will understand:

### Hardware
- ✅ ESP32-S3 architecture (CPU, peripherals, interrupts)
- ✅ SX1262 LoRa radio chip (registers, commands, modes)
- ✅ SPI protocol (timing, transactions, modes)
- ✅ I2C protocol (addressing, ACK/NACK, timing)
- ✅ GPIO interrupts (DIO1, atomic flags, ISR)
- ✅ Thermal monitoring (internal sensor, safety limits)

### Protocols
- ✅ LoRa physical layer (SF, BW, CR, frequency)
- ✅ Meshtastic packet structure (header, encryption, payload)
- ✅ Protobuf wire format (varints, field tags, wire types)
- ✅ GPS coordinate encoding (1e-7 degrees, zigzag)

### Cryptography
- ✅ AES-128/256 encryption (block cipher)
- ✅ CTR mode (counter, keystream, XOR)
- ✅ Nonce construction (packet ID, node ID)
- ✅ PSK (Pre-Shared Key) management
- ✅ Session keys (two-layer encryption)

### Software Architecture
- ✅ State machines (4 modes, transitions)
- ✅ FreeRTOS queues (ISR → main loop)
- ✅ Atomic operations (thread safety)
- ✅ Watchdog timers (timeout, recovery)
- ✅ Error handling (graceful degradation)
- ✅ Double-buffering (display updates)

### Testing & Validation
- ✅ Unit testing (PSK tests, 84 cases)
- ✅ Hardware stress testing (7 test types)
- ✅ Safety monitoring (thermal, stability)
- ✅ Field testing procedures
- ✅ Debug logging strategies

### Tools & Analysis
- ✅ Python log analysis (statistics, visualization)
- ✅ Real-time monitoring (serial parsing)
- ✅ KML/GeoJSON export (mapping)
- ✅ Conference demonstration techniques

---

## Next Steps

### 1. Hands-On Experimentation

```bash
# Build and flash
pio run --target upload

# Monitor serial output
pio device monitor

# Run live visualizer
python tools/live_visualizer.py COM3

# Analyze logs
python tools/analyze_logs.py packets.jsonl --plot
```

### 2. Modify and Extend

**Easy Modifications:**
- Change frequency scan range (edit configs in state machine)
- Adjust dwell timing (12s → custom value)
- Add more PSK keys (psk_tests.h)
- Customize display layout (oled_display.cpp)

**Intermediate Modifications:**
- Add new state machine modes (e.g., ADAPTIVE_SF)
- Implement packet filtering (by node ID, type)
- Add SD card logging (save JSONL to file)
- Expand stress tests (new test types)

**Advanced Modifications:**
- Implement active transmission (beacon, mesh join)
- Add WiFi/BLE interfaces (remote control)
- Machine learning (classify packets, predict channels)
- Implement full Meshtastic protocol (become mesh node)

### 3. Present Your Work

**You now have enough knowledge to:**
- ✅ Present at DEF CON, Black Hat, or local security meetups
- ✅ Write technical blog posts or whitepapers
- ✅ Teach workshops on LoRa security
- ✅ Contribute to Meshtastic security research
- ✅ Extend to other LoRa protocols (LoRaWAN, Helium)

---

## Additional Resources

### Conference Guides

Located in `conference/` directory:

- `README.md` - Conference presentation guide
- `README_SECURITY.md` - Ethical and legal considerations
- `SESSION_KEY_HARVESTING_GUIDE.md` - Advanced techniques
- `HARDWARE_STRESS_TESTING_GUIDE.md` - Safety procedures
- `TEXT_MESSAGE_COMPLETE_GUIDE.md` - Message interception
- `tdeck_port_guide.md` - T-Deck specific targeting

### Documentation

Located in `docs/` directory:

- `architecture.md` - System overview
- `BUILD_GUIDE.md` - Compilation instructions
- `FEATURES.md` - Feature list
- `TROUBLESHOOTING_MESHTASTIC.md` - Common issues
- `PSK_DECRYPTION_TROUBLESHOOTING.md` - Decryption debugging
- `RSSI_EXPLAINED.md` - Signal strength interpretation

---

## Credits

**Hardware:**
- ESP32-S3: Espressif Systems
- SX1262: Semtech Corporation
- Heltec WiFi LoRa 32 V3: Heltec Automation

**Libraries:**
- RadioLib: Jan Gromeš (@jgromes)
- U8g2: Oliver Kraus (@olikraus)
- Tiny-AES-C: kokke
- Meshtastic: Meshtastic Project

**Documentation:**
- Deep Dive Series: Generated for ESP32-Sniffer project
- Last Updated: 2025-10-16

---

## Mission Accomplished 🎉

**From "it works" to "I understand why and how it works"**

You now have:
- ✅ **11 comprehensive deep-dive documents** (~15,000 lines)
- ✅ **Conference-level technical understanding** (all subsystems)
- ✅ **Complete code walkthrough** (4,600 lines C++, 800 lines Python)
- ✅ **Practical tools** (analysis, visualization)
- ✅ **Testing framework** (validation, stress testing)
- ✅ **Ethical guidelines** (responsible disclosure)

**You are ready to present, teach, and extend this system with confidence!**

---

*ESP32 LoRa Sniffer Deep Dive Master Index*  
*Generated: 2025-10-16*  
*Total Documentation: ~15,000 lines across 11 documents*
