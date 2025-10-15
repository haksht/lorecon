# ESP32 LoRa Sniffer - Quick Start Guide

**Last Updated:** October 10, 2025  
**Version:** 1.9 Production Ready  
**Status:** ✅ Ready to Use

---

## 🚀 Get Started in 5 Minutes

### Prerequisites
- ESP32-S3 board with SX1262 LoRa radio (Heltec WiFi LoRa 32 V3 recommended)
- USB cable
- PlatformIO installed (see [BUILD_GUIDE.md](docs/BUILD_GUIDE.md) for setup)
- 902-928 MHz antenna (US ISM band)

### 1. Clone and Build
```powershell
# Clone repository
git clone https://github.com/tiarno/esp32-sniffer.git
cd esp32-sniffer

# Build and upload (default = full-featured)
pio run --target upload

# Start monitoring
pio device monitor
```

### 2. First Run
The tool will:
1. Initialize radio and display (if present)
2. Start scanning 16 LoRa configurations
3. Auto-discover nearby Meshtastic/LoRaWAN devices
4. Display packet info on OLED and serial

### 3. Basic Usage
Press **'m'** anytime to see the menu:
- Select discovered device (1-9) for focused capture
- Press 'c' to save packets for replay
- Press 'p' to view and replay captured packets
- Press 'q' for quiet mode (faster capture)
- Press 'r' to restart scanning

---

## 📊 What This Tool Does

### Core Capabilities
✅ **Reconnaissance** - Scans 16 LoRa configurations automatically  
✅ **Packet Capture** - Interrupt-driven, real-time packet reception  
✅ **Device Discovery** - Identifies Meshtastic nodes and LoRaWAN devices  
✅ **Protocol Analysis** - Parses Meshtastic/LoRaWAN packet structures  
✅ **Packet Replay** - Store and retransmit captured packets (10 slots)  
✅ **Broadcast Decryption** - Decrypts position, telemetry, channel messages (with default PSKs)  
✅ **Session Key Harvesting** - Captures encryption keys from mesh traffic (legacy firmware)  
✅ **OLED Display** - Standalone operation without serial connection  
✅ **Geographic Intelligence** - Extracts GPS from position packets  
✅ **Export Tools** - KML/GeoJSON for mapping applications

**Note on Encryption**: Meshtastic firmware 2.5.0+ (June 2024) uses Public Key Cryptography for direct messages. This tool decrypts broadcasts and channel messages, but cannot decrypt DMs. See [ENCRYPTION_REALITY.md](docs/ENCRYPTION_REALITY.md).

✅ **Export Tools** - KML/GeoJSON for mapping applications

---

## 🎯 Typical Workflow

### Research & Analysis
1. **Scan** - Wait ~3 minutes for initial reconnaissance
2. **Discover** - Review detected devices (press 'm')
3. **Target** - Select specific device for focused capture
4. **Analyze** - View packet details, signal strength, protocols
5. **Export** - Save data for offline analysis

### Testing & Demonstration
1. **Capture** - Save interesting packets (press 'c')
2. **Replay** - Retransmit saved packets (press 'p')
3. **Broadcast Decryption** - Automatic testing of default PSKs on position/telemetry/channel messages
4. **Stress Testing** - Hardware vulnerability assessment (press 't')

**Testing Note**: To test channel message decryption, send messages to the **Channel** (not Direct Messages) in the Meshtastic app. DMs use PKC and cannot be decrypted.

---

## � Documentation

### Essential Reading
- **[README.md](README.md)** - Complete project overview
- **[BUILD_GUIDE.md](docs/BUILD_GUIDE.md)** - Compilation and setup
- **[FEATURES.md](docs/FEATURES.md)** - Full feature documentation
- **[TROUBLESHOOTING_MESHTASTIC.md](docs/TROUBLESHOOTING_MESHTASTIC.md)** - Common issues

### Advanced Topics
- **[UNDERSTANDING.md](docs/UNDERSTANDING.md)** - Deep technical guide
- **[architecture.md](docs/architecture.md)** - System design
- **[conference/](conference/)** - Security research materials

---

## ⚠️ Legal and Ethical Use

This tool is for:
- ✅ Monitoring networks you own or have permission to test
- ✅ Educational and research purposes
- ✅ Security assessment of authorized systems
- ✅ Understanding LoRa protocol behavior

**NOT for:**
- ❌ Unauthorized monitoring of third-party networks
- ❌ Intercepting private communications
- ❌ Jamming or interfering with radio transmissions
- ❌ Any illegal activities

**Remember:** This is a passive receive-only tool for public ISM band frequencies. Always respect privacy and comply with local regulations.

---

## 🆘 Need Help?

1. **Check documentation** - See [docs/](docs/) folder
2. **Review troubleshooting** - See [TROUBLESHOOTING_MESHTASTIC.md](docs/TROUBLESHOOTING_MESHTASTIC.md)
3. **Examine code** - Well-commented source in [firmware/src/](firmware/src/)
4. **GitHub issues** - Report bugs or ask questions

---

**Ready to capture some packets!** 📡
