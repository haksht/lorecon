# 🔌 ESP32 LoRa Sniffer - REST API Reference

**Version:** 2.3.1
**Base URL:** `http://192.168.4.1` or `http://lora-XXXXXX.local`  
**Protocol:** HTTP/1.1  
**Format:** JSON  
**Authentication:** Token-based for protected endpoints (see [Authentication](#-authentication))  

---

## 📋 Table of Contents

1. [Overview](#overview)
2. [Authentication](#authentication)
3. [Response Format](#response-format)
4. [Quick Reference - All Endpoints](#quick-reference---all-endpoints)
5. [Device Endpoints](#device-endpoints)
6. [Capture Control](#capture-control)
7. [Geographic Data](#geographic-data)
8. [Status & Statistics](#status--statistics)
9. [Security Assessment](#security-assessment)
10. [Scan Control](#scan-control)
11. [WiFi Configuration](#wifi-configuration)
12. [WebSocket API](#websocket-api)
13. [Error Codes](#error-codes)
14. [Rate Limiting](#rate-limiting)
15. [Examples](#examples)

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

### **Token-Based Authentication**

**Version 2.2.0+** implements token-based API authentication for sensitive endpoints.

### **Private Network Auto-Trust**

Auto-trust applies **only when the device is connected to an external network (STA mode)** and the client is on that same private network. Clients on the device's own AP subnet (`192.168.4.x`) are always required to authenticate with a token, regardless of RFC 1918 address.

**Rationale:** Anyone who knows the AP password gets a `192.168.4.x` address — granting auto-trust there would allow unauthenticated access to sensitive endpoints. Auto-trust is reserved for users already on a trusted LAN (home router, phone hotspot the device joined).

This means:
- ✅ Home WiFi (192.168.1.x) — auto-authenticated (device in STA/AP_STA mode, client on same LAN)
- ✅ Phone hotspot the device joined (172.20.x.x, 192.168.x.x) — auto-authenticated
- ❌ Direct AP connection (192.168.4.x) — **requires token** (device acting as hotspot, client connected to it)
- ❌ Public internet — requires token header (if you ever port-forward)

**In practice:** If you connect your phone directly to the device's WiFi hotspot (`LoRa-XXXXXX`), you will be prompted to enter the API token the first time you perform a protected action. After entry, it is saved in your browser's `localStorage` and you won't be asked again on that device.

### **How It Works**

1. A unique 32-character hex token is generated at first boot
2. Token is stored persistently in ESP32 NVS (survives reboots)
3. Token is displayed in serial output at startup
4. Protected endpoints require the `X-API-Token` header

### **Getting Your Token**

The API token is displayed at boot in serial output:

```
🔐 API Security Enabled
  Token: a1b2c3d4e5f6789012345678abcdef01
  Header: X-API-Token
  Protected endpoints require this token
```

### **Using the Token**

```bash
# Protected endpoint (requires auth)
curl -X POST http://192.168.4.1/api/devices/clear \
     -H "X-API-Token: a1b2c3d4e5f6789012345678abcdef01"

# Public endpoint (no auth needed)
curl http://192.168.4.1/api/devices
```

### **Protected Endpoints**

| Endpoint | Method | Reason |
|----------|--------|--------|
| `/api/devices/clear` | POST | Clears device database |
| `/api/replay/transmit` | POST | RF transmission |
| `/api/replay/clear` | POST | Clears replay slots |
| `/api/wifi/configure` | POST | Sets WiFi credentials |
| `/api/wifi/clear` | POST | Clears credentials, reboots |
| `/api/command` | POST | System commands (reboot, power off) |
| `/api/firmware/upload` | POST | OTA firmware updates |

### **Auth Info Endpoint**

```http
GET /api/auth/info
```

Returns authentication configuration (does not expose the token):

```json
{
  "authEnabled": true,
  "authHeader": "X-API-Token",
  "tokenHint": "Check serial output at boot for API token",
  "protectedEndpoints": [
    "/api/devices/clear",
    "/api/replay/transmit",
    ...
  ]
}
```

### **Error Response (401 Unauthorized)**

```json
{
  "status": "error",
  "message": "Authentication required",
  "hint": "Include X-API-Token header. Check serial output for token."
}
```

### **Security Notes**

- Token uses cryptographically secure random generation (`esp_random()`)
- Constant-time comparison prevents timing attacks
- Read-only endpoints remain public for convenience
- WiFi AP password is now device-unique (`recon-XXYYZZ` format)

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

## � Quick Reference - All Endpoints

| Endpoint | Method | Auth | Description |
|----------|--------|------|-------------|
| `/api/status` | GET | No | System status and mode |
| `/api/dashboard` | GET | No | Combined data for UI (status + devices + activity) |
| `/api/devices` | GET | No | List discovered devices |
| `/api/device?nodeId=XXX` | GET | No | Get specific device details |
| `/api/devices/clear` | POST | **Yes** | Clear all discovered devices |
| `/api/capture/start` | POST | No | Start targeted capture on device |
| `/api/capture/stop` | POST | No | Stop capture, resume reconnaissance |
| `/api/positions` | GET | No | GPS positions from packets |
| `/api/export/csv` | GET | No | Download current session CSV log from SD card |
| `/api/export/geojson` | GET | No | Export positions as GeoJSON (attachment download) |
| `/api/export/kml` | GET | No | Export positions as KML (attachment download) |
| `/api/export/pcap` | GET | No | Export packets as PCAP |
| `/api/export/report` | GET | No | **Download consolidated JSON report** |
| `/api/files` | GET | No | List files in SD card `/logs/` directory |
| `/api/files/download` | GET | No | Download a named file from SD card `/logs/` |
| `/api/statistics` | GET | No | Detailed packet statistics |
| `/api/activity` | GET | No | RF activity summary |
| `/api/config` | GET | No | Frequency configurations |
| `/api/config/system` | GET | No | System configuration constants |
| `/api/recon/summary` | GET | No | Reconnaissance summary |
| `/api/recon/device-types` | GET | No | Device type breakdown |
| `/api/recon/security` | GET | No | Security vulnerability assessment |
| `/api/replay/slots` | GET | No | List captured packets for replay |
| `/api/replay/transmit` | POST | **Yes** | Transmit captured packet |
| `/api/replay/clear` | POST | **Yes** | Clear replay slots |
| `/api/frequency/target` | POST | No | Start frequency targeting |
| `/api/scan/start` | POST | No | Resume reconnaissance scan |
| `/api/scan/stop` | POST | No | Stop scanning, enter menu mode |
| `/api/diagnostics` | GET | No | Text packet diagnostics |
| `/api/diagnostics/verbose` | POST | No | Toggle verbose logging |
| `/api/anomalies` | GET | No | Protocol anomalies detected |
| `/api/anomaly/acknowledge` | POST | No | Acknowledge an anomaly |
| `/api/temporal` | GET | No | Temporal activity patterns |
| `/api/psk/stats` | GET | No | PSK decryption statistics |
| `/api/wifi/status` | GET | No | WiFi connection status |
| `/api/wifi/configure` | POST | **Yes** | Set WiFi credentials |
| `/api/wifi/clear` | POST | **Yes** | Clear credentials, reboot |
| `/api/command` | POST | **Yes** | Execute system command (reboot, power off) |
| `/api/firmware/upload` | POST | **Yes** | OTA firmware update |
| `/api/health` | GET | No | Simple health check |
| `/api/auth/info` | GET | No | Authentication info |

---

## �📱 Device Endpoints

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

### **POST /api/devices/clear**

Clear all discovered devices and nodes from tracking.

**Request:**
```http
POST /api/devices/clear HTTP/1.1
Host: 192.168.4.1
```

**Response:**
```json
{
  "status": "success",
  "message": "Cleared 5 devices and 8 nodes"
}
```

**Behavior:**
1. Clears all targetable devices from the device list
2. Clears all tracked node entries
3. Counters reset; device discovery starts fresh
4. Does NOT clear replay slots or packet statistics

**cURL Example:**
```bash
curl -X POST http://192.168.4.1/api/devices/clear
```

---

## 🔄 Packet Replay

### **GET /api/replay/slots**

List all captured packets available for replay.

**Request:**
```http
GET /api/replay/slots HTTP/1.1
Host: 192.168.4.1
```

**Response:**
```json
{
  "status": "success",
  "capacity": 10,
  "count": 3,
  "available": 7,
  "slots": [
    {
      "index": 1,
      "length": 64,
      "protocol": "Meshtastic",
      "configIndex": 5,
      "frequencyMHz": 906.875,
      "rssi": -68.5,
      "capturedSecondsAgo": 120,
      "nodeId": "9EA3D744",
      "packetId": "0012AB34",
      "decryptedText": "Hello world"
    },
    {
      "index": 2,
      "length": 48,
      "protocol": "Meshtastic",
      "configIndex": 5,
      "frequencyMHz": 906.875,
      "rssi": -72.0,
      "capturedSecondsAgo": 85,
      "nodeId": "A1B2C3D4"
    }
  ]
}
```

**Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `capacity` | number | Maximum replay slots (10) |
| `count` | number | Currently occupied slots |
| `available` | number | Free slots remaining |
| `slots[].index` | number | Slot index (1-based) |
| `slots[].length` | number | Packet length in bytes |
| `slots[].protocol` | string | Detected protocol |
| `slots[].configIndex` | number | Frequency config index |
| `slots[].frequencyMHz` | number | Capture frequency |
| `slots[].rssi` | number | Signal strength at capture (dBm) |
| `slots[].snr` | number | Signal-to-noise ratio at capture (dB) |
| `slots[].capturedSecondsAgo` | number | Time since capture |
| `slots[].nodeId` | string | **Optional** - Source node ID (hex) |
| `slots[].packetId` | string | **Optional** - Meshtastic packet ID (hex) |
| `slots[].hopCount` | number | **Optional** - Remaining hops (Meshtastic) |
| `slots[].destId` | string | **Optional** - Destination node ID (hex, omitted for broadcast) |
| `slots[].isBroadcast` | boolean | Whether packet is broadcast (destId = 0xFFFFFFFF) |
| `slots[].channel` | number | **Meshtastic only** - Channel hash (byte 13 of packet header) |
| `slots[].channelName` | string | **Meshtastic only, optional** - Known channel name (e.g., "LongFast", "admin") |
| `slots[].wantAck` | boolean | **Optional** - Packet requests acknowledgment |
| `slots[].viaMqtt` | boolean | **Optional** - Packet originated via MQTT gateway |
| `slots[].priority` | number | **Optional** - Packet priority level (1-127) |
| `slots[].decryptedText` | string | **Optional** - Decrypted message content |

**cURL Example:**
```bash
curl -X GET http://192.168.4.1/api/replay/slots
```

---

### **POST /api/replay/clear**

Clear all captured replay slots.

**Request:**
```http
POST /api/replay/clear HTTP/1.1
Host: 192.168.4.1
```

**Response (Success):**
```json
{
  "status": "success",
  "message": "Replay slots cleared"
}
```

**Response (No Slots):**
```json
{
  "status": "error",
  "message": "No replay slots to clear"
}
```

**cURL Example:**
```bash
curl -X POST http://192.168.4.1/api/replay/clear
```

---

### **POST /api/replay/transmit**

Transmit a captured packet from a replay slot.

**Request:**
```http
POST /api/replay/transmit HTTP/1.1
Host: 192.168.4.1
Content-Type: application/x-www-form-urlencoded

slotIndex=1&repeatCount=3&delayMs=500
```

**Request Parameters:**

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `slotIndex` | number | Yes | - | Replay slot index (1-based) |
| `repeatCount` | number | No | 1 | Number of times to transmit |
| `delayMs` | number | No | 1000 | Delay between repeats (ms) |

**Response (Success):**
```json
{
  "status": "success",
  "message": "Replayed slot 1 (3 times)"
}
```

**Response (Invalid Slot):**
```json
{
  "status": "error",
  "message": "Invalid slot index"
}
```

**cURL Example:**
```bash
curl -X POST http://192.168.4.1/api/replay/transmit \
  -d "slotIndex=1&repeatCount=3&delayMs=500"
```

**⚠️ Important: Half-Duplex Radio Limitation**

The SX1262 radio is **half-duplex** — it can either transmit OR receive, never both simultaneously. This means:

- **You will NOT see your own replayed packet recaptured** by the same device
- The radio switches to TX mode, transmits the packet, then returns to RX mode
- By the time RX resumes, your transmission has already completed

**To verify replay transmission:**
1. Use a second Meshtastic device in range — it will receive and display the message
2. Use an SDR (RTL-SDR + SDR++) to observe the RF transmission directly
3. Watch serial output for "TX complete" confirmation
4. If another mesh node relays your packet, you may capture the *relayed* copy (from the other node, not your own transmission)

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

### **GET /api/export/report**

**Download a consolidated JSON report** containing security assessments, device inventory, session statistics, and GPS positions in a single file. This is the **recommended export** for security researchers—one download captures all reconnaissance data.

**Request:**
```http
GET /api/export/report HTTP/1.1
Host: 192.168.4.1
```

**Response Headers:**
```http
Content-Type: application/json
Content-Disposition: attachment; filename="lora-recon-report.json"
```

**Response Structure:**
```json
{
  "status": "success",
  "metadata": {
    "generatedAt": 1234567,
    "firmwareVersion": "2.3.1",
    "reportVersion": "1.0"
  },
  "security": {
    "devices": [
      {
        "nodeId": "!abc12345",
        "nodeIdDecimal": 2880229189,
        "deviceType": "Router",
        "protocol": "Meshtastic",
        "score": 75,
        "riskLevel": "vulnerable",
        "findings": [
          "High signal strength (physical proximity)",
          "Router device - elevated attack surface"
        ]
      }
    ],
    "summary": {
      "totalDevices": 5,
      "vulnerable": 2,
      "moderate": 1,
      "secure": 2
    }
  },
  "devices": {
    "count": 5,
    "list": [
      {
        "nodeId": "!abc12345",
        "nodeIdDecimal": 2880229189,
        "protocol": "Meshtastic",
        "deviceType": "Router",
        "firmwareVersion": "2.5.1.abc",
        "packetCount": 47,
        "originatedPackets": 12,
        "relayedPackets": 35,
        "avgRSSI": -68,
        "bestRSSI": -52,
        "rssiStdDev": "4.2",
        "isRouter": true,
        "powerClass": 2,
        "frequency": 906.875,
        "firstSeenSecondsAgo": 3420,
        "lastSeenSecondsAgo": 15
      }
    ]
  },
  "statistics": {
    "totalPackets": 1247,
    "totalDevices": 5,
    "droppedPackets": 0,
    "uptimeSeconds": 3600,
    "protocolDistribution": {
      "Meshtastic": 5,
      "LoRaWAN": 0,
      "Helium": 0,
      "Other": 0
    }
  },
  "gps": {
    "count": 3,
    "positions": [
      {
        "nodeId": "!abc12345",
        "lat": 37.7749,
        "lon": -122.4194,
        "alt": 15.0,
        "precision": 10
      }
    ]
  }
}
```

**cURL Example:**
```bash
# Download report
curl http://192.168.4.1/api/export/report -o recon-report.json

# Pretty-print with jq
curl -s http://192.168.4.1/api/export/report | jq .

# Extract just vulnerable devices
curl -s http://192.168.4.1/api/export/report | jq '.security.devices[] | select(.riskLevel == "vulnerable")'
```

**Use Cases:**
- Single-click export for security audits
- Share findings with other researchers
- Archive reconnaissance sessions
- Feed into external analysis tools

---

### **GET /api/export/csv**

Download the current session's CSV packet log directly from the SD card.

**Request:**
```http
GET /api/export/csv HTTP/1.1
Host: 192.168.4.1
```

**Response (Success - 200):**
- **Content-Type:** `text/csv`
- **Content-Disposition:** `attachment; filename="lora_capture_<session>.csv"`
- **Body:** CSV stream — header row + one row per logged packet

**Response (Error - 404):**
```json
{
  "status": "error",
  "message": "No active session or SD card unavailable"
}
```

**CSV Columns:**
`timestamp_ms, session_id, node_id_hex, protocol, frequency_mhz, config_index, rssi_dbm, snr_db, length_bytes, packet_type, encrypted, psk_result, psk_id, lat_deg, lon_deg, alt_m, hop_count, is_router, power_class, raw_hex`

**Notes:**
- SD card required. Returns 404 if no SD card or no active session.
- File is flushed before streaming; last captured packets are always included.
- PCAP export must be enabled in firmware for concurrent PCAP capture.

**cURL Example:**
```bash
curl http://192.168.4.1/api/export/csv -o lora_capture.csv
```

---

### **GET /api/files**

List all files in the SD card `/logs/` directory (all sessions, not just current).

**Request:**
```http
GET /api/files HTTP/1.1
Host: 192.168.4.1
```

**Response (Success - 200):**
```json
{
  "files": [
    {"name": "snf_12345.csv", "size": 8192},
    {"name": "snf_12345.pcap", "size": 4096},
    {"name": "snf_98765.csv", "size": 16384},
    {"name": "devices_summary.csv", "size": 512}
  ]
}
```

**Response (Error - 404):**
```json
{
  "status": "error",
  "message": "SD card unavailable or /logs directory not found"
}
```

**Notes:**
- Returns basenames only (no path prefix).
- SD card required.

**cURL Example:**
```bash
curl http://192.168.4.1/api/files
```

---

### **GET /api/files/download**

Download a named file from the SD card `/logs/` directory.

**Request:**
```http
GET /api/files/download?name=snf_12345.csv HTTP/1.1
Host: 192.168.4.1
```

**Query Parameters:**

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `name` | string | Yes | Basename of file in `/logs/` (e.g., `snf_12345.csv`) |

**Response (Success - 200):**
- **Content-Type:** `text/csv` (`.csv`), `application/vnd.tcpdump.pcap` (`.pcap`), or `application/octet-stream`
- **Body:** File stream

**Response (Error - 400):**
```json
{
  "status": "error",
  "message": "Invalid filename"
}
```

**Response (Error - 404):**
```json
{
  "status": "error",
  "message": "File not found"
}
```

**Security:**
- Filenames containing `/` or `..` are rejected (path traversal prevention).
- Only files within `/logs/` are accessible.

**Notes:**
- If `name` refers to the current active session file, it is flushed before streaming.
- Use `/api/files` to enumerate available filenames first.

**cURL Example:**
```bash
curl "http://192.168.4.1/api/files/download?name=snf_12345.csv" -o snf_12345.csv
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
    "totalConfigs": 26,
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
| `batteryVoltage` | string | Battery voltage in volts (e.g. `"3.85"`). Read from AXP2101 coulomb counter on T-Beam Supreme; ADC on other boards. |
| `batteryPercent` | number | Estimated battery level 0–100%. |
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
| `gps` | object | **Optional** - Only present on boards with `HAS_GPS` (T-Beam Supreme). Sniffer's own GPS position. |
| `gps.hasFix` | boolean | True if a valid location fix is available |
| `gps.satellites` | number | Number of satellites in view |
| `gps.lat` | string | **Optional** - Latitude in decimal degrees (present when `hasFix` is true) |
| `gps.lon` | string | **Optional** - Longitude in decimal degrees (present when `hasFix` is true) |
| `gps.alt` | string | **Optional** - Altitude in metres (present when `hasFix` is true) |

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
    "maxDevices": 50,
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
    "numDefaultKeys": 23,
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

## � Reconnaissance Summary

### **GET /api/recon/summary**

Get comprehensive reconnaissance summary including network intelligence, RF activity, LoRaWAN key testing stats, and discovered devices.

**Request:**
```http
GET /api/recon/summary HTTP/1.1
Host: 192.168.4.1
```

**Response:**
```json
{
  "status": "success",
  "summary": {
    "mode": "reconnaissance",
    "uptimeSeconds": 3600,
    "reconDurationSeconds": 3580,
    "totalPackets": 1247,
    "totalDetections": 892,
    "targetableDevices": 5,
    "capturedPackets": 3,
    "verboseDiagnostics": false,
    "networkIntel": {
      "beaconDevices": 2,
      "activeTransmitters": 4,
      "relayNodes": 1,
      "mixedNodes": 2,
      "encryptedNetworks": 1,
      "floodingEvents": 0,
      "anomalyCount": 0
    },
    "lorawanStats": {
      "joinRequestsSeen": 15,
      "joinRequestsDecoded": 12,
      "defaultKeysFound": 3,
      "keysTestedPerPacket": 16
    }
  },
  "targetedCapture": {
    "configIndex": 5,
    "protocol": "Meshtastic_LF_906",
    "frequencyMHz": 906.875,
    "spreadingFactor": 11,
    "bandwidthKHz": 250,
    "replaySlotsUsed": 3
  },
  "rfActivity": [
    {
      "configIndex": 5,
      "protocol": "Meshtastic_LF_906",
      "frequencyMHz": 906.875,
      "packets": 847,
      "avgRSSI": -68.5,
      "peakRSSI": -45.0,
      "activityLevel": "HIGH",
      "lastActivitySecondsAgo": 2
    }
  ],
  "devices": [
    {
      "index": 0,
      "nodeId": "!9ea3d744",
      "protocol": "Meshtastic",
      "deviceType": "TRACKER_APP",
      "packetCount": 47,
      "avgRSSI": -68.5,
      "lastSeenSecondsAgo": 15
    }
  ]
}
```

**Summary Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `mode` | string | Current operation mode |
| `uptimeSeconds` | number | Device uptime |
| `reconDurationSeconds` | number | Active reconnaissance time |
| `totalPackets` | number | All packets received |
| `totalDetections` | number | Valid protocol detections |
| `targetableDevices` | number | Unique devices discovered |
| `capturedPackets` | number | Packets in replay slots |
| `verboseDiagnostics` | boolean | Verbose logging enabled |

**Network Intelligence Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `beaconDevices` | number | Devices transmitting periodic beacons |
| `activeTransmitters` | number | Devices actively transmitting |
| `relayNodes` | number | Pure relay/router nodes |
| `mixedNodes` | number | Nodes that both originate and relay |
| `encryptedNetworks` | number | Detected encrypted channels |
| `floodingEvents` | number | Mesh flooding events detected |
| `anomalyCount` | number | Protocol anomalies detected |

**LoRaWAN Stats Fields (present only if Join Requests seen):**

| Field | Type | Description |
|-------|------|-------------|
| `joinRequestsSeen` | number | LoRaWAN Join Request packets captured |
| `joinRequestsDecoded` | number | Successfully parsed Join Requests |
| `defaultKeysFound` | number | Packets decrypted with default AppKeys |
| `keysTestedPerPacket` | number | Number of default keys tested (16) |

**cURL Example:**
```bash
curl -X GET http://192.168.4.1/api/recon/summary
```

---

## �🛡️ Security Assessment

### **GET /api/recon/security**

Get security vulnerability assessment for all discovered devices.

**Request:**
```http
GET /api/recon/security HTTP/1.1
Host: 192.168.4.1
```

**Response:**
```json
{
  "status": "success",
  "summary": {
    "totalDevices": 5,
    "vulnerable": 1,
    "moderate": 2,
    "secure": 2
  },
  "devices": [
    {
      "nodeId": "9EA3D744",
      "nodeIdDecimal": 2662901572,
      "protocol": "Meshtastic",
      "score": 55,
      "riskLevel": "vulnerable",
      "packetCount": 150,
      "bestRSSI": -45,
      "isRouter": true,
      "findings": [
        "Physical proximity risk (RSSI > -50 dBm)",
        "Router device (elevated attack surface)",
        "High packet count (information leakage)"
      ]
    }
  ],
  "timestamp": 1699900800000
}
```

### **Security Scoring Methodology**

Devices start at **100 points** (fully secure) and lose points for risk factors:

| Risk Factor | Points Lost | Detection Criteria |
|-------------|-------------|-------------------|
| Physical proximity | -15 | `bestRSSI > -50 dBm` (device within ~5 meters) |
| Router device | -10 | Device has relayed ≥2 packets (see Router Detection below) |
| High packet count | -15 | `packetCount > 100` (chatty device leaks metadata) |
| Low packet count | -5 | `packetCount < 5` (may indicate battery/reliability issues) |
| Outdated firmware | -20 | Firmware version contains "v1.x" or "v2.0" |

### **Risk Level Thresholds**

| Score Range | Risk Level | UI Display |
|-------------|------------|------------|
| 80-100 | `secure` | 🟢 Low |
| 60-79 | `moderate` | 🟡 Med |
| 0-59 | `vulnerable` | 🔴 High |

### **Router Detection**

A device is marked as a **router** when it has relayed ≥2 packets. Router detection uses Meshtastic hop count analysis:

1. Meshtastic packets have a default hop limit of 3
2. When a packet is forwarded, the hop count decrements
3. If we receive a packet with `hopCount < 3`, it was relayed by an intermediate node
4. The node that relayed it is marked as a router

**Why routers are a risk factor:**
- Routers forward all mesh traffic, making them high-value targets
- Compromising a router allows traffic interception/injection
- Routers have larger attack surface due to message forwarding code paths

### **cURL Example:**
```bash
curl -X GET http://192.168.4.1/api/recon/security
```

---

## �🔍 Scan Control

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

### Pre-Provisioned Credentials (Optional)

For headless deployments or to skip the web-based setup, you can pre-provision WiFi credentials by creating a `wifi_creds.json` file in the `data/` folder before uploading the filesystem:

**File: `data/wifi_creds.json`**
```json
{
  "ssid": "YourHotspotSSID",
  "password": "YourPassword"
}
```

**Behavior:**
1. If `wifi_creds.json` exists in LittleFS and no credentials are stored, the device will load these credentials on boot
2. Device attempts to connect to the specified hotspot automatically
3. Once connected, credentials are saved to `wifi_config.json` (the runtime credential store)
4. The original `wifi_creds.json` is NOT deleted; it acts as a fallback
5. This file is uploaded via `pio run --target uploadfs`

**Use Cases:**
- Pre-configure devices before field deployment
- Quickly restore connectivity after filesystem format
- Share device images with pre-configured WiFi

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

### **POST /api/command**

Execute a system command on the device. Requires authentication.

**Request:**
```http
POST /api/command HTTP/1.1
Host: 192.168.4.1
Content-Type: application/x-www-form-urlencoded

command=b
```

**Commands:**

| Command | Value | Description |
|---------|-------|-------------|
| Reboot | `b` | Restart the device (equivalent to power cycle) |
| Power Off | `s` | Safe shutdown — stops radio, blanks display, then cuts PMIC power rails (T-Beam Supreme) or enters deep sleep (all other boards) |

**Response (success):**
```json
{ "status": "success", "message": "Rebooting device..." }
```
```json
{ "status": "success", "message": "Shutting down device..." }
```

**Notes:**
- The HTTP response is sent before the device acts, so the connection will drop immediately after.
- On T-Beam Supreme, Power Off cuts all AXP2101 rails — the device will not restart until the button is pressed or USB power is toggled.
- On Heltec V3/V4 and T3-S3, Power Off enters ESP32 deep sleep (no PMIC to cut power entirely).

**cURL Example:**
```bash
# Reboot
curl -X POST http://192.168.4.1/api/command -d "command=b" -H "Authorization: Bearer <token>"

# Power off
curl -X POST http://192.168.4.1/api/command -d "command=s" -H "Authorization: Bearer <token>"
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
