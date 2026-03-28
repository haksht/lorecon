# Getting Started with ESP32 LoRa Sniffer

**Version:** 2.3.2
**Status:** ✅ Ready to Use
**Last Updated:** March 25, 2026

---

## 🚀 Quick Start (5 Minutes)

### Prerequisites
- One of the supported boards:
  - **Heltec WiFi LoRa 32 V3 or V4** (ESP32-S3 + SX1262 + OLED; use `heltec_v3` env, or `heltec_v4` if your V4 has the L76K GPS module)
  - **LilyGO T3-S3 V1.2/V1.3** (ESP32-S3 + SX1262 + OLED + native SD slot)
  - **LilyGO T-Beam Supreme** (ESP32-S3 + SX1262 + SH1106 OLED + SD + GPS + AXP2101 PMIC)
- USB cable
- 902-928 MHz antenna (US ISM band)

### Option A: Pre-compiled Binary (Easiest)

**No PlatformIO required!**

1. Download from [Releases](https://github.com/tiarno/esp32-sniffer/releases/latest)
2. Extract `esp32-lora-sniffer-v2.3.1-binaries.zip`
3. Install esptool: `pip install esptool`
4. Run `flash.bat` (Windows) or `./flash.sh` (Linux/Mac):
   ```
   flash.bat heltec_v3 COM3
   ```
   Replace `COM3` with your device's port. On Windows, check **Device Manager → Ports (COM & LPT)** to find it. If you only have one device plugged in you can omit the port and it will auto-detect.
5. Done! Skip to "First Run - WiFi Setup" below.

### Option B: Build from Source

Requires PlatformIO installed.

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

### 2. First Run - WiFi Setup

On first boot, your device creates a **unique WiFi network**:

```
╔═══════════════════════════════════════════════╗
║          WIFI SETUP MODE                      ║
╠═══════════════════════════════════════════════╣
║  1. Connect phone to: LoRa-A1B2C3             ║
║  2. Config page opens automatically!          ║
║  3. Enter your hotspot credentials            ║
╚═══════════════════════════════════════════════╝
```

**Important:** Each device has a unique network name based on its MAC address (e.g., `LoRa-A1B2C3`). This prevents conflicts when multiple devices are used at conferences!

**What happens when you connect:**
1. Your phone connects to the device's WiFi (e.g., `LoRa-A1B2C3`)
2. A **captive portal** automatically opens the setup page (like hotel WiFi!)
3. Enter your phone's hotspot name and password
4. Device restarts and connects to YOUR hotspot
5. Now you keep SMS, calls, and internet while using the sniffer!

---

## 📱 WiFi Configuration Options

### Option A: Phone Hotspot Mode (Recommended)

**Best for:** Field work, conferences, mobile use

Your phone acts as the WiFi hotspot. The ESP32 connects to your phone.

**Benefits:**
- ✅ Keep receiving SMS, calls, notifications
- ✅ Phone keeps internet access
- ✅ No laptop needed
- ✅ Works anywhere with your phone

**Setup:**
1. Power on ESP32 (shows unique SSID like `LoRa-A1B2C3`)
2. Connect phone to `LoRa-XXXXXX` WiFi (password: `recon-XXXXXX` matching your device ID)
3. Setup page opens automatically (captive portal)
4. Enter your phone's hotspot name/password
5. Device restarts → connects to your hotspot
6. Access web UI at the IP shown on serial/OLED

**Note:** Each device has a unique password derived from its MAC address (e.g., `recon-A1B2C3`). Check serial output at boot for the exact password.

### Option B: Access Point Mode (Default Fallback)

**Best for:** Quick testing, demos, when hotspot unavailable

The ESP32 creates its own WiFi network.

**Limitations:**
- ❌ Phone disconnects from cellular
- ❌ No SMS/calls while connected
- ❌ No internet access

**Usage:**
1. Connect to `LoRa-XXXXXX` WiFi
2. Open `http://192.168.4.1`
3. Use the web interface directly

### Option C: Pre-Provisioned Credentials (Developer)

**Best for:** Headless deployment, testing, skip setup wizard

**Method 1: Legacy JSON file** (migrated to NVS on first boot)

Create `data/wifi_creds.json` before uploading filesystem:

```json
{
  "ssid": "YourHotspotSSID",
  "password": "YourPassword"
}
```

Then upload: `pio run --target uploadfs`

> **Note:** Credentials are automatically migrated to secure NVS (Non-Volatile Storage) on first boot. The JSON file is then deleted. All subsequent credential operations use NVS.

Device will auto-connect on boot. See [API_REFERENCE.md](API_REFERENCE.md#pre-provisioned-credentials-optional) for details.

### Switching Modes

**To reconfigure WiFi (return to setup mode):**
1. Go to **Settings** tab in web UI
2. Click **Clear Credentials**
3. Device restarts in AP mode for reconfiguration

**Or via API:**
```bash
curl -X POST http://192.168.4.1/api/wifi/clear -H "X-API-Token: YOUR_TOKEN"
```

---

## 🎯 Three Ways to Use

### Option 1: Serial Interface (USB)

Connect via USB and use serial commands:

**Important:** Serial commands require activation. Press **Enter twice** within 1.5 seconds to activate the serial console, then commands work normally. This prevents phantom commands from USB electrical noise.

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
Press 's' - Show summary
Press 'v' - Security assessment
Press 'l' - Clear captured packets
Press 'n' - Clear discovered devices
Press 'd' - Device type analysis
Press 'k' - Export KML
Press 'j' - Export GeoJSON
Press 't' - Show API token
Press 'w' - LoRaWAN key testing stats
Press 'b' - Reboot device
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

#### After Initial Setup

Once configured for your hotspot:

1. Enable phone hotspot
2. Power on ESP32 (auto-connects)
3. Check serial output or OLED for IP address
4. Open browser to that IP (or `http://lora-xxxxxx.local`)

#### Web Interface Features

**7 Interactive Tabs:**

1. **Info** - System overview, GPS data, security assessment
2. **Devices** - Discovered devices sorted by vulnerability
3. **Packets** - Packet history with replay controls
4. **Frequency** - 26 scan configurations
5. **Network** - Interactive topology map
6. **Stats** - Protocol charts (War Room)
7. **Settings** - WiFi config, system settings, OTA updates

**Best for:** Field deployment, mobile monitoring, demonstrations

---

## � API Authentication

### Why API Tokens?

Destructive actions (packet replay, WiFi reset, device targeting) require authentication to prevent unauthorized remote control of your device. **Read-only endpoints (devices, status, packets) are public** for easy visualization.

### Finding Your Token

The API token is displayed at boot in **serial output**:

```
[INFO] API authentication: ENABLED
[INFO] Token: a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6
[INFO] Use header: X-API-Token: <token>
```

### Using the Token

**Web UI**: The built-in web interface handles authentication automatically.

**Python Tools**: The tools in `tools/` use the token via query parameter:
```bash
python api_client.py --host 192.168.4.1 --token YOUR_TOKEN replay 0
```

**cURL/HTTP**:
```bash
# Protected endpoint (requires token)
curl -H "X-API-Token: YOUR_TOKEN" http://192.168.4.1/api/replay/transmit -d '{"slotIndex":0}'

# Public endpoint (no token needed)
curl http://192.168.4.1/api/devices
```

### Protected vs Public Endpoints

| Endpoint Type | Authentication | Examples |
|---------------|----------------|----------|
| **Public (GET)** | None | `/api/status`, `/api/devices`, `/api/packets` |
| **Protected (POST)** | Token required | `/api/replay/transmit`, `/api/capture/start`, `/api/wifi/clear` |

### Regenerating Token

If your token is compromised:
1. Go to **Settings** tab → **Security** section
2. Click **Regenerate Token**
3. New token appears in serial output

### Disabling Authentication (Demo Mode)

For demos where security isn't needed, edit `firmware/src/config.h`:
```cpp
constexpr bool AUTH_ENABLED = false;  // Change from true
```
Rebuild and upload. All endpoints become public.

---

## �📡 What This Tool Does

### Core Capabilities

✅ **Reconnaissance** - Scans 26 LoRa configurations automatically  
✅ **Packet Capture** - Interrupt-driven, real-time reception  
✅ **Device Discovery** - Identifies Meshtastic nodes and LoRaWAN devices  
✅ **Protocol Analysis** - Parses packet structures  
✅ **Packet Replay** - Store and retransmit (10 slots)  
✅ **Broadcast Decryption** - Tests 23 default/common PSKs including legacy admin channel keys  
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

WiFi credentials are stored automatically in device flash memory. No code changes needed!

**First-time setup:**
1. Device creates unique AP: `LoRa-XXXXXX` (based on MAC address)
2. Connect and enter your hotspot credentials via web UI
3. Credentials saved permanently until cleared

**Configuration files:**
- Credentials stored in: `/wifi_config.json` (LittleFS)
- Default AP password: `recon-XXYYZZ` (device-unique, matches SSID suffix)
- Unique device ID derived from: Last 3 bytes of MAC address

**To change stored credentials:**
- Web UI: Settings → Clear Credentials → Reconfigure
- Serial: Clear LittleFS and re-upload

**Build-time defaults** in `firmware/src/config.h`:
```cpp
namespace Config::WiFi {
    constexpr const char* AP_SSID_PREFIX = "LoRa-";              // + MAC suffix
    constexpr const char* DEFAULT_AP_PASSWORD_PREFIX = "recon-"; // + MAC suffix (device-unique)
    constexpr const char* MDNS_PREFIX = "lora-";                 // + MAC suffix
}
```

### Build Flags

All features enabled by default. No build flags needed for:
- PSK testing
- GPS parsing
- Packet replay
- Web interface

### Optional Hardware

**SD Card (Optional):**
- Pins are board-specific (all use a dedicated SPI bus separate from LoRa):
  - **T3-S3**: CS=13, SCK=14, MISO=2, MOSI=11 (native slot, no mod needed)
  - **T-Beam Supreme**: CS=47, SCK=36, MISO=37, MOSI=35 (native slot)
  - **Heltec V3/V4**: requires external SD module (no native slot)
- Enables packet logging to CSV and PCAP

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
1. Check serial monitor for `[DISPLAY]` initialization messages
2. Verify I2C pins match your board:
   - **Heltec V3/V4**: uses default I2C pins (auto-configured)
   - **T3-S3**: SDA=18, SCL=17
   - **T-Beam Supreme**: SDA=17, SCL=18
3. Tool continues without display (graceful degradation)
4. Check that the OLED is powered (T-Beam Supreme requires AXP2101 PMIC to be initialized first)

**See `docs/user-guides/TROUBLESHOOTING.md` for more solutions**

---

## 🏢 Conference & Multi-Device Deployment

When deploying multiple devices at conferences, workshops, or training:

### Unique Device Identification

Each device automatically generates unique identifiers from its MAC address:

| Component | Example | Purpose |
|-----------|---------|---------|
| **Device ID** | `A1B2C3` | Short identifier shown in UI |
| **WiFi SSID** | `LoRa-A1B2C3` | Unique network name |
| **mDNS** | `lora-a1b2c3.local` | DNS hostname |

**No configuration needed!** Each device is unique out of the box.

### For Workshop Organizers

**Pre-event preparation:**
1. Flash all devices with same firmware
2. Power on each device once to verify unique SSID
3. Optionally label devices with their ID (shown in serial output)

**Participant instructions:**
1. "Look for WiFi networks starting with `LoRa-`"
2. "Connect to the one matching your device label"
3. "The setup page opens automatically"
4. "Enter YOUR phone's hotspot credentials"

### Finding Your Device

If you have multiple `LoRa-XXXXXX` networks visible:

1. **Check the device label** (if organizer labeled them)
2. **Check serial output**: Shows `Device ID: A1B2C3` on boot
3. **Check OLED**: Device ID can be shown in status display
4. **Try connecting**: Wrong device? Disconnect and try another

### Best Practices for Events

- ✅ **Label devices** with their ID before distributing
- ✅ **Test each device** boots to unique SSID
- ✅ **Prepare handout** with quick setup instructions
- ✅ **Have USB cables** for serial debugging if needed

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
