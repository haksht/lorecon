# REST API Reference

Quick reference for the firmware's HTTP and WebSocket endpoints.

- **Base URL:** `http://192.168.4.1` (AP) or `http://lora-XXYYZZ.local` (STA)
- **Content-Type:** `application/json`
- **Implementation:** [`firmware/src/api_handlers.cpp`](../../firmware/src/api_handlers.cpp) — canonical source for request/response shapes
- **CLI client:** `lorecon dev api` wraps every endpoint; use `--help` for usage

---

## Authentication

Protected endpoints require an `X-API-Token` header. The token is:

- Generated on first boot, stored in NVS, persists across reboots
- Printed to serial at startup (Heltec V3, T3-S3, T-Beam)
- Retrievable via `GET /api/auth/info` (metadata only, not the token) and shown in the web UI Settings tab on Heltec V4

### Auto-trust by network

| Client's network | Auth required? |
|---|---|
| Device AP subnet (`192.168.4.x`) | Yes — token required |
| External LAN (device joined your hotspot, client on same LAN) | No — RFC 1918 auto-trust |
| Public internet | Yes — token required |

**Why the AP subnet is not auto-trusted:** anyone who knows the AP password lands in `192.168.4.x`. Auto-trust there would bypass authentication entirely. Auto-trust is reserved for clients already on a network you control.

The web UI saves the token to `localStorage` after first entry, so you are prompted once per browser.

### Using the token

```bash
curl -X POST http://192.168.4.1/api/devices/clear \
     -H "X-API-Token: a1b2c3d4e5f6789012345678abcdef01"
```

### 401 response

```json
{
  "status": "error",
  "message": "Authentication required",
  "hint": "Include X-API-Token header. Check serial output for token."
}
```

Token comparison is constant-time (`api_security.cpp`).

---

## Response shape

Success:
```json
{ "status": "success", "data": { ... }, "timestamp": 1699900800000 }
```

Error:
```json
{ "status": "error", "message": "...", "code": "ERROR_CODE", "timestamp": 1699900800000 }
```

Some endpoints inline fields directly rather than nesting under `data` (e.g. `/api/devices` returns `devices` at the top level). Check `api_handlers.cpp` for exact shapes.

---

## Endpoints

### Devices

| Endpoint | Method | Auth | Description |
|---|---|---|---|
| `/api/devices` | GET | No | List discovered devices |
| `/api/device?nodeId=XXX` | GET | No | Single-device details (hex, with or without `0x`) |
| `/api/devices/clear` | POST | **Yes** | Clear device tracking (does not clear replay slots) |

### Capture / scan control

| Endpoint | Method | Auth | Description |
|---|---|---|---|
| `/api/capture/start` | POST | No | Target a specific device by `nodeId` |
| `/api/capture/stop` | POST | No | Stop targeting, resume reconnaissance |
| `/api/frequency/target` | POST | No | Lock radio to a scan-config index |
| `/api/scan/start` | POST | No | Resume reconnaissance scan |
| `/api/scan/stop` | POST | No | Stop scanning, enter menu mode |

### Packet replay

| Endpoint | Method | Auth | Description |
|---|---|---|---|
| `/api/replay/slots` | GET | No | List captured packets available for replay |
| `/api/replay/transmit` | POST | **Yes** | Transmit a captured packet (bounded repeat/delay) |
| `/api/replay/clear` | POST | **Yes** | Clear replay slots |

### Geographic data

| Endpoint | Method | Auth | Description |
|---|---|---|---|
| `/api/positions` | GET | No | GPS positions from decoded packets |
| `/api/export/kml` | GET | No | Positions as KML (attachment) |
| `/api/export/geojson` | GET | No | Positions as GeoJSON (attachment) |

### Export / file access

| Endpoint | Method | Auth | Description |
|---|---|---|---|
| `/api/export/csv` | GET | No | Current session CSV (SD card boards) |
| `/api/export/pcap` | GET | No | Current session PCAP |
| `/api/export/report` | GET | No | Consolidated JSON report |
| `/api/files` | GET | No | List files in `/logs/` on SD card |
| `/api/files/download?name=...` | GET | No | Download a named file from `/logs/` |

### Status / statistics

| Endpoint | Method | Auth | Description |
|---|---|---|---|
| `/api/status` | GET | No | System status, mode, uptime, free heap |
| `/api/health` | GET | No | Simple alive check |
| `/api/dashboard` | GET | No | Combined payload for the web UI (status + devices + activity) |
| `/api/statistics` | GET | No | Detailed packet statistics |
| `/api/activity` | GET | No | RF activity summary |
| `/api/temporal` | GET | No | Temporal activity patterns |
| `/api/psk/stats` | GET | No | PSK decryption counts |
| `/api/config` | GET | No | 29 frequency configurations |
| `/api/config/system` | GET | No | System configuration constants |

### Reconnaissance / security

| Endpoint | Method | Auth | Description |
|---|---|---|---|
| `/api/recon/summary` | GET | No | Reconnaissance summary |
| `/api/recon/device-types` | GET | No | Device-type breakdown |
| `/api/recon/security` | GET | No | Security assessment per device (see [HOW_IT_WORKS.md](../reference/HOW_IT_WORKS.md)) |

### Diagnostics

| Endpoint | Method | Auth | Description |
|---|---|---|---|
| `/api/diagnostics` | GET | No | Text-packet diagnostic report |
| `/api/diagnostics/verbose` | POST | No | Toggle verbose logging |
| `/api/anomalies` | GET | No | Detected protocol anomalies |
| `/api/anomaly/acknowledge` | POST | No | Acknowledge an anomaly |

### WiFi / system

| Endpoint | Method | Auth | Description |
|---|---|---|---|
| `/api/wifi/status` | GET | No | WiFi connection state |
| `/api/wifi/configure` | POST | **Yes** | Set WiFi credentials and reboot |
| `/api/wifi/clear` | POST | **Yes** | Clear credentials and reboot |
| `/api/command` | POST | **Yes** | System command (`b` reboot, `s` power off) |
| `/api/firmware/upload` | POST | **Yes** | OTA firmware update — see [BUILDING.md](BUILDING.md#ota-updates) |

### Auth info

| Endpoint | Method | Auth | Description |
|---|---|---|---|
| `/api/auth/info` | GET | No | Auth config metadata (does not expose the token) |

---

## WebSocket

**Endpoint:** `ws://192.168.4.1/ws`

Pushed message types:

**`packet`** — new packet received:
```json
{ "type": "packet", "nodeId": "9EA3D744", "protocol": "Meshtastic",
  "rssi": -68.5, "snr": 8.2, "frequency": 906.875, "length": 64,
  "timestamp": 1699900800000 }
```

**`deviceUpdate`** — device state changed:
```json
{ "type": "deviceUpdate", "nodeId": "9EA3D744", "packetCount": 43,
  "timestamp": 1699900800000 }
```

**`status`** — periodic broadcast (~5s):
```json
{ "type": "status", "data": { "mode": "reconnaissance", "devices": 5,
  "totalPackets": 1234, "freeHeap": 204800 }, "timestamp": 1699900800000 }
```

GPS-enabled builds add `latitude`/`longitude`/`altitude` to `packet` events when a fix is available.

Minimal listener:
```python
import asyncio, json, websockets

async def listen():
    async with websockets.connect("ws://192.168.4.1/ws") as ws:
        async for msg in ws:
            d = json.loads(msg)
            if d["type"] == "packet":
                print(d["nodeId"], d["rssi"])

asyncio.run(listen())
```

For a richer client, use `lorecon dev monitor --host 192.168.4.1 --tui`.

---

## Error codes

| Code | HTTP | Meaning |
|---|---|---|
| `DEVICE_NOT_FOUND` | 404 | Requested device does not exist |
| `INVALID_NODE_ID` | 400 | Malformed node ID |
| `INVALID_REQUEST` | 400 | Missing required parameters |
| `CAPTURE_ACTIVE` | 409 | Capture already active |
| `NOT_IN_CAPTURE_MODE` | 400 | Not currently capturing |
| `OUT_OF_MEMORY` | 507 | Insufficient heap |
| `INTERNAL_ERROR` | 500 | Server-side error |

---

## Rate limiting

No explicit rate limit. The 100-packet queue and 90 KB heap guard on `/api/devices` (`api_handlers.cpp`) provide implicit back-pressure. Avoid polling large endpoints (`/api/devices`, `/api/export/*`) faster than once per second.

---

## Quick examples

Get device list:
```bash
curl http://192.168.4.1/api/devices
```

Target a device:
```bash
curl -X POST http://192.168.4.1/api/capture/start \
     -H "X-API-Token: $TOKEN" \
     -H "Content-Type: application/json" \
     -d '{"nodeId":"9EA3D744"}'
```

Export positions:
```bash
curl -s http://192.168.4.1/api/export/geojson > positions.geojson
```

Reboot:
```bash
curl -X POST http://192.168.4.1/api/command \
     -H "X-API-Token: $TOKEN" \
     -H "Content-Type: application/json" \
     -d '{"command":"b"}'
```

For full-coverage scripting, use `lorecon dev api --help`.
