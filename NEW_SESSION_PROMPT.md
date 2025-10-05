# New Session Context - ESP32 LoRa Reconnaissance Tool

**Version:** 1.7 Production  
**Date:** October 4, 2025  
**Status:** ✅ Fully Operational

---

## Project Overview

Production-ready ESP32-S3 LoRa packet sniffer and reconnaissance tool for security research. Scans ISM band (902-928 MHz), captures Meshtastic/LoRaWAN packets, and provides geographic intelligence with KML/GeoJSON export.

**Hardware:** Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262)  
**Primary Use:** Security research, mesh network analysis, educational demonstrations

---

## Current System Status

### ✅ Working Features
1. **Multi-frequency reconnaissance** - 16 LoRa configurations
2. **Device targeting** - Lock onto specific nodes for capture
3. **PSK decryption testing** - 5 default Meshtastic keys with automated test suite
4. **Geographic intelligence** - GPS extraction from position packets
5. **KML/GeoJSON export** - Map device locations
6. **Packet replay** - 10-slot capture and retransmit
7. **Hardware stress testing** - Real ESP32 temperature monitoring
8. **Security assessment** - Vulnerability scoring for mesh networks

### Build Status
- ✅ Compiles with zero warnings/errors
- ✅ All PSK tests passing (4/4)
- ✅ Hardware validated on COM3
- ✅ Command handler operational (fixed '8' command priority)

---

## Architecture Summary

### Core Modules (firmware/src/)
```
main.cpp (35 lines)
├── lora_recon_tool.cpp/h (770 lines) - Radio control, mode handlers
├── recon_state.cpp/h (600 lines) - State management, device tracking
├── protocol_analyzer.cpp/h (300 lines) - Packet classification
├── command_handler.cpp/h (250 lines) - O(1) command dispatch
├── error_handler.cpp/h (400 lines) - Production error recovery
├── user_interface.cpp/h (800 lines) - Menu system
├── hardware_stress_tester.cpp/h (500 lines) - Stress testing with real temp
├── psk_decryption_simple.cpp/h (380 lines) - PSK testing
├── psk_tests.h (100 lines) - Automated test suite
├── geo_intelligence.cpp/h (450 lines) - GPS extraction, KML/GeoJSON
├── ui_components.cpp/h (250 lines) - Modular display functions
└── data_structures.h (150 lines) - Shared structs
```

### Key Design Patterns
- **Command Pattern** - O(1) dispatch table
- **State Management** - Centralized in ReconState class
- **ISR Safety** - std::atomic<bool> with memory ordering
- **Error Recovery** - Automatic radio/memory/state recovery
- **Modular Components** - Clean separation of concerns

---

## Key Technical Details

### Radio Configuration
- **Frequency Range:** 902-928 MHz (US ISM band)
- **Radio IC:** SX1262 (via RadioLib 6.4.2)
- **Pin Configuration:**
  - NSS=8, DIO1=14, RST=12, BUSY=13
  - SPI: SCK=9, MISO=11, MOSI=10
- **Primary Frequencies:**
  - 906.875 MHz (Meshtastic Long Fast, SF11)
  - 915.375 MHz (Meshtastic SF8, encrypted messages)
  - 903.875 MHz (Meshtastic Medium Fast, SF10)

### Scan Configurations (16 total)
1. **Meshtastic US** (6 configs): 902-927 MHz, SF7-SF12, BW125/250
2. **LoRaWAN US** (4 configs): 903-905 MHz, SF7-10, BW125
3. **Generic ISM** (6 configs): Various test frequencies

### Memory Layout
- **50 node tracking slots** (NodeInfo structs)
- **20 targetable device slots** (TargetableDevice structs)
- **10 packet replay slots** (256 bytes each)
- **50 GPS position slots** (GeoPoint structs)
- **Fixed allocations** - No heap fragmentation

---

## Command Reference

### Device Operations
- **`1-9`** - Target device by number (8 is reserved for SF8 command)
- **`m`** - Show menu and results
- **`r`** - Restart reconnaissance
- **`f`** - Frequency targeting mode
- **`8`** - Quick target SF8 (encrypted messages)

### Analysis Commands
- **`a`** - RF activity details
- **`d`** - Device type breakdown
- **`v`** - Security assessment
- **`s`** - Show summary

### Geographic Intelligence (NEW in v1.7)
- **`g`** - GPS position summary
- **`k`** - Export KML (Google Earth)
- **`j`** - Export GeoJSON (web mapping)

### Packet Operations
- **`c`** - Capture packet for replay
- **`p`** - Packet replay menu

### Testing
- **`t`** - Hardware stress testing menu

---

## Recent Changes (v1.7)

### New Features
1. **Geographic Intelligence Module** (`geo_intelligence.cpp/h`)
   - Automatic GPS extraction from Meshtastic position packets
   - Protobuf parsing for lat/lon/altitude
   - KML export for Google Earth visualization
   - GeoJSON export for web mapping tools
   - 50-point storage buffer

2. **PSK Test Suite** (`psk_tests.h`)
   - 4 automated tests (Base64, packet detection, PSK loading, message extraction)
   - Runs at startup when ENABLE_PSK_TESTING defined
   - 100% pass rate validation

3. **Simplified Stress Testing** (`hardware_stress_tester.cpp`)
   - Real ESP32 temperature monitoring via temperatureRead()
   - Removed device profiling complexity
   - Generic stress test framework

4. **UI Components** (`ui_components.cpp/h`)
   - Extracted modular display functions
   - Prepared for web app JSON API
   - displayDeviceList(), displayRFActivity(), displaySecurityScores()

5. **Intelligence Storage**
   - ENABLE_INTELLIGENCE_STORAGE flag enabled
   - Extended packet analysis capabilities

### Bug Fixes
- Fixed command '8' priority (SF8 command vs device #8 selection)
- Fixed PSKStats memset warnings (using constructor now)
- Fixed unused variable warnings in stress tester

---

## Build & Flash

### Compile and Upload
```bash
# Default build (research platform with all features)
pio run -e default --target upload

# Monitor serial output
pio device monitor

# Or combined
pio run -e default --target upload && pio device monitor
```

### Build Flags (platformio.ini)
```ini
-DENABLE_PSK_TESTING          # PSK decryption with test suite
-DENABLE_STRESS_TESTING       # Hardware validation framework
-DENABLE_INTELLIGENCE_STORAGE # Extended packet analysis
```

---

## Common Tasks

### Capturing GPS Data from Meshtastic
```bash
# On your Meshtastic device (COM3)
meshtastic --port COM3 --sendposition

# On ESP32 sniffer, you'll see:
# 📍 GPS POSITION EXTRACTED!
#    Node: 0x401ACD4E
#    Lat:  37.774900° N
#    Lon:  122.419400° W
```

Then press `g` to see summary, `k` to export KML, or `j` to export GeoJSON.

### Targeting Encrypted Messages
1. Press `8` to target SF8 frequency (915.375 MHz)
2. System will attempt PSK decryption on every packet
3. Statistics shown in PSK testing output
4. If default key found, plaintext message displayed

### Device Profiling
1. Let reconnaissance scan complete (~3 minutes)
2. Press `m` to see discovered devices
3. Press device number (1-9) to target
4. Press `c` to capture interesting packets
5. Press `p` to replay captured packets

---

## Python Tools

### Log Analysis
```bash
cd tools
python analyze_logs.py ../data/packets.json
```

### Live Visualization
```bash
python live_visualizer.py COM3
```
Real-time RSSI graphs, packet histograms, device dashboard

---

## Known Issues & Limitations

### Current Limitations
- **GPS Capture:** Position packets sent infrequently (every 15-30 min)
- **PSK Success Rate:** Low on production networks (most use custom keys)
- **Range:** 100m-10km depending on environment and antenna
- **Temperature Accuracy:** ±5°C (relative changes more reliable than absolute)

### Not Issues (Working as Designed)
- SF8 targeting shows "no devices" initially - this is normal, it's listening for new packets
- Command '8' takes priority over device #8 - intentional for quick SF8 access
- PSK tests may show 0 successes - means no default keys detected (good security!)

---

## File Organization

### Root Directory
```
README.md                    - Main documentation
PROJECT_STATUS_FINAL.md      - Technical status and metrics
BUILD_GUIDE.md              - Compilation instructions
TROUBLESHOOTING_MESHTASTIC.md - Meshtastic-specific issues
SECURITY_AUDIT.md           - Security assessment
platformio.ini              - Build configuration
```

### Important Source Files
```
firmware/src/
  main.cpp                  - Entry point (35 lines)
  lora_recon_tool.*        - Main reconnaissance engine
  geo_intelligence.*       - GPS extraction (NEW v1.7)
  psk_tests.h              - Test suite (NEW v1.7)
  ui_components.*          - Display modules (NEW v1.7)
  data_structures.h        - Shared structs with GeoPoint
```

### Python Tools
```
tools/
  analyze_logs.py          - Offline log analysis
  live_visualizer.py       - Real-time visualization
  requirements.txt         - Python dependencies
```

---

## Quick Reference - What Works Right Now

✅ **Radio scanning** - All 16 configurations operational  
✅ **Device detection** - Meshtastic nodes discovered and tracked  
✅ **Protocol analysis** - Meshtastic/LoRaWAN identification  
✅ **PSK testing** - 5 default keys, automated test suite  
✅ **GPS extraction** - Position packets decoded automatically  
✅ **KML/GeoJSON export** - Working with captured GPS data  
✅ **Packet replay** - Capture and retransmit functionality  
✅ **Stress testing** - Real ESP32 temperature monitoring  
✅ **Command handler** - All commands operational including new 'g/k/j'  
✅ **Error recovery** - Watchdog and automatic recovery active  

---

## For AI Assistants

### Code Quality
- **9/10 rating** - Production-grade embedded code
- Zero compiler warnings with -Wall -Wextra -Werror
- Clean architecture with Command Pattern, State Management
- ISR-safe with std::atomic and memory ordering
- Comprehensive error handling with automatic recovery

### When Helping
1. **Preserve existing architecture** - Command Pattern, State Management work well
2. **Maintain ISR safety** - std::atomic<bool> packetReceived pattern is correct
3. **Keep memory fixed** - No dynamic allocation, everything pre-allocated
4. **Test after changes** - Always compile with `pio run -e default`
5. **Consider hardware limits** - ESP32 has limited RAM/flash

### Common Requests
- "Add new scan frequency" → Edit data_structures.h scanConfigs array
- "Change dwell time" → Modify SCAN_DWELL_TIME in lora_recon_tool.h
- "Add new command" → Update command_handler.cpp dispatch table
- "Export new format" → Add method to geo_intelligence.cpp

---

## Version History

- **v1.0-1.4** - Initial development and stabilization
- **v1.5** - Production hardening, live visualization, security assessment
- **v1.7** - Geographic intelligence, PSK test suite, simplified stress testing (current)

---

**System is stable, tested, and ready for security research and demonstrations.**
