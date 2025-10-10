# System Architecture

ESP32 LoRa packet capture and analysis tool architecture overview.

## 🎯 **Project Goals**

Create an ESP32-based LoRa packet capture and analysis tool for security research, focused on Meshtastic protocol analysis and RF reconnaissance.

## 🏗️ **System Overview**

### **Core Components**
1. **Hardware Layer** — ESP32-S3 with SX1262 LoRa radio
2. **Firmware Layer** — Interrupt-driven packet capture with protocol analysis
3. **Storage Layer** — LittleFS JSON logging
4. **Display Layer** — Optional OLED for standalone operation
5. **Interface Layer** — Serial terminal with interactive menu system

## 📡 **Hardware Setup**

### **Working Configuration**
```
ESP32-S3 Development Board
├── SX1262 LoRa Radio
│   ├── NSS: GPIO 8
│   ├── DIO1: GPIO 14 
│   ├── RST: GPIO 12
│   └── BUSY: GPIO 13
└── 902-928 MHz Antenna
```

### **Radio Specifications**
- **Frequency Range**: 902-928 MHz (US ISM band)
- **Common Protocols**: Meshtastic, TTN/LoRaWAN
- **Range**: 100m - 10km (antenna and environment dependent)
- **Power**: ~100mA during active scanning

## 🔧 FIRMWARE ARCHITECTURE

### Main Application Class (`LoRaReconTool`)
```cpp
┌─────────────────────────────────────────────────────────┐
│               LoRaReconTool (main engine)               │
├─────────────────────────────────────────────────────────┤
│  • SX1262 Radio Control (RadioLib)                     │
│  • Interrupt-driven Packet Reception                   │
│  • State Management (ReconState)                       │
│  • Command Dispatch (CommandHandler)                   │
│  • Protocol Analysis (ProtocolAnalyzer)                │
│  • PSK Decryption (optional)                           │
│  • Session Key Manager                                 │
│  • Geographic Intelligence                              │
│  • OLED Display Manager                                │
│  • Hardware Stress Tester (optional)                   │
└─────────────────────────────────────────────────────────┘
```

### Key Classes and Modules
- **LoRaReconTool** - Main application controller
- **ReconState** - Device tracking, replay buffer, statistics
- **CommandHandler** - Command pattern dispatcher for user input
- **ProtocolAnalyzer** - Meshtastic/LoRaWAN packet parsing
- **SessionKeyManager** - Session key harvesting and management
- **GeoIntelligence** - GPS extraction and KML/GeoJSON export
- **OLEDDisplay** - Optional display management
- **ErrorHandler** - Production error recovery system

### Software Stack
- **Platform**: PlatformIO with ESP32 Arduino Framework
- **Board**: esp32-s3-devkitc-1
- **Radio Library**: jgromes/RadioLib@^6.4.2
- **JSON**: bblanchon/ArduinoJson@^7.0.4
- **Display**: olikraus/U8g2@^2.35.9 (optional)

### Critical Configuration
```ini
# Prevents GPIO conflicts and enables proper execution
build_flags = -DARDUINO_USB_CDC_ON_BOOT=0
```

## 📊 DATA FLOW ARCHITECTURE

### Packet Capture Pipeline
```
RF Signal → SX1262 → ISR → Queue → Analysis → Display/Storage
    ↓          ↓       ↓      ↓        ↓           ↓
902-928MHz  Hardware  DIO1   Buffer  Protocol   Serial/OLED
                                      Analyzer   +LittleFS
```

### Real-Time Processing Flow
1. **RF Reception**: SX1262 receives LoRa packets (16 configurations scanned)
2. **Interrupt Trigger**: DIO1 pin triggers ESP32 ISR (atomic flag set)
3. **Packet Queuing**: ISR queues packet data with RSSI/SNR/timestamp
4. **Main Loop Processing**: 
   - Dequeue packet from buffer
   - Protocol analysis (Meshtastic/LoRaWAN detection)
   - PSK decryption attempt (if enabled)
   - Session key extraction (if applicable)
   - Geographic data parsing (GPS coordinates)
5. **Output**: Serial display, OLED update, JSON logging
6. **State Update**: Device tracking, statistics, replay buffer

## 🎯 IMPLEMENTATION STATUS

### ✅ Phase 1: Basic Packet Capture (COMPLETE)
- Real-time packet capture with interrupt-driven reception
- Signal quality analysis (RSSI, SNR monitoring)
- Meshtastic protocol detection
- Serial output with packet display
- Multi-frequency scanning (16 configurations)

### ✅ Phase 2: PSK Decryption & Intelligence (COMPLETE)
- AES-256 decryption for Meshtastic channel PSK
- 5 default PSK database with automatic testing
- GPS coordinate extraction from position packets
- Session key harvesting system
- Message content decryption capability

### ✅ Phase 3: Enhanced Analysis & Control (COMPLETE)
- Device fingerprinting and node tracking
- Packet replay system (10 slots)
- Hardware stress testing framework
- OLED display with button control
- Geographic intelligence (KML/GeoJSON export)
- Command pattern menu system
- Production error handling

### Current Focus
- Field testing and validation
- Conference demonstration materials
- Security research documentation

---

## 💾 STORAGE ARCHITECTURE

### LittleFS Implementation
```
LittleFS Root
└── packets.jsonl    # JSON Lines format for packet logs
```

### Data Format
```json
{
  "timestamp": 1234567890,
  "frequency": 906.875,
  "sf": 11,
  "bw": 250,
  "rssi": -45.2,
  "snr": 8.5,
  "protocol": "Meshtastic",
  "node_id": "0x401ACD4E",
  "data_hex": "0x..."
}
```

**Note**: Simple append-only logging. No complex session management or PCAP export (by design for simplicity).

---

## 🚀 BUILD & DEPLOYMENT

### Build Commands
```powershell
# Full-featured (default)
pio run --target upload
pio device monitor

# Educational simple version
pio run -e simple --target upload
pio device monitor
```

### Development Workflow
1. **Firmware Development**: C++ with PlatformIO
2. **Testing**: Serial monitor + hardware validation
3. **Debugging**: Comprehensive error logging system
4. **Optimization**: Fixed allocations, atomic operations

---

## 🎯 PERFORMANCE SPECIFICATIONS

### Verified Metrics
- **Packet Capture Rate**: Real-time, no packet loss
- **Signal Sensitivity**: -15 dBm to -120 dBm range  
- **Processing Latency**: <50ms interrupt-to-display
- **Memory Usage**: ~200KB RAM, ~1MB flash
- **Power Consumption**: ~100mA during active scanning
- **Configuration Scan**: 16 configs in ~3 minutes
- **Replay Capacity**: 10 packets (256 bytes each)

### Reliability Features
- **Hardware Watchdog**: 30-second timeout protection
- **Atomic Interrupts**: Thread-safe packet reception
- **Error Recovery**: Automatic radio reset on failures
- **Input Timeouts**: 30s timeout on all user inputs
- **Memory Safety**: Fixed buffers, no dynamic allocation

---

**Architecture Status**: ✅ Production Ready  
**Current Version**: 1.9  
**Last Updated**: October 10, 2025


