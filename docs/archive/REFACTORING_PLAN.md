# Code Cleanup & Refactoring Plan

**Goal**: Clean, simple, modular embedded C++ code with minimal globals

**Philosophy**: Each module should be testable, the data flow should be clear, and dependencies should be explicit.

---

## Problems Identified

### 1. **Global State Everywhere**
```cpp
// Current mess:
extern ReconState reconState;           // main.cpp, user_interface.cpp
extern PSKStats pskStats;               // psk_decryption_simple.cpp
SX1262 radio = new Module(...);         // main.cpp only
std::atomic<bool> packetReceived;       // main.cpp only
HardwareStressTester* stressTester;     // main.cpp only
```

**Problem**: Tight coupling, hard to test, unclear ownership

### 2. **Mixed Responsibilities**
- `main.cpp`: Has protocol parsing, device type detection, power estimation
- `user_interface.cpp`: Reads from global `reconState` directly
- Helper functions scattered everywhere

### 3. **"Dead" Code Still in Repo**
```
firmware/src/
├── traffic_analysis.cpp/.h      (~600 lines, disabled)
├── network_reconnaissance.cpp/.h (~500 lines, disabled)
├── intelligence_storage.cpp/.h   (~800 lines, disabled)
├── advanced_psk_recovery.cpp/.h  (~400 lines, disabled)
```

**Problem**: False complexity, maintenance burden, confusing to contributors

### 4. **Module Dependencies Are Implicit**
```
main.cpp includes user_interface.h
user_interface.h includes recon_state.h
But user_interface.cpp modifies reconState directly
```

**Problem**: Circular dependencies, unclear data flow

---

## Refactoring Strategy

### Phase 1: Delete Dead Code (30 minutes)
Remove all disabled modules to see true architecture

### Phase 2: Dependency Injection (2-3 hours)
Pass state as parameters instead of using globals

### Phase 3: Extract Protocol Logic (1-2 hours)
Move protocol parsing into dedicated class

### Phase 4: Simplify User Interface (1 hour)
Make UI a pure display layer with no business logic

---

## Phase 1: Delete Dead Code ✂️

### Files to DELETE Completely
```bash
# These are disabled and add no value
rm firmware/src/traffic_analysis.cpp
rm firmware/src/traffic_analysis.h
rm firmware/src/network_reconnaissance.cpp
rm firmware/src/network_reconnaissance.h
rm firmware/src/advanced_psk_recovery.cpp
rm firmware/src/advanced_psk_recovery.h
rm firmware/src/intelligence_storage.cpp
rm firmware/src/intelligence_storage.h
rm firmware/src/psk_decryption.h  # Old PSK header, unused

# Test files (keep test_lilygo_device.cpp for now)
# rm firmware/src/test_lilygo_device.cpp
```

### Files to DELETE from docs/
```bash
# Outdated documentation
rm docs/device_detection_strategy.md
rm docs/universal_lora_architecture.md
rm firmware/UART_TAPPING_CABLES.md
rm firmware/STANDALONE_UART_TAPPING.md
rm firmware/INTELLIGENCE_MODULES.md
rm firmware/PSK_DECRYPTION_GUIDE.md
rm firmware/modules/README.md
rm firmware/modules/serial_intelligence.cpp
rm firmware/modules/unified_intelligence.cpp
```

**Expected Result**: Project is ~2500 lines smaller, only active code remains

---

## Phase 2: Dependency Injection 💉

### Current Architecture (Bad)
```
main.cpp:
  - Creates global `SX1262 radio`
  - Creates global `reconState`
  - Calls functions that access globals

user_interface.cpp:
  - Directly accesses `extern reconState`
  - Modifies state without main.cpp knowing

psk_decryption_simple.cpp:
  - Uses global `pskStats`
```

### Target Architecture (Good)
```cpp
class LoRaReconTool {
private:
    SX1262 radio;
    ReconState state;
    PSKDecryption pskTester;
    HardwareStressTester* stressTester;
    std::atomic<bool> packetReceived;
    
public:
    void setup();
    void loop();
    
private:
    void handleUserInput(char cmd);
    void handleReconnaissance();
    void handleTargetedCapture();
    void handlePacketReception();
};
```

### Step-by-Step Refactor

#### Step 2.1: Create Main Application Class
**New File**: `firmware/src/lora_recon_tool.h`

```cpp
#ifndef LORA_RECON_TOOL_H
#define LORA_RECON_TOOL_H

#include <RadioLib.h>
#include <atomic>
#include "recon_state.h"
#include "protocol_analyzer.h"

#ifdef ENABLE_PSK_TESTING
#include "psk_decryption_simple.h"
#endif

#ifdef ENABLE_STRESS_TESTING
#include "hardware_stress_tester.h"
#endif

class LoRaReconTool {
public:
    LoRaReconTool();
    
    bool initialize();
    void loop();
    
    // Public for interrupt handler access
    void markPacketReceived() { 
        packetReceived.store(true, std::memory_order_release); 
    }
    
private:
    // Hardware
    SX1262 radio;
    std::atomic<bool> packetReceived;
    
    // State
    ReconState state;
    
    // Optional modules
#ifdef ENABLE_PSK_TESTING
    PSKDecryption pskTester;
#endif
    
#ifdef ENABLE_STRESS_TESTING
    HardwareStressTester* stressTester;
#endif
    
    // Core operations
    void handleUserInput(char cmd);
    void handleReconnaissance(uint32_t now);
    void handleTargetedCapture(uint32_t now);
    void handlePacketReception();
    
    // Radio operations
    bool applyConfig(uint8_t configIndex);
    
    // Helpers
    void logPacket(const uint8_t* data, size_t length, 
                   float rssi, float snr, const char* protocol);
};

#endif
```

#### Step 2.2: Refactor User Interface
**Change**: Pass state as parameter instead of using extern

```cpp
// user_interface.h - BEFORE
void showReconResults();  // Accesses extern reconState

// user_interface.h - AFTER
void showReconResults(const ReconState& state);
```

#### Step 2.3: Refactor PSK Testing
**Change**: Return stats instead of using global

```cpp
// psk_decryption_simple.h - BEFORE
extern PSKStats pskStats;

// psk_decryption_simple.h - AFTER
class PSKDecryption {
private:
    PSKStats stats;
    
public:
    const PSKStats& getStats() const { return stats; }
    // ... rest of interface
};
```

---

## Phase 3: Extract Protocol Logic 🧩

### Current Problem
Protocol parsing scattered in main.cpp:
- `identifyProtocol()` - standalone function
- `identifyDeviceType()` - standalone function  
- `extractNodeId()` - standalone function
- `isRoutingDevice()` - standalone function
- `estimatePowerClass()` - standalone function

### Solution: ProtocolAnalyzer Class

**New File**: `firmware/src/protocol_analyzer.h`

```cpp
#ifndef PROTOCOL_ANALYZER_H
#define PROTOCOL_ANALYZER_H

#include <Arduino.h>

struct PacketInfo {
    const char* protocol;
    const char* deviceType;
    uint32_t nodeId;
    uint8_t powerClass;
    bool isRouter;
};

class ProtocolAnalyzer {
public:
    // Main analysis function
    PacketInfo analyzePacket(const uint8_t* data, size_t length, float rssi);
    
private:
    // Protocol identification
    const char* identifyProtocol(const uint8_t* data, size_t length);
    
    // Meshtastic-specific
    uint32_t extractMeshtasticNodeId(const uint8_t* data, size_t length);
    const char* identifyMeshtasticDeviceType(const uint8_t* data, size_t length, float rssi);
    bool isMeshtasticRouter(const uint8_t* data, size_t length);
    
    // LoRaWAN-specific
    uint32_t extractLoRaWANDevAddr(const uint8_t* data, size_t length);
    const char* identifyLoRaWANDeviceType(const uint8_t* data, size_t length);
    
    // General analysis
    uint8_t estimatePowerClass(float rssi);
};

#endif
```

**Benefits**:
- All protocol logic in one place
- Easy to add new protocols (Helium, etc.)
- Testable independently
- Clear single responsibility

---

## Phase 4: Simplify User Interface 📺

### Current Problem
`user_interface.cpp` has 783 lines doing too much:
- Direct state access via extern
- Menu logic mixed with display logic
- Stress testing logic embedded

### Solution: Pure Display Layer

```cpp
// user_interface.h - NEW INTERFACE

class UserInterface {
public:
    // Display functions (const - no modification)
    void showWelcome();
    void showReconStart();
    void showReconResults(const ReconState& state);
    void showActivityDetails(const ReconState& state);
    void showDeviceTypeSummary(const ReconState& state);
    
    // Menu display (returns user selection)
    char showMainMenu(const ReconState& state);
    char showFrequencyMenu(const ReconState& state);
    char showReplayMenu(const ReconState& state);
    
#ifdef ENABLE_STRESS_TESTING
    char showStressTestMenu();
#endif

private:
    // Pure formatting functions
    void printHeader(const char* title);
    void printDeviceTable(const TargetableDevice* devices, uint8_t count);
    void printActivityTable(const RFActivity* activities, uint8_t count);
};
```

**Key Change**: UI never modifies state, only displays it

---

## File Structure After Refactoring

```
firmware/src/
├── main.cpp                      # Minimal setup/loop, delegates to LoRaReconTool
├── lora_recon_tool.cpp/.h        # Main application class (replaces global functions)
├── protocol_analyzer.cpp/.h      # Protocol parsing logic (extracted from main.cpp)
├── recon_state.cpp/.h            # State management (keeps existing)
├── user_interface.cpp/.h         # Pure display layer (simplified)
├── psk_decryption_simple.cpp/.h  # PSK testing (refactored to class)
├── hardware_stress_tester.cpp/.h # Hardware testing (keeps existing)
├── data_structures.h             # Shared data types (keeps existing)
└── main_realistic.cpp            # Simple version (keeps for education)
```

**Total Lines**: ~2000 (down from ~4500)

---

## Implementation Order

### Week 1: Foundation
- [ ] Day 1: Delete dead code (traffic analysis, network recon, etc.)
- [ ] Day 2: Create ProtocolAnalyzer class, extract functions from main.cpp
- [ ] Day 3: Test protocol analyzer independently

### Week 2: Dependency Injection  
- [ ] Day 1: Create LoRaReconTool class skeleton
- [ ] Day 2: Move functions from main.cpp to LoRaReconTool methods
- [ ] Day 3: Refactor UserInterface to take const references

### Week 3: Testing & Polish
- [ ] Day 1: Refactor PSKDecryption to class (remove global stats)
- [ ] Day 2: Test all features still work
- [ ] Day 3: Update documentation, clean up comments

---

## Success Criteria

### Code Quality
- ✅ Zero global state (except interrupt handler glue)
- ✅ Each class has single responsibility
- ✅ Dependencies are explicit (passed as parameters)
- ✅ All business logic in classes, not standalone functions

### Simplicity
- ✅ < 2000 lines of active code
- ✅ Clear data flow (input → process → output)
- ✅ Easy to understand what each file does
- ✅ New contributors can navigate quickly

### Testability
- ✅ ProtocolAnalyzer testable without hardware
- ✅ ReconState testable independently
- ✅ UserInterface testable with mock state

---

## Anti-Patterns to Avoid

### ❌ Don't Do This
```cpp
// Global access everywhere
extern ReconState reconState;
void someFunction() {
    reconState.updateNode(...);  // Who called this? When? Why?
}
```

### ✅ Do This Instead
```cpp
// Explicit dependencies
class LoRaReconTool {
    ReconState state;
    
    void handlePacket(const uint8_t* data, size_t len) {
        // Clear ownership: LoRaReconTool owns state
        state.updateNode(...);
    }
};
```

---

## Expected Improvements

### Before Refactoring
```
Complexity: 7/10 (too many globals)
Modularity: 6/10 (mixed responsibilities)
Testability: 4/10 (hard to test without hardware)
Maintainability: 6/10 (unclear dependencies)
```

### After Refactoring
```
Complexity: 8/10 (cleaner architecture)
Modularity: 9/10 (clear separation)
Testability: 8/10 (most code testable without hardware)
Maintainability: 9/10 (explicit dependencies)
```

---

## Next Steps

1. **Review this plan** - Make sure approach makes sense
2. **Start with Phase 1** - Delete dead code (safe, immediate improvement)
3. **Implement ProtocolAnalyzer** - Extract and test protocol logic
4. **Create LoRaReconTool class** - Wrap main application
5. **Refactor UserInterface** - Make it pure display layer
6. **Test everything** - Ensure features still work
7. **Update docs** - Reflect new architecture

---

**Estimated Total Effort**: 15-20 hours over 2-3 weeks  
**Risk Level**: Low (incremental changes, test after each phase)  
**Reversibility**: High (git branches for each phase)

