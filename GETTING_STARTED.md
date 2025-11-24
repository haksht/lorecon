# Getting Started with ESP32 LoRa Sniffer

**Version:** 2.0 Production Ready  
**Status:** ✅ Ready to Use  
**Last Updated:** November 24, 2025

---

## 🚀 Quick Start (5 Minutes)

### Prerequisites
- **Heltec WiFi LoRa 32 V3** (ESP32-S3 + SX1262 + OLED)
- USB cable
- PlatformIO installed
- 902-928 MHz antenna (US ISM band)

### 1. Build and Upload

```powershell
# Clone repository
git clone https://github.com/tiarno/esp32-sniffer.git
cd esp32-sniffer

# Build and upload firmware
pio run --target upload

# Upload web interface files
pio run --target uploadfs

# Start monitoring
pio device monitor
```

### 2. First Run

The tool will:
1. Initialize radio and display
2. Start scanning 26 LoRa configurations
3. Auto-discover nearby devices
4. Display packet info on OLED and serial

---

## 🎯 Three Ways to Use

### Option 1: Serial Interface (USB)

Connect via USB and use serial commands:

```
Press 'm' - Main menu with device list
Press 'f' - Frequency targeting
Press 'a' - RF activity analysis
Press 'c' - Capture packet for replay
Press 'p' - Replay menu
Press 'q' - Toggle quiet mode
Press 'r' - Resume reconnaissance
Press 'x' - Text packet diagnostics
Press 'g' - GPS positions
Press 'k' - Export KML
Press 'j' - Export GeoJSON
```

**Best for:** Debugging, detailed packet analysis, development

### Option 2: OLED Display (Standalone)

The device runs standalone with built-in display:

- **6 display modes**: Welcome, Idle, Device Info, Signal Info, Stats, Summary
- **Button control**: 
  - Short press (< 3s): Cycle display modes
  - Long press (≥ 3s): Shutdown sequence
- **Auto-cycle**: Display rotates through modes automatically

**Best for:** Field work without laptop, portable monitoring

### Option 3: Web Interface (Phone/Browser)

#### Step 1: Connect to WiFi

1. Power on ESP32
2. Connect to WiFi network:
   - **SSID:** `ESP32-LoRa-Sniffer`
   - **Password:** `recon123`

#### Step 2: Open Web Interface

**Method 1:** Direct IP
```
http://192.168.4.1
```

**Method 2:** mDNS (easier)
```
http://esp32-lora.local
```

**Method 3:** Install as PWA
- Open browser to above URL
- Tap **Share** → **Add to Home Screen**
- Launch like a native app

#### Step 3: Use the Dashboard

**8 Interactive Tabs:**

1. **Status** - System overview, uptime, quick actions
2. **Live Stream** - Real-time packet feed with audio feedback
3. **Stats** - Protocol pie/bar charts
4. **Network** - Interactive topology map
5. **Devices** - Discovered devices with targeting
6. **Packets** - Replay menu with transmission controls
7. **Frequency** - 26 scan configuration targeting
8. **GPS** - Geographic positions with export

**Audio Feedback:**
- Different tones for each protocol (Meshtastic/LoRaWAN/Helium)
- "Geiger counter" effect for packet activity
- Toggle on/off in Live Stream tab

**Best for:** Field deployment, mobile monitoring, demonstrations

---

## 📡 What This Tool Does

### Core Capabilities

✅ **Reconnaissance** - Scans 26 LoRa configurations automatically  
✅ **Packet Capture** - Interrupt-driven, real-time reception  
✅ **Device Discovery** - Identifies Meshtastic nodes and LoRaWAN devices  
✅ **Protocol Analysis** - Parses packet structures  
✅ **Packet Replay** - Store and retransmit (10 slots)  
✅ **Broadcast Decryption** - Tests 14 default PSKs for channel messages  
✅ **Geographic Intelligence** - Extracts GPS coordinates  
✅ **Export Tools** - KML/GeoJSON for mapping

### Encryption Support

**What it decrypts:**
- ✅ Channel/group messages (with channel PSK)
- ✅ Position broadcasts
- ✅ Telemetry data
- ✅ Node information

**What it cannot decrypt:**
- ❌ Direct messages (use PKC with Curve25519)

See `docs/technical/ENCRYPTION.md` for complete details.

### Network Coverage

**Optimized for US frequency plan (902-928 MHz):**
- **Meshtastic** (11 configs): ShortFast/Slow, MedFast/Slow, LongFast/Slow variants
- **LoRaWAN/TTN** (7 configs): The Things Network uplink channels
- **Helium Network** (6 configs): Uplink and downlink frequencies
- **ISM Band** (2 configs): Generic experimental frequencies

---

## 🎨 Typical Workflows

### Research & Analysis

1. **Scan** - Wait ~3 minutes for initial reconnaissance
2. **Discover** - Review detected devices (press 'm' or check Devices tab)
3. **Target** - Select specific device for focused capture
4. **Analyze** - View packet details, signal strength, protocols
5. **Export** - Save data for offline analysis (KML/GeoJSON/CSV)

### Testing & Demonstration

1. **Capture** - Save interesting packets (press 'c' or use web UI)
2. **Replay** - Retransmit saved packets (press 'p' or Packets tab)
3. **Decryption** - Automatic testing of default PSKs
4. **Geographic** - Export GPS data for mapping

### Field Deployment

**Standalone Mode (OLED):**
- No laptop needed
- Button controls all functions
- View stats on display
- SD card logging (optional)

**Mobile Mode (Phone):**
- Connect via WiFi AP
- Full web interface
- Real-time visualization
- Interactive network map

---

## 🔧 Configuration

### WiFi Settings

Default configuration in `firmware/src/main.cpp`:

```cpp
const char* WIFI_AP_SSID = "ESP32-LoRa-Sniffer";  // Change this
const char* WIFI_AP_PASSWORD = "recon123";         // Change this!
const char* MDNS_HOSTNAME = "esp32-lora";
```

**Security Recommendations:**
- ✅ Change default password
- ✅ Use unique SSID per device
- ✅ Disable WiFi when not needed

### Build Flags

All features enabled by default. No build flags needed for:
- PSK testing
- GPS parsing
- Packet replay
- Web interface

### Optional Hardware

**SD Card (Optional):**
- CS pin: GPIO 5
- SPI: SCK=9, MISO=11, MOSI=10
- Enables packet logging to CSV

---

## 🌐 Web API Quick Reference

### REST Endpoints

```bash
# Get system status
GET http://192.168.4.1/api/status

# List discovered devices
GET http://192.168.4.1/api/devices

# Start targeted capture
POST http://192.168.4.1/api/capture/start
Body: {"nodeId": "9EA3D744"}

# Get GPS positions
GET http://192.168.4.1/api/positions

# Export GeoJSON
GET http://192.168.4.1/api/export/geojson

# Get replay slots
GET http://192.168.4.1/api/replay/slots

# Replay packet
POST http://192.168.4.1/api/replay/transmit
Body: {"slotIndex": "0", "repeatCount": "5", "delayMs": "1000"}
```

### WebSocket

```javascript
const ws = new WebSocket('ws://192.168.4.1/ws');

ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    // Handle: packet, deviceUpdate, status events
};
```

**See `API_REFERENCE.md` for complete documentation**

---

## 📚 Additional Documentation

### User Guides
- **`docs/user-guides/FEATURES.md`** - Complete feature documentation
- **`docs/user-guides/BUILD_GUIDE.md`** - Compilation and hardware details
- **`docs/user-guides/TROUBLESHOOTING.md`** - Common issues and solutions

### Technical Documentation
- **`docs/technical/ARCHITECTURE.md`** - Deep dive into v2.0 architecture
- **`docs/technical/ENCRYPTION.md`** - Security and encryption details
- **`docs/technical/NETWORK_HUNTING_GUIDE.md`** - Advanced network analysis

### API & Development
- **`API_REFERENCE.md`** - Complete REST API and WebSocket documentation
- **`docs/api/recon_service.md`** - Reconnaissance service details

---

## 🐛 Troubleshooting

### Cannot Connect to WiFi

**Symptoms:** Phone doesn't see WiFi network

**Solutions:**
1. Check ESP32 powered on (LED should be lit)
2. Wait 10 seconds after boot for WiFi to start
3. Check serial monitor: Should see "Access Point started"
4. Try restarting ESP32
5. Change SSID if conflicts exist

### Web Interface Not Loading

**Symptoms:** Connected to WiFi, but browser can't load page

**Solutions:**
1. Verify IP address: `192.168.4.1`
2. Try `http://esp32-lora.local`
3. Disable cellular data on phone
4. Use `http://` not `https://`
5. Run `pio run --target uploadfs` to upload web files

### No Packets Captured

**Symptoms:** Device scanning but no packets appear

**Solutions:**
1. Check antenna connection
2. Verify frequency plan matches your region
3. Use 'q' for quiet mode (faster capture)
4. Try different location (RF environment)
5. Check serial monitor for errors

### OLED Display Not Working

**Symptoms:** Display blank or garbled

**Solutions:**
1. Check hardware compatibility (Heltec V3 only)
2. Verify I2C connections (SDA=17, SCL=18)
3. Tool continues without display (graceful degradation)
4. Check serial monitor for initialization messages

**See `docs/user-guides/TROUBLESHOOTING.md` for more solutions**

---

## ⚠️ Legal and Ethical Use

This tool is for:
- ✅ Monitoring networks you own or have permission to test
- ✅ Educational and research purposes
- ✅ Security assessment of authorized systems

**NOT for:**
- ❌ Unauthorized monitoring of third-party networks
- ❌ Intercepting private communications
- ❌ Jamming or interfering with radio transmissions
- ❌ Any illegal activities

**Remember:** This is a passive receive-only tool for public ISM band frequencies. Always respect privacy and comply with local regulations.

---

## 🆘 Getting Help

1. **Check documentation** - See `docs/` folder
2. **Review troubleshooting** - See `docs/user-guides/TROUBLESHOOTING.md`
3. **Examine code** - Well-commented source in `firmware/src/`
4. **GitHub issues** - Report bugs or ask questions

---

**Ready to start capturing packets!** 📡
