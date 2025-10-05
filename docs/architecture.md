# System Architecture

Simple architecture for the ESP32 LoRa packet capture tool.

## 🎯 **Project Goals**

### **Realistic Objectives**
Create a basic ESP32-based LoRa packet capture and analysis tool for educational use and personal network monitoring.

## 🏗️ **Simple System Overview**

### **Core Components**
1. **Hardware Layer** — ESP32-S3 with SX1262 LoRa radio
2. **Firmware Layer** — Basic packet capture with sequential scanning
3. **Storage Layer** — Local logging to flash memory
4. **Optional** — Simple web interface for configuration

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

### Current Implementation (`firmware/src/main.cpp`)
```cpp
┌─────────────────────────────────────┐
│           Main Application          │
├─────────────────────────────────────┤
│  • RadioLib SX1262 Driver          │
│  • Interrupt-driven Packet Capture │
│  • Real-time Signal Analysis       │
│  • Serial Output & Debugging       │
│  • Meshtastic Protocol Detection   │
└─────────────────────────────────────┘
```

### Software Stack
- **Platform**: PlatformIO with ESP32 Arduino Framework
- **Board Definition**: esp32-s3-devkitc-1 (generic, conflict-free)
- **Primary Library**: jgromes/RadioLib@^6.4.2 (pure implementation)
- **Build System**: PlatformIO with optimized compilation flags

### Critical Configuration
```ini
# Prevents GPIO conflicts and enables proper execution
build_flags = -DARDUINO_USB_CDC_ON_BOOT=0
```

## 📊 DATA FLOW ARCHITECTURE

### Packet Capture Pipeline
```
RF Signal → SX1262 → ESP32 → Analysis → Storage → Web UI
    ↓          ↓       ↓        ↓         ↓        ↓
902.125MHz  Hardware  ISR   Protocol   SPIFFS   REST API
```

### Real-Time Processing
1. **RF Reception**: SX1262 receives Meshtastic packets on 902.125 MHz
2. **Interrupt Handling**: DIO1 triggers ESP32 interrupt service routine
3. **Packet Extraction**: RadioLib retrieves packet data from SX1262 FIFO
4. **Signal Analysis**: RSSI, SNR, frequency error measurement
5. **Protocol Detection**: Meshtastic header and structure validation
6. **Data Formatting**: Hex/ASCII display with metadata annotation

## 🎯 FUTURE ARCHITECTURE: MULTI-PHASE DEVELOPMENT

### Phase 1: ✅ COMPLETED - Basic Packet Sniffer
- Real-time packet capture with interrupt-driven reception
- Signal quality analysis (RSSI, SNR monitoring)
- Meshtastic protocol detection and header recognition
- Serial output with hex/ASCII packet display
- Stable 902.125 MHz Long-Fast preset operation

### Phase 2: ✅ COMPLETED - PSK Decryption & Intelligence Extraction
```cpp
┌─────────────────────────────────────┐
│     PSK Decryption Module          │
├─────────────────────────────────────┤
│  • Meshtastic AES-256 Decryption │
│  • 5 Known Default PSK Database    │
│  • Automated Test Suite           │
│  • GPS Coordinate Extraction      │
│  • Message Content Interception    │
│  • Compile-time Enable/Disable     │
│                                     │
│  Status: CODE COMPLETE              │
│  Testing: Awaiting encrypted traffic│
└─────────────────────────────────────┘
```

### Phase 3: ✅ COMPLETED - Enhanced Analysis & Replay
```cpp
┌─────────────────────────────────────┐
│     Enhanced Reconnaissance         │
├─────────────────────────────────────┤
│  • Full Meshtastic Protocol Decoder │
│  • Device Fingerprinting System     │
│  • Node ID Tracking & Correlation   │
│  • Geographic Data Extraction (GPS) │
│  • Traffic Pattern Analysis         │
│  • Multi-frequency Support (16)     │
│  • Packet Replay System (10 slots)  │
│  • Hardware Stress Testing          │
└─────────────────────────────────────┘
```

### Phase 4: ❌ REMOVED - Persistent Storage
**Decision**: Not needed for focused recon tool  
**Current Approach**: LittleFS packet logging is sufficient  
**Rationale**: Session persistence adds complexity without clear benefit

### Phase 5: 📋 FUTURE - Conference Demo Polish
**Focus**: DefCon/BlackHat presentation readiness  
**Timeline**: 6-8 weeks

**Priority Items**:
1. **PSK Testing Validation** - Test with multi-node encrypted mesh
2. **Live Visualization** - Python companion tool for demos
3. **Demo Script** - Scripted walkthrough with backup plans
4. **Documentation** - Methodology guides and defensive recommendations

**Optional Web Interface**: Deferred to post-conference (if there's interest)

## 🔐 SECURITY & RECONNAISSANCE ARCHITECTURE

### Intelligence Gathering Capabilities
1. **Passive Reconnaissance**: Silent monitoring without transmission
2. **🔓 PSK Cryptanalysis**: Real-time decryption of default PSK networks
3. **📍 GPS Intelligence**: Location tracking via decrypted coordinates  
4. **💬 Communication Interception**: Message content extraction and analysis
5. **Network Topology Mapping**: Device relationship and routing analysis
6. **Traffic Pattern Analysis**: Communication frequency and timing
7. **Geographic Intelligence**: Position data extraction and correlation
8. **Device Fingerprinting**: Hardware and software identification

### Security Research Features (Future)
- **Replay Attacks**: Captured packet retransmission for testing
- **Protocol Fuzzing**: Malformed packet generation and injection  
- **Mesh Network Assessment**: Coverage analysis and vulnerability identification
- **Encryption Analysis**: Protocol security evaluation capabilities

## 💾 STORAGE ARCHITECTURE

### File System Structure (Planned)
```
SPIFFS/LittleFS Root
├── /captures/
│   ├── session_YYYYMMDD_HHMMSS/
│   │   ├── metadata.json
│   │   ├── packets.pcap
│   │   └── analysis.json
│   └── ...
├── /config/
│   ├── radio_settings.json
│   ├── filter_rules.json  
│   └── web_config.json
└── /web/
    ├── index.html
    ├── app.js
    ├── style.css
    └── assets/
```

### Data Format Standards
- **PCAP**: Standard packet capture format for external analysis
- **JSON**: Structured metadata and analysis results
- **Raw Binary**: Efficient storage for high-volume captures

## 🌐 WEB INTERFACE ARCHITECTURE

### Frontend Technologies
- **Framework**: Modern JavaScript (React/Vue.js consideration)
- **Real-time Updates**: WebSocket connection to ESP32
- **Visualization**: D3.js or Chart.js for signal analysis
- **Mapping**: Leaflet.js for geographic visualization
- **Responsive Design**: Mobile-friendly interface

### API Endpoints (Planned)
```
GET  /api/status          # System status and statistics
GET  /api/packets         # Recent packet list with filtering
GET  /api/sessions        # Capture session management
POST /api/capture/start   # Begin packet capture
POST /api/capture/stop    # End packet capture  
GET  /api/config          # Current configuration
POST /api/config          # Update configuration
GET  /api/export/{format} # Export captured data
```

## 🚀 DEPLOYMENT ARCHITECTURE

### Build Process
```powershell
# Firmware compilation and deployment
cd c:\Users\tim\lora\esp32-sniffer
pio run -t upload

# Web assets preparation (future)  
npm run build
pio run -t uploadfs
```

### Development Workflow
1. **Firmware Development**: C++ with PlatformIO and RadioLib
2. **Web Development**: JavaScript/HTML/CSS with modern toolchain
3. **Integration Testing**: End-to-end functionality validation
4. **Performance Optimization**: Memory and processing efficiency
5. **Security Testing**: Reconnaissance capability validation

## 🎯 PERFORMANCE SPECIFICATIONS

### Current Metrics (Verified)
- **Packet Capture Rate**: Real-time without loss
- **Signal Sensitivity**: -16 dBm successfully captured  
- **Processing Latency**: <100ms interrupt-to-display
- **Memory Usage**: Minimal Arduino framework footprint
- **Power Consumption**: Standard ESP32-S3 operational levels

### Target Specifications (Future)
- **Storage Capacity**: 2-4 MB capture sessions
- **Web Interface Response**: <200ms API response time
- **Concurrent Users**: 5-10 simultaneous web connections
- **Export Speed**: 1 MB/s data export throughput
- **Analysis Throughput**: 100+ packets/second processing

---

**Architecture Status**: ✅ Phase 1-3 Complete, DefCon Demo Preparation Phase  
**Current Focus**: PSK validation with live traffic, demo polish, presentation materials  
**Tool Identity**: Focused recon/sniff/capture/replay with hardware attack surface analysis  
**Last Updated**: October 2025


