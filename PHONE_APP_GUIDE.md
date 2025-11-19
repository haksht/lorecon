# 📱 ESP32 LoRa Sniffer - Phone App Guide

**Version:** 2.0.0  
**Feature Branch:** `feature/phone-app-integration`  
**Date:** November 19, 2025

---

## 🎯 Overview

The ESP32 LoRa Sniffer now includes a **Progressive Web App (PWA)** for wireless control and monitoring via phone or browser. This enables field operation without requiring a laptop or serial connection.

### **Key Features**

✅ **WiFi Access Point** - ESP32 creates its own network  
✅ **REST API** - Complete control via HTTP endpoints  
✅ **WebSocket** - Real-time packet updates  
✅ **Progressive Web App** - Install to phone homescreen  
✅ **Offline Support** - Works without internet connection  
✅ **Device Discovery & Targeting** - View and target devices  
✅ **Packet Replay** - Retransmit captured packets via web UI  
✅ **GPS Positions** - View device locations  
✅ **Live Dashboard** - Real-time statistics and packet feed  
✅ **RF Activity Analysis** - View signal strength and protocol stats  
✅ **Security Assessment** - Vulnerability analysis  
✅ **Device Type Classification** - Identify device categories  

### **Serial vs. Web Feature Parity**

**Full Parity (Available in Both):**
- Device discovery and targeting
- Packet capture and replay
- Frequency targeting
- GPS position viewing
- RF activity analysis
- Security assessment
- Device type analysis
- Resume reconnaissance

**Serial-Only Features:**
- Detailed packet hex dumps
- PSK decryption output (decrypted text shown)
- Verbose diagnostic logging
- Interactive reboot confirmation

**Web-Only Features:**
- Progressive Web App installation
- Multi-client simultaneous access
- GeoJSON/KML export from browser
- Mobile-responsive touch interface  

---

## 🚀 Quick Start (5 Minutes)

### **Step 1: Build & Upload Firmware**

```bash
# From project root
cd c:\Users\tim\lora\esp32-sniffer

# Ensure on feature branch
git status

# Build and upload
pio run --target upload
```

### **Step 2: Connect Phone to ESP32**

1. **Power on ESP32** - Wait ~5 seconds for boot
2. **Open WiFi settings** on your phone
3. **Connect to network:**
   - **SSID:** `ESP32-LoRa-Sniffer`
   - **Password:** `recon123`
4. **Wait for connection** - You should see "Connected"

### **Step 3: Open Web App**

**Method 1: Direct IP**
- Open browser: `http://192.168.4.1`

**Method 2: mDNS (easier to remember)**
- Open browser: `http://esp32-lora.local`

**Method 3: Install as App**
- Open browser to above URL
- Tap **Share** → **Add to Home Screen**
- App icon appears on homescreen
- Tap icon to launch (looks like native app!)

### **Step 4: Start Using**

- View discovered devices in **Devices** tab
- See GPS positions on **Map** tab
- Monitor RF activity in **Activity** tab
- Watch live packet feed on **Dashboard**

---

## 🏗️ Architecture

### **System Components**

```
┌─────────────────┐          ┌─────────────────┐
│   Phone/Browser │          │     ESP32-S3    │
│                 │          │                 │
│  Progressive    │  WiFi    │  LoRa Radio +   │
│  Web App        │ ◄──────► │  Web Server +   │
│  (HTML/JS/CSS)  │          │  WiFi AP        │
│                 │          │                 │
└─────────────────┘          └─────────────────┘
     ↓                              ↓
 IndexedDB Storage          Packet Capture
 (100MB+ local)            & Analysis
```

### **Communication Channels**

**REST API** (HTTP)
- `GET /api/devices` - List discovered devices
- `POST /api/capture/start` - Target specific device
- `GET /api/positions` - Get GPS coordinates
- `GET /api/status` - System status
- Full API documented below

**WebSocket** (Real-time)
- Live packet updates
- Device discovery notifications
- Status changes
- Minimal latency (<100ms)

---

## 📡 WiFi Configuration

### **Default Settings (Change These!)**

Located in `firmware/src/main.cpp`:

```cpp
const char* WIFI_AP_SSID = "ESP32-LoRa-Sniffer";  // Change this
const char* WIFI_AP_PASSWORD = "recon123";         // Change this!
const char* MDNS_HOSTNAME = "esp32-lora";          // For .local access
```

### **Security Recommendations**

**For Field Testing:**
- ✅ Use WPA2 password (default: enabled)
- ✅ Change default password
- ✅ Use unique SSID per device

**For Production/Research:**
- ✅ Add HTTP Basic Authentication
- ✅ Use strong passwords (16+ characters)
- ✅ Consider VPN or SSH tunnel
- ✅ Disable WiFi when not needed

### **WiFi Modes**

**Access Point (Default)**
- ESP32 creates network
- No existing WiFi needed
- IP: `192.168.4.1`
- Range: ~50 meters

**Station Mode (Alternative)**
- ESP32 joins existing WiFi
- Shares network with other devices
- IP: Assigned by DHCP
- Modify code to enable

---

## 🌐 REST API Reference

### **Device Management**

#### `GET /api/devices`
List all discovered devices

**Response:**
```json
{
  "status": "success",
  "totalDevices": 3,
  "devices": [
    {
      "index": 1,
      "nodeId": "9EA3D744",
      "nodeIdDecimal": 2662901572,
      "frequency": 906.875,
      "protocol": "Meshtastic",
      "rssi": -68.5,
      "snr": 8.2,
      "packetCount": 42,
      "lastSeen": 12345678,
      "powerClass": 1,
      "isRouter": false
    }
  ]
}
```

#### `GET /api/device?nodeId=0x9EA3D744`
Get specific device details

#### `POST /api/capture/start`
Start targeted capture

**Request Body:**
```json
{
  "nodeId": "9EA3D744"
}
```

**Response:**
```json
{
  "status": "success",
  "message": "Targeted capture started on device 0x9EA3D744"
}
```

#### `POST /api/capture/stop`
Stop capture and resume reconnaissance

### **Packet Replay**

#### `GET /api/replay/slots`
Get all captured packets

**Response:**
```json
{
  "status": "success",
  "count": 3,
  "capacity": 10,
  "slots": [
    {
      "index": 0,
      "valid": true,
      "protocol": "Meshtastic",
      "length": 64,
      "nodeId": "9EA3D744",
      "frequencyMHz": 906.875,
      "rssi": -68.5,
      "configIndex": 0,
      "capturedSecondsAgo": 120,
      "decryptedText": "Hello World"
    }
  ]
}
```

#### `POST /api/replay/transmit`
Replay (retransmit) a captured packet

**Request Body:**
```json
{
  "slotIndex": "0",
  "repeatCount": "5",
  "delayMs": "1000"
}
```

**Parameters:**
- `slotIndex` (required): Packet slot index (0-9)
- `repeatCount` (optional): Number of times to transmit (1-100, default: 1)
- `delayMs` (optional): Delay between transmissions in ms (100-10000, default: 1000)

**Response:**
```json
{
  "status": "success",
  "message": "Replay complete: 5/5 successful"
}
```

**Notes:**
- Uses original packet's radio configuration (frequency, SF, BW)
- Transmission power: 10 dBm
- Each transmission takes 0.5-5 seconds depending on spreading factor
- Multiple repeats useful for testing reliability or battery drain attacks

#### `POST /api/replay/clear`
Clear all captured packets

**Response:**
```json
{
  "status": "success",
  "message": "Replay slots cleared"
}
```

### **Scan Control**

#### `POST /api/frequency/target`
Target a specific frequency configuration

**Request Body:**
```json
{
  "configIndex": "3"
}
```

**Response:**
```json
{
  "status": "success",
  "message": "Frequency targeting started on Meshtastic_MedFast (906.875 MHz)"
}
```

### **Analysis Endpoints**

#### `GET /api/recon/summary`
Get reconnaissance summary including RF activity

**Response:**
```json
{
  "status": "success",
  "summary": {
    "reconDurationSeconds": 180,
    "targetableDevices": 5,
    "nodesTracked": 12,
    "totalPackets": 1234
  },
  "rfActivity": [
    {
      "configIndex": 0,
      "protocol": "Meshtastic_ShortFast",
      "frequency": 906.875,
      "packets": 42,
      "avgRSSI": -68.5,
      "peakRSSI": -55.2
    }
  ]
}
```

#### `GET /api/recon/device-types`
Get device type classification

**Response:**
```json
{
  "status": "success",
  "summary": {
    "totalDeviceTypes": 3,
    "totalDevices": 8,
    "routersDetected": 2
  },
  "deviceTypes": [
    {
      "type": "TRACKER_APP",
      "count": 5,
      "avgRSSI": -65.3,
      "routers": 0
    }
  ]
}
```

#### `GET /api/recon/security`
Get security vulnerability assessment

**Response:**
```json
{
  "status": "success",
  "summary": {
    "status": "vulnerabilities_detected",
    "vulnerable": 3,
    "moderate": 2,
    "secure": 0
  },
  "devices": [
    {
      "nodeId": "9EA3D744",
      "deviceType": "TRACKER_APP",
      "riskLevel": "high",
      "bestRSSI": -68.5,
      "packetCount": 42
    }
  ]
}
```

### **Geographic Data**

#### `GET /api/positions`
Get all GPS positions

**Response:**
```json
{
  "status": "success",
  "totalPositions": 5,
  "positions": [
    {
      "nodeId": "9EA3D744",
      "lat": 37.7749,
      "lon": -122.4194,
      "alt": 15,
      "timestamp": 1699900800000
    }
  ]
}
```

#### `GET /api/export/geojson`
Export positions as GeoJSON

#### `GET /api/export/kml`
Export positions as KML (Google Earth)

### **Status & Statistics**

#### `GET /api/status`
System status

**Response:**
```json
{
  "status": "success",
  "mode": "reconnaissance",
  "uptime": 3600,
  "devices": 5,
  "totalPackets": 1234,
  "freeHeap": 204800,
  "heapSize": 327680,
  "scan": {
    "currentConfig": 3,
    "totalConfigs": 16,
    "cyclesCompleted": 2
  }
}
```

#### `GET /api/statistics`
Comprehensive statistics

#### `GET /api/activity`
RF activity analysis

### **Scan Control**

#### `POST /api/scan/start`
Start reconnaissance scan

#### `POST /api/scan/stop`
Stop scanning

---

## 🔌 WebSocket Events

### **Connection**

```javascript
const ws = new WebSocket('ws://192.168.4.1/ws');

ws.onopen = () => {
  console.log('Connected to ESP32');
};

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  handleEvent(data);
};
```

### **Event Types**

**Packet Update**
```json
{
  "type": "packet",
  "nodeId": "9EA3D744",
  "protocol": "Meshtastic",
  "rssi": -68.5,
  "snr": 8.2,
  "length": 64,
  "timestamp": 1699900800000
}
```

**Device Update**
```json
{
  "type": "deviceUpdate",
  "nodeId": "9EA3D744",
  "timestamp": 1699900800000
}
```

**Status Update**
```json
{
  "type": "status",
  "data": { /* same as GET /api/status */ },
  "timestamp": 1699900800000
}
```

---

## 📱 Progressive Web App Features

### **Dashboard**
- **Live packet feed** - Real-time packet reception
- **Statistics cards** - Devices, packets, uptime
- **System status** - Mode, heap, connection

### **Devices Tab**
- **Device list** - All discovered devices
- **Device cards** - RSSI, SNR, packet count
- **Target action** - Start targeted capture on specific device
- **Signal quality indicators** - Visual signal strength

### **Packets Tab**
- **Captured packets** - View all replay slots
- **Packet details** - Protocol, node ID, RSSI, frequency, age
- **Replay action** - Click any packet to retransmit
- **Repeat count** - Configure multiple transmissions
- **Delay configuration** - Set delay between repeats
- **Clear function** - Clear all replay slots

### **Frequency Tab**
- **Configuration list** - All 16 scan configs
- **Activity indicators** - Shows which frequencies have traffic
- **Direct targeting** - Lock onto specific frequency/SF/BW combination
- **Protocol details** - Spreading factor and bandwidth info

### **GPS Tab**
- **Position list** - All captured GPS coordinates
- **Node tracking** - See which devices broadcast location
- **Altitude data** - Included when available
- **Export** - GeoJSON, KML formats

### **Activity Tab**
- **RF activity** - Per-frequency statistics
- **Protocol distribution** - Device types and counts
- **Signal analysis** - RSSI averages and peaks
- **Router detection** - Identifies mesh routing nodes

### **Offline Support**
- Service Worker caches app for offline use
- Works without internet connection
- Only ESP32 WiFi needed

---

## 🛠️ Development & Customization

### **File Structure**

```
firmware/src/
├── wifi_manager.h/.cpp       # WiFi AP/Station management
├── web_server.h/.cpp          # HTTP + WebSocket server
├── api_controller.h/.cpp      # REST endpoint handlers
└── main.cpp                   # WiFi & server initialization

data/webapp/
├── index.html                 # Main app HTML
├── manifest.json              # PWA manifest
├── sw.js                      # Service worker
├── css/
│   └── style.css              # App styling
├── js/
│   ├── app.js                 # Main app logic
│   ├── api-client.js          # API communication
│   └── map.js                 # Map handling
└── icons/                     # PWA icons
```

### **Customizing the UI**

**Colors** (`data/webapp/css/style.css`):
```css
:root {
    --primary-color: #4a90e2;      /* Main accent color */
    --secondary-color: #7b68ee;    /* Secondary accent */
    --dark-bg: #1a1a2e;            /* Background */
    --card-bg: #16213e;            /* Card background */
}
```

**Update Frequency** (`data/webapp/js/app.js`):
```javascript
this.updateInterval = setInterval(async () => {
    await this.updateStatus();
}, 5000);  // Change to desired interval (ms)
```

### **Adding New API Endpoints**

1. **Add handler in `api_controller.cpp`:**
```cpp
String APIController::getMyNewData() {
    JsonDocument doc;
    doc["status"] = "success";
    doc["data"] = "my data";
    
    String response;
    serializeJson(doc, response);
    return response;
}
```

2. **Register route in `web_server.cpp`:**
```cpp
server->on("/api/mydata", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = APIController::getMyNewData();
    request->send(200, "application/json", json);
});
```

3. **Call from frontend:**
```javascript
async getMyNewData() {
    return await this.request('/mydata');
}
```

---

## 🐛 Troubleshooting

### **Cannot Connect to WiFi**

**Symptoms:** Phone doesn't see WiFi network

**Solutions:**
1. Check ESP32 powered on (LED should be lit)
2. Wait 10 seconds after boot for WiFi to start
3. Check serial monitor: Should see "Access Point started"
4. Try restarting ESP32
5. Check for WiFi conflicts (change SSID if needed)

### **Can Connect to WiFi but Not Web**

**Symptoms:** Connected to WiFi, but browser can't load page

**Solutions:**
1. Verify IP address: `192.168.4.1` (default for AP mode)
2. Try `http://esp32-lora.local` (mDNS)
3. Disable cellular data on phone (force WiFi use)
4. Check browser address: Must use `http://` not `https://`
5. Check serial monitor for web server status

### **WebSocket Not Connecting**

**Symptoms:** "Disconnected from ESP32" message

**Solutions:**
1. Check if HTTP API works first (try `/api/status`)
2. Verify WebSocket URL in browser console
3. Check firewall settings
4. Try different browser (Chrome/Safari recommended)
5. Check serial monitor for connection logs

### **Map Not Showing**

**Symptoms:** Map tab is blank or shows error

**Solutions:**
1. Check internet connection (Leaflet.js loads from CDN)
2. Wait for GPS positions to be captured
3. Check browser console for JavaScript errors
4. Verify `/api/positions` returns data
5. Try refreshing page

### **PWA Not Installing**

**Symptoms:** "Add to Home Screen" option missing

**Solutions:**
1. Use HTTPS or localhost (or phone connected to ESP32 directly)
2. Try Chrome or Safari (best PWA support)
3. Check manifest.json loads correctly
4. Verify service worker registered (browser dev tools)
5. Try clearing browser cache

---

## 📊 Performance Considerations

### **Memory Usage**

**ESP32 Heap:**
- Web server: ~40KB
- WiFi stack: ~50KB
- WebSocket per client: ~10KB
- Total overhead: ~100KB

**Recommendations:**
- Limit WebSocket clients to 5 simultaneous
- Use packet queue wisely (current: 10 packets)
- Monitor free heap in dashboard

### **Battery Life**

**Power Consumption:**
- LoRa only: ~100mA
- LoRa + WiFi AP: ~180mA
- LoRa + WiFi + client: ~220mA

**Battery Tips:**
- Disable WiFi when not needed (serial still works)
- Use deep sleep between scans (future feature)
- Power off display when using phone app

### **WiFi Range**

**Expected Range:**
- Indoor: 20-30 meters
- Outdoor: 50-100 meters
- With external antenna: 100+ meters

**Tips:**
- Keep ESP32 elevated for better range
- Avoid metal enclosures
- Use directional antenna if needed

---

## 🔐 Security Best Practices

### **For Testing**
- ✅ Change default WiFi password
- ✅ Use WPA2 encryption
- ✅ Unique SSID per device
- ✅ Disable when not in use

### **For Production**
- ✅ Add HTTP authentication
- ✅ Use strong passwords (16+ chars)
- ✅ Enable HTTPS (self-signed cert OK)
- ✅ Implement rate limiting
- ✅ Log all API access
- ✅ Consider VPN tunnel

### **For Public Demos**
- ✅ Use isolated network
- ✅ Disable sensitive endpoints
- ✅ Read-only mode
- ✅ Clear stored data between demos

---

## 🎯 Use Cases

### **Field Research**
- Walk around with phone in pocket
- Monitor packet capture in real-time
- Target devices without laptop
- Export GPS data on-site

### **Security Auditing**
- Present findings on tablet
- Live demonstration of captures
- Show vulnerability in real-time
- Professional visualization

### **Conference Demos**
- Audience can connect via WiFi
- Multiple viewers simultaneously
- No cables or serial connections
- Interactive Q&A with live data

### **Educational**
- Students can use own phones
- No special software required
- Visual, interactive learning
- Real-time radio experimentation

---

## 🚀 Future Enhancements

### **Planned Features**
- [ ] User authentication system
- [ ] Session recording/playback
- [ ] Advanced packet filtering
- [ ] Custom alert triggers
- [ ] Multi-language support
- [ ] Dark/light theme toggle
- [ ] CSV export from web UI
- [ ] Historical graphs
- [ ] Device fingerprinting UI
- [ ] Network topology visualization

### **Community Requests**
- Submit feature requests via GitHub Issues
- Contribute via pull requests
- Share UI improvements
- Report bugs and UX issues

---

## 📚 Additional Resources

### **Related Documentation**
- [README.md](../README.md) - Main project documentation
- [QUICKSTART.md](../QUICKSTART.md) - Getting started guide
- [FEATURES.md](docs/FEATURES.md) - Complete feature list
- [BUILD_GUIDE.md](docs/BUILD_GUIDE.md) - Compilation instructions

### **External Resources**
- [Leaflet.js Documentation](https://leafletjs.com/)
- [Progressive Web Apps Guide](https://web.dev/progressive-web-apps/)
- [WebSocket API](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket)
- [ESP32 AsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)

---

## ✅ Verification Checklist

Before deploying:

- [ ] WiFi password changed from default
- [ ] Serial monitor shows "Web server started"
- [ ] Phone can connect to WiFi
- [ ] Browser can load `http://192.168.4.1`
- [ ] Dashboard shows live statistics
- [ ] WebSocket connects (green indicator)
- [ ] Device list populates
- [ ] Map tab initializes
- [ ] API endpoints respond
- [ ] PWA can install to homescreen

---

## 🆘 Getting Help

**Quick Checks:**
1. Read troubleshooting section above
2. Check serial monitor output
3. Verify browser console (F12) for errors
4. Test API with `curl` commands

**Support Channels:**
- GitHub Issues: Report bugs or feature requests
- Serial Monitor: Primary debugging output
- Browser DevTools: Frontend debugging
- Community Forums: User discussions

---

**Happy Sniffing! 📡**

*This phone app makes field reconnaissance more accessible and professional than ever before. Enjoy the wireless freedom!*
