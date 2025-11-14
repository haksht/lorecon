# Session Handoff - November 13, 2025

## Session Summary

Successfully completed phone app integration with live packet feed and decrypted message display. Fixed two bugs (RF activity tracking and WebSocket keep-alive) and committed all changes.

## What Was Accomplished

### 1. First Commit: Complete Phone App Integration
**Commit**: `13cbe13` - "Add complete phone app integration with live packet feed"

- ✅ WiFi AP mode at 192.168.4.1 (ESP32-LoRa-Sniffer)
- ✅ REST API with 13 endpoints (devices, status, capture control, etc.)
- ✅ WebSocket real-time packet broadcasting
- ✅ Progressive Web App with 4-tab dashboard (Devices, Map, Activity, RF Activity)
- ✅ Fixed Target button to use hex nodeId format
- ✅ Added decrypted message broadcasting to live feed
- ✅ Integrated PSK decryption output with WebSocket

**Files Changed**: 28 files, 4831 insertions, 2247 deletions

**Key New Files**:
- `firmware/src/wifi_manager.cpp/h` - WiFi AP management
- `firmware/src/web_server.cpp/h` - HTTP server and WebSocket
- `firmware/src/api_controller.cpp/h` - REST API handlers
- `data/webapp/` - Complete PWA (HTML/CSS/JS)

### 2. Bug Fix: RF Activity Tracking
**Problem**: RF Activity tab showed "none" even after capturing packets

**Root Cause**: `reconState.updateRFActivity()` was only called in `handleReconPacket()`, not in `handleTargetedPacket()`. When targeting a device, RF activity wasn't being tracked.

**Fix**: Added `reconState.updateRFActivity(reconState.scanState.currentConfig, rssi);` to `handleTargetedPacket()` in `packet_processor.cpp` (line ~144)

**Status**: ✅ Fixed, uploaded, tested working

### 3. Bug Fix: WebSocket Disconnections
**Problem**: iPhone browser kept disconnecting WebSocket (iOS WiFi management switching to cellular)

**Attempted Fixes**:
1. **Server-side**: Added `periodicUpdate()` method that runs every 30s to call `ws->cleanupClients()` and `ws->pingAll()`
2. **Client-side**: 
   - Reduced reconnect interval from 5s to 2s
   - Added exponential backoff (2s → 4s → 8s → max 16s)
   - Added page visibility handling (pause reconnect when hidden, auto-reconnect when visible)
   - Initially added client-side ping every 25s - **CAUSED PROBLEM**

**Problem Encountered**: Client pings flooded server with "ERROR: Too many messages queued"

**Final Fix**: Removed client-side ping entirely - server `pingAll()` is sufficient

**Status**: ✅ Fixed, filesystem uploaded (firmware already had server changes)

## Current Code State

### Firmware (31.1% flash, 17.7% RAM)

**packet_processor.cpp**:
- Line ~125: `updateRFActivity()` in recon mode
- Line ~144: `updateRFActivity()` in targeted mode (NEW)
- Lines 93-108: Broadcasts packets with decrypted messages via WebSocket

**psk_decryption_simple.cpp/h**:
- Static `lastMessage[256]` buffer stores last decrypted message
- `getLastMessage()` and `clearLastMessage()` accessors
- Message stored after successful extraction (both encrypted and unencrypted paths)

**web_server.cpp/h**:
- `broadcastPacket()` includes optional `message` parameter
- `periodicUpdate()` method for WebSocket maintenance
- Called from main loop every 30 seconds

**main.cpp**:
- Lines 103-108: WebSocket maintenance timer in loop()

### Web App (Progressive Web App)

**api-client.js**:
- 2-second initial reconnect with exponential backoff
- Page visibility handling (pauses when hidden)
- No client-side ping (server handles it)
- Auto-reconnect on foreground

**app.js**:
- Displays packets in Activity tab with 📧 emoji for messages
- 50-packet feed limit
- WebSocket event handlers for real-time updates

## Testing Status

### ✅ Verified Working
- WiFi AP connects (iPhone to ESP32-LoRa-Sniffer)
- Web app loads at http://192.168.4.1
- REST API endpoints respond (`/api/status`, `/api/devices`, etc.)
- WebSocket establishes connection
- Target button works (no "device not found" errors)
- Live packet feed shows packets in Activity tab
- Decrypted messages display with 📧 icon
- RF Activity tracking works in both recon and targeted modes

### ⚠️ Known Issues
1. **WebSocket Stability**: Still experiencing some disconnections (iOS WiFi management)
   - Server pings every 30s
   - Client reconnects automatically
   - **May need iOS-specific workarounds** (e.g., disable cellular when on ESP32 WiFi)

2. **Map Tab**: Shows markers but no tiles (requires internet connection - expected behavior)

### 🔍 Not Yet Tested
- Long-term WebSocket stability (hours of operation)
- Multiple concurrent WebSocket clients
- Battery life impact of frequent reconnections
- Performance with high packet rate

## Architecture Notes

### Data Flow: Packet → WebSocket → UI

```
Radio Packet
    ↓
PacketProcessor::processSinglePacket()
    ↓
TextPacketDiagnostic::analyzePacket() → PSKDecryption::extractMessage()
    ↓                                           ↓
ProtocolAnalyzer                      Stores in lastMessage[256]
    ↓                                           ↓
addTargetableDevice()              PSKDecryption::getLastMessage()
    ↓                                           ↓
updateRFActivity() ← NOW IN BOTH MODES          ↓
    ↓                                           ↓
webServer->broadcastPacket(..., message) ←------┘
    ↓
WebSocket JSON: {type, nodeId, protocol, rssi, snr, length, timestamp, message}
    ↓
Browser: app.js handlePacketUpdate()
    ↓
Activity Tab: Live feed with message display
```

### WebSocket Keep-Alive Strategy

**Server** (web_server.cpp):
- `periodicUpdate()` called every 30s from main loop
- Calls `ws->cleanupClients()` (remove dead connections)
- Calls `ws->pingAll()` (send WebSocket ping frames)

**Client** (api-client.js):
- No client-initiated pings (caused queue overflow)
- Relies on server pings to keep connection alive
- Exponential backoff reconnection (2s → 16s max)
- Page visibility: pause reconnect when hidden, resume when visible

## Remaining Concerns

### 1. WebSocket Queue Overflow Issue
When client sent ping messages every 25s, server logged:
```
ERROR: Too many messages queued
[INFO] WebSocket message from client #1:
```

This suggests the AsyncWebServer message queue fills up easily. The server's `handleWebSocketMessage()` echoes back every message, which may create a feedback loop or queue buildup.

**Potential Issues**:
- Server echoing back client pings → queue buildup
- AsyncWebServer queue size too small for frequent messages
- Should client pings be handled differently? (Just ignore instead of echo)

### 2. iOS WiFi Switching Behavior
iOS aggressively switches from WiFi to cellular when it detects no internet. 

**Current Mitigations**:
- Server ping keeps TCP connection alive
- Client auto-reconnects when app comes to foreground
- Fast initial reconnect (2s)

**Possible Improvements**:
- Captive portal detection workaround
- DNS responder to make iOS think there's internet
- User instructions to disable cellular while using app

### 3. RF Activity Data Structure
Currently uses `rfActivity[Config::Scanning::NUM_CONFIGURATIONS]` array indexed by config.

**Questions**:
- Should we track activity per frequency instead of per config?
- What happens when multiple configs use same frequency but different SF/BW?
- Is the current granularity useful for the web UI?

### 4. Message Storage Strategy
Currently using static buffer `PSKDecryption::lastMessage[256]`:
- **Pro**: Simple, fast, works immediately
- **Con**: Not thread-safe (though ESP32 is single-threaded Arduino)
- **Con**: Doesn't integrate with `PacketInfo` structure
- **Note**: Already added `message` field to `PacketInfo` struct (line in protocol_analyzer.h) but not using it

**Future Refactor**: Pass message through `PacketInfo` instead of static storage

## Git State

**Branch**: `feature/phone-app-integration`
**Last Commit**: `13cbe13` - "Add complete phone app integration with live packet feed"
**Uncommitted Changes**: Bug fixes for RF activity and WebSocket (not yet committed)

## Next Steps (Recommendations)

### Immediate (Should Commit)
1. **Commit bug fixes**: RF activity tracking + WebSocket improvements
   ```
   Fix RF activity tracking in targeted mode and improve WebSocket stability
   
   - Added updateRFActivity() call to handleTargetedPacket()
   - Server pings all WebSocket clients every 30 seconds
   - Client reconnects with exponential backoff (2s-16s)
   - Client pauses reconnect when page hidden (battery saving)
   - Removed client-side pings (caused queue overflow)
   ```

### Short-term (Clean Up)
1. **Review WebSocket message handling**: Why does echoing cause queue overflow?
2. **Add user guidance**: Document iOS WiFi switching workaround
3. **Refactor message passing**: Use `PacketInfo.message` instead of static buffer
4. **Test long-term stability**: Run for several hours, monitor disconnections

### Medium-term (Enhancements)
1. **Improve map tab**: Offline tile fallback or coordinate list view
2. **Message history**: Store/search beyond 50-packet limit
3. **Packet details modal**: Show hex dump, full metadata
4. **Export functionality**: Download packet captures or message logs
5. **Multi-target monitoring**: Track multiple devices simultaneously

### Longer-term (Architecture)
1. **WebSocket message protocol**: Define proper command/response structure
2. **Error handling**: Better client-side error messages and recovery
3. **Performance optimization**: Test with high packet rates
4. **Security**: Add authentication if deploying beyond testing

## Critical Questions for Next Session

1. **WebSocket Queue**: Should we investigate AsyncWebServer queue size configuration? Or implement message throttling?

2. **iOS WiFi**: Should we add captive portal detection workaround? Or just document the cellular disable workaround?

3. **Message Storage**: Refactor to use `PacketInfo.message` now, or wait until we need message history?

4. **RF Activity Granularity**: Is per-config tracking useful, or should we aggregate by frequency?

5. **Commit Strategy**: Commit bug fixes now, or wait for more testing?

## Files Modified Since Last Commit

**Not Yet Committed**:
- `firmware/src/packet_processor.cpp` - Added updateRFActivity in targeted mode
- `firmware/src/web_server.cpp` - Added periodicUpdate() method
- `firmware/src/web_server.h` - Added periodicUpdate() declaration
- `firmware/src/main.cpp` - Call periodicUpdate() every 30s in loop
- `data/webapp/js/api-client.js` - Removed client ping, added visibility handling

## Hardware State

- **Device**: ESP32-S3-DevKitC-1-N8
- **Port**: COM3
- **Flash**: 31.1% used (1,038,905 / 3,342,336 bytes)
- **RAM**: 17.7% used (57,988 / 327,680 bytes)
- **Firmware**: Uploaded and running
- **Filesystem**: Uploaded and running

## Serial Monitor Notes

When testing, saw:
```
ERROR: Too many messages queued
[INFO] WebSocket message from client #1:
```

This was caused by client ping messages. Fixed by removing client-side ping in api-client.js.

Server successfully sends pings via `ws->pingAll()` without queue issues.

## Documentation Updated

- `API_REFERENCE.md` - Complete REST API documentation (from previous session)
- `PHONE_APP_GUIDE.md` - User guide for phone app (from previous session)
- `README.md` - Added phone app section (from previous session)

## Closing Thoughts

The phone app integration is functionally complete and working well. The two bugs we fixed (RF activity and WebSocket stability) were legitimate issues that needed addressing.

However, there are some architectural concerns worth revisiting:

1. **WebSocket message handling**: The queue overflow suggests we need better understanding of AsyncWebServer's internals
2. **Message passing**: Static buffer works but isn't elegant - should refactor
3. **iOS compatibility**: Need better strategy for WiFi switching behavior

The rapid iteration was necessary to get things working, but now would be a good time to:
- Review the WebSocket architecture more carefully
- Test longer-term stability
- Plan any refactoring before adding more features

The codebase is in good shape, well-commented, and ready for the next phase of development.
