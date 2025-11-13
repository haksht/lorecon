# ESP32 LoRa Sniffer - Deep Dive Master Index

**Last Updated:** November 12, 2025  
**Project Version:** 2.0 (Architecture Refactored)

## Overview

This document serves as a **master index and learning guide** for the ESP32 LoRa reconnaissance tool. It provides a structured approach to understanding the v2.0 architecture with its clean component separation (RadioController, PacketProcessor, IReconTool).

**Primary Documentation:**
- **[UNDERSTANDING_v2.md](UNDERSTANDING_v2.md)** (~7,000 lines) - Complete v2.0 architecture guide 🚀

---

## Core System Architecture (v2.0)

### Current Architecture Components

The v2.0 refactoring (November 2025) introduced clean separation of concerns:

#### **RadioController** (`radio_controller.cpp/.h`)
**~200 lines** | SX1262 hardware abstraction, interrupt handling

- ✅ Hardware initialization (SPI, pins, interrupts)
- ✅ Configuration management (frequency, bandwidth, spreading factor)
- ✅ Thread-safe packet reception (atomic flags)
- ✅ Signal quality metrics (RSSI, SNR) with caching
- ✅ Interrupt Service Routine (ISR) handling

**Key Concepts:** `IRAM_ATTR`, `std::atomic<bool>`, interrupt-driven reception

---

#### **PacketProcessor** (`packet_processor.cpp/.h`)
**~180 lines** | Queue management, protocol analysis coordination

- ✅ Packet queue management (max 10 packets, overflow protection)
- ✅ Protocol analysis coordination (Meshtastic, LoRaWAN)
- ✅ PSK decryption coordination (14 default keys)
- ✅ Node tracking and RF activity (via ReconState)
- ✅ Mode-specific packet handling (recon vs targeted)

**Key Concepts:** Queue management, protocol coordination, analysis pipeline

---

#### **IReconTool Interface** (`irecon_tool.h`)
**~30 lines** | Abstract interface for dependency inversion

- ✅ Pure virtual interface (dependency inversion principle)
- ✅ Breaks circular dependencies (CommandHandler ↔ LoRaReconTool)
- ✅ Enables unit testing with mock implementations
- ✅ Clean contract for reconnaissance operations

**Key Concepts:** Interface-based design, dependency inversion, testability

---

### Legacy Deep Dive Topics

The following topics are covered in UNDERSTANDING.md but reflect pre-v2.0 architecture:

### 1. Packet Reception
**Topic:** ISR mechanics, atomic flags, FreeRTOS queues

- ✅ Interrupt Service Routine (ISR) fundamentals
- ✅ Atomic flag patterns for thread safety
- ✅ Watchdog timer (30-second timeout)
- ✅ Timing analysis: ISR→Queue→Processing pipeline

**Status:** Core concepts remain valid; implementation now in RadioController

---

### 2. AES-CTR Decryption
**Topic:** Encryption fundamentals, PSK testing

- ✅ AES-128/256-CTR counter mode encryption
- ✅ Nonce construction (packet ID + node ID)
- ✅ PSK (Pre-Shared Key) testing framework (14 keys in v2.0)
- ✅ mbedtls library integration

**Status:** Valid; PSK count updated from 5 to 14 keys

---

### 3. Protocol Analysis
**Topic:** Protobuf wire format, Meshtastic packets

- ✅ Protobuf wire format (varints, field tags, wire types)
- ✅ Meshtastic packet structure (header + encrypted payload)
- ✅ GPS coordinate extraction (1e-7 degrees precision, wire type 0 and 5)
- ✅ Field parsing without full Protobuf library
- ✅ Text message, position, and telemetry packets

**Status:** Valid; GPS extraction fully implemented in v2.0

---

### 4. State Machine
**Topic:** Operation modes, transitions, timing

- ✅ Operation modes: reconnaissance and targeted capture
- ✅ State transition logic and timing (12-second dwell)
- ✅ Configuration management (frequency, SF, bandwidth)
- ✅ Error recovery and mode validation

**Status:** Valid; managed by LoRaReconTool + ReconState

---

### 5. Error Handling
**Topic:** Watchdog, recovery, diagnostics

- ✅ Hardware watchdog timer (ESP32 Task WDT)
- ✅ 30-second timeout with automatic reset
- ✅ Error recovery strategies (graceful degradation)
- ✅ Diagnostic logging and error codes
- ✅ RadioLib error interpretation

**Status:** Valid; error_handler.cpp provides production-grade reliability

---

## Removed Features (v2.0 Refactoring)

The following features were removed in the v2.0 architecture simplification (~2,100 lines):

### ❌ Attack/Stress Testing Framework (Removed Nov 11, 2025)
- `device_stress_tester.cpp/.h` (~1,400 lines) - External device testing
- `attack_scenarios.cpp` (~75 lines) - Attack scenario framework  
- `vulnerability_scanner.cpp` (~400 lines) - Vulnerability assessment
- `hardware_stress_tester.cpp/.h` (~650 lines) - Hardware stress testing

**Rationale:** Fundamental design flaw - cannot reliably test external devices without instrumentation. Removed to focus on core reconnaissance capabilities.

**See:** [ARCHITECTURE_REFACTOR_NOV11.md](../../ARCHITECTURE_REFACTOR_NOV11.md) for complete rationale

### ❌ Session Key Harvesting (Never Fully Implemented)
- `session_key_manager.cpp` - Two-layer encryption management
- Passive/active key harvesting techniques

**Rationale:** Meshtastic firmware 2.5.0+ (June 2024) uses Public Key Cryptography (Curve25519) for direct messages. Session key harvesting no longer applicable for modern firmware.

**See:** [ENCRYPTION_REALITY.md](../ENCRYPTION_REALITY.md) for encryption details

---

## Quick Reference

### By Topic

**v2.0 Architecture:**
- RadioController (hardware abstraction), PacketProcessor (queue + analysis), IReconTool (interface)

**Hardware:**
- Packet Reception, Hardware Abstraction, SPI/I2C protocols

**Encryption:**
- AES-CTR Decryption (14 PSKs), PSK testing framework

**Protocols:**
- Protocol Analysis, GPS extraction (wire type 0/5), Geographic Intelligence

**System Design:**
- State Machine (reconnaissance/targeted modes), Error Handling (watchdog)

**User Interface:**
- Display System (6 modes), OLED rendering, button control

**Tools:**
- Python Tools (pc_analyzer.py, analyze_logs.py, live_visualizer.py)

---

### Learning Path for v2.0

**Beginner (Start Here):**
1. Read [UNDERSTANDING_v2.md](UNDERSTANDING_v2.md) - **Complete v2.0 architecture guide with examples** 🚀
2. Read [README.md](../../README.md) - Understand v2.0 architecture overview
3. Read [ARCHITECTURE_REFACTOR_NOV11.md](../../ARCHITECTURE_REFACTOR_NOV11.md) - Phases 1-5 refactoring
4. Review source code: `radio_controller.cpp/.h`, `packet_processor.cpp/.h`, `irecon_tool.h`

**Intermediate:**
1. Study packet reception flow (RadioController ISR → PacketProcessor queue) in UNDERSTANDING_v2.md
2. Complete week 2 study guide (sections 5-9: components deep dive)
3. Review PSK decryption (14 keys, mbedtls integration) - section 12
4. Learn error handling and watchdog mechanisms - section 15

**Advanced:**
1. Deep dive into AES-CTR decryption - section 12
2. Study GPS coordinate extraction (protobuf wire types) - section 11
3. Complete self-test questions - section 20
4. Explore performance optimization - section 17

---

## Reading Paths

### Path 1: "Understanding v2.0 Architecture"

```
1. UNDERSTANDING_v2.md → Complete v2.0 guide with examples (START HERE) 🚀
2. README.md → Overview of v2.0 with component descriptions
3. ARCHITECTURE_REFACTOR_NOV11.md → Detailed refactoring documentation (Phases 1-5)
4. Review source code: radio_controller.cpp, packet_processor.cpp, irecon_tool.h
```

**Time:** ~4-5 hours | **Level:** Beginner-Intermediate

---

### Path 2: "Security Research & PSK Decryption"

```
1. ENCRYPTION_REALITY.md → Understand what can/cannot be decrypted
2. psk_decryption_simple.cpp → Study 14 PSK keys and AES-CTR implementation
3. geo_intelligence.cpp → GPS extraction from decrypted packets
4. FEATURES.md → Full PSK decryption capabilities
```

**Time:** ~2 hours | **Level:** Intermediate

---

### Path 3: "Hardware & Low-Level Understanding"

```
1. RadioController → SPI, SX1262 abstraction, interrupt handling
2. PacketProcessor → Queue management, analysis pipeline
3. UNDERSTANDING.md → ISR mechanics, atomic operations, watchdog timers
4. error_handler.cpp → Production-grade reliability
```

**Time:** ~3 hours | **Level:** Advanced

---

### Path 4: "Complete System Mastery"

```
Read all v2.0 documentation in order:
1. UNDERSTANDING_v2.md (complete guide with study path)
2. README.md
3. ARCHITECTURE_REFACTOR_NOV11.md
4. BUILD_GUIDE.md
5. FEATURES.md
6. Source code review (all components)
```

**Time:** ~10-12 hours | **Level:** All levels

**Result:** Complete understanding of v2.0 architecture and implementation with self-test questions

---

## Code Location Reference (v2.0)

### Primary Firmware Files

| File | Lines | Purpose |
|------|-------|---------|
| `main.cpp` | ~50 | Entry point, setup/loop |
| `lora_recon_tool.cpp/.h` | ~800 | Main orchestrator (implements IReconTool) |
| `radio_controller.cpp/.h` | ~200 | SX1262 hardware abstraction (NEW in v2.0) |
| `packet_processor.cpp/.h` | ~180 | Queue management and analysis (NEW in v2.0) |
| `irecon_tool.h` | ~30 | Abstract interface (NEW in v2.0) |
| `psk_decryption_simple.cpp/.h` | ~500 | AES-CTR decryption, 14 PSK testing |
| `protocol_analyzer.cpp/.h` | ~300 | Protobuf parsing, packet dissection |
| `geo_intelligence.cpp/.h` | ~250 | GPS extraction (wire type 0/5) |
| `oled_display.cpp/.h` | ~400 | Display driver and rendering (6 modes) |
| `user_interface.cpp/.h` | ~650 | Menu system and command routing |
| `command_handler.cpp/.h` | ~450 | Serial command processing |
| `ui_components.cpp/.h` | ~300 | Display component library |
| `recon_state.cpp/.h` | ~400 | State management, device tracking |
| `error_handler.cpp/.h` | ~400 | Watchdog, error recovery |
| `text_packet_diagnostic.cpp/.h` | ~200 | Packet timing and encryption analysis |
| `packet_logger.cpp/.h` | ~200 | SD card logging (ready for integration) |
| `data_structures.h` | ~100 | Shared structs (CapturedPacket, etc.) |

**Total Firmware:** ~5,400 lines of C++ code (reduced from ~7,500 in v1.9)

**Code Reduction in v2.0:** ~2,100 lines removed (attack/stress testing framework)

---

### Python Tools

| File | Lines | Purpose |
|------|-------|---------|
| `tools/analyze_logs.py` | ~350 | Statistical analysis, CSV export, plots |
| `tools/live_visualizer.py` | ~450 | Real-time serial monitoring, 4-panel dashboard |
| `tools/pc_analyzer.py` | ~300 | PC-side comprehensive analysis (NEW in v2.0) |

**Total Tools:** ~1,100 lines of Python code

---

## v2.0 Architecture Benefits

### For Developers
- ✅ **Clean Separation**: RadioController (hardware), PacketProcessor (analysis), IReconTool (interface)
- ✅ **No Circular Dependencies**: Interface-based design breaks dependency cycles
- ✅ **Testability**: Mock implementations possible via IReconTool interface
- ✅ **Maintainability**: Each component has single responsibility
- ✅ **Code Quality**: 9.0/10 (up from 7.0/10 in v1.9)

### For Users
- ✅ **Production Ready**: Comprehensive error handling and watchdog protection
- ✅ **Feature Rich**: 14 PSKs, GPS parsing, packet replay, geographic export
- ✅ **Reliable**: Thread-safe interrupt handling, atomic flags
- ✅ **Well Documented**: Extensive inline comments and external documentation

---

## Documentation Status

| Document | Status | Last Updated | Notes |
|----------|--------|--------------|-------|
| README.md | ✅ v2.0 | Nov 12, 2025 | Complete v2.0 architecture |
| ARCHITECTURE_REFACTOR_NOV11.md | ✅ v2.0 | Nov 12, 2025 | Phases 1-5 documentation |
| BUILD_GUIDE.md | ✅ v2.0 | Nov 12, 2025 | Updated for v2.0 components |
| FEATURES.md | ✅ v2.0 | Nov 12, 2025 | 14 PSKs, GPS parsing |
| **UNDERSTANDING_v2.md** | ✅ **v2.0** | **Nov 12, 2025** | **Complete v2.0 guide (~7k lines)** 🚀 |
| UNDERSTANDING.md | ⚠️ Pre-v2.0 | Oct 10, 2025 | Core concepts valid, examples predate refactoring |
| DEEP_DIVE_MASTER_INDEX.md | ✅ v2.0 | Nov 12, 2025 | This file - updated for v2.0 |

---

## Learning Outcomes

After studying the v2.0 documentation and code, you will understand:

### Architecture
- ✅ RadioController: Hardware abstraction for SX1262 radio
- ✅ PacketProcessor: Queue management and protocol analysis coordination
- ✅ IReconTool: Interface-based design and dependency inversion
- ✅ Clean component separation and SOLID principles

### Hardware
- ✅ ESP32-S3 architecture (CPU, peripherals, interrupts)
- ✅ SX1262 LoRa radio chip (registers, commands, modes)
- ✅ SPI protocol (timing, transactions, RadioLib abstraction)
- ✅ I2C protocol (OLED display communication)
- ✅ GPIO interrupts (DIO1, atomic flags, ISR patterns)

### Protocols
- ✅ LoRa physical layer (SF, BW, CR, frequency)
- ✅ Meshtastic packet structure (header, encryption, payload)
- ✅ Protobuf wire format (varints, field tags, wire types 0 and 5)
- ✅ GPS coordinate encoding (1e-7 degrees precision, zigzag/sfixed32)

### Cryptography
- ✅ AES-128/256 encryption (CTR mode)
- ✅ Nonce construction (packet ID, node ID)
- ✅ PSK (Pre-Shared Key) management (14 default keys)
- ✅ What can/cannot be decrypted (channel messages vs DMs)

### Software Engineering
- ✅ Interrupt-driven packet reception (atomic operations, thread safety)
- ✅ Queue management (overflow protection, producer/consumer pattern)
- ✅ State machines (reconnaissance/targeted modes, transitions)
- ✅ Watchdog timers (30s timeout, auto-recovery)
- ✅ Error handling (graceful degradation, production-grade reliability)
- ✅ Interface-based design (dependency inversion, testability)

### Tools & Analysis
- ✅ PC-based analysis workflow (ESP32 captures, PC analyzes)
- ✅ Python log analysis (statistics, visualization)
- ✅ Real-time monitoring (serial parsing, live display)
- ✅ Geographic export (KML/GeoJSON for mapping)

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

# Analyze captured data
python tools/pc_analyzer.py logs/recon_123456.csv
```

### 2. Study v2.0 Architecture

**Start with these files:**
1. `firmware/src/radio_controller.h` - Hardware abstraction interface
2. `firmware/src/packet_processor.h` - Analysis pipeline interface
3. `firmware/src/irecon_tool.h` - Dependency inversion interface
4. `firmware/src/lora_recon_tool.h` - Main orchestrator

**Then review implementations:**
1. `radio_controller.cpp` - ISR handling, SPI communication
2. `packet_processor.cpp` - Queue management, protocol coordination
3. `lora_recon_tool.cpp` - State machine, mode management

### 3. Modify and Extend

**Easy Modifications:**
- Change frequency scan range (edit LoRaReconTool configs)
- Adjust dwell timing (12s → custom value)
- Add more PSK keys (psk_decryption_simple.cpp)
- Customize display modes (ui_components.cpp)

**Intermediate Modifications:**
- Add new protocol analyzer (extend ProtocolAnalyzer)
- Implement packet filtering (by node ID, type)
- Add SD card logging integration
- Expand PC analysis tools

**Advanced Modifications:**
- Implement active transmission (beacon, mesh join)
- Add WiFi/BLE interfaces (remote control via web)
- Multi-board coordination (triangulation, distributed sensing)
- Machine learning (packet classification, pattern detection)

### 4. Present Your Work

**You now have enough knowledge to:**
- ✅ Present at security conferences (DEF CON, Black Hat, BSides)
- ✅ Write technical blog posts or whitepapers
- ✅ Teach workshops on LoRa security and embedded systems
- ✅ Contribute to Meshtastic security research
- ✅ Extend to other LoRa protocols (LoRaWAN, Helium, ChirpStack)

---

## Additional Resources

### Project Documentation

Located in repository root and `docs/` directory:

- **README.md** - v2.0 project overview and architecture
- **QUICKSTART.md** - Getting started guide
- **ARCHITECTURE_REFACTOR_NOV11.md** - Complete v2.0 refactoring documentation
- **ROADMAP.md** - Future development phases
- **BUILD_GUIDE.md** - Compilation and component structure
- **FEATURES.md** - Complete feature list
- **ENCRYPTION_REALITY.md** - What can/cannot be decrypted
- **TROUBLESHOOTING_MESHTASTIC.md** - Common issues and solutions

### Conference Materials

Located in `conference/` directory (note: some reference removed features):

- **README.md** - Conference presentation guide
- **ETHICAL_TESTING_GUIDELINES.md** - Responsible disclosure and ethics
- **OFFENSIVE_TESTING_COMPLETE_GUIDE.md** - Legacy attack framework documentation

---

## Credits

**Hardware:**
- ESP32-S3: Espressif Systems
- SX1262: Semtech Corporation
- Heltec WiFi LoRa 32 V3: Heltec Automation

**Libraries:**
- RadioLib: Jan Gromeš (@jgromes)
- U8g2: Oliver Kraus (@olikraus)
- mbedtls: Mbed TLS Team
- ArduinoJson: Benoît Blanchon

**Project:**
- v2.0 Architecture Refactoring: November 11-12, 2025
- Phases 1-5: RadioController, PacketProcessor, IReconTool, GPS parsing, code cleanup

---

## Summary

This deep dive index provides a roadmap for understanding the ESP32 LoRa reconnaissance tool's v2.0 architecture. The v2.0 refactoring introduced clean component separation, removed ~2,100 lines of speculative code, and focused on the tool's core strengths: **passive reconnaissance with comprehensive PC-based analysis**.

**Start Your Journey Here:** [UNDERSTANDING_v2.md](UNDERSTANDING_v2.md) - Complete v2.0 architecture guide 🚀

**Key v2.0 Changes:**
- ✅ RadioController: Hardware abstraction (200 lines)
- ✅ PacketProcessor: Queue and analysis coordination (180 lines)
- ✅ IReconTool: Interface-based design (30 lines)
- ✅ GPS parsing: Full wire type 0 and 5 support
- ✅ 14 PSK keys: Expanded from 5 keys
- ✅ Code cleanup: Removed ~2,100 lines of attack/stress testing
- ✅ **UNDERSTANDING_v2.md: Complete v2.0 documentation (~7,000 lines)**

**From "it works" to "I understand the architecture and can extend it"**

You now have:
- ✅ **v2.0 architecture understanding** (clean component separation)
- ✅ **Complete documentation** (UNDERSTANDING_v2.md with study guide and self-tests)
- ✅ **Practical tools** (PC analyzer, live visualizer)
- ✅ **Production-grade codebase** (9.0/10 quality score)
- ✅ **Extensibility** (interface-based design, clear responsibilities)

**You are ready to understand, modify, and present this system with confidence!**

---

*ESP32 LoRa Sniffer Deep Dive Master Index*  
*Updated for v2.0: November 12, 2025*  
*Primary Documentation: [UNDERSTANDING_v2.md](UNDERSTANDING_v2.md) (~7,000 lines)*  
*v2.0 Architecture: RadioController + PacketProcessor + IReconTool*
