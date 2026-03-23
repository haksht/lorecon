# 🏗️ ESP32 LoRa Sniffer - Build Guide

## 🎯 **Overview**

This project is a **production-ready LoRa reconnaissance platform** using PlatformIO for ESP32-S3 + SX1262 hardware. The v2.0 architecture features clean separation of concerns with RadioController, PacketProcessor, and IReconTool interface components.

### **🔬 Production Platform**
- **Purpose**: Professional security research and RF analysis  
- **Architecture**: Clean component separation (RadioController, PacketProcessor, IReconTool)
- **Features**: PSK decryption (23 default keys incl. legacy admin defaults), GPS parsing, packet replay, geographic export
- **Target Audience**: Security researchers, RF engineers, network analysts
- **Code Quality**: Production-ready with comprehensive error handling

---

## 🛒 **Bill of Materials (~$35-50 USD)**

### Essential Hardware

| Component | Price | Notes | Where to Buy |
|-----------|-------|-------|-------------|
| **Heltec WiFi LoRa 32 V3 / V4** | $22-32 | ESP32-S3 + SX1262 + 0.96" OLED | [Amazon](https://www.amazon.com/dp/B0B697NLJ5) / [AliExpress](https://www.aliexpress.com/item/1005005967763162.html) / [Heltec Store](https://heltec.org/project/wifi-lora-32-v3/) |
| 915MHz LoRa Antenna | $0-8 | Usually included with board; RP-SMA connector | [Amazon](https://www.amazon.com/dp/B09K3WMCVN) |
| USB-C Data Cable | $0 | For programming and power | Any data-capable USB-C cable |

### Recommended Additions

| Component | Price | Why You Want It | Where to Buy |
|-----------|-------|-----------------|-------------|
| 3D Printed Enclosure | $5-15 | Protection, professional look, belt clip | [Printables](https://www.printables.com/search/models?q=heltec%20v3) / [Thingiverse](https://www.thingiverse.com/search?q=heltec+v3) |
| 18650 Li-Ion Battery | $8-12 | 4-8 hours portable operation | [Amazon](https://www.amazon.com/dp/B0BG8XJMQX) |
| External 915MHz Antenna | $10-20 | Better range (6dBi vs 2dBi stock) | [Amazon](https://www.amazon.com/dp/B086ZG5WMR) |
| MicroSD Card (8-32GB) | $5-8 | PCAP logging, CSV export | Any Class 10 card |

### Total Cost

- **Minimal build**: ~$25-30 (board + included antenna)
- **Recommended build**: ~$40-50 (+ case + battery)
- **Full build**: ~$60-70 (+ better antenna + SD card)

**Time to first packet**: Under 5 minutes (flash firmware → power on → start scanning)

---

## 🚀 **Quick Build Commands**

```bash
# Build and upload firmware to ESP32
pio run --target upload

# Upload web app files to LittleFS filesystem
pio run --target uploadfs

# Monitor serial output
pio device monitor

# Build, upload, and monitor (all-in-one)
pio run --target upload --target monitor

# Full setup (firmware + filesystem + monitor)
pio run --target upload --target uploadfs --target monitor
```

**Note:** The `uploadfs` target uploads the web interface files from `data/webapp/` to the ESP32's LittleFS filesystem. This is required for the phone/browser UI to work. You only need to run this once, or when web files change.

---

## 📊 **Core Features**

| Capability | Status | Notes |
|------------|--------|-------|
| **📡 LoRa Packet Capture** | ✅ Production | 26 frequency configurations, interrupt-driven |
| **🔓 PSK Decryption** | ✅ Production | 23 keys (incl. legacy admin channel defaults) |
| **📍 GPS Position Parsing** | ✅ Production | Latitude/longitude extraction from POSITION_APP |
| **🎯 Device Targeting** | ✅ Production | Interactive menu, frequency locking |
| **📦 Packet Replay** | ✅ Production | 10 slots, repeat count configuration |
| **🗺️ Geographic Export** | ✅ Production | KML and GeoJSON for mapping tools |
| **📱 Protocol Analysis** | ✅ Production | Meshtastic, LoRaWAN identification |
| **💾 SD Card Logging** | ✅ Production | CSV + PCAP on boards with SD slot |
| **�️ OLED Display** | ✅ Production | 6 display modes, button control |
| **� Activity Analysis** | ✅ Production | Packet timing, encryption status, RF stats |

---

## 🔧 **Architecture Overview**

### **v2.0 Component Structure**

```
firmware/src/
├── 🎯 CORE ARCHITECTURE
│   ├── main.cpp                       # Application entry point (~50 lines)
│   ├── lora_recon_tool.cpp/.h         # Main orchestrator (implements IReconTool)
│   ├── irecon_tool.h                  # Abstract interface (dependency inversion)
│   ├── radio_controller.cpp/.h        # SX1262 hardware abstraction (~200 lines)
│   ├── packet_processor.cpp/.h        # Queue management and analysis (~180 lines)
│   └── recon_state.cpp/.h             # Device tracking and state management
│
├── 🎨 USER INTERFACE
│   ├── user_interface.cpp/.h          # Menu system and command routing
│   ├── command_handler.cpp/.h         # Serial command processing
│   ├── oled_display.cpp/.h            # Display driver and rendering
│   └── ui_components.cpp/.h           # Display component library
│
├── 🔐 ANALYSIS MODULES
│   ├── psk_decryption_simple.cpp/.h   # PSK testing framework (23 keys)
│   ├── protocol_analyzer.cpp/.h       # Protocol identification
│   ├── geo_intelligence.cpp/.h        # GPS position extraction
│   └── text_packet_diagnostic.cpp/.h  # Packet timing and encryption analysis
│
├── � DATA MANAGEMENT
│   ├── packet_logger.cpp/.h           # SD card logging (CSV + PCAP)
│   └── data_structures.h              # Shared structs and definitions
│
└── 🛡️ RELIABILITY
    └── error_handler.cpp/.h           # Error reporting and recovery
```

### **Key Architectural Principles**

1. **Separation of Concerns**: RadioController handles hardware, PacketProcessor handles analysis
2. **Dependency Inversion**: IReconTool interface breaks circular dependencies
3. **Thread Safety**: Atomic flags for interrupt-driven packet reception
4. **Clean Interfaces**: Each component has clear responsibilities and APIs
5. **Testability**: Interface-based design enables unit testing with mocks

---

## 🛠️ **Development Workflow**

### **Building and Testing**

```bash
# Clean build (recommended after major changes)
pio run --target clean
pio run --target upload

# Quick compile check (no upload)
pio run

# Upload without rebuild
pio run --target upload

# Monitor serial output
pio device monitor

# Build with verbose output (debugging)
pio run -v
```

### **Adding New Features**

1. **Identify Component**: Determine which module (RadioController, PacketProcessor, etc.)
2. **Edit Source**: Make changes to appropriate .cpp/.h files
3. **Test Compilation**: `pio run` to verify it compiles
4. **Upload and Test**: `pio run --target upload --target monitor`
5. **Document**: Update relevant docs and comments

### **Debugging Strategy**

1. **Serial Monitor**: Primary debugging via `Serial.println()`
2. **Error Handler**: Check error_handler.cpp for error reporting
3. **Watchdog**: System auto-recovers from hangs (30s timeout)
4. **OLED Display**: Visual feedback for device state
5. **Activity Analysis**: Use 'a' command to check packet flow

---

## 🔍 **Common Build Issues**

### **Upload Fails**

```bash
# Check device port
pio device list

# Specify port manually
pio run --target upload --upload-port COM3  # Windows
pio run --target upload --upload-port /dev/ttyUSB0  # Linux
```

### **Compilation Errors**

```bash
# Clean and rebuild
pio run --target clean
pio run

# Update libraries
pio pkg update

# Check PlatformIO version
pio --version
```

### **Out of Memory**

- Check partition scheme in platformio.ini
- Reduce MAX_QUEUE_SIZE if needed
- Disable verbose debug output
- Consider removing unused features

---

## 📈 **Benefits of Current Architecture**

### **✅ Developer Benefits**
- **Clear Component Boundaries**: RadioController, PacketProcessor, IReconTool
- **No Circular Dependencies**: Clean dependency graph with interface-based design
- **Easier Testing**: Mock implementations possible via IReconTool interface
- **Better Maintainability**: Each component has single responsibility
- **Extensible**: Add new protocols, commands, or display modes easily

### **✅ User Benefits**  
- **Production Ready**: Comprehensive error handling and watchdog protection
- **Feature Rich**: PSK decryption, GPS parsing, packet replay, geographic export
- **Reliable**: Thread-safe interrupt handling, atomic flags
- **Well Documented**: Extensive inline comments and external documentation

### **✅ Code Quality**
- **Clean Architecture**: SOLID principles applied
- **No Dead Code**: Removed ~2,100 lines of speculative features
- **Debug Output**: Clean, non-verbose production logging

---

## 🚨 **Important Notes**

### **⚠️ Build System Requirements**
- **PlatformIO Core**: This system requires PlatformIO (not Arduino IDE)
- **Supported Hardware**:
  - ✅ **Heltec WiFi LoRa 32 V3 / V4** — build env: `heltec_v3` (V3 and V4 without GPS module use identical firmware)
  - ✅ **Heltec WiFi LoRa 32 V4 + L76K GNSS** — build env: `heltec_v4` (V4 with the optional L76K GPS module attached)
  - ✅ **LilyGO T3-S3 V1.2/V1.3** — build env: `t3_s3`
  - ✅ **LilyGO T-Beam Supreme** — build env: `tbeam_supreme`
  - ⚠️ **T-Deck variants** - Not supported. See `../hardware/TDECK_PLUS_INVESTIGATION.md`.
  - ℹ️ **Other boards** - Would require porting (display drivers, pin configs, etc.)
- **RadioLib 6.4.2**: Specific version required for SX1262 support
- **ArduinoJson 7.0.4**: For data serialization

### **💡 Best Practices**
- **Clean Builds**: Run `pio run --target clean` after major changes
- **Serial Monitor**: Always monitor during first upload to catch errors
- **Documentation**: Check GETTING_STARTED.md and FEATURES.md for usage
- **Version Control**: Commit before making major architectural changes
- **Hardware**: Verify antenna connection before powering on

---

## 📚 **Additional Resources**

- **[GETTING_STARTED.md](../../GETTING_STARTED.md)**: Complete setup and usage guide
- **[FEATURES.md](FEATURES.md)**: Complete feature documentation
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)**: Common issues
- **[../technical/ARCHITECTURE.md](../technical/ARCHITECTURE.md)**: Deep technical dive

---

The v2.0 architecture provides a stable foundation for extending the tool with new protocols, boards, or analysis capabilities.