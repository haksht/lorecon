# Recon Service API Endpoints

## Overview
The recon service endpoints surface the same intelligence previously exclusive to the serial console so the web app and companion phone client can remain in sync with interactive menu features. All responses follow the JSON structures described below.

### Base URL
```
GET /api/recon/
```

---

## GET /api/recon/summary
Aggregated session status, activity, and device inventory.

### Response
```json
{
  "status": "success",
  "summary": {
    "mode": "reconnaissance",
    "uptimeSeconds": 1234,
    "reconDurationSeconds": 987,
    "totalPackets": 4321,
    "totalDetections": 27,
    "targetableDevices": 5,
    "nodesTracked": 11,
    "capturedPackets": 2,
    "verboseDiagnostics": false
  },
  "targetedCapture": {
    "configIndex": 4,
    "protocol": "Meshtastic_902_SF10",
    "frequencyMHz": 902.125,
    "spreadingFactor": 10,
    "bandwidthKHz": 250,
    "replaySlotsUsed": 2
  },
  "rfActivity": [
    {
      "configIndex": 0,
      "protocol": "Meshtastic_LF_906",
      "frequencyMHz": 906.875,
      "packets": 14,
      "avgRSSI": -78.5,
      "peakRSSI": -65.2,
      "activityLevel": "MEDIUM",
      "lastActivitySecondsAgo": 12
    }
  ],
  "rfActivityCount": 1,
  "devices": [
    {
      "index": 1,
      "nodeId": "89ABCDEF",
      "nodeIdDecimal": 2309737967,
      "frequency": 902.125,
      "protocol": "Meshtastic",
      "deviceType": "Meshtastic Router",
      "firmwareVersion": "v2.2+",
      "rssi": -63.2,
      "avgRSSI": -70.1,
      "packetCount": 112,
      "lastSeen": 1699985234,
      "powerClass": 2,
      "isRouter": true,
      "lastSeenSecondsAgo": 8,
      "firstSeenSecondsAgo": 342
    }
  ]
}
```
`targetedCapture` appears only when the system is in targeted mode (capture or replay).

---

## GET /api/recon/device-types
Device-type rollups mirroring the serial "device type analysis" view.

### Response
```json
{
  "status": "success",
  "deviceTypes": [
    {
      "type": "Meshtastic Router",
      "count": 3,
      "avgRSSI": -71.0,
      "routers": 3,
      "avgPowerClass": 1.7,
      "powerDescriptor": "High"
    }
  ],
  "summary": {
    "totalDeviceTypes": 2,
    "totalDevices": 5,
    "routersDetected": 3,
    "hasDevices": true
  }
}
```

---

## GET /api/recon/security
Security posture assessment derived from the serial security menu.

### Response
```json
{
  "status": "success",
  "devices": [
    {
      "nodeId": "89ABCDEF",
      "nodeIdDecimal": 2309737967,
      "deviceType": "Meshtastic Router",
      "protocol": "Meshtastic",
      "firmwareVersion": "v2.2+",
      "bestRSSI": -63.2,
      "avgRSSI": -70.1,
      "packetCount": 112,
      "isRouter": true,
      "lastSeenSecondsAgo": 8,
      "findings": [
        "High signal strength (physical proximity risk)",
        "Router device - elevated attack surface"
      ],
      "score": 65,
      "riskLevel": "moderate"
    }
  ],
  "summary": {
    "totalDevices": 5,
    "vulnerable": 1,
    "moderate": 2,
    "secure": 2,
    "status": "Some devices warrant investigation"
  },
  "recommendations": [
    "Monitor moderate-risk nodes for changes in behavior"
  ]
}
```

---

## GET /api/replay/slots
Inspect replay slot occupancy.

### Response
```json
{
  "status": "success",
  "capacity": 10,
  "count": 2,
  "available": 8,
  "slots": [
    {
      "index": 1,
      "length": 128,
      "protocol": "Meshtastic",
      "configIndex": 4,
      "frequencyMHz": 902.125,
      "rssi": -64.0,
      "capturedSecondsAgo": 42
    }
  ]
}
```

### POST /api/replay/clear
Clears all captured packets.

#### Response (success)
```json
{
  "status": "success",
  "message": "Replay slots cleared"
}
```

#### Response (no slots)
```json
{
  "status": "error",
  "error": "No replay slots to clear"
}
```

---

## POST /api/frequency/target
Start frequency targeting without selecting a specific device.

### Request Parameters
- `configIndex` (form or JSON body, stringified number)
  - Accepts zero-based indices or one-based values (the handler auto-adjusts).

### Response
```json
{
  "status": "success",
  "message": "Frequency targeting started on Meshtastic_902_SF10 (902.125 MHz)"
}
```

Errors return `status":"error"` with a descriptive `error` message (e.g., invalid index).

---

## GET /api/diagnostics
Expose packet diagnostic statistics (quiet/verbose context, gap counts, plaintext sightings).

### Response
```json
{
  "status": "success",
  "diagnostics": {
    "verboseMode": false,
    "gapCount": 42,
    "maxGapMs": 870,
    "largeGapCount": 7,
    "encryption": {
      "encryptedPackets": 120,
      "plaintextPackets": 3,
      "unknownPackets": 5
    },
    "packetTypes": {
      "routing": {
        "count": 14,
        "minSize": 32,
        "maxSize": 64,
        "averageSize": 45.7
      },
      "position": {
        "count": 9,
        "minSize": 48,
        "maxSize": 112,
        "averageSize": 76.3
      },
      "text": {
        "count": 0,
        "minSize": 0,
        "maxSize": 0,
        "averageSize": 0
      },
      "other": {
        "count": 2,
        "minSize": 40,
        "maxSize": 60,
        "averageSize": 50.0
      }
    }
  },
  "recommendations": [
    "Large timing gaps detected; consider reducing verbose output or buffering packets",
    "No text messages captured; verify PSK alignment and frequency settings"
  ]
}
```

---

## POST /api/diagnostics/verbose
Toggle diagnostic verbosity (quiet vs. verbose mode).

### Request Parameters
- `enable` (form or JSON body)
  - Accepts: `true`, `false`, `1`, `0`, `yes`, `no`, `on`, `off`, `quiet`, `verbose` (case-insensitive).

### Responses
**Enable:**
```json
{
  "status": "success",
  "message": "Verbose diagnostics enabled"
}
```

**Disable:**
```json
{
  "status": "success",
  "message": "Quiet mode enabled"
}
```

**Already in requested state:**
```json
{
  "status": "error",
  "error": "Verbose mode already enabled"
}
```

**Invalid parameter:**
```json
{
  "status": "error",
  "error": "Invalid enable value"
}
```
