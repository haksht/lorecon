# Session Handoff - November 13, 2025 (Part 2)

## Critical Issue Status
**ACTIVE BUG**: Device crashes on WebSocket client disconnect despite multiple safety checks

### Crash Signature
```
[INFO] WebSocket client #1 disconnected
Guru Meditation Error: Core 1 panic'ed (LoadProhibited)
. Exception was unhandled.
Core 1 register dump:
...
EXCVADDR: 0x00000000
```

**Error Type**: Null pointer dereference in AsyncWebSocket library internals  
**Trigger**: Immediate crash after WebSocket disconnect event  
**Timing**: Occurs while aggregation timer (500ms) may be trying to broadcast

## Architecture Changes Completed

### 1. WebSocket Aggregation Implementation ✅
- **Purpose**: Prevent queue overflow from per-packet broadcasts
- **Implementation**: 500ms buffering with batch updates
- **Location**: `firmware/src/web_server.cpp` lines 243-318
- **Key Components**:
  - `AggregatedStats` struct with message buffer `char lastMessage[256]`
  - `handlePacketEvent()` - receives packet events via callback
  - `broadcastAggregatedUpdate()` - sends batched updates every 500ms
  - Timer setup in `begin()` at line 137

### 2. Decoupling via Callback Pattern ✅
- **Before**: PacketProcessor directly called WebServer methods (tight coupling)
- **After**: Event emission via `std::function<void(const PacketEvent&)>` callback
- **Changes**:
  - `packet_processor.h`: Added `PacketEvent` struct, replaced `setWebServer()` with `setPacketCallback()`
  - `packet_processor.cpp`: Emits events in `processSinglePacket()` instead of direct calls
  - `lora_recon_tool.cpp`: Wires callback lambda: `[ws](const PacketEvent& evt) { ws->handlePacketEvent(evt); }`

### 3. Message Field Addition ✅
- **Location**: `protocol_analyzer.h` line 142
- **Change**: `const char* message` → `char message[256]` + `bool hasMessage`
- **Reason**: Prevent dangling pointer crashes (PSK decrypted text was freed)
- **Impact**: Message copied with `strncpy()` in packet processing pipeline

### 4. Web App Simplification ✅
- **Removed**: Service worker (sw.js), PWA manifest, complex reconnection logic, Leaflet.js mapping
- **Result**: Single-page app (~420 lines total, was 1000+ across multiple files)
- **Location**: `data/webapp/index.html`
- **Features**: Live packet feed, device list with target button, statistics, simple 2-second WebSocket reconnect

## Safety Checks Added (Still Insufficient)

All broadcast methods in `web_server.cpp` now have:
```cpp
if (!ws || !server || ws->count() == 0) return;
// ... prepare JSON ...
if (ws && ws->count() > 0) {
    ws->textAll(json);
}
```

**Applied to**:
- `broadcastAggregatedUpdate()` (line 280)
- `broadcastDeviceUpdate()` (line 383)
- `broadcastStatusUpdate()` (line 402)

**Result**: Still crashes despite these checks, suggesting:
1. Race condition between disconnect cleanup and timer callback
2. AsyncWebSocket internal state corruption
3. Need for mutex/semaphore protection
4. May need to disable timer when client count reaches zero

## What Works
✅ Firmware compiles and uploads successfully  
✅ Filesystem uploads successfully  
✅ WiFi AP starts (192.168.4.1, SSID: ESP32-LoRa-Sniffer)  
✅ Web server initializes  
✅ WebSocket connects  
✅ Live packet feed displays  
✅ PSK decryption functional (last message: "dcjlll")  
✅ Device list displays  
✅ Target button works  
❌ **Crash on disconnect**

## Debug Context

### Last Known Good State
- Device booted successfully
- PSK tests passed
- WiFi started
- WebSocket connected
- Packets streaming to web app
- User quote: "live packets are good"

### Crash Sequence
1. WebSocket client connects (client #1)
2. Packets stream successfully via WebSocket
3. PSK decryption works: "TEXT MESSAGE: dcjlll"
4. User closes browser tab/window
5. Log shows: "[INFO] WebSocket client #1 disconnected"
6. **IMMEDIATE CRASH**: LoadProhibited at 0x00000000
7. Device reboots (Guru Meditation Error)

### Technical Details
- **Hardware**: ESP32-S3-DevKitC-1-N8 (8MB Flash, 327KB RAM)
- **Framework**: Arduino + PlatformIO
- **Key Library**: AsyncWebServer-esphome 3.4.0
- **Flash Usage**: 31.1% (1,039,481 bytes)
- **RAM Usage**: 17.7% (58,020 bytes)
- **Aggregation Timer**: 500ms interval (potential race condition source)

## Recommended Next Steps

### Priority 1: Fix Disconnect Crash
**Option A - Disable Timer on Disconnect**:
```cpp
void WebServer::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                          AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DISCONNECT) {
        if (ws->count() == 0) {
            // Stop aggregation timer
            aggTimer.detach();
            hasAggregatedData = false;
            aggStats = {}; // Clear stats
        }
    } else if (type == WS_EVT_CONNECT) {
        if (ws->count() == 1) {
            // Restart aggregation timer
            aggTimer.attach_ms(500, +[](WebServer* self) {
                self->broadcastAggregatedUpdate();
            }, this);
        }
    }
}
```

**Option B - Add Mutex Protection**:
```cpp
// In web_server.h
SemaphoreHandle_t wsMutex;

// In web_server.cpp begin()
wsMutex = xSemaphoreCreateMutex();

// In all broadcast methods
if (xSemaphoreTake(wsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (ws && ws->count() > 0) {
        ws->textAll(json);
    }
    xSemaphoreGive(wsMutex);
}
```

**Option C - Investigate AsyncWebSocket Cleanup**:
- Add detailed logging in disconnect handler
- Check AsyncWebSocket library version/issues
- May be library bug requiring update or workaround

### Priority 2: Testing
Once crash is fixed, test:
1. Multiple connect/disconnect cycles
2. Rapid connect/disconnect
3. Disconnect during high packet rate
4. Multiple clients (even though design is single-client)

### Priority 3: Optional Enhancements
- Unify command handling (make `CommandHandler` methods reusable by `APIController`)
- Update README to reflect simplified architecture
- Update API_REFERENCE.md for new WebSocket message format

## File Modification Summary

### Modified Files (Ready to Upload)
1. `firmware/src/protocol_analyzer.h` - Message buffer change
2. `firmware/src/packet_processor.h` - Callback pattern
3. `firmware/src/packet_processor.cpp` - Event emission
4. `firmware/src/web_server.h` - Aggregation support
5. `firmware/src/web_server.cpp` - Aggregation + safety checks
6. `firmware/src/lora_recon_tool.h` - Forward declaration
7. `firmware/src/lora_recon_tool.cpp` - Callback wiring
8. `data/webapp/index.html` - Complete rewrite

### Current Build Status
- **Compilation**: Success ✅
- **Upload**: Not performed (per user request)
- **Code State**: All changes made, safety checks in place
- **Known Issue**: Disconnect crash unresolved

## Key Architecture Insights

### Event Flow (Current)
```
LoRa Packet Received
    ↓
RadioController::onReceive()
    ↓
PacketProcessor::queuePacket()
    ↓
PacketProcessor::processSinglePacket()
    ↓
[Emits PacketEvent via callback]
    ↓
WebServer::handlePacketEvent()
    ↓
[Aggregates for 500ms]
    ↓
WebServer::broadcastAggregatedUpdate()
    ↓
AsyncWebSocket::textAll()
```

### Race Condition Theory
The crash likely occurs when:
1. Aggregation timer fires (every 500ms)
2. Simultaneously, WebSocket disconnect cleanup runs
3. AsyncWebSocket internal state is being torn down
4. `textAll()` accesses freed/null pointer
5. Even though we check `ws->count()`, the internal buffer or client list may be invalid

### Memory Model Concern
ESP32 dual-core architecture:
- **Core 0**: WiFi/BLE stack (where AsyncWebSocket runs)
- **Core 1**: Application code (where panic occurs)
- Timer callback may execute on different core than disconnect handler
- Need synchronization primitives (mutex/semaphore) for cross-core safety

## Code References

### Key Functions to Review
- `WebServer::broadcastAggregatedUpdate()` - Line 280 in web_server.cpp
- `WebServer::handlePacketEvent()` - Line 243 in web_server.cpp
- `WebServer::onWsEvent()` - Line 167 in web_server.cpp
- Timer initialization - Line 137 in web_server.cpp

### Safety Check Pattern (All Broadcast Methods)
```cpp
if (!ws || !server || ws->count() == 0) return;
// ... build JSON ...
if (ws && ws->count() > 0) {
    ws->textAll(json);
}
```

## Build Commands

### Compile Only
```powershell
C:\Users\tim\.platformio\penv\Scripts\platformio.exe run
```

### Upload Firmware
```powershell
C:\Users\tim\.platformio\penv\Scripts\platformio.exe run --target upload
```

### Upload Filesystem
```powershell
Start-Sleep -Seconds 3; C:\Users\tim\.platformio\penv\Scripts\platformio.exe run --target uploadfs
```

### Monitor Serial
```powershell
C:\Users\tim\.platformio\penv\Scripts\platformio.exe device monitor --baud 115200
```

## User Requirements Context

Original goals from conversation:
- "the web app should be efficient and as simple as possible"
- "It should provide all the functionality available in the monitor menu and more"
- "aggregation is needed (every .5 second seems plenty for now)"
- "def add message field to packetinfo"
- "one field operative at a time" (single client design)

**Status**: All requirements implemented except crash fix

## Questions for Continuation

1. **Crash Fix Approach**: Which option (timer disable, mutex, or library investigation)?
2. **Testing Strategy**: How to validate disconnect safety?
3. **Library Update**: Should we update AsyncWebServer-esphome to newer version?
4. **Fallback Plan**: If AsyncWebSocket is fundamentally broken, consider alternative (ESPAsyncWebServer fork, different library)?

## Additional Notes

- Device operates perfectly until WebSocket disconnect
- This is a critical blocker for field deployment
- All other architecture improvements successful
- Web app UX is good when connected
- Consider adding heartbeat/keepalive to detect stale connections

---

**Session End**: November 13, 2025  
**Branch**: feature/phone-app-integration  
**Repository**: esp32-sniffer  
**Last Build**: Success (firmware not uploaded per user request)
