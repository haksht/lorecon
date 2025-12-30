# ESP32 LoRa Sniffer - Debug Session Handoff

## Session Date: December 30, 2024

## Issue Investigated
**Spontaneous reboots in frequency target mode** - Device ran fine overnight in recon mode but crashed repeatedly when:
1. Running in freq target mode (locked on single frequency)
2. Using the web UI (especially switching between Devices and Packets tabs)

Reset reason always showed "Power-on (code 1)" due to disabled brownout detector.

## Root Cause Identified
**Race condition** between two execution contexts accessing shared data structures without synchronization:

1. **Main loop task**: Captures packets → writes to `PacketStore` and `DeviceRepository`
2. **AsyncWebServer task**: Handles API requests → reads from same data structures

In freq target mode, sustained high packet volume dramatically increased collision probability. In recon mode, 12-second frequency hopping created natural gaps that made crashes rare.

## Fix Implemented (Flashed to Device)

Added FreeRTOS mutex to `ReconState` protecting repository access:

### Files Modified:
- `firmware/src/recon_state.h` - Added mutex member, `lock()`/`unlock()` methods, `ScopedLock` RAII helper, `getPacketStore()` accessor
- `firmware/src/recon_state.cpp` - Mutex creation in constructor, thread-safe wrappers for all repository methods
- `firmware/src/json_builders.cpp` - `buildDevicesJson()` and `buildReplaySlotsJson()` now use `ScopedLock` for iteration

### Key Code Pattern:
```cpp
// API handler pattern (holds lock during entire JSON build)
ReconState::ScopedLock lock(reconState);
if (!lock) {
    // Return error response
}
// Safe to iterate - main loop blocked from writing
```

## Previous Session Fixes (Also on Device)
- **Button debouncing** (100ms) - Prevents phantom GPIO 0 noise from triggering display toggle
- **Crash context logging** - Saves mode/heap/action to NVS every 10 seconds for post-mortem

## Current Device State
- Firmware with mutex fix flashed to COM3
- Serial monitor running
- In freq target mode on Meshtastic_LF_906
- Initial testing shows stable operation with "(locked)" appearing in logs

## Verification Needed
1. **Short-term**: Rapid Devices/Packets tab switching in web UI while in freq target mode
2. **Long-term**: Overnight run in freq target mode with periodic web UI access
3. **Watch for**: `[WARN] Failed to acquire lock` messages (would indicate mutex contention)

## If Crashes Continue
Investigate other shared state not yet protected:
- `rfActivity[]` array
- `scanState` struct  
- `anomalies[]` array
- WebSocket broadcast paths (`broadcastAggregatedUpdate`, `broadcastDeviceUpdate`)

## Commands Reference
```powershell
# Flash firmware
C:\Users\tim\.platformio\penv\Scripts\pio.exe run --target upload --upload-port COM3

# Serial monitor  
C:\Users\tim\.platformio\penv\Scripts\pio.exe device monitor --port COM3 --baud 115200

# Upload web UI files
C:\Users\tim\.platformio\penv\Scripts\pio.exe run --target uploadfs
```

## Device Access
- **WiFi SSID**: LoRa-3DAF74
- **Password**: recon-3DAF74
- **Web UI**: http://192.168.4.1
- **API Token**: fb5884cf1b0f968397d67a50a2838409
