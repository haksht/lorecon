# Deep Dive: State Machine & Mode Management

## Executive Summary

This document explains how your ESP32 LoRa sniffer manages its operational modes, transitions between reconnaissance and targeting, tracks devices, and coordinates timing. You'll understand the state machine architecture, why certain design decisions were made, and how all the pieces work together in the main loop.

---

## Table of Contents
1. [State Machine Architecture](#state-machine-architecture)
2. [Operation Modes](#operation-modes)
3. [State Transitions](#state-transitions)
4. [Main Loop Mechanics](#main-loop-mechanics)
5. [Timing & Scheduling](#timing-scheduling)
6. [ReconState Management](#reconstate-management)
7. [Real-World Scenarios](#real-world-scenarios)

---

## State Machine Architecture

### The Four States

```cpp
enum OperationMode : uint8_t {
  MODE_RECONNAISSANCE,      // 0: Scan all frequencies
  MODE_TARGETED_CAPTURE,    // 1: Focus on specific device/frequency
  MODE_INTERACTIVE_MENU,    // 2: Wait for user selection
  MODE_PACKET_REPLAY        // 3: Transmit captured packets
};
```

**Visual Representation:**

```
┌────────────────────────────────────────────────────────────┐
│              ESP32 SNIFFER STATE MACHINE                   │
├────────────────────────────────────────────────────────────┤
│                                                            │
│   ┌──────────────────────────────────────────┐           │
│   │   MODE_RECONNAISSANCE (Scan)             │           │
│   │   • Rotate through 16 frequencies         │           │
│   │   • Dwell 12 seconds each                │           │
│   │   • Build device database                │           │
│   │   • Track RF activity                    │           │
│   └────┬─────────────────────────────────┬───┘           │
│        │ "t" or "8" command              │               │
│        │ (user selects target)           │               │
│        ↓                                 │               │
│   ┌─────────────────────────────────────┴───┐           │
│   │   MODE_TARGETED_CAPTURE (Focus)          │           │
│   │   • Lock to single frequency             │           │
│   │   • Capture all packets                  │           │
│   │   • Decrypt and analyze                  │           │
│   │   • Log to file                          │           │
│   └────┬─────────────────────────────────┬───┘           │
│        │ "m" command                      │               │
│        │ (show menu)                      │               │
│        ↓                                 │               │
│   ┌─────────────────────────────────────┴───┐           │
│   │   MODE_INTERACTIVE_MENU (Wait)           │           │
│   │   • Display device list                  │           │
│   │   • Show RF activity                     │           │
│   │   • Accept user commands                 │           │
│   │   • Minimal processing                   │           │
│   └────┬─────────────────────────────────┬───┘           │
│        │ "1-20" (select device)           │               │
│        │ or "r" (restart recon)           │               │
│        ↓                                 │               │
│   ┌─────────────────────────────────────┴───┐           │
│   │   MODE_PACKET_REPLAY (Transmit)          │           │
│   │   • Load captured packet                 │           │
│   │   • Configure TX power                   │           │
│   │   • Transmit packet                      │           │
│   │   • Return to previous mode              │           │
│   └────┬─────────────────────────────────────┘           │
│        │ Automatic after replay               │           │
│        │ or "m" (cancel)                      │           │
│        └────────────────────────────────────→│           │
│                                                            │
└────────────────────────────────────────────────────────────┘

Shortcuts:
  "r" from any mode → MODE_RECONNAISSANCE
  "m" from any mode → MODE_INTERACTIVE_MENU
  "s" → Print statistics
  "c" → Clear tracked devices
```

### Why This Architecture?

**Design Goals:**
1. **Separation of concerns**: Each mode has single responsibility
2. **User control**: Easy to switch between automated/manual
3. **Efficiency**: Minimal work in waiting states
4. **Flexibility**: Add new modes without breaking existing logic

**Alternative Approaches (Rejected):**

**Option A: No states, just flags**
```cpp
// ❌ REJECTED: Becomes spaghetti quickly
bool isScanning = true;
bool isTargeting = false;
bool waitingForInput = false;

void loop() {
    if (isScanning && !isTargeting && !waitingForInput) {
        // ...
    } else if (!isScanning && isTargeting && !waitingForInput) {
        // ...
    } else if (waitingForInput) {
        // ...
    }
    // Nightmare to maintain!
}
```

**Option B: Callback-based**
```cpp
// ❌ REJECTED: Too complex for embedded
void setMode(OperationMode mode) {
    exitCallbacks[currentMode]();
    enterCallbacks[mode]();
    currentMode = mode;
}
// Requires dynamic memory, function pointers, complexity
```

**Option C: Hierarchical state machine**
```cpp
// ❌ OVERKILL: Not needed for 4 states
State → ReconState → ScanState → ...
// Adds complexity without benefit
```

**Chosen: Simple enum + switch statement** ✅
- **Clear**: Easy to understand
- **Fast**: Direct jumps (no indirection)
- **Maintainable**: Add cases easily
- **Debuggable**: Can print current state

---

## Operation Modes

### MODE_RECONNAISSANCE

**Purpose:** Discover what's transmitting and where

**Behavior:**

```cpp
void LoRaReconTool::handleReconnaissanceMode(uint32_t now) {
    // Switch frequency every 12 seconds
    if (now - reconState.scanState.lastScanSwitch >= SCAN_DWELL_TIME) {
        // Rotate to next config
        reconState.scanState.currentConfig = 
            (reconState.scanState.currentConfig + 1) % reconState.getNumConfigs();
        
        reconState.scanState.lastScanSwitch = now;
        
        // Progress report every full cycle
        if (reconState.scanState.currentConfig == 0) {
            uint32_t elapsed = (now - reconState.scanState.reconStartTime) / 1000;
            Serial.printf("[RECON] Cycle complete - %u seconds elapsed, %d devices\n", 
                          elapsed, reconState.numTargetableDevices);
        }
        
        // Apply new radio config
        applyConfig(reconState.scanState.currentConfig);
        radio.startReceive();
    }
}
```

**Timing Analysis:**

```
16 configs × 12 seconds = 192 seconds per full cycle (3.2 minutes)

Example timeline:
  T+0:    Config 0 (Meshtastic_LF_906)
  T+12s:  Config 1 (Meshtastic_LF_902)
  T+24s:  Config 2 (Meshtastic_MF)
  T+36s:  Config 3 (Meshtastic_SF)
  ...
  T+180s: Config 15 (ISM_920_SF8)
  T+192s: Back to Config 0 (cycle complete!)
```

**What Happens During Dwell:**
1. **Radio listens**: Continuously receive mode
2. **Packets arrive**: ISR fires, queue fills
3. **Processing**: Decrypt, analyze, track devices
4. **RF activity**: Update statistics per config
5. **Display updates**: Show current frequency

**Device Discovery:**

```cpp
// When packet is processed:
void processPacket(const uint8_t* data, size_t length, float rssi) {
    PacketInfo info = protocolAnalyzer.analyze(data, length, rssi);
    
    if (info.nodeId != 0) {
        // Add to tracked nodes
        trackNode(info.nodeId, info.protocol, rssi);
        
        // Add to targetable devices (if decoded successfully)
        trackTargetableDevice(info.nodeId, currentConfig, rssi, 
                             info.protocol, data, length);
    }
    
    // Update RF activity for this frequency
    trackRFActivity(currentConfig, rssi);
}
```

**Exit Conditions:**
- User presses 't' or '8' → Select target → MODE_TARGETED_CAPTURE
- User presses 'm' → Show menu → MODE_INTERACTIVE_MENU
- Runs indefinitely otherwise

---

### MODE_TARGETED_CAPTURE

**Purpose:** Focus on single device or frequency for deep capture

**Behavior:**

```cpp
void LoRaReconTool::handleTargetedCaptureMode(uint32_t now) {
    // NO frequency switching - stay locked!
    // Just process incoming packets continuously
    
    // Radio stays on targetConfig:
    // - Higher packet rate (not switching away)
    // - Better chance to decrypt
    // - Capture burst traffic
}
```

**Two Variants:**

**1. Device Targeting** (user selects from menu)
```cpp
void startTargetedCapture(uint8_t deviceIndex) {
    const TargetableDevice& target = reconState.getTargetableDevice(deviceIndex);
    
    reconState.scanState.mode = MODE_TARGETED_CAPTURE;
    reconState.scanState.targetConfig = target.configIndex;  // Their best freq
    reconState.scanState.currentConfig = target.configIndex;
    
    Serial.printf("Target: 0x%08X\n", target.nodeId);
    Serial.printf("Config: %s\n", reconState.getScanConfig(target.configIndex).protocol);
    Serial.printf("Best RSSI: %.1f dBm\n", target.bestRSSI);
    
    applyConfig(reconState.scanState.targetConfig);
    radio.startReceive();
}
```

**2. Frequency Targeting** (user types '8')
```cpp
void startFrequencyTargeting(uint8_t configIndex) {
    const ScanConfig& cfg = reconState.getScanConfig(configIndex);
    
    reconState.scanState.mode = MODE_TARGETED_CAPTURE;
    reconState.scanState.targetConfig = configIndex;
    reconState.scanState.currentConfig = configIndex;
    
    Serial.printf("Target: %s\n", cfg.protocol);
    Serial.printf("Frequency: %.3f MHz\n", cfg.frequency);
    
    applyConfig(configIndex);
    radio.startReceive();
}
```

**Why Lock to One Frequency?**

**Without locking (reconnaissance mode):**
```
Device transmits:  ──┬──  ←─── Message!
Config timeline:   ──0────1────2────3────4────5────6────7────8──
                     ↑ Miss! We're on config 1 now
```

**With locking (targeted mode):**
```
Device transmits:  ──┬──┬──┬──┬──┬──  ←─── Multiple messages!
Config timeline:   ──8────8────8────8────8────8────8────8──
                     ↑   ↑   ↑   ↑   ↑   ←─ Catch them all!
```

**Exit Conditions:**
- User presses 'm' → Show menu → MODE_INTERACTIVE_MENU
- User presses 'r' → Restart → MODE_RECONNAISSANCE

---

### MODE_INTERACTIVE_MENU

**Purpose:** Display results and accept user commands

**Behavior:**

```cpp
case MODE_INTERACTIVE_MENU:
    // Just wait for user input
    delay(BUTTON_DEBOUNCE_DELAY_MS);  // 100ms
    return;  // ← Exit update() early!
```

**Why So Simple?**

**Design rationale:**
- **Minimal CPU**: Don't waste cycles while waiting
- **Responsive**: Check serial frequently (10Hz)
- **Battery friendly**: Avoid unnecessary work
- **Clear separation**: Command handler does the heavy lifting

**What Actually Happens:**

```cpp
void LoRaReconTool::update() {
    // Serial check happens BEFORE mode switch
    if (Serial.available()) {
        char cmd = Serial.read();
        commandHandler->handleCommand(cmd);  // ← Does the work
        return;
    }
    
    // If no serial input, mode switch runs
    switch (reconState.scanState.mode) {
        case MODE_INTERACTIVE_MENU:
            delay(100);  // ← We're here
            return;
    }
}
```

**Command Handler:**

```cpp
void CommandHandler::handleCommand(char cmd) {
    if (cmd >= '1' && cmd <= '9') {
        // Select device by number
        uint8_t deviceIndex = cmd - '1';
        if (deviceIndex < reconState.numTargetableDevices) {
            tool->startTargetedCapture(deviceIndex);
        }
    } else if (cmd == 'm') {
        // Show menu
        showDeviceMenu();
        reconState.scanState.mode = MODE_INTERACTIVE_MENU;
    } else if (cmd == 'r') {
        // Restart reconnaissance
        reconState.clearTargetableDevices();
        reconState.clearRFActivity();
        reconState.scanState.mode = MODE_RECONNAISSANCE;
    }
    // ... more commands
}
```

**Entry Actions:**
- Display device list
- Show RF activity summary
- Print available commands
- Set `waitingForUserInput = true`

**Exit Actions:**
- Clear `waitingForUserInput`
- Apply new configuration (if targeting)

---

### MODE_PACKET_REPLAY

**Purpose:** Retransmit captured packets for testing

**Behavior:**

```cpp
case MODE_PACKET_REPLAY:
    // Packet replay is handled via command handler
    delay(BUTTON_DEBOUNCE_DELAY_MS);
    return;
```

**Replay Process:**

```cpp
void LoRaReconTool::replayPacket(uint8_t slotIndex) {
    if (slotIndex >= reconState.getNumCapturedPackets()) {
        Serial.println("❌ Invalid replay slot");
        return;
    }
    
    const CapturedPacket& packet = reconState.getReplayPacket(slotIndex);
    
    if (!packet.valid) {
        Serial.println("❌ Packet slot is empty");
        return;
    }
    
    Serial.printf("\n📡 REPLAYING PACKET #%d\n", slotIndex + 1);
    Serial.printf("   Protocol: %s\n", packet.protocol);
    Serial.printf("   Length: %d bytes\n", packet.length);
    Serial.printf("   Original RSSI: %.1f dBm\n", packet.originalRSSI);
    
    // Apply original radio config
    applyConfig(packet.configIndex);
    
    // Increase TX power for replay
    radio.setOutputPower(10);  // 10 dBm
    
    // Transmit (RadioLib requires non-const buffer)
    uint8_t txBuffer[MAX_PACKET_SIZE];
    memcpy(txBuffer, packet.data, packet.length);
    
    int state = radio.transmit(txBuffer, packet.length);
    
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("✅ Packet transmitted successfully");
    } else {
        Serial.printf("❌ Transmission failed (error %d)\n", state);
    }
    
    // Back to RX mode
    radio.setOutputPower(0);
    radio.startReceive();
    
    // Return to previous mode
    reconState.scanState.mode = MODE_INTERACTIVE_MENU;
}
```

**Capture Process:**

```cpp
// User command: 'y' = capture for replay
bool capturePacketForReplay(const uint8_t* data, size_t length, 
                           uint8_t configIndex, float rssi, 
                           const char* protocol) {
    if (numCapturedPackets >= MAX_REPLAY_SLOTS) {
        return false;  // Slots full (max 10)
    }
    
    CapturedPacket& slot = replaySlots[numCapturedPackets];
    memcpy(slot.data, data, length);
    slot.length = length;
    slot.configIndex = configIndex;
    slot.originalRSSI = rssi;
    slot.captureTime = millis();
    strncpy(slot.protocol, protocol, sizeof(slot.protocol) - 1);
    slot.valid = true;
    
    numCapturedPackets++;
    
    Serial.printf("[REPLAY] Packet captured to slot #%d\n", numCapturedPackets);
    
    return true;
}
```

**Use Cases:**
- **Testing**: Verify device can receive
- **Mesh join**: Replay beacon to trigger response
- **Range testing**: Compare original vs replayed RSSI
- **Protocol fuzzing**: Modify and retransmit

**Safety:**
- **Low power**: 10 dBm (10mW) = legal, won't interfere
- **Receive-only default**: Must explicitly enable TX
- **Manual trigger**: No automatic replay

---

## State Transitions

### Transition Diagram

```
                  ┌─────────────────┐
                  │   Power On /    │
                  │   Reset ('r')   │
                  └────────┬─────────┘
                           │
                           ↓
           ┌───────────────────────────────────┐
           │   MODE_RECONNAISSANCE             │
           │   (Scan all frequencies)          │
           └───┬───────────────────────────┬───┘
               │                           │
        't' or │                           │ 'm'
          '8'  │                           │
               ↓                           ↓
    ┌──────────────────────┐    ┌──────────────────────┐
    │ MODE_TARGETED_CAPTURE│    │ MODE_INTERACTIVE_MENU│
    │ (Focus on target)    │◄───│ (Show device list)   │
    └──┬───────────────────┘'1-9'└──┬───────────────────┘
       │                           │
    'm'│                    'y'    │
       │                   (capture)│
       │                           ↓
       │                 ┌──────────────────────┐
       │                 │ MODE_PACKET_REPLAY   │
       └────────────────→│ (Retransmit packet)  │
                         └────────┬─────────────┘
                                  │
                          (automatic after TX)
                                  │
                                  ↓
                      MODE_INTERACTIVE_MENU
```

### Transition Triggers

**Command-Driven Transitions:**

```cpp
// From command_handler.cpp
void CommandHandler::handleCommand(char cmd) {
    switch (cmd) {
        case 'r':  // Restart reconnaissance
            reconState.scanState.mode = MODE_RECONNAISSANCE;
            break;
            
        case 't':  // Target mode (ask for device)
            showDeviceMenu();
            reconState.scanState.mode = MODE_INTERACTIVE_MENU;
            break;
            
        case 'm':  // Show menu
            showDeviceMenu();
            reconState.scanState.mode = MODE_INTERACTIVE_MENU;
            break;
            
        case '8':  // Quick target SF8 (encrypted messages)
            tool->startFrequencyTargeting(6);  // Config 6 = 902.125MHz SF8
            // (startFrequencyTargeting sets MODE_TARGETED_CAPTURE)
            break;
            
        case '1'...'9':  // Select device by number
            uint8_t deviceIndex = cmd - '1';
            if (deviceIndex < reconState.numTargetableDevices) {
                tool->startTargetedCapture(deviceIndex);
                // (startTargetedCapture sets MODE_TARGETED_CAPTURE)
            }
            break;
            
        case 'y':  // Capture last packet for replay
            if (reconState.scanState.mode == MODE_TARGETED_CAPTURE) {
                // Capture packet, stay in current mode
            }
            break;
            
        // ... more commands
    }
}
```

**Automatic Transitions:**

```cpp
// After packet replay
void replayPacket(uint8_t slot) {
    // ... transmit packet ...
    
    // Automatically return to menu
    reconState.scanState.mode = MODE_INTERACTIVE_MENU;
}
```

**Guard Conditions:**

```cpp
// Can't target if no devices found
if (reconState.numTargetableDevices == 0) {
    Serial.println("❌ No targetable devices found yet");
    Serial.println("   Run reconnaissance first (press 'r')");
    return;  // Stay in current mode
}

// Can't replay if no packets captured
if (reconState.getNumCapturedPackets() == 0) {
    Serial.println("❌ No packets captured for replay");
    return;  // Stay in current mode
}
```

### State Persistence

**What Survives Transitions:**

```cpp
// Persistent across mode changes:
reconState.targetableDevices[]      ✅ (until 'c' command)
reconState.trackedNodes[]           ✅ (until 'c' command)
reconState.rfActivity[]             ✅ (until 'c' command)
reconState.replaySlots[]            ✅ (until cleared)
#ifdef ENABLE_PSK_TESTING
reconState.pskStats                 ✅ (until reset)
#endif

// Reset on specific transitions:
currentConfig  ← Changes in MODE_RECONNAISSANCE
targetConfig   ← Set when entering MODE_TARGETED_CAPTURE
lastScanSwitch ← Updated in MODE_RECONNAISSANCE

// Never persisted (lost on reboot):
All state (RAM only, no EEPROM/flash storage)
```

---

## Main Loop Mechanics

### The Update Cycle

```cpp
void LoRaReconTool::update() {
    uint32_t now = millis();
    
    // [PHASE 1] Watchdog Reset ────────────────────────────
    esp_task_wdt_reset();  // Pet the dog (prevent reboot)
    
    // [PHASE 2] Packet Reception ──────────────────────────
    handlePacketReception();  // Check atomic flag, read packets
    
    // [PHASE 3] Button Processing ─────────────────────────
    if (buttonPressed) {
        // T-Deck specific: cycle through modes
        buttonPressed = false;
    }
    
    // [PHASE 4] Serial Commands ───────────────────────────
    if (Serial.available()) {
        char cmd = Serial.read();
        commandHandler->handleCommand(cmd);
        return;  // ← Early exit after command!
    }
    
    // [PHASE 5] Mode-Specific Logic ───────────────────────
    switch (reconState.scanState.mode) {
        case MODE_RECONNAISSANCE:
            handleReconnaissanceMode(now);
            break;
            
        case MODE_TARGETED_CAPTURE:
            handleTargetedCaptureMode(now);
            break;
            
        case MODE_INTERACTIVE_MENU:
            delay(BUTTON_DEBOUNCE_DELAY_MS);
            return;  // ← Early exit!
            
        case MODE_PACKET_REPLAY:
            delay(BUTTON_DEBOUNCE_DELAY_MS);
            return;  // ← Early exit!
    }
    
    // [PHASE 6] Polling Delay ─────────────────────────────
    delay(LOOP_POLLING_DELAY_MS);  // 10ms
}
```

**Timing Budget:**

```
Operation                     Time      Notes
────────────────────────────────────────────────────────────
Watchdog reset                <1μs      Single function call
Packet reception check        10ns      Atomic flag load
  └─ If packet present:       7ms       SPI read + queue
Button check                  <1μs      GPIO read
Serial check                  <1μs      UART status register
  └─ If command present:      variable  Command execution
Mode-specific logic:
  └─ Reconnaissance           <1ms      Time check + potential config switch
  └─ Targeted capture         0ms       No-op (just process packets)
  └─ Interactive menu         0ms       No-op (wait for input)
  └─ Packet replay            0ms       No-op (replay via command)
Polling delay                 10ms      Intentional CPU yield
────────────────────────────────────────────────────────────
TOTAL (no packet)             ~10ms     Loop rate: 100Hz
TOTAL (with packet)           ~17ms     Loop rate: 59Hz
```

**Why 10ms Polling Delay?**

```cpp
delay(LOOP_POLLING_DELAY_MS);  // 10ms
```

**Reasoning:**
- **Radio dead time**: 7ms to read packet (can't receive during this)
- **CPU efficiency**: Don't spin-wait (wastes power)
- **Responsiveness**: 100Hz is plenty for UI commands
- **Priority inversion**: Lets watchdog timer, WiFi stack run

**What if we used 0ms?**
```
CPU usage: 100% (bad for battery)
Responsiveness: Same (serial interrupt-driven)
Radio performance: Same (ISR-driven)
Result: Wasted CPU cycles, more heat
```

**What if we used 100ms?**
```
CPU usage: <1% (better for battery)
Responsiveness: Laggy (commands delayed up to 100ms)
Radio performance: Same
Result: Annoying user experience
```

**10ms is the sweet spot**: Responsive + efficient

---

## Timing & Scheduling

### Dwell Time Strategy

**Why 12 seconds per frequency?**

```cpp
#define SCAN_DWELL_TIME 12000  // 12 seconds
```

**Reasoning:**

```
Meshtastic typical transmission intervals:
  - Position updates: 30-300 seconds (configurable)
  - Text messages: Human-paced (30+ seconds between)
  - Routing packets: 10-60 seconds (keepalives)
  - Beacons: 5-10 seconds (discovery)

12 seconds provides:
  ✅ High chance of catching beacons (1-2 transmissions)
  ⚠️ Medium chance of catching messages (depends on timing)
  ❌ Low chance of catching position updates (too slow)

With 16 frequencies:
  Full cycle: 192 seconds (3.2 minutes)
  In 10 minutes: 3 full cycles
  In 1 hour: 18 full cycles
  
Result: Good balance between coverage and probability
```

**Alternative Strategies:**

**1. Adaptive Dwell** (not implemented)
```cpp
// Stay longer on active frequencies
if (rfActivity[currentConfig].activityCount > 5) {
    dwellTime = 30000;  // 30 seconds
} else {
    dwellTime = 12000;  // 12 seconds
}
```
**Pros**: Better capture on active freqs  
**Cons**: Uneven coverage, complex logic

**2. Activity-Triggered** (not implemented)
```cpp
// Switch immediately after packet
if (packetReceived && noActivityFor(5000)) {
    switchToNextConfig();
}
```
**Pros**: Faster scanning when quiet  
**Cons**: Misses burst traffic, unpredictable

**3. Fixed Short Dwell** (rejected)
```cpp
#define SCAN_DWELL_TIME 5000  // 5 seconds
```
**Pros**: Faster full cycle (80 seconds)  
**Cons**: Misses many transmissions (too brief)

**Current approach**: Simple fixed dwell = predictable, reliable

### Cycle Tracking

**Progress Reporting:**

```cpp
if (reconState.scanState.currentConfig == 0) {
    // Just completed full cycle
    uint32_t elapsed = (now - reconState.scanState.reconStartTime) / 1000;
    Serial.printf("[RECON] Cycle complete - %u seconds elapsed, %d devices\n", 
                  elapsed, reconState.numTargetableDevices);
}
```

**Cycle Counter (if needed):**

```cpp
// Not currently implemented, but could add:
struct ScanState {
    // ... existing fields ...
    uint16_t cycleCount;  // Increments at config 0
};

// In handleReconnaissanceMode():
if (reconState.scanState.currentConfig == 0) {
    reconState.scanState.cycleCount++;
    Serial.printf("[RECON] Cycle #%d complete\n", reconState.scanState.cycleCount);
}
```

**Timeout Logic (future feature):**

```cpp
// Stop reconnaissance after N cycles
#define MAX_RECON_CYCLES 10

if (reconState.scanState.cycleCount >= MAX_RECON_CYCLES) {
    Serial.println("[RECON] Maximum cycles reached, stopping");
    reconState.scanState.mode = MODE_INTERACTIVE_MENU;
    showDeviceMenu();
}
```

---

## ReconState Management

### State Encapsulation

**Design Pattern: Singleton State Object**

```cpp
class ReconState {
private:
    // All state variables
    TargetableDevice targetableDevices[MAX_TRACKED_NODES];
    TrackedNode trackedNodes[MAX_TRACKED_NODES];
    RFActivity rfActivity[16];
    CapturedPacket replaySlots[MAX_REPLAY_SLOTS];
    ScanState scanState;
    
public:
    // Controlled access only
    void addTargetableDevice(...);
    const TargetableDevice& getTargetableDevice(uint8_t index) const;
    // ... more methods
};

// Global instance
ReconState reconState;
```

**Benefits:**

1. **Encapsulation**: Internal state hidden
2. **Validation**: Methods can check bounds
3. **Thread-safety**: Single point of control
4. **Testing**: Can mock or reset easily
5. **Namespace**: No global variable pollution

**Why Not OOP Heaven?**

**Could have done:**
```cpp
class Mode {
    virtual void enter() = 0;
    virtual void update() = 0;
    virtual void exit() = 0;
};

class ReconMode : public Mode { ... };
class TargetedMode : public Mode { ... };
```

**Reasons for simpler approach:**
- **Memory**: Virtual tables cost RAM
- **Speed**: Direct calls faster than virtual dispatch
- **Simplicity**: State machine is only 4 states
- **Embedded**: KISS principle (Keep It Simple, Stupid)

### State Initialization

```cpp
ReconState::ReconState() {
    initialize();
}

void ReconState::initialize() {
    // Clear counters
    numTargetableDevices = 0;
    nodeCount = 0;
    numCapturedPackets = 0;
    
    // Zero arrays
    memset(rfActivity, 0, sizeof(rfActivity));
    memset(targetableDevices, 0, sizeof(targetableDevices));
    memset(trackedNodes, 0, sizeof(trackedNodes));
    memset(replaySlots, 0, sizeof(replaySlots));
    
    // Initialize scan state
    scanState.mode = MODE_RECONNAISSANCE;
    scanState.currentConfig = 0;
    scanState.targetConfig = 0;
    scanState.lastScanSwitch = millis();
    scanState.totalPackets = 0;
    scanState.totalDetections = 0;
    scanState.packetPending = false;
    scanState.reconStartTime = millis();
    scanState.waitingForUserInput = false;
    
    #ifdef ENABLE_PSK_TESTING
    pskStats = PSKStats();  // Use constructor
    #endif
}
```

**Why memset() instead of loop?**

```cpp
// Option A: memset (chosen)
memset(rfActivity, 0, sizeof(rfActivity));
// Compiles to: Single DMA transfer or optimized memset
// Time: ~1-2μs for 1KB

// Option B: Loop (slower)
for (int i = 0; i < 16; i++) {
    rfActivity[i] = {0};  // Call constructor per element
}
// Time: ~10-20μs (10× slower)

// For 256-byte buffers, memset is MUCH faster
```

### State Queries

**Read-only access patterns:**

```cpp
// ✅ GOOD: Return const reference
const ScanConfig& getScanConfig(uint8_t index) const {
    if (index >= NUM_CONFIGS) {
        return scanConfigs[0];  // Fallback to first
    }
    return scanConfigs[index];
}

// ✅ GOOD: Return by value (small struct)
uint8_t getNumConfigs() const {
    return NUM_CONFIGS;
}

// ✅ GOOD: Boolean query
bool hasTargetableDevices() const {
    return numTargetableDevices > 0;
}

// ❌ BAD: Return non-const pointer (allows external modification)
TargetableDevice* getTargetableDevice(uint8_t index) {
    return &targetableDevices[index];  // Dangerous!
}
```

**Why const correctness matters:**

```cpp
// Without const:
ScanConfig& cfg = reconState.getScanConfig(0);
cfg.frequency = 999.9;  // ← Accidentally modified!
// Now radio will try invalid frequency

// With const:
const ScanConfig& cfg = reconState.getScanConfig(0);
cfg.frequency = 999.9;  // ← Compiler error! ✅
```

---

## Real-World Scenarios

### Scenario 1: First Boot

**Timeline:**

```
T+0s:    Power on
         └─ ReconState::initialize()
         └─ Mode = MODE_RECONNAISSANCE
         └─ currentConfig = 0
         
T+0.1s:  Apply config 0 (Meshtastic_LF_906, 906.875 MHz)
         └─ Radio starts receiving
         
T+5s:    Packet received on 906.875 MHz!
         └─ ISR fires, flag set
         └─ handlePacketReception() reads packet
         └─ Protocol analysis: Meshtastic node 0x12345678
         └─ Add to targetableDevices[0]
         └─ Update rfActivity[0]
         
T+12s:   Dwell time expired
         └─ Switch to config 1 (Meshtastic_LF_902, 902.125 MHz)
         └─ Apply config, restart receive
         
T+20s:   Packet received on 902.125 MHz!
         └─ Different node: 0xABCDEF01
         └─ Add to targetableDevices[1]
         └─ Update rfActivity[1]
         
T+192s:  Full cycle complete (back to config 0)
         └─ Print progress report
         └─ "Cycle complete - 192 seconds elapsed, 2 devices"
         
T+384s:  Second cycle complete
         └─ Now have 4 devices in database
         
T+600s:  User presses 'm' (show menu)
         └─ Mode = MODE_INTERACTIVE_MENU
         └─ Display 4 targetable devices
```

### Scenario 2: Targeting Specific Device

**Timeline:**

```
T+0s:    In MODE_INTERACTIVE_MENU
         Have 3 devices:
           1. 0x12345678 (Meshtastic Mobile, 902.125 MHz SF10)
           2. 0xABCDEF01 (Meshtastic Router, 906.875 MHz SF11)
           3. 0x55555555 (Meshtastic Handheld, 902.125 MHz SF8)
         
T+0s:    User presses '3' (select device 3)
         
T+0.1s:  CommandHandler::handleCommand('3')
         └─ deviceIndex = 2 (0-based)
         └─ Call startTargetedCapture(2)
         
T+0.2s:  startTargetedCapture(2) executes:
         └─ target = targetableDevices[2]  // 0x55555555
         └─ Mode = MODE_TARGETED_CAPTURE
         └─ targetConfig = 6  // Config 6 = 902.125 MHz SF8
         └─ currentConfig = 6
         └─ Apply config 6
         └─ Radio locked to 902.125 MHz SF8
         
T+1s:    Radio listening on 902.125 MHz SF8 (target's frequency)
         
T+5s:    Packet received!
         └─ From 0x55555555 ✓ (our target)
         └─ Protocol analysis, decryption attempt
         └─ Log to file: /packets.jsonl
         
T+8s:    Another packet from 0x55555555 ✓
         
T+12s:   NO frequency switch (still on config 6)
         
T+15s:   Packet from different node (0x99999999)
         └─ Still process (we capture everything on this freq)
         
T+30s:   User presses 'm' (back to menu)
         └─ Mode = MODE_INTERACTIVE_MENU
         └─ Display updated statistics
         └─ Device #3 now has more packets captured
```

### Scenario 3: Packet Replay

**Timeline:**

```
T+0s:    In MODE_TARGETED_CAPTURE (locked to 902.125 MHz SF8)
         Monitoring node 0x55555555
         
T+5s:    Packet received from 0x55555555
         Length: 87 bytes, RSSI: -73 dBm
         Decrypted: "Hello from the trail!"
         
T+5.1s:  User presses 'y' (capture for replay)
         
T+5.2s:  capturePacketForReplay() executes:
         └─ Copy to replaySlots[0]
         └─ Store: data, length, configIndex, RSSI, protocol
         └─ numCapturedPackets = 1
         └─ Print: "[REPLAY] Packet captured to slot #1"
         └─ Stay in MODE_TARGETED_CAPTURE
         
T+30s:   User presses 'l' (list replay slots)
         └─ Print all captured packets
         
T+35s:   User presses 'R' (replay menu)
         └─ Show replay slots
         
T+40s:   User types '1' (replay slot 1)
         
T+40.1s: replayPacket(0) executes:
         └─ Load replaySlots[0]
         └─ Apply config 6 (same as capture)
         └─ Set TX power to 10 dBm
         └─ Transmit 87 bytes
         
T+40.5s: Transmission complete
         └─ Reset to RX mode
         └─ Mode = MODE_INTERACTIVE_MENU
         └─ Print "✅ Packet transmitted successfully"
         
Result:  Target device receives duplicate "Hello from the trail!"
         (Possible: Appears as echo or duplicate message)
```

### Scenario 4: Error Recovery

**Scenario: Radio configuration fails**

```
T+0s:    In MODE_RECONNAISSANCE, config 5
         
T+12s:   Time to switch to config 6
         
T+12.1s: applyConfig(6) called
         └─ radio.setFrequency(902.125)
            ↓ Returns RADIOLIB_ERR_INVALID_FREQUENCY
         
T+12.2s: applyConfig() returns false
         
T+12.3s: handleReconnaissanceMode() detects failure:
         if (applyConfig(...)) {
             radio.startReceive();  // ← Skipped!
         }
         // Continue with old config (5)
         
T+24s:   Try again on next switch (config 7)
         └─ This time succeeds
         └─ Config 6 skipped (acceptable loss)
         
Result:  Graceful degradation (skip bad config, continue)
```

**Scenario: Watchdog timeout**

```
T+0s:    Normal operation, update() running
         
T+0s-30s: update() calling esp_task_wdt_reset() every ~10ms
         
T+30s:   BUG INTRODUCED: Infinite loop somewhere
         while (true) { /* stuck! */ }
         └─ Never calls esp_task_wdt_reset()
         
T+60s:   Watchdog timer expires (30s timeout)
         └─ Hardware interrupt fires
         └─ ESP32 performs hard reset
         
T+60.1s: setup() runs again
         └─ ReconState::initialize()
         └─ All state lost (back to config 0)
         └─ Start reconnaissance from scratch
         
Result:  Automatic recovery, no permanent harm
```

---

## Summary

### State Machine Strengths

**✅ What Works Well:**

1. **Simple & Clear**: 4 states, easy to understand
2. **Mode Separation**: Each mode has single responsibility
3. **User Control**: Easy to navigate via serial commands
4. **Graceful Degradation**: Errors don't crash system
5. **Efficient**: Minimal CPU in waiting states
6. **Extensible**: New modes can be added easily

**⚠️ Trade-offs:**

1. **No Persistence**: Lost on reboot (could add EEPROM)
2. **Fixed Dwell Time**: Not adaptive to activity
3. **Linear Scan**: Could prioritize active frequencies
4. **Manual Mode Changes**: Could auto-target on single strong device

### Key Takeaways

**State Management:**
- ✅ Enum + switch = simple, fast, clear
- ✅ ReconState encapsulates all state
- ✅ Const correctness prevents accidents
- ✅ memset() for fast initialization

**Timing:**
- ✅ 12s dwell = good balance
- ✅ 10ms polling = responsive + efficient
- ✅ Watchdog timer = automatic recovery
- ✅ Cycle tracking = progress visibility

**Mode Transitions:**
- ✅ Command-driven = user control
- ✅ Guard conditions = prevent invalid states
- ✅ Early returns = CPU efficiency
- ✅ State persistence = data survives transitions

**Real-World Performance:**
- ✅ Discovers devices in ~3-10 minutes
- ✅ Captures burst traffic in targeted mode
- ✅ Handles errors gracefully
- ✅ Battery-friendly operation

---

## Code Locations Reference

**Primary files:**
- `recon_state.cpp` (lines 1-360): State management implementation
- `recon_state.h` (lines 1-100): State structures and interface
- `lora_recon_tool.cpp` (lines 180-280): Main loop and mode handlers
- `command_handler.cpp`: User command processing

**Key functions:**
- `update()`: Main loop coordinator (line 155)
- `handleReconnaissanceMode()`: Frequency scanning (line 264)
- `handleTargetedCaptureMode()`: Focused capture (line 198)
- `startTargetedCapture()`: Begin device targeting (line 505)
- `applyConfig()`: Configure radio parameters (line 215)

**State structures:**
- `OperationMode` enum: `data_structures.h` (lines 12-17)
- `ScanState` struct: `data_structures.h` (lines 69-80)
- `ReconState` class: `recon_state.h` (lines 17-90)

---

## Next Steps

Now that you understand the state machine, we can explore:

1. **Error Handling**: Recovery strategies and fault tolerance
2. **Display System**: OLED rendering and UI optimization
3. **Session Key Harvesting**: Active mesh participation
4. **Hardware Abstraction**: Radio driver internals
5. **Testing Strategy**: How to validate state transitions

**Pick a topic for the next deep dive!**

---

*Generated for ESP32-Sniffer deep-dive session*  
*Last updated: 2025-10-16*
