# Architecture Refactoring - Completed December 18, 2025

This document describes the refactoring from a god-object pattern (`ReconState`) to a Repository pattern.

## Problem Statement

`ReconState` was a 757-line god object managing too many concerns. We implemented a Repository pattern to isolate storage/retrieval logic into focused, testable classes.

## What Was Completed

### Phase 1: Repository Layer ✅

Created isolated repository classes in `firmware/src/repositories/`:

#### DeviceRepository (`device_repository.h/cpp`)
- Manages `TargetableDevice` storage
- Uses Welford's algorithm for running RSSI statistics
- Supports custom device identification callbacks

#### PacketStore (`packet_store.h/cpp`)
- Manages captured packets for replay
- Fixed-size circular buffer (10 slots)
- Clean capture/retrieve/clear interface

#### NodeTracker (`node_tracker.h/cpp`)
- Tracks "hot" nodes for behavioral analysis
- Running average RSSI with overflow protection
- Iteration support for range-based for loops

All repositories share common patterns:
- `count()`, `capacity()`, `isEmpty()`, `isFull()`
- Iterator support (`begin()`/`end()`)
- Logging via `LOG_INFO`/`LOG_WARN` macros

### Phase 2: Integration ✅

`ReconState` now delegates to repositories internally:

```cpp
class ReconState {
private:
    PacketStore packetStore_;
    NodeTracker nodeTracker_;
    DeviceRepository deviceRepo_;
public:
    // Public API uses getters - delegates to repositories
    uint8_t getNumTargetableDevices() const { return deviceRepo_.count(); }
    uint8_t getNodeCount() const { return nodeTracker_.count(); }
    uint8_t getNumCapturedPackets() const { return packetStore_.count(); }
    const TargetableDevice& getTargetableDevice(uint8_t i) const { return deviceRepo_.getByIndex(i); }
};
```

### Phase 3: Legacy Array Removal ✅ (Dec 18, 2025)

Removed duplicate legacy arrays that were consuming ~11.8KB of RAM:
- `targetableDevices[20]` - 2,800 bytes
- `trackedNodes[150]` - 6,000 bytes  
- `replaySlots[10]` - 3,040 bytes

All consumers now use getter methods instead of direct array access.

### Quick Wins ✅

- Removed duplicate `findTargetableDevice()` from ReconService
- Removed inline `extern` declarations from APIController
- Standardized JSON error responses using `JsonUtils`

## What Was NOT Done (Intentionally)

### Phase 3 & 4: Service Layer Split - CANCELLED

Originally planned to split `ReconService` into QueryService + CaptureService. After review, this was deemed unnecessary:

- `ReconService` as a static class is efficient (no instance overhead)
- The 13 `build*Json()` methods + 9 command methods coexist fine at ~880 lines
- `APIController` being a thin wrapper is actually good separation (HTTP vs business logic)
- Splitting would add complexity without meaningful architectural benefit

## Results

| Metric | Before | After |
|--------|--------|-------|
| `recon_state.cpp` | 757 lines | ~660 lines |
| Storage logic | Mixed in ReconState | Isolated in repositories |
| Testability | Hard to unit test | Repositories testable in isolation |

## File Structure

```
firmware/src/
├── repositories/
│   ├── device_repository.h/cpp    (167 + 211 lines)
│   ├── packet_store.h/cpp         (72 + 87 lines)
│   └── node_tracker.h/cpp         (78 + 88 lines)
├── recon_state.h/cpp              (Delegates to repositories)
├── recon_service.h/cpp            (Unchanged - static service)
└── api_controller.h/cpp           (HTTP layer, uses ReconService)
```

## Future Considerations

If further refactoring is needed:
1. Consumers could use repositories directly instead of through ReconState
2. Legacy public arrays in ReconState could be removed once consumers migrate
3. Device identification helpers could be extracted from ReconState if reused elsewhere
