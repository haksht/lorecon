# 🔌 ESP32 LoRa Sniffer - REST API Reference

**Version:** 1.0.0  
**Base URL:** `http://192.168.4.1` or `http://esp32-lora.local`  
**Protocol:** HTTP/1.1  
**Format:** JSON  

---

## 📋 Table of Contents

1. [Overview](#overview)
2. [Authentication](#authentication)
3. [Response Format](#response-format)
4. [Device Endpoints](#device-endpoints)
5. [Capture Control](#capture-control)
6. [Geographic Data](#geographic-data)
7. [Status & Statistics](#status--statistics)
8. [Scan Control](#scan-control)
9. [WiFi Configuration](#wifi-configuration)
10. [WebSocket API](#websocket-api)
11. [Error Codes](#error-codes)
12. [Rate Limiting](#rate-limiting)
13. [Examples](#examples)

---

## 🌐 Overview

The ESP32 LoRa Sniffer exposes a RESTful API over HTTP for programmatic control and monitoring. All responses are JSON-formatted.

### **Base Configuration**

```
Protocol: HTTP
Host: 192.168.4.1 (default AP mode)
Port: 80
Content-Type: application/json
CORS: Enabled (*)
```

### **Quick Test**

```bash
# Test connectivity
curl http://192.168.4.1/api/status

# Should return JSON with system status
```

---

## 🔐 Authentication

### **Current Implementation**

**None** - No authentication required (suitable for isolated WiFi network)

### **Future Enhancement**

HTTP Basic Authentication planned:

```bash
curl -u username:password http://192.168.4.1/api/devices
```

**Recommendation:** Use strong WiFi password as primary security layer for now.

---

## 📦 Response Format

### **Success Response**

All successful responses follow this structure:

```json
{
  "status": "success",
  "data": { /* endpoint-specific data */ },
  "timestamp": 1699900800000
}
```

### **Error Response**

```json
{
  "status": "error",
  "message": "Human-readable error description",
  "code": "ERROR_CODE",
  "timestamp": 1699900800000
}
```

### **Common Fields**

| Field | Type | Description |
|-------|------|-------------|
| `status` | string | `"success"` or `"error"` |
| `message` | string | Status message (errors only) |
| `timestamp` | number | Unix timestamp (milliseconds) |

---

## 📱 Device Endpoints

### **GET /api/devices**

List all discovered LoRa devices.

**Request:**
```http
GET /api/devices HTTP/1.1
Host: 192.168.4.1
```

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
      "lastSeen": 1699900800000,
      "powerClass": 1,
      "isRouter": false
    },
    {
      "index": 2,
      "nodeId": "A1B2C3D4",
      "nodeIdDecimal": 2714850260,
      "frequency": 915.0,
      "protocol": "Helium",
      "rssi": -72.0,
      "snr": 6.5,
      "packetCount": 18,
      "lastSeen": 1699900750000,
      "powerClass": 2,
      "isRouter": true
    }
  ],
  "timestamp": 1699900800000
}
```

**Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `index` | number | Device index (1-based) |
| `nodeId` | string | Hexadecimal node identifier |
| `nodeIdDecimal` | number | Decimal node ID |
| `frequency` | number | Frequency in MHz |
| `protocol` | string | Detected protocol (Meshtastic, Helium, LoRaWAN, Unknown) |
| `rssi` | number | Signal strength (dBm) |
| `snr` | number | Signal-to-noise ratio (dB) |
| `packetCount` | number | Total packets captured |
| `lastSeen` | number | Last packet timestamp (Unix ms) |
| `powerClass` | number | TX power class (0-4) |
| `isRouter` | boolean | Device acts as router |

**cURL Example:**
```bash
curl -X GET http://192.168.4.1/api/devices
```

---

### **GET /api/device**

Get details for a specific device by node ID.

**Request:**
```http
GET /api/device?nodeId=0x9EA3D744 HTTP/1.1
Host: 192.168.4.1
```

**Query Parameters:**

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `nodeId` | string | Yes | Hex node ID (with or without "0x" prefix) |

**Response:**
```json
{
  "status": "success",
  "device": {
    "index": 1,
    "nodeId": "9EA3D744",
    "nodeIdDecimal": 2662901572,
    "frequency": 906.875,
    "protocol": "Meshtastic",
    "rssi": -68.5,
    "snr": 8.2,
    "packetCount": 42,
    "lastSeen": 1699900800000,
    "powerClass": 1,
    "isRouter": false,
    "packets": [
      {
        "timestamp": 1699900800000,
        "rssi": -68.5,
        "snr": 8.2,
        "length": 64,
        "type": "position"
      }
    ]
  },
  "timestamp": 1699900800000
}
```

**Error Response (Device Not Found):**
```json
{
  "status": "error",
  "message": "Device not found",
  "code": "DEVICE_NOT_FOUND"
}
```

**cURL Example:**
```bash
curl -X GET "http://192.168.4.1/api/device?nodeId=0x9EA3D744"
```

---

## 🎯 Capture Control

### **POST /api/capture/start**

Start targeted packet capture on specific device.

**Request:**
```http
POST /api/capture/start HTTP/1.1
Host: 192.168.4.1
Content-Type: application/json

{
  "nodeId": "9EA3D744"
}
```

**Request Body:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `nodeId` | string | Yes | Hex node ID (without "0x") |

**Response:**
```json
{
  "status": "success",
  "message": "Targeted capture started on device 0x9EA3D744",
  "mode": "targeted",
  "targetNodeId": "9EA3D744",
  "timestamp": 1699900800000
}
```

**Error Response (Invalid Device):**
```json
{
  "status": "error",
  "message": "Device not found or invalid node ID",
  "code": "INVALID_TARGET"
}
```

**cURL Example:**
```bash
curl -X POST http://192.168.4.1/api/capture/start \
  -H "Content-Type: application/json" \
  -d '{"nodeId":"9EA3D744"}'
```

---

### **POST /api/capture/stop**

Stop targeted capture and resume reconnaissance mode.

**Request:**
```http
POST /api/capture/stop HTTP/1.1
Host: 192.168.4.1
```

**Response:**
```json
{
  "status": "success",
  "message": "Capture stopped, returning to reconnaissance mode",
  "mode": "reconnaissance",
  "timestamp": 1699900800000
}
```

**cURL Example:**
```bash
curl -X POST http://192.168.4.1/api/capture/stop
```

---

## 🗺️ Geographic Data

### **GET /api/positions**

Retrieve GPS positions for all devices with location data.

**Request:**
```http
GET /api/positions HTTP/1.1
Host: 192.168.4.1
```

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
      "timestamp": 1699900800000,
      "accuracy": 10,
      "source": "gps"
    },
    {
      "nodeId": "A1B2C3D4",
      "lat": 37.7750,
      "lon": -122.4195,
      "alt": 20,
      "timestamp": 1699900750000,
      "accuracy": 8,
      "source": "gps"
    }
  ],
  "timestamp": 1699900800000
}
```

**Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `nodeId` | string | Device identifier |
| `lat` | number | Latitude (decimal degrees) |
| `lon` | number | Longitude (decimal degrees) |
| `alt` | number | Altitude (meters above sea level) |
| `timestamp` | number | Position timestamp (Unix ms) |
| `accuracy` | number | Position accuracy (meters) |
| `source` | string | Position source (`gps`, `triangulation`) |

**cURL Example:**
```bash
curl -X GET http://192.168.4.1/api/positions
```

---

### **GET /api/export/geojson**

Export positions in GeoJSON format for GIS tools.

**Request:**
```http
GET /api/export/geojson HTTP/1.1
Host: 192.168.4.1
```

**Response:**
```json
{
  "type": "FeatureCollection",
  "features": [
    {
      "type": "Feature",
      "geometry": {
        "type": "Point",
        "coordinates": [-122.4194, 37.7749, 15]
      },
      "properties": {
        "nodeId": "9EA3D744",
        "protocol": "Meshtastic",
        "timestamp": 1699900800000,
        "rssi": -68.5,
        "snr": 8.2
      }
    }
  ]
}
```

**cURL Example:**
```bash
curl -X GET http://192.168.4.1/api/export/geojson > positions.geojson
```

---

### **GET /api/export/kml**

Export positions in KML format for Google Earth.

**Request:**
```http
GET /api/export/kml HTTP/1.1
Host: 192.168.4.1
```

**Response:**
```xml
<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://www.opengis.net/kml/2.2">
  <Document>
    <name>LoRa Device Positions</name>
    <Placemark>
      <name>Device 9EA3D744</name>
      <description>Protocol: Meshtastic, RSSI: -68.5 dBm</description>
      <Point>
        <coordinates>-122.4194,37.7749,15</coordinates>
      </Point>
    </Placemark>
  </Document>
</kml>
```

**cURL Example:**
```bash
curl -X GET http://192.168.4.1/api/export/kml > positions.kml
```

---

### **GET /api/export/pcap**

Downloads Wireshark-compatible PCAP file with LoRa packet captures.

**Request:**
```http
GET /api/export/pcap HTTP/1.1
Host: 192.168.4.1
```

**Response (Success - 200):**
- **Content-Type:** `application/vnd.tcpdump.pcap`
- **Body:** Binary PCAP stream with global header and packet records

**Response (Error - 404):**
```json
{
  "status": "error",
  "message": "No PCAP session active or SD card unavailable"
}
```

**Response (Error - 501):**
```json
{
  "status": "error",
  "message": "PCAP export not enabled in firmware"
}
```

**PCAP Structure:**
- **Global Header:** PCAP 2.4 format with `DLT_USER0` (147) link type
- **Packet Records:** 24-byte LoRa pseudo-header followed by payload
  ```c
  struct LoRaPseudoHeader {
    int16_t rssi;           // Signal strength in dBm
    float snr;              // Signal-to-noise ratio
    uint32_t frequency;     // Frequency in Hz
    uint8_t spreadingFactor; // 7-12
    uint16_t bandwidth;     // kHz (125, 250, 500)
    uint8_t codingRate;     // 5-8
    uint32_t timestamp;     // Unix epoch
    uint16_t payloadLength; // Actual LoRa payload size
  };
  ```

**Wireshark Usage:**
```bash
curl http://192.168.4.1/api/export/pcap -o capture.pcap
wireshark capture.pcap
```

**Dissector Notes:**
- LoRa metadata preserved in pseudo-header for RF analysis
- Custom Wireshark dissector needed to parse 24-byte header
- Payload decryption requires PSK or AES256 keys depending on protocol
- See `docs/technical/ENCRYPTION.md` for decryption workflow

**cURL Example:**
```bash
curl -X GET http://192.168.4.1/api/export/pcap -o lora_capture.pcap
```

---

## 📊 Status & Statistics

### **GET /api/status**

Get current system status and operational mode.

**Request:**
```http
GET /api/status HTTP/1.1
Host: 192.168.4.1
```

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
  "clientCount": 2,
  "scan": {
    "currentConfig": 3,
    "totalConfigs": 16,
    "cyclesCompleted": 2
  },
  "target": {
    "configIndex": 3,
    "frequency": 906.875,
    "protocol": "Meshtastic",
    "bandwidth": 125,
    "spreadingFactor": 10,
    "nodeId": "9EA3D744",
    "deviceType": "TRACKER_APP",
    "rssi": -68.5,
    "packetCount": 42
  }
}
```

**Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `mode` | string | Current mode (`reconnaissance`, `targeted`, `menu`, `replay`) |
| `uptime` | number | System uptime (seconds) |
| `devices` | number | Total discovered devices |
| `totalPackets` | number | Total packets captured |
| `freeHeap` | number | Free RAM (bytes) |
| `heapSize` | number | Total RAM (bytes) |
| `clientCount` | number | Active WebSocket clients connected to the UI |
| `scan.currentConfig` | number | Current frequency config |
| `scan.totalConfigs` | number | Total configs to scan |
| `scan.cyclesCompleted` | number | Full scan cycles completed |
| `target` | object | **Optional** - Only present in targeted capture mode |
| `target.configIndex` | number | Configuration index being monitored |
| `target.frequency` | number | Target frequency in MHz |
| `target.protocol` | string | Protocol name (e.g., "Meshtastic") |
| `target.bandwidth` | number | Bandwidth in kHz |
| `target.spreadingFactor` | number | LoRa spreading factor |
| `target.nodeId` | string | **Optional** - Node ID if targeting specific device |
| `target.deviceType` | string | **Optional** - Device type classification |
| `target.rssi` | number | **Optional** - Current RSSI in dBm |
| `target.packetCount` | number | **Optional** - Packets received from target |

**cURL Example:**
```bash
curl -X GET http://192.168.4.1/api/status
```

---

### **GET /api/statistics**

Get comprehensive statistics and metrics.

**Request:**
```http
GET /api/statistics HTTP/1.1
Host: 192.168.4.1
```

**Response:**
```json
{
  "status": "success",
  "statistics": {
    "totalDevices": 5,
    "totalPackets": 1234,
    "packetsPerSecond": 2.5,
    "avgRSSI": -70.2,
    "avgSNR": 7.8,
    "protocolDistribution": {
      "Meshtastic": 3,
      "Helium": 1,
      "LoRaWAN": 1
    },
    "frequencyDistribution": {
      "906.875": 42,
      "915.0": 18,
      "923.3": 12
    },
    "captureRate": {
      "lastMinute": 150,
      "lastHour": 8400,
      "total": 1234
    }
  },
  "timestamp": 1699900800000
}
```

**cURL Example:**
```bash
curl -X GET http://192.168.4.1/api/statistics
```

---

### **GET /api/config/system**

Get comprehensive system configuration, limits, and current usage statistics.

**Request:**
```http
GET /api/config/system HTTP/1.1
Host: 192.168.4.1
```

**Response:**
```json
{
  "status": "success",
  "scanning": {
    "dwellTimeMs": 12000,
    "numConfigs": 26,
    "cycleTimeMinutes": 5
  },
  "processing": {
    "queueSize": 100,
    "maxPacketSize": 256,
    "metricCacheMs": 100
  },
  "tracking": {
    "maxDevices": 20,
    "maxNodes": 30,
    "maxGeoPoints": 50
  },
  "usage": {
    "devices": 12,
    "nodes": 15,
    "replaySlots": 3,
    "droppedPackets": 5,
    "totalPackets": 1234
  },
  "replay": {
    "maxSlots": 10
  },
  "psk": {
    "numDefaultKeys": 14,
    "keySize": 16
  },
  "ui": {
    "menuTimeoutMs": 300000,
    "buttonLongPressMs": 3000
  },
  "system": {
    "watchdogTimeoutSec": 30,
    "freeHeap": 4194304,
    "minFreeHeap": 3145728,
    "uptimeMs": 3600000
  },
  "hardware": {
    "board": "HELTEC_V3",
    "hasOLED": true,
    "hasSD": false
  },
  "timestamp": 1699900800000
}
```

**Response Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `scanning.dwellTimeMs` | number | Milliseconds spent per frequency config |
| `scanning.numConfigs` | number | Total frequency configurations available |
| `scanning.cycleTimeMinutes` | number | Time for full scan cycle |
| `processing.queueSize` | number | Max packet queue size |
| `tracking.maxDevices` | number | Max targetable devices |
| `tracking.maxNodes` | number | Max tracked unique nodes |
| `usage.devices` | number | Current devices tracked |
| `usage.nodes` | number | Current nodes tracked |
| `usage.droppedPackets` | number | Packets dropped due to queue overflow |
| `psk.numDefaultKeys` | number | Number of default Meshtastic keys |
| `system.freeHeap` | number | Free heap memory in bytes |
| `hardware.hasOLED` | boolean | OLED display present |
| `hardware.hasSD` | boolean | SD card detected |

**Use Cases:**
- Debug queue overflow issues (check `usage.droppedPackets`)
- Monitor memory usage (`system.freeHeap`, `system.minFreeHeap`)
- Verify device/node limits not exceeded
- Check hardware capabilities before feature use

**cURL Example:**
```bash
curl -X GET http://192.168.4.1/api/config/system
```

---

### **GET /api/activity**

Get RF activity analysis per frequency.

**Request:**
```http
GET /api/activity HTTP/1.1
Host: 192.168.4.1
```

**Response:**
```json
{
  "status": "success",
  "activity": [
    {
      "frequency": 906.875,
      "packets": 42,
      "devices": 3,
      "avgRSSI": -68.5,
      "lastActivity": 1699900800000
    },
    {
      "frequency": 915.0,
      "packets": 18,
      "devices": 1,
      "avgRSSI": -72.0,
      "lastActivity": 1699900750000
    }
  ],
  "timestamp": 1699900800000
}
```

**cURL Example:**
```bash
curl -X GET http://192.168.4.1/api/activity
```

---

## 🔍 Scan Control

### **POST /api/scan/start**

Start reconnaissance scanning.

**Request:**
```http
POST /api/scan/start HTTP/1.1
Host: 192.168.4.1
```

**Response:**
```json
{
  "status": "success",
  "message": "Reconnaissance scan started",
  "mode": "reconnaissance",
  "timestamp": 1699900800000
}
```

**cURL Example:**
```bash
curl -X POST http://192.168.4.1/api/scan/start
```

---

### **POST /api/scan/stop**

Stop scanning (enter idle mode).

**Request:**
```http
POST /api/scan/stop HTTP/1.1
Host: 192.168.4.1
```

**Response:**
```json
{
  "status": "success",
  "message": "Scanning stopped",
  "mode": "idle",
  "timestamp": 1699900800000
}
```

---

## 📶 WiFi Configuration

The device supports two WiFi modes:
- **AP Mode (Setup):** Device creates its own WiFi network for initial setup
- **Station Mode:** Device connects to your phone's hotspot, preserving phone features

### **GET /api/wifi/status**

Get current WiFi status and mode.

**Request:**
```http
GET /api/wifi/status HTTP/1.1
Host: 192.168.4.1
```

**Response (Setup Mode):**
```json
{
  "mode": "setup",
  "connected": true,
  "ip": "192.168.4.1",
  "ssid": "LoRa-A1B2C3",
  "rssi": 0,
  "wifiMode": "AP",
  "hasStoredCredentials": false,
  "storedSSID": "",
  "deviceId": "A1B2C3",
  "apSSID": "LoRa-A1B2C3",
  "mdnsHostname": "lora-a1b2c3"
}
```

**Response (Station Mode):**
```json
{
  "mode": "normal",
  "connected": true,
  "ip": "172.20.10.5",
  "ssid": "iPhone",
  "rssi": -54,
  "wifiMode": "STA",
  "hasStoredCredentials": true,
  "storedSSID": "iPhone",
  "deviceId": "A1B2C3",
  "apSSID": "LoRa-A1B2C3",
  "mdnsHostname": "lora-a1b2c3"
}
```

**Field Descriptions:**

| Field | Type | Description |
|-------|------|-------------|
| `mode` | string | `"setup"` (first-run AP mode) or `"normal"` (configured) |
| `connected` | boolean | WiFi connection status |
| `ip` | string | Current IP address |
| `ssid` | string | Current network SSID |
| `rssi` | number | Signal strength in dBm (0 in AP mode) |
| `wifiMode` | string | `"AP"`, `"STA"`, or `"AP_STA"` |
| `hasStoredCredentials` | boolean | Whether hotspot creds are saved |
| `storedSSID` | string | Saved hotspot SSID (empty if none) |
| `deviceId` | string | Unique device ID from MAC (e.g., `"A1B2C3"`) |
| `apSSID` | string | This device's AP network name |
| `mdnsHostname` | string | This device's mDNS hostname |

---

### **POST /api/wifi/configure**

Save WiFi hotspot credentials. Device will restart and connect to the specified network.

**Request:**
```http
POST /api/wifi/configure HTTP/1.1
Host: 192.168.4.1
Content-Type: application/x-www-form-urlencoded

ssid=MyPhoneHotspot&password=secret123
```

**Response:**
```json
{
  "status": "success",
  "message": "Credentials saved. Device restarting to connect to your hotspot...",
  "ssid": "MyPhoneHotspot"
}
```

**Behavior:**
1. Credentials are saved to LittleFS
2. Device restarts after 2 seconds
3. On restart, device attempts to connect to the hotspot
4. If connection fails after 3 attempts, falls back to AP mode

**cURL Example:**
```bash
curl -X POST http://192.168.4.1/api/wifi/configure \
  -d "ssid=MyPhoneHotspot&password=secret123"
```

---

### **POST /api/wifi/clear**

Clear stored credentials and return to setup mode.

**Request:**
```http
POST /api/wifi/clear HTTP/1.1
Host: 192.168.4.1
```

**Response:**
```json
{
  "status": "success",
  "message": "Credentials cleared. Device restarting in setup mode..."
}
```

**Behavior:**
1. Stored credentials are deleted from LittleFS
2. Device restarts after 2 seconds
3. On restart, device enters AP mode (setup mode)

**cURL Example:**
```bash
curl -X POST http://192.168.4.1/api/scan/stop
```

---

## 🔧 Firmware Management

### **POST /api/firmware/upload**

Upload and install new firmware over-the-air (OTA). Device will reboot automatically on success.

**Request:**
```http
POST /api/firmware/upload HTTP/1.1
Host: 192.168.4.1
Content-Type: multipart/form-data; boundary=----WebKitFormBoundary

------WebKitFormBoundary
Content-Disposition: form-data; name="firmware"; filename="firmware.bin"
Content-Type: application/octet-stream

<binary data>
------WebKitFormBoundary--
```

**Response (Success):**
```json
{
  "status": "success",
  "message": "Firmware uploaded successfully. Rebooting in 3 seconds...",
  "timestamp": 1699900800000
}
```

**Response (Failure):**
```json
{
  "status": "error",
  "message": "Firmware upload failed. Check serial output.",
  "timestamp": 1699900800000
}
```

**Important Notes:**
- **File Type:** Only `.bin` files accepted
- **Size Limit:** 2MB maximum (enforced client-side, check partition size)
- **Safety:** Uses ESP32 dual-partition OTA with auto-rollback on boot failure
- **Downtime:** Device will disconnect for ~10 seconds during reboot
- **Power:** Ensure stable power supply during upload to prevent bricking

**cURL Example:**
```bash
curl -X POST -F "firmware=@firmware.bin" http://192.168.4.1/api/firmware/upload
```

**JavaScript Example (with Progress):**
```javascript
const fileInput = document.getElementById('firmware-file');
const file = fileInput.files[0];
const formData = new FormData();
formData.append('firmware', file);

const xhr = new XMLHttpRequest();

xhr.upload.addEventListener('progress', (e) => {
  if (e.lengthComputable) {
    const percent = Math.round((e.loaded / e.total) * 100);
    console.log(`Upload progress: ${percent}%`);
  }
});

xhr.addEventListener('load', () => {
  if (xhr.status === 200) {
    console.log('Firmware uploaded successfully');
    // Wait 10 seconds for reboot, then reconnect
    setTimeout(() => window.location.reload(), 10000);
  }
});

xhr.open('POST', 'http://192.168.4.1/api/firmware/upload');
xhr.send(formData);
```

**Python Example:**
```python
import requests

with open('firmware.bin', 'rb') as f:
    files = {'firmware': ('firmware.bin', f, 'application/octet-stream')}
    response = requests.post('http://192.168.4.1/api/firmware/upload', files=files)
    print(response.json())
```

**Web UI Access:**
- Navigate to Settings tab in web interface
- Select `.bin` file using file picker
- Click "Upload Firmware" button
- Monitor progress bar
- Device will auto-reconnect after reboot

**Build Firmware:**
```powershell
# Using PlatformIO to generate firmware.bin
pio run

# Firmware binary location:
# .pio/build/heltec_wifi_lora_32_V3/firmware.bin
```

---

## 🔌 WebSocket API

### **Connection**

**Endpoint:** `ws://192.168.4.1/ws`

**JavaScript Example:**
```javascript
const ws = new WebSocket('ws://192.168.4.1/ws');

ws.onopen = () => {
  console.log('Connected to ESP32');
};

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log('Received:', data);
};

ws.onerror = (error) => {
  console.error('WebSocket error:', error);
};

ws.onclose = () => {
  console.log('Disconnected from ESP32');
};
```

### **Message Types**

#### **Packet Update**

Sent when new packet received.

```json
{
  "type": "packet",
  "nodeId": "9EA3D744",
  "protocol": "Meshtastic",
  "rssi": -68.5,
  "snr": 8.2,
  "frequency": 906.875,
  "length": 64,
  "timestamp": 1699900800000
}
```

#### **Device Update**

Sent when device state changes.

```json
{
  "type": "deviceUpdate",
  "nodeId": "9EA3D744",
  "packetCount": 43,
  "timestamp": 1699900800000
}
```

#### **Status Update**

Periodic status broadcasts (every 5 seconds).

```json
{
  "type": "status",
  "data": {
    "mode": "reconnaissance",
    "devices": 5,
    "totalPackets": 1234,
    "freeHeap": 204800
  },
  "timestamp": 1699900800000
}
```

---

## ❌ Error Codes

| Code | HTTP Status | Description |
|------|-------------|-------------|
| `DEVICE_NOT_FOUND` | 404 | Requested device does not exist |
| `INVALID_NODE_ID` | 400 | Malformed node ID format |
| `INVALID_REQUEST` | 400 | Missing required parameters |
| `CAPTURE_ACTIVE` | 409 | Cannot start capture (already active) |
| `NOT_IN_CAPTURE_MODE` | 400 | Cannot stop capture (not active) |
| `INTERNAL_ERROR` | 500 | Server-side error |
| `OUT_OF_MEMORY` | 507 | Insufficient heap memory |

**Example Error:**
```json
{
  "status": "error",
  "message": "Device not found",
  "code": "DEVICE_NOT_FOUND",
  "timestamp": 1699900800000
}
```

---

## ⏱️ Rate Limiting

### **Current Implementation**

**None** - No rate limiting currently enforced.

### **Recommendations**

- WebSocket: Max 5 concurrent connections
- REST API: Max 100 requests/minute per client
- Export endpoints: Max 10 requests/minute (resource intensive)

### **Future Implementation**

Rate limiting headers (planned):

```http
X-RateLimit-Limit: 100
X-RateLimit-Remaining: 95
X-RateLimit-Reset: 1699900860
```

---

## 💡 Examples

### **Python - Get Device List**

```python
import requests
import json

# Get all devices
response = requests.get('http://192.168.4.1/api/devices')
data = response.json()

print(f"Found {data['totalDevices']} devices:")
for device in data['devices']:
    print(f"  - {device['nodeId']}: {device['protocol']} @ {device['frequency']} MHz")
```

### **Python - Target Device**

```python
import requests

# Start targeted capture
response = requests.post(
    'http://192.168.4.1/api/capture/start',
    json={'nodeId': '9EA3D744'}
)

if response.json()['status'] == 'success':
    print("Capture started successfully")
```

### **Python - WebSocket Listener**

```python
import asyncio
import websockets
import json

async def listen():
    uri = "ws://192.168.4.1/ws"
    async with websockets.connect(uri) as websocket:
        async for message in websocket:
            data = json.loads(message)
            if data['type'] == 'packet':
                print(f"Packet from {data['nodeId']}: RSSI {data['rssi']} dBm")

asyncio.run(listen())
```

### **JavaScript - Fetch Status**

```javascript
async function getStatus() {
  const response = await fetch('http://192.168.4.1/api/status');
  const data = await response.json();
  
  console.log(`Mode: ${data.mode}`);
  console.log(`Devices: ${data.devices}`);
  console.log(`Uptime: ${data.uptime}s`);
}

getStatus();
```

### **Bash - Export GeoJSON**

```bash
#!/bin/bash

# Export positions to file
curl -s http://192.168.4.1/api/export/geojson > positions.geojson

# Count positions
POSITIONS=$(jq '.features | length' positions.geojson)
echo "Exported $POSITIONS positions"

# Open in QGIS or other GIS tool
qgis positions.geojson
```

### **PowerShell - Monitor Devices**

```powershell
# Get device list
$response = Invoke-RestMethod -Uri "http://192.168.4.1/api/devices"

Write-Host "Total Devices: $($response.totalDevices)"

foreach ($device in $response.devices) {
    Write-Host "$($device.nodeId): $($device.protocol) @ $($device.frequency) MHz"
    Write-Host "  RSSI: $($device.rssi) dBm, Packets: $($device.packetCount)"
}
```

### **Node.js - Real-time Dashboard**

```javascript
const WebSocket = require('ws');

const ws = new WebSocket('ws://192.168.4.1/ws');

ws.on('open', () => {
  console.log('Connected to ESP32');
});

ws.on('message', (data) => {
  const msg = JSON.parse(data);
  
  if (msg.type === 'packet') {
    console.log(`📡 ${msg.nodeId}: ${msg.protocol} @ ${msg.frequency} MHz`);
    console.log(`   RSSI: ${msg.rssi} dBm, SNR: ${msg.snr} dB`);
  }
});

ws.on('error', (error) => {
  console.error('Error:', error);
});
```

---

## 🔗 Related Documentation

- [GETTING_STARTED.md](GETTING_STARTED.md) - Complete setup and usage guide
- [README.md](README.md) - Main project documentation
- [docs/user-guides/FEATURES.md](docs/user-guides/FEATURES.md) - Complete feature list

---

## 📝 Changelog

**v1.0.0** (November 13, 2025)
- Initial API release
- 13 REST endpoints
- WebSocket real-time streaming
- GeoJSON/KML export
- Comprehensive device management

---

**Questions or Issues?**

Report API bugs or feature requests via GitHub Issues.

**Happy Hacking! 🚀**
