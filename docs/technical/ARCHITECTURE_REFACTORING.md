# Architecture Refactoring Plan

This document describes the ongoing refactoring from a god-object pattern (`ReconState`) to a Repository + Query pattern (CQRS-like).

## Current State (v2.2.0)

### Problems Identified

1. **ReconState is a 757-line god object** managing:
   - Device tracking (`targetableDevices[]`, `numTargetableDevices`)
   - Node tracking (`trackedNodes[]`, `nodeCount`)
   - Packet capture (`replaySlots[]`, `numCapturedPackets`)
   - RF activity stats (`rfActivity[]`)
   - Anomaly detection (`anomalies[]`)
   - Network intelligence (`networkIntel`)
   - PSK statistics (`pskStats`)
   - Traffic histograms (`trafficHist`)

2. **Layering confusion** between `APIController` and `ReconService`:
   - Both handle similar concerns (JSON building, command dispatch)
   - `ReconService` has 13 `build*Json()` methods and 9 command methods
   - `APIController` is mostly pass-through to `ReconService`

3. **Scattered state access** throughout codebase via `extern ReconState reconState`

## Phase 1: Repository Layer ✅ COMPLETED

Created isolated repository classes in `firmware/src/repositories/`:

### DeviceRepository (`device_repository.h/cpp`)
- Manages `TargetableDevice` storage
- Uses Welford's algorithm for running RSSI statistics
- Supports custom device identification callbacks
- Thread-safe considerations documented (caller responsibility)

### PacketStore (`packet_store.h/cpp`)
- Manages captured packets for replay
- Fixed-size circular buffer (10 slots)
- Clean capture/retrieve/clear interface

### NodeTracker (`node_tracker.h/cpp`)
- Tracks "hot" nodes for behavioral analysis
- Running average RSSI with overflow protection
- Iteration support for range-based for loops

All repositories share common patterns:
- `count()`, `capacity()`, `isEmpty()`, `isFull()`
- Iterator support (`begin()`/`end()`)
- Logging via `LOG_INFO`/`LOG_WARN` macros

## Phase 2: Integration (PENDING)

**Goal**: Have `ReconState` delegate to repositories internally while maintaining the existing public API.

```cpp
// Future ReconState.h
class ReconState {
private:
    DeviceRepository deviceRepo_;
    PacketStore packetStore_;
    NodeTracker nodeTracker_;
    
public:
    // Existing API preserved
    void addTargetableDevice(...) { deviceRepo_.addOrUpdate(...); }
    uint8_t numTargetableDevices() const { return deviceRepo_.count(); }
    // ... etc
};
```

This allows incremental migration without breaking consumers.

## Phase 3: Service Layer (FUTURE)

Split `ReconService` into:

### QueryService
- All `build*Json()` methods
- Read-only access to repositories
- No side effects

### CaptureService  
- `startTargetedCapture()`, `stopCapture()`
- Replay management
- Mode transitions

### APIController
- HTTP request handling only
- Delegates to query/command services
- Authentication checks

## Phase 4: Thin ReconState (FUTURE)

Once repositories handle storage:
- `ReconState` becomes pure data holder
- Remove business logic from state class
- Consider renaming to `ReconContext` or `ScanSession`

## File Size Targets

Current state (pre-refactor):
- `recon_state.cpp`: 757 lines ❌ Too large
- `recon_service.cpp`: 880 lines ❌ Too large

Target:
- Each file < 300 lines
- Single responsibility per module

## Migration Strategy

1. **Non-breaking**: Keep public APIs stable
2. **Incremental**: One repository at a time
3. **Tested**: Build after each change
4. **Documented**: Update this file with progress

## Quick Wins Completed

- ✅ Removed duplicate `findTargetableDevice()` from ReconService
- ✅ Removed inline `extern` declarations from APIController
- ✅ Standardized JSON error responses using `JsonUtils`
