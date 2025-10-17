# Deep Dive: Error Handling & Recovery

## Executive Summary

This document explains how your ESP32 LoRa sniffer handles failures, recovers from errors, and maintains system stability. You'll understand the error handling architecture, watchdog timer mechanics, recovery strategies, and defensive programming patterns that make the system robust.

---

## Table of Contents
1. [Error Handling Philosophy](#error-handling-philosophy)
2. [Watchdog Timer Deep Dive](#watchdog-timer)
3. [Error Handler Architecture](#error-handler-architecture)
4. [Recovery Strategies](#recovery-strategies)
5. [Defensive Programming Patterns](#defensive-programming)
6. [Failure Mode Analysis](#failure-modes)
7. [Debugging & Diagnostics](#debugging)

---

## Error Handling Philosophy

### The Three Pillars

```
┌────────────────────────────────────────────────────────┐
│         ERROR HANDLING STRATEGY                        │
├────────────────────────────────────────────────────────┤
│                                                        │
│  1. PREVENT                                            │
│     ├─ Validation at boundaries                       │
│     ├─ Fixed-size buffers (no malloc)                 │
│     ├─ Const correctness                              │
│     └─ Static analysis (compiler warnings)            │
│                                                        │
│  2. DETECT                                             │
│     ├─ Watchdog timer (30s timeout)                   │
│     ├─ Bounds checking                                │
│     ├─ Return value validation                        │
│     └─ Error logging (structured)                     │
│                                                        │
│  3. RECOVER                                            │
│     ├─ Graceful degradation                           │
│     ├─ Automatic retry (radio)                        │
│     ├─ State reset (reconState)                       │
│     └─ Hardware reset (watchdog)                      │
│                                                        │
└────────────────────────────────────────────────────────┘
```

### Design Principles

**1. Fail Gracefully**
```cpp
// ❌ BAD: Crash on error
if (!radio.begin()) {
    while(1);  // Stuck forever!
}

// ✅ GOOD: Continue with degraded functionality
if (!radio.begin()) {
    Serial.println("Radio failed - logging only mode");
    LOG_RADIO_ERROR(ErrorCodes::RADIO_INIT_FAILED, "Radio init failed");
    // Continue without radio (still useful for logging)
}
```

**2. Log Everything**
```cpp
// Every error goes through ErrorHandler
ErrorHandler::logError(
    ERR_RADIO,           // Category
    SEVERITY_ERROR,      // Severity
    101,                 // Error code
    "Config failed",     // Message
    __FUNCTION__,        // Function name
    __LINE__             // Line number
);

// Creates audit trail for debugging
```

**3. Attempt Recovery**
```cpp
// Don't just log and give up
if (radioError) {
    bool recovered = ErrorHandler::attemptRecovery(error);
    if (recovered) {
        continue;  // Back to normal operation
    } else {
        // Escalate or degrade gracefully
    }
}
```

---

## Watchdog Timer

### What Is a Watchdog?

**Hardware watchdog timer:**
- **Independent timer**: Runs separately from CPU
- **Must be reset periodically**: "Pet the dog" every ~10ms
- **If not reset**: Hardware forces system reboot
- **Purpose**: Recover from infinite loops, deadlocks, crashes

**Visual:**

```
Normal Operation:
─────────────────────────────────────────────────────────
Time:       0    10ms   20ms   30ms   40ms   50ms   60ms
WDT Reset:  ●────●──────●──────●──────●──────●──────●───
WDT Count:  0→10→0→10→0→10→0→10→0→10→0→10→0→10→0───
Status:     OK   OK     OK     OK     OK     OK     OK

Hung System:
─────────────────────────────────────────────────────────
Time:       0    10ms   20ms   30ms   ...   30s
WDT Reset:  ●─────────────────────────────────(none)────
WDT Count:  0→10→20→30→...→30000 → TIMEOUT!
Status:     OK   OK     OK     ...   💥 REBOOT
```

### Initialization

```cpp
void LoRaReconTool::initialize() {
    // Initialize watchdog timer (30 second timeout)
    esp_task_wdt_init(30, true);  // timeout=30s, panic=true
    esp_task_wdt_add(NULL);        // Monitor current task (loopTask)
    
    Serial.println("[WATCHDOG] Hardware watchdog enabled (30s timeout)");
}
```

**Parameter Breakdown:**

```cpp
esp_task_wdt_init(
    30,     // Timeout in seconds
            // Why 30? Long enough for slow operations (display, file I/O)
            //         Short enough for quick recovery
    
    true    // Panic on timeout
            // true = Print stack trace + reboot (debugging)
            // false = Just reboot (production)
);
```

**Why 30 seconds?**

```
Longest Operations in Code:
──────────────────────────────────────────────────────────
Operation                    Time        Headroom
──────────────────────────────────────────────────────────
Display update               20ms        99.93%
Packet decryption (5 PSKs)   21ms        99.93%
File write (JSON)            50ms        99.83%
User thinking time           infinite    (!)
──────────────────────────────────────────────────────────

30 seconds allows:
  ✅ User can read menu slowly
  ✅ Serial commands can be typed
  ✅ All legitimate operations complete
  ❌ Infinite loops caught quickly
```

**Alternative Timeouts:**

```cpp
// Too short (5 seconds)
esp_task_wdt_init(5, true);
// Problem: User reading menu → TIMEOUT → Reboot (annoying!)

// Too long (120 seconds)
esp_task_wdt_init(120, true);
// Problem: Infinite loop → Wait 2 minutes to recover (frustrating!)

// Just right (30 seconds)
esp_task_wdt_init(30, true);
// Goldilocks zone: Patient with user, quick to recover from bugs
```

### Resetting the Watchdog

**Main Loop:**

```cpp
void LoRaReconTool::update() {
    // FIRST thing in update loop
    esp_task_wdt_reset();  // "I'm still alive!"
    
    // ... rest of update logic ...
}
```

**Call Frequency:**

```
Loop rate: ~100 Hz (every 10ms)
Reset rate: ~100 Hz (every loop)
Timeout: 30 seconds (30,000ms)

Safety margin: 30,000ms / 10ms = 3,000×
  → Would need to miss 3,000 consecutive resets to timeout
  → Essentially impossible without infinite loop
```

**Why Reset at Top of Loop?**

```cpp
// ✅ GOOD: Reset at start
void update() {
    esp_task_wdt_reset();  // Pet immediately
    
    // If any code below hangs, watchdog catches it:
    potentiallyBuggCodeGoesHere();
}

// ❌ BAD: Reset at end
void update() {
    potentiallyBuggyCodeGoesHere();
    
    // If code above hangs, never reach reset!
    esp_task_wdt_reset();  // Too late!
}
```

### What Watchdog Can't Catch

**1. Frequent Brief Hangs**
```cpp
void update() {
    esp_task_wdt_reset();
    
    for (int i = 0; i < 100; i++) {
        delay(20);  // 20ms × 100 = 2 seconds total
    }
    // Returns before 30s timeout, but UI is frozen
}
// Watchdog doesn't trigger (resets every loop)
// But system feels unresponsive
```

**2. Memory Corruption**
```cpp
uint8_t buffer[10];
buffer[1000] = 0xFF;  // Oops! Out of bounds
// May corrupt watchdog timer state
// May corrupt stack → watchdog reset function crashes
// Watchdog can't save you from memory corruption!
```

**3. Hardware Failures**
```cpp
// Radio chip physically damaged
radio.begin();  // Hangs in hardware wait loop
// But maybe returns after 5 seconds with error
// Watchdog doesn't trigger (under 30s)
```

### Watchdog Timeout Sequence

**What Happens When Timeout Occurs:**

```
Step 1: Timeout detected (30s elapsed since last reset)
        └─ Hardware interrupt fires
        
Step 2: ESP32 IDF watchdog handler executes
        └─ Print "Task watchdog timeout" to serial
        └─ Print current function call stack
        └─ Show CPU registers
        
Step 3: Panic handler (because panic=true)
        └─ Dump system state
        └─ Print reset reason to flash
        
Step 4: Hardware reset
        └─ CPU reboots
        └─ setup() runs again
        └─ All RAM lost (start fresh)
        
Step 5: Check reset reason (optional)
        └─ esp_reset_reason()
        └─ Can detect "watchdog reset" vs "power on"
```

**Example Serial Output:**

```
Task watchdog got triggered. The following tasks did not reset the watchdog in time:
 - loopTask (CPU 1)
Tasks currently running:
CPU 0: IDLE
CPU 1: loopTask
Print CPU 1 backtrace
0x4008e7b4:0x3ffb0d10 |<-CORRUPTED
...
Rebooting...
```

**Post-Reboot Detection:**

```cpp
void setup() {
    esp_reset_reason_t reason = esp_reset_reason();
    
    switch (reason) {
        case ESP_RST_POWERON:
            Serial.println("Reset: Power on");
            break;
        case ESP_RST_TASK_WDT:
            Serial.println("Reset: Watchdog timeout");
            Serial.println("⚠️ System recovered from hang");
            break;
        case ESP_RST_PANIC:
            Serial.println("Reset: Panic/exception");
            break;
        // ... more reset reasons
    }
}
```

---

## Error Handler Architecture

### Structured Error Logging

**Error Information:**

```cpp
struct ErrorInfo {
    ErrorCategory category;    // Where did it happen?
    ErrorSeverity severity;    // How bad is it?
    uint16_t code;            // Specific error number
    const char* message;      // Human-readable description
    const char* function;     // Which function?
    uint16_t line;           // Which line?
    uint32_t timestamp;      // When? (millis)
    bool recovered;          // Did recovery work?
};
```

**Example:**

```cpp
ErrorInfo {
    category: ERR_RADIO,
    severity: SEVERITY_ERROR,
    code: 101,
    message: "Radio configuration failed",
    function: "applyConfig",
    line: 227,
    timestamp: 45230,
    recovered: true
}
```

### Severity Levels

```cpp
enum ErrorSeverity {
    SEVERITY_INFO,      // 0: FYI, no action needed
    SEVERITY_WARNING,   // 1: Attention, but ok
    SEVERITY_ERROR,     // 2: Failed, try recovery
    SEVERITY_CRITICAL,  // 3: System unstable
    SEVERITY_FATAL      // 4: Must halt system
};
```

**Decision Tree:**

```
┌───────────────────────────────────────────────────────┐
│              SEVERITY DECISION TREE                   │
├───────────────────────────────────────────────────────┤
│                                                       │
│  Can system continue normally?                        │
│    ├─ Yes → INFO or WARNING                          │
│    └─ No ↓                                            │
│                                                       │
│  Is core functionality broken?                        │
│    ├─ No → ERROR (try recovery)                      │
│    └─ Yes ↓                                           │
│                                                       │
│  Is data corrupted or safety at risk?                │
│    ├─ No → CRITICAL (system degraded)                │
│    └─ Yes → FATAL (halt now!)                        │
│                                                       │
└───────────────────────────────────────────────────────┘
```

**Examples:**

```cpp
// INFO: "Radio received packet" (normal event)
SEVERITY_INFO

// WARNING: "Display initialization failed - continuing without display"
SEVERITY_WARNING  // System works, just no screen

// ERROR: "Radio configuration failed"
SEVERITY_ERROR  // Try recovery, might work

// CRITICAL: "Heap memory critically low (5KB free)"
SEVERITY_CRITICAL  // System unstable, likely to crash soon

// FATAL: "Stack overflow detected, memory corrupted"
SEVERITY_FATAL  // Must stop before doing damage
```

### Error Categories

```cpp
enum ErrorCategory {
    ERR_RADIO,          // Radio hardware/SPI issues
    ERR_MEMORY,         // Heap/stack issues
    ERR_STATE,          // Invalid state transitions
    ERR_CONFIG,         // Bad configuration parameters
    ERR_INTERRUPT,      // ISR problems
    ERR_STORAGE,        // File system errors
    ERR_PROTOCOL,       // Packet parsing failures
    ERR_USER_TIMEOUT,   // User interaction timeouts
    ERR_SYSTEM          // System-level errors
};
```

**Category-Specific Handling:**

```cpp
switch (error.category) {
    case ERR_RADIO:
        // Try radio reset, re-init, config retry
        return recoverRadioError(error);
        
    case ERR_MEMORY:
        // Check heap, print diagnostics, maybe clear cache
        return recoverMemoryError(error);
        
    case ERR_STATE:
        // Reset state machine, return to reconnaissance
        return recoverStateError(error);
        
    // ... more categories
}
```

### Helper Macros

**With File/Line Info:**

```cpp
// Old way (tedious)
ErrorHandler::logError(
    ERR_RADIO, 
    SEVERITY_ERROR, 
    101, 
    "Radio init failed",
    "LoRaReconTool::initialize",  // Manually typed
    65                              // Manually typed
);

// New way (automatic)
LOG_RADIO_ERROR(101, "Radio init failed");
// __FUNCTION__ and __LINE__ inserted by preprocessor
```

**Macro Definition:**

```cpp
#define LOG_RADIO_ERROR(code, msg) \
    ErrorHandler::radioError(code, msg, __FUNCTION__, __LINE__)
```

**Preprocessor Magic:**

```
Source code:
  LOG_RADIO_ERROR(101, "Radio init failed");

After preprocessing:
  ErrorHandler::radioError(
      101, 
      "Radio init failed", 
      "LoRaReconTool::initialize",  ← __FUNCTION__
      65                              ← __LINE__
  );
```

### Error History Buffer

**Circular Buffer:**

```cpp
#define MAX_ERROR_HISTORY 20

ErrorInfo errorHistory[MAX_ERROR_HISTORY];  // Fixed-size array
uint8_t errorIndex = 0;                     // Next write position
uint8_t errorCount = 0;                     // Total errors (capped at 20)
```

**How It Works:**

```
Initial State:
errorHistory = [empty, empty, empty, ..., empty]
errorIndex = 0
errorCount = 0

After 5 errors:
errorHistory = [err0, err1, err2, err3, err4, empty, ...]
errorIndex = 5
errorCount = 5

After 20 errors:
errorHistory = [err0, err1, ..., err19]
errorIndex = 0  ← Wraps around (20 % 20)
errorCount = 20

After 25 errors:
errorHistory = [err20, err21, err22, err23, err24, err5, ..., err19]
                ↑ Oldest                           ↑ Newest
errorIndex = 5
errorCount = 20 (max)
```

**Why Circular Buffer?**

```cpp
// Pros:
✅ Fixed memory (no malloc)
✅ O(1) write (no shifting)
✅ Keeps most recent errors
✅ Never runs out of space

// Cons:
❌ Loses old errors (after 20)
❌ Slightly complex indexing
```

**Reading History:**

```cpp
void printErrorHistory() {
    uint8_t start = (errorCount < MAX_ERROR_HISTORY) ? 0 : errorIndex;
    uint8_t count = min(errorCount, MAX_ERROR_HISTORY);
    
    for (uint8_t i = 0; i < count; i++) {
        uint8_t idx = (start + i) % MAX_ERROR_HISTORY;
        const ErrorInfo& err = errorHistory[idx];
        
        // Print error...
    }
}
```

**Example: Reading after 25 errors**

```
start = errorIndex = 5
count = 20

Iteration 0: idx = (5 + 0) % 20 = 5  → err5 (oldest)
Iteration 1: idx = (5 + 1) % 20 = 6  → err6
...
Iteration 14: idx = (5 + 14) % 20 = 19 → err19
Iteration 15: idx = (5 + 15) % 20 = 0  → err20 ← Wrapped!
Iteration 16: idx = (5 + 16) % 20 = 1  → err21
...
Iteration 19: idx = (5 + 19) % 20 = 4  → err24 (newest)
```

---

## Recovery Strategies

### Radio Recovery

**Problem:** Radio becomes unresponsive or misconfigured

**Recovery Steps:**

```cpp
bool ErrorHandler::recoverRadioError(const ErrorInfo& error) {
    Serial.println("[ERROR_HANDLER] Attempting radio recovery...");
    
    // Step 1: Get radio reference
    if (!g_reconTool) return false;
    SX1262& radio = g_reconTool->getRadio();
    
    // Step 2: Brief pause (let hardware settle)
    delay(100);
    
    // Step 3: Hardware reset
    int state = radio.reset();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.println("[ERROR_HANDLER] Radio reset failed");
        return false;
    }
    
    // Step 4: Another pause
    delay(100);
    
    // Step 5: Re-initialize radio
    state = radio.begin();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.println("[ERROR_HANDLER] Radio re-init failed");
        return false;
    }
    
    // Step 6: Restore current configuration
    g_reconTool->applyConfigPublic(reconState.scanState.currentConfig);
    g_reconTool->startReceiving();
    
    Serial.println("[ERROR_HANDLER] Radio recovery successful");
    return true;
}
```

**Why This Works:**

```
Problem: Radio in unknown state
  ↓
reset() → Pulse RST pin LOW → HIGH
  ↓
Radio hardware resets (like power cycle)
  ↓
begin() → Initialize SPI, load defaults
  ↓
applyConfig() → Restore frequency, SF, BW, etc.
  ↓
startReceive() → Back to RX mode
  ↓
Success: Radio in known-good state
```

**When Does This Fail?**

```
1. Hardware problem (chip damaged)
   → reset() returns error
   → Recovery impossible
   
2. SPI bus problem (wiring issue)
   → begin() returns error
   → Recovery impossible
   
3. Config problem (invalid frequency)
   → applyConfig() fails
   → But might work with different config
```

### Memory Recovery

**Problem:** Low heap memory or potential corruption

**Recovery Steps:**

```cpp
bool ErrorHandler::recoverMemoryError(const ErrorInfo& error) {
    Serial.println("[ERROR_HANDLER] Attempting memory recovery...");
    
    // Step 1: Check heap status
    size_t freeHeap = ESP.getFreeHeap();
    size_t minFreeHeap = ESP.getMinFreeHeap();
    
    Serial.printf("[ERROR_HANDLER] Free heap: %d bytes, Min: %d bytes\n", 
                  freeHeap, minFreeHeap);
    
    // Step 2: Check if critically low
    if (freeHeap < 10000) {
        Serial.println("[ERROR_HANDLER] ⚠️ Low memory condition detected");
        
        // Could trigger cleanup here:
        // - Clear old nodes
        // - Clear replay slots
        // - Clear error history
        // BUT: We use fixed buffers, so not much to free!
        
        return false;  // Can't recover from low heap
    }
    
    return true;  // Memory adequate
}
```

**Why Memory Recovery Is Limited:**

```
Our Architecture:
  ✅ Fixed-size buffers (stack/BSS allocated)
  ✅ No malloc/free (no heap fragmentation)
  ✅ Predictable memory usage
  
  → Not much to "free" during recovery!
  
If We Used Dynamic Allocation:
  ❌ String objects on heap
  ❌ std::vector growable arrays
  ❌ Object pools
  
  → Could free unused objects during recovery
  → But we don't, by design (embedded best practice)
```

**Memory Monitoring:**

```cpp
void printMemoryStatus() {
    size_t total = ESP.getHeapSize();
    size_t free = ESP.getFreeHeap();
    size_t minFree = ESP.getMinFreeHeap();
    
    Serial.printf("Heap: %d / %d bytes free (%d%%)\n",
                  free, total, (free * 100) / total);
    Serial.printf("Min free: %d bytes (worst case)\n", minFree);
    
    if (minFree < 50000) {
        Serial.println("⚠️ WARNING: Heap fragmentation or leak!");
    }
}
```

### State Recovery

**Problem:** Invalid state, stuck mode, bad config index

**Recovery Steps:**

```cpp
bool ErrorHandler::recoverStateError(const ErrorInfo& error) {
    Serial.println("[ERROR_HANDLER] Attempting state recovery...");
    
    // State errors are usually non-critical
    // Just reset to known-good state
    
    reconState.reset();  // Clear everything
    reconState.scanState.mode = MODE_RECONNAISSANCE;
    reconState.scanState.currentConfig = 0;
    
    Serial.println("[ERROR_HANDLER] State reset to reconnaissance mode");
    
    return true;
}
```

**What Gets Reset:**

```cpp
void ReconState::reset() {
    // Counters
    numTargetableDevices = 0;
    nodeCount = 0;
    numCapturedPackets = 0;
    
    // Arrays
    memset(rfActivity, 0, sizeof(rfActivity));
    memset(targetableDevices, 0, sizeof(targetableDevices));
    memset(trackedNodes, 0, sizeof(trackedNodes));
    memset(replaySlots, 0, sizeof(replaySlots));
    
    // Scan state
    scanState.mode = MODE_RECONNAISSANCE;
    scanState.currentConfig = 0;
    scanState.lastScanSwitch = millis();
    
    // Everything back to initial state!
}
```

**Use Cases:**

```
1. User reports "stuck in weird mode"
   → Press 'X' → Triggers state reset → Back to recon

2. Config index somehow becomes 255
   → Validation catches it → State reset → Back to config 0

3. Display shows wrong information
   → State mismatch → Reset → Display refreshes correctly
```

---

## Defensive Programming

### Validation at Boundaries

**Example: Config Index Validation**

```cpp
bool ReconState::isValidConfigIndex(uint8_t index) const {
    return index < NUM_CONFIGS;
}

// Use it everywhere:
const ScanConfig& ReconState::getScanConfig(uint8_t index) const {
    if (!isValidConfigIndex(index)) {
        // Return fallback instead of crashing
        return scanConfigs[0];
    }
    return scanConfigs[index];
}
```

**Without Validation (dangerous):**

```cpp
// ❌ DANGEROUS
const ScanConfig& getScanConfig(uint8_t index) const {
    return scanConfigs[index];  // What if index = 255?
    // scanConfigs[255] = out of bounds access!
    // → Memory corruption or crash
}
```

**With Validation (safe):**

```cpp
// ✅ SAFE
const ScanConfig& getScanConfig(uint8_t index) const {
    if (index >= NUM_CONFIGS) {
        // Log the problem
        Serial.printf("[ERROR] Invalid config index %d (max: %d)\n", 
                      index, NUM_CONFIGS - 1);
        // Return safe fallback
        return scanConfigs[0];
    }
    return scanConfigs[index];
}
```

### Null Pointer Checks

**Example: Global Pointer Guard**

```cpp
// Global pointer (can be NULL during init)
LoRaReconTool* g_reconTool = nullptr;

// Guard every use:
if (g_reconTool) {
    g_reconTool->update();
} else {
    // Don't crash, just skip
    Serial.println("[ERROR] ReconTool not initialized");
}
```

**ISR Example:**

```cpp
void IRAM_ATTR onPacketReceived() {
    if (g_reconTool) {  // ← Critical null check
        g_reconTool->markPacketReceived();
    }
    // If NULL, ISR just returns (harmless)
    // Without check: NULL dereference → crash → watchdog reset
}
```

### Return Value Checking

**Example: Radio Configuration**

```cpp
bool LoRaReconTool::applyConfig(uint8_t configIndex) {
    // Validate input
    if (!reconState.isValidConfigIndex(configIndex)) return false;
    
    const ScanConfig& cfg = reconState.getScanConfig(configIndex);
    
    // Check each radio operation
    int state = radio.setFrequency(cfg.frequency);
    if (state != RADIOLIB_ERR_NONE) {
        LOG_RADIO_ERROR(ErrorCodes::RADIO_CONFIG_FAILED, 
                       "Failed to set frequency");
        return false;  // Don't continue if frequency invalid
    }
    
    state = radio.setBandwidth(cfg.bandwidth);
    if (state != RADIOLIB_ERR_NONE) {
        LOG_RADIO_ERROR(ErrorCodes::RADIO_CONFIG_FAILED, 
                       "Failed to set bandwidth");
        return false;
    }
    
    // ... more checks ...
    
    return true;  // Success only if all checks passed
}
```

**Why This Matters:**

```
Without Checks:
  setFrequency(999.9) → Fails silently
  setBandwidth(500) → Uses old value
  → Radio receives nothing (confused state)
  → User thinks "no packets" (actually misconfigured)

With Checks:
  setFrequency(999.9) → Returns error
  → LOG_RADIO_ERROR() called
  → applyConfig() returns false
  → Caller knows it failed
  → Can retry or skip this config
```

### Buffer Overflow Protection

**Example: Packet Copy with Size Check**

```cpp
bool ReconState::capturePacketForReplay(const uint8_t* data, size_t length, ...) {
    // Check 1: Slot available?
    if (numCapturedPackets >= MAX_REPLAY_SLOTS) {
        Serial.println("[ERROR] Replay slots full");
        return false;
    }
    
    // Check 2: Packet fits in buffer?
    if (length > MAX_PACKET_SIZE) {
        Serial.printf("[ERROR] Packet too large: %d > %d\n", 
                      length, MAX_PACKET_SIZE);
        return false;
    }
    
    // Safe to copy now
    CapturedPacket& slot = replaySlots[numCapturedPackets];
    memcpy(slot.data, data, length);  // Guaranteed safe
    slot.length = length;
    slot.valid = true;
    
    numCapturedPackets++;
    return true;
}
```

**What Could Go Wrong:**

```cpp
// ❌ UNSAFE VERSION
void capturePacket(const uint8_t* data, size_t length) {
    CapturedPacket& slot = replaySlots[numCapturedPackets];
    memcpy(slot.data, data, length);  // What if length = 1000?
    // Buffer is only 256 bytes!
    // → Overflow into next slot
    // → Corruption
    // → Crash
}
```

### Const Correctness

**Example: Read-Only State Access**

```cpp
// ✅ GOOD: Can't accidentally modify
const ScanConfig& ReconState::getScanConfig(uint8_t index) const {
    return scanConfigs[index];
}

// Usage:
const ScanConfig& cfg = reconState.getScanConfig(0);
cfg.frequency = 999.9;  // ← Compiler error! (const)

// ❌ BAD: Allows accidental modification
ScanConfig& getScanConfig(uint8_t index) {
    return scanConfigs[index];
}

// Usage:
ScanConfig& cfg = reconState.getScanConfig(0);
cfg.frequency = 999.9;  // ← Compiles! (Bug introduced)
```

**Why This Matters:**

```
Without const:
  1. Get config for display
  2. Accidentally typo: cfg.frequency += 10; (meant =)
  3. Radio now receives wrong frequency
  4. Spend hours debugging "why no packets?"

With const:
  1. Get config for display
  2. Try to modify: cfg.frequency += 10;
  3. Compiler error: "cannot modify const reference"
  4. Fix typo immediately
```

---

## Failure Modes

### Radio Failures

**Mode 1: Initialization Failure**

```cpp
int state = radio.begin();
if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("FAILED (%d)\n", state);
    LOG_RADIO_ERROR(ErrorCodes::RADIO_INIT_FAILED, 
                   "SX1262 initialization failed");
    // System continues without radio (log-only mode)
}
```

**Possible Causes:**
- SPI wiring issue (wrong pins)
- Power supply problem (voltage too low)
- Hardware damage (chip fried)
- Wrong RadioLib version (API mismatch)

**Recovery:** Try reset + re-init once, then give up

---

**Mode 2: Configuration Failure**

```cpp
state = radio.setFrequency(cfg.frequency);
if (state != RADIOLIB_ERR_NONE) {
    LOG_RADIO_ERROR(ErrorCodes::RADIO_CONFIG_FAILED, 
                   "Frequency out of range");
    return false;  // Skip this config
}
```

**Possible Causes:**
- Invalid frequency (outside 902-928 MHz)
- Invalid bandwidth (not 125/250/500 kHz)
- Invalid spreading factor (not 7-12)

**Recovery:** Skip bad config, try next one

---

**Mode 3: Reception Timeout**

```cpp
void handlePacketReception() {
    if (!packetReceived.load(std::memory_order_acquire)) {
        return;  // No packet, nothing to do
    }
    
    // Read packet...
}
// Note: Timeout is NOT an error!
// Just means no packets transmitted
```

**Not Actually an Error:** Normal for quiet frequency

---

### Memory Failures

**Mode 1: Heap Exhaustion**

```cpp
// Not likely with our fixed buffers, but could happen if:
String bigString = "";
while (true) {
    bigString += "data";  // Eventually runs out of heap
}
// Detection:
if (ESP.getFreeHeap() < 10000) {
    LOG_MEMORY_ERROR(ErrorCodes::HEAP_FRAGMENTATION, 
                    "Critically low memory");
}
```

**Recovery:** Clear caches, reset state

---

**Mode 2: Stack Overflow**

```cpp
void recursiveFunction(int depth) {
    uint8_t buffer[1024];  // 1KB on stack
    recursiveFunction(depth + 1);  // Recurse
}
// Eventually: Stack overflows into heap
// → Memory corruption
// → Crash
```

**Detection:** Stack canary (enabled by default on ESP32)

**Recovery:** Watchdog reboot only option

---

**Mode 3: Buffer Overflow**

```cpp
uint8_t buffer[256];
size_t length = 1000;  // Oops!
memcpy(buffer, data, length);  // Overflow!
// → Corruption
// → Unpredictable behavior
```

**Prevention:** Size checks (see defensive programming)

---

### State Machine Failures

**Mode 1: Invalid State Transition**

```cpp
// Current mode: MODE_TARGETED_CAPTURE
// User presses '1' (select device #1)
// → Would transition to... MODE_TARGETED_CAPTURE again?

// Guard condition:
if (reconState.scanState.mode == MODE_TARGETED_CAPTURE) {
    Serial.println("Already in targeted capture mode");
    return;  // Ignore command
}
```

**Recovery:** Ignore invalid command

---

**Mode 2: Config Index Out of Range**

```cpp
reconState.scanState.currentConfig = 255;  // Bug!
// Later:
applyConfig(reconState.scanState.currentConfig);
// → Tries to access scanConfigs[255]
// → Out of bounds!

// Protection:
if (!reconState.isValidConfigIndex(index)) {
    LOG_STATE_ERROR(ErrorCodes::CONFIG_OUT_OF_RANGE, 
                   "Invalid config index");
    reconState.scanState.currentConfig = 0;  // Reset to safe value
}
```

**Recovery:** Reset to config 0

---

### Storage Failures

**Mode 1: LittleFS Mount Failed**

```cpp
if (!LittleFS.begin()) {
    Serial.println("[FS] Storage init failed");
    // Continue without logging to file
    // Still functional (just no persistence)
}
```

**Possible Causes:**
- Flash partition not formatted
- Corruption from power loss
- Flash wear (end of life)

**Recovery:** Format flash, or continue without storage

---

**Mode 2: File Write Failed**

```cpp
File logFile = LittleFS.open("/packets.jsonl", "a");
if (!logFile) {
    LOG_STORAGE_ERROR(ErrorCodes::STORAGE_WRITE_FAILED, 
                     "Cannot open log file");
    return;  // Skip logging this packet
}
```

**Possible Causes:**
- Filesystem full (no space)
- File locked (shouldn't happen)
- Corruption

**Recovery:** Skip logging, continue operation

---

## Debugging

### Error History

**View Last 20 Errors:**

```cpp
// User command: 'E' or via menu
ErrorHandler::printErrorHistory();
```

**Output:**

```
╔══════════════════════════════════════════════════════════════╗
║              ERROR HISTORY (Last 20 Errors)                  ║
╠══════════════════════════════════════════════════════════════╣
║ [  1] RADIO/ERROR (Code: 101)
║       Radio configuration failed
║       at applyConfig:227
║       Time: +45230 ms, Recovered: YES
║
║ [  2] STATE/WARNING (Code: 302)
║       Invalid config index
║       at getScanConfig:85
║       Time: +67891 ms, Recovered: YES
║
║ [  3] MEMORY/CRITICAL (Code: 203)
║       Heap critically low
║       at updateNode:156
║       Time: +120345 ms, Recovered: NO
║
╚══════════════════════════════════════════════════════════════╝
```

### Health Report

**System Status:**

```cpp
ErrorHandler::printHealthReport();
```

**Output:**

```
╔══════════════════════════════════════════════════════════════╗
║                    SYSTEM HEALTH REPORT                      ║
╠══════════════════════════════════════════════════════════════╣
║ Overall Status: ✅ HEALTHY                                   ║
║                                                              ║
║ Error Counts by Category:                                    ║
║   Radio     :   3 errors                                     ║
║   State     :   1 errors                                     ║
║                                                              ║
║ Memory Status:                                               ║
║   Free Heap:  285432 / 327680 bytes ( 87%)                  ║
║   Min Free:   278901 bytes                                   ║
║                                                              ║
║ Uptime: 3845 seconds (64 min)                                ║
╚══════════════════════════════════════════════════════════════╝
```

### Memory Diagnostics

**ESP32 Built-in Functions:**

```cpp
void printMemoryDiagnostics() {
    Serial.println("\n=== MEMORY DIAGNOSTICS ===");
    
    // Heap
    Serial.printf("Heap Size:      %d bytes\n", ESP.getHeapSize());
    Serial.printf("Free Heap:      %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Min Free Heap:  %d bytes\n", ESP.getMinFreeHeap());
    Serial.printf("Max Alloc:      %d bytes\n", ESP.getMaxAllocHeap());
    
    // PSRAM (if available)
    if (ESP.getPsramSize() > 0) {
        Serial.printf("PSRAM Size:     %d bytes\n", ESP.getPsramSize());
        Serial.printf("Free PSRAM:     %d bytes\n", ESP.getFreePsram());
    }
    
    // Flash
    Serial.printf("Flash Size:     %d bytes\n", ESP.getFlashChipSize());
    Serial.printf("Flash Speed:    %d MHz\n", ESP.getFlashChipSpeed() / 1000000);
    
    // CPU
    Serial.printf("CPU Frequency:  %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("Chip Model:     %s\n", ESP.getChipModel());
    Serial.printf("Chip Revision:  %d\n", ESP.getChipRevision());
}
```

---

## Summary

### Error Handling Strengths

**✅ What Works Well:**

1. **Watchdog Timer**: Catches infinite loops, automatic reboot
2. **Structured Logging**: Know exactly what failed and where
3. **Automatic Recovery**: Radio errors often self-heal
4. **Graceful Degradation**: System continues even with failures
5. **Defensive Coding**: Validation prevents most errors
6. **Diagnostic Tools**: Easy to debug with error history

**⚠️ Limitations:**

1. **Memory Corruption**: Watchdog can't save you
2. **Hardware Failures**: Can't fix physical damage
3. **No Persistence**: Error history lost on reboot
4. **Limited Recovery**: Some errors require manual intervention

### Key Takeaways

**Watchdog Timer:**
- ✅ 30-second timeout (patient with user, quick on bugs)
- ✅ Reset at start of loop (catch hangs)
- ✅ Hardware reset on timeout (automatic recovery)
- ⚠️ Can't catch brief hangs or memory corruption

**Error Handler:**
- ✅ Structured logging (category, severity, code, location)
- ✅ Circular buffer (last 20 errors, fixed memory)
- ✅ Automatic recovery attempts
- ✅ Health monitoring (memory, uptime, error rates)

**Defensive Programming:**
- ✅ Validate inputs (config indices, buffer sizes)
- ✅ Check return values (radio operations)
- ✅ Null pointer guards (ISR, global pointers)
- ✅ Const correctness (prevent accidental modification)
- ✅ Fixed buffers (no heap fragmentation)

**Recovery Strategies:**
- ✅ Radio: Reset + re-init
- ✅ Memory: Monitor + clear caches
- ✅ State: Reset to reconnaissance mode
- ⚠️ Hardware: Reboot only option

---

## Code Locations Reference

**Primary files:**
- `error_handler.cpp` (lines 1-420): Error logging and recovery
- `error_handler.h` (lines 1-120): Error structures and API
- `lora_recon_tool.cpp` (lines 61-75): Watchdog initialization
- `lora_recon_tool.cpp` (line 174): Watchdog reset in main loop

**Key functions:**
- `ErrorHandler::logError()`: Main logging interface (line 27)
- `ErrorHandler::attemptRecovery()`: Recovery dispatcher (line 76)
- `ErrorHandler::recoverRadioError()`: Radio reset logic (line 91)
- `ErrorHandler::printHealthReport()`: System diagnostics (line 255)
- `esp_task_wdt_reset()`: Pet the watchdog (line 174)

**Error codes:**
- `ErrorCodes` namespace: `error_handler.h` (lines 95-120)

---

## Next Steps

You've now completed the core system understanding:
- ✅ Packet Reception
- ✅ AES-CTR Decryption
- ✅ Protocol Analysis
- ✅ State Machine
- ✅ Error Handling

**Remaining topics:**

1. **Display System**: OLED rendering, UI optimization
2. **Session Key Harvesting**: Active mesh participation
3. **Hardware Stress Testing**: Performance limits
4. **Testing Strategy**: Validation and debugging

**Pick your next deep dive, or you're ready to present this at a conference!** 🎉

---

*Generated for ESP32-Sniffer deep-dive session*  
*Last updated: 2025-10-16*
