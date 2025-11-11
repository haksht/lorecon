# Packet Reception Issue - Session Handoff

## Problem Statement
Offensive testing baseline collection cannot see packets from target device even when user confirms packets are being sent at 1 message/second.

## Root Cause Analysis

### Architecture Understanding
```
Arduino loop() → reconTool.update() → handlePacketReception() → processQueuedPackets() → trackTargetableDevice()
                                    ↓
                                handleCommand() → executeAttack() → collectTargetBaseline() [BLOCKS FOR 30s]
```

**THE ISSUE:** When `collectTargetBaseline()` runs:
- It's called from within `update()` via command handler
- It enters a 30-second blocking while loop
- Arduino's `loop()` never gets called again during this time
- Therefore `update()` is never called
- Therefore `handlePacketReception()` is never called
- Therefore `trackTargetableDevice()` never increments packet counts
- **Result: Baseline sees zero packets even though they're arriving**

### Evidence from Last Test
```
Starting packet count: 3        ← Device exists and was tracked before
Packets from target: 0          ← No new packets in 30s (expected ~30)
User confirmed: Sending 1/sec   ← Packets definitely transmitting
```

### What We've Tried (All Failed)
1. ❌ Direct polling with `radio->getPacketLength()` - Interrupt handler interferes
2. ❌ Reading from packet queue directly - Main loop drains it before we see it
3. ❌ Pausing queue processing - Doesn't help because main loop isn't running anyway
4. ❌ Using reconState packet counts - Not updated during blocking call

## Solution Approaches

### Option 1: Non-Blocking Baseline (Recommended)
Don't block for 30 seconds. Instead:
```cpp
// In DeviceStressTester class:
uint32_t baselineStartTime;
uint32_t baselineStartCount;
bool baselineInProgress;

void startBaseline() {
    baselineInProgress = true;
    baselineStartTime = millis();
    baselineStartCount = getDevicePacketCount();
    // Return immediately - don't block
}

bool isBaselineComplete() {
    return baselineInProgress && (millis() - baselineStartTime >= 30000);
}

uint32_t getBaselineResult() {
    baselineInProgress = false;
    return getDevicePacketCount() - baselineStartCount;
}
```

Then modify attack flow:
```cpp
bool DeviceStressTester::executeAttack(const AttackConfig& config) {
    // Start baseline
    startBaseline();
    
    // Return to main loop - let it process packets
    // Store attack state in class members
    attackState = ATTACK_STATE_BASELINE;
    return true; // Attack continues asynchronously
}

// Add to update() method called from main loop:
void DeviceStressTester::update() {
    switch (attackState) {
        case ATTACK_STATE_BASELINE:
            if (isBaselineComplete()) {
                uint32_t baseline = getBaselineResult();
                attackState = ATTACK_STATE_FLOODING;
                startFlood();
            }
            break;
        case ATTACK_STATE_FLOODING:
            continueFlood();
            break;
    }
}
```

### Option 2: Manual Packet Processing
Call update() manually during baseline:
```cpp
uint32_t DeviceStressTester::collectTargetBaseline(uint32_t durationMs) {
    unsigned long startTime = millis();
    uint32_t startCount = getDevicePacketCount();
    
    while (millis() - startTime < durationMs) {
        esp_task_wdt_reset();
        
        // CRITICAL: Process packets manually
        if (reconTool) {
            reconTool->update();  // This processes the queue
        }
        
        delay(100);
    }
    
    return getDevicePacketCount() - startCount;
}
```

**Problem:** This creates nested `update()` calls which could cause issues.

### Option 3: Separate Processing Task (Advanced)
Create FreeRTOS task for packet processing that runs independently.

**Problem:** Major architectural change, adds complexity.

## Recommended Implementation

### Step 1: Add Helper Methods
```cpp
// In device_stress_tester.h
private:
    enum AttackState {
        ATTACK_STATE_IDLE,
        ATTACK_STATE_BASELINE,
        ATTACK_STATE_FLOODING,
        ATTACK_STATE_COMPLETE
    };
    
    AttackState attackState;
    uint32_t baselineStartTime;
    uint32_t baselineStartCount;
    AttackConfig currentConfig;

// In device_stress_tester.cpp
int DeviceStressTester::findDeviceIndex(uint32_t nodeId) {
    for (uint8_t i = 0; i < reconState.numTargetableDevices; i++) {
        if (reconState.getTargetableDevice(i).nodeId == nodeId) {
            return i;
        }
    }
    return -1;
}

uint32_t DeviceStressTester::getDevicePacketCount(uint32_t nodeId) {
    int idx = findDeviceIndex(nodeId);
    return (idx >= 0) ? reconState.getTargetableDevice(idx).packetCount : 0;
}
```

### Step 2: Make Attack Non-Blocking
```cpp
bool DeviceStressTester::executeAttack(const AttackConfig& config) {
    currentConfig = config;
    attackState = ATTACK_STATE_BASELINE;
    baselineStartTime = millis();
    baselineStartCount = getDevicePacketCount(config.targetNodeId);
    
    Serial.println("📊 Starting baseline collection (non-blocking)...");
    
    // DON'T BLOCK - return and let main loop continue
    return true;
}
```

### Step 3: Add Update Method
```cpp
// Called from main loop
void DeviceStressTester::update() {
    switch (attackState) {
        case ATTACK_STATE_BASELINE:
            if (millis() - baselineStartTime >= 30000) {
                uint32_t baseline = getDevicePacketCount(currentConfig.targetNodeId) - baselineStartCount;
                Serial.printf("✅ Baseline: %u packets\n", baseline);
                
                // Start flooding phase
                attackState = ATTACK_STATE_FLOODING;
                floodStartTime = millis();
            }
            break;
            
        case ATTACK_STATE_FLOODING:
            // Execute flood in chunks, return to loop between chunks
            if (millis() - floodStartTime >= currentConfig.duration) {
                attackState = ATTACK_STATE_COMPLETE;
                displayResults();
            } else {
                sendFloodPackets(10); // Send 10 packets then return
            }
            break;
    }
}
```

### Step 4: Integrate with Main Loop
```cpp
// In lora_recon_tool.cpp update():
#ifdef ENABLE_OFFENSIVE_TESTING
    if (deviceStressTester) {
        deviceStressTester->update();
    }
#endif
```

## Testing Plan
1. Start packet flood attack on active target
2. Send test messages at 1/sec during baseline
3. Verify baseline shows ~30 packets received
4. Verify attack phases execute correctly
5. Check that normal recon mode still works

## Files to Modify
- `firmware/src/device_stress_tester.h` - Add state machine, update method
- `firmware/src/device_stress_tester.cpp` - Implement non-blocking flow
- `firmware/src/lora_recon_tool.cpp` - Call `deviceStressTester->update()`
- `firmware/src/lora_recon_tool.h` - Expose update call

## Key Principles
1. **Never block in Arduino** - Always return to `loop()` quickly
2. **Use state machines** for multi-phase operations
3. **Let interrupts work** - Don't fight the architecture
4. **Leverage existing tracking** - reconState already counts packets perfectly

## Success Criteria
- Baseline collection sees packets in real-time
- No polling loops longer than ~100ms
- Main loop continues to run at all times
- Existing recon functionality unaffected

---
*Generated: November 11, 2025*
*Issue: Blocking baseline prevents packet processing*
*Solution: Non-blocking state machine approach*
