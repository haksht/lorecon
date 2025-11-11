# Offensive Testing Implementation Plan

## Current Status
The offensive testing framework in `device_stress_tester.cpp` is **architecturally complete but functionally incomplete**. The UI integration is done (accessible via 'A' command), but the attacks don't actually work because response monitoring is not implemented.

## Problem Summary
All attack methods transmit packets but never monitor if the target responds. This causes:
- **False positives**: Reports "target crashed" when it hasn't
- **False negatives**: Misses actual vulnerabilities
- **No validation**: Can't tell if attacks succeeded or failed

### Root Cause
Two critical functions are stubs:
```cpp
// Line 716-719 in device_stress_tester.cpp
bool DeviceStressTester::isTargetResponding() {
    // Simplified - in real implementation, check packet buffer
    // for recent packets from target node
    return target.responding;  // ❌ Never updated!
}

// Line 721-724
float DeviceStressTester::measureResponseTime() {
    // Simplified - needs actual response detection
    return random(100, 500);  // ❌ Fake data!
}
```

The `target.responding` flag is initialized but never updated during attacks.

## Implementation Plan

### **PHASE 1: Response Monitoring (CRITICAL) - 4-6 hours**
This is the minimum work needed to make attacks functional.

#### Task 1.1: Implement Real-time Target Monitoring (2 hours)
**File**: `firmware/src/device_stress_tester.cpp`

**Current state**:
```cpp
bool DeviceStressTester::isTargetResponding() {
    return target.responding;  // Never changes!
}
```

**Required implementation**:
```cpp
bool DeviceStressTester::isTargetResponding() {
    // Check if we've seen packets from target recently
    unsigned long timeSinceLastSeen = millis() - target.lastSeen;
    
    // If more than 5 seconds since last packet, consider unresponsive
    if (timeSinceLastSeen > 5000) {
        target.responding = false;
        return false;
    }
    
    target.responding = true;
    return true;
}
```

**Challenge**: Need to capture packets FROM target while transmitting TO target.

#### Task 1.2: Add Concurrent RX During Attacks (2-3 hours)
**File**: `firmware/src/device_stress_tester.cpp`

RadioLib (the SX1262 library) needs to alternate between TX and RX modes. Currently attacks only transmit.

**In each attack function**, after transmitting, add RX period:

```cpp
// Example in executePacketFlood() around line 215
while (millis() - startTime < config.duration) {
    // Transmit attack packet
    uint8_t packet[256];
    size_t length = 0;
    buildValidPacket(packet, &length, config.targetNodeId);
    
    int state = radio->transmit(packet, length);
    if (state == RADIOLIB_ERR_NONE) {
        lastResult.packetsTransmitted++;
        
        // ✅ ADD THIS: Switch to RX and listen for responses
        radio->startReceive();
        delay(100);  // Listen for 100ms
        
        if (radio->getPacketLength() > 0) {
            // Packet received - check if from target
            uint8_t rxBuffer[256];
            int rxLen = radio->readData(rxBuffer, 256);
            
            if (rxLen > 0) {
                // Parse packet to check nodeId
                PacketInfo info = protocolAnalyzer.analyze(rxBuffer, rxLen, radio->getRSSI());
                
                if (info.nodeId == config.targetNodeId) {
                    // Target responded!
                    target.lastSeen = millis();
                    target.responding = true;
                    lastResult.responsesSeen++;
                }
            }
        }
        
        // Every 20 packets, check target status
        if (lastResult.packetsTransmitted % 20 == 0) {
            bool responding = isTargetResponding();
            Serial.printf("📊 Sent: %u packets, Target: %s\n",
                         lastResult.packetsTransmitted,
                         responding ? "RESPONDING" : "NOT RESPONDING");
            
            if (!responding && target.responding) {
                lastResult.targetUnresponsive = true;
                lastResult.vulnerabilityFound = true;
                break;
            }
        }
    }
    
    delay(delayBetweenPackets);
}
```

**Challenges**:
- Need access to `ProtocolAnalyzer` instance (requires passing through or storing in class)
- RadioLib state management (TX/RX switching)
- Timing coordination (how long to listen vs transmit)

#### Task 1.3: Add ProtocolAnalyzer to DeviceStressTester (1 hour)
**File**: `firmware/src/device_stress_tester.h`

Add member variable:
```cpp
class DeviceStressTester {
private:
    SX1262* radio;
    ProtocolAnalyzer* analyzer;  // ✅ ADD THIS
    TargetDevice target;
    // ... rest of members
```

**File**: `firmware/src/device_stress_tester.cpp`

Update constructor:
```cpp
DeviceStressTester::DeviceStressTester(SX1262* radioModule, ProtocolAnalyzer* protocolAnalyzer) {
    radio = radioModule;
    analyzer = protocolAnalyzer;  // ✅ ADD THIS
    initialized = false;
    ethicalMode = true;
    // ...
}
```

**File**: `firmware/src/lora_recon_tool.cpp` (line ~605)

Update initialization:
```cpp
void LoRaReconTool::ensureDeviceStressTesterInitialized() {
    if (!deviceStressTester) {
        deviceStressTester = new DeviceStressTester(&radio, &protocolAnalyzer);  // ✅ Pass analyzer
        deviceStressTester->initialize();
    }
}
```

### **PHASE 2: Enhanced Attack Logic (MEDIUM PRIORITY) - 3-4 hours**

#### Task 2.1: Baseline Collection (2 hours)
Before starting attack, monitor target for 30 seconds to establish baseline behavior.

**Add to DeviceStressTester class**:
```cpp
struct TargetBaseline {
    float avgPacketRate;      // Packets per minute
    float avgResponseTime;    // Response latency in ms
    uint32_t sampleDuration;  // How long we monitored
    bool valid;               // Was baseline successfully collected
};

TargetBaseline collectBaseline(uint32_t durationMs);
```

**Implementation**:
```cpp
TargetBaseline DeviceStressTester::collectBaseline(uint32_t durationMs) {
    Serial.println("📊 Collecting baseline behavior...");
    
    TargetBaseline baseline;
    baseline.valid = false;
    
    uint32_t packetCount = 0;
    unsigned long startTime = millis();
    
    // Listen for target's normal traffic
    radio->startReceive();
    
    while (millis() - startTime < durationMs) {
        if (radio->getPacketLength() > 0) {
            uint8_t buffer[256];
            int len = radio->readData(buffer, 256);
            
            if (len > 0) {
                PacketInfo info = analyzer->analyze(buffer, len, radio->getRSSI());
                if (info.nodeId == target.nodeId) {
                    packetCount++;
                    target.lastSeen = millis();
                }
            }
        }
        delay(10);
    }
    
    if (packetCount > 0) {
        baseline.avgPacketRate = (float)packetCount / (durationMs / 60000.0);
        baseline.sampleDuration = durationMs;
        baseline.valid = true;
        
        Serial.printf("✅ Baseline: %.1f packets/min over %us\n", 
                     baseline.avgPacketRate, durationMs/1000);
    } else {
        Serial.println("⚠️  No packets seen during baseline - target may be idle");
    }
    
    return baseline;
}
```

#### Task 2.2: Implement measureResponseTime() (1-2 hours)
Send ping packet and measure round-trip time.

```cpp
float DeviceStressTester::measureResponseTime() {
    // Build and send ping packet
    uint8_t pingPacket[64];
    size_t length = 0;
    buildValidPacket(pingPacket, &length, target.nodeId);
    
    unsigned long sendTime = millis();
    radio->transmit(pingPacket, length);
    
    // Switch to RX and wait for response
    radio->startReceive();
    unsigned long timeout = 5000;  // 5 second timeout
    unsigned long startWait = millis();
    
    while (millis() - startWait < timeout) {
        if (radio->getPacketLength() > 0) {
            uint8_t buffer[256];
            int len = radio->readData(buffer, 256);
            
            if (len > 0) {
                PacketInfo info = analyzer->analyze(buffer, len, radio->getRSSI());
                if (info.nodeId == target.nodeId) {
                    // Got response!
                    unsigned long responseTime = millis() - sendTime;
                    return (float)responseTime;
                }
            }
        }
        delay(10);
    }
    
    // No response
    return -1.0;  // Negative indicates timeout
}
```

### **PHASE 3: Attack Refinements (LOW PRIORITY) - 2-3 hours**

#### Task 3.1: Improve Packet Crafting (2 hours)
Currently `buildValidPacket()` creates dummy packets. Need real Meshtastic protobuf packets.

**Reference**: Check `psk_decryption_simple.cpp` for protobuf structure
**Files to update**: 
- `device_stress_tester.cpp` lines 616-655 (buildValidPacket, buildMalformedPacket, etc)

#### Task 3.2: Attack Result Logging (1 hour)
Save attack results to LittleFS for later analysis.

```cpp
void DeviceStressTester::exportResults(const String& filename) {
    File file = LittleFS.open("/" + filename, "w");
    if (!file) return;
    
    JsonDocument doc;
    doc["attack_type"] = attackTypeToString(lastResult.type);
    doc["target_node"] = target.nodeId;
    doc["packets_sent"] = lastResult.packetsTransmitted;
    doc["responses"] = lastResult.responsesSeen;
    doc["crashed"] = lastResult.targetCrashed;
    doc["unresponsive"] = lastResult.targetUnresponsive;
    doc["vulnerability"] = lastResult.vulnerabilityFound;
    doc["findings"] = lastResult.findings;
    
    serializeJson(doc, file);
    file.close();
}
```

## Testing Strategy

### Test 1: Packet Flood with Response Monitoring
1. Start reconnaissance, find target device
2. Press 'A' for offensive testing
3. Select '1' for Packet Flood
4. Verify:
   - ✅ Attack transmits packets
   - ✅ Response counter increments when target replies
   - ✅ "Target responding" status is accurate
   - ✅ Attack completes without false crash detection

### Test 2: Baseline Collection
1. Before attack, verify baseline collection works
2. Check that normal packet rate is captured
3. Compare behavior during attack vs baseline

### Test 3: Multiple Attack Types
Test all 10 attack types to ensure response monitoring works across all implementations.

## File Modifications Summary

### Files to Modify:
1. **`firmware/src/device_stress_tester.h`**
   - Add `ProtocolAnalyzer*` member
   - Add `TargetBaseline` struct
   - Update constructor signature

2. **`firmware/src/device_stress_tester.cpp`**
   - Implement `isTargetResponding()` (line 716)
   - Implement `measureResponseTime()` (line 721)
   - Add `collectBaseline()` method
   - Update all 10 attack methods to include RX monitoring
   - Update constructor to accept ProtocolAnalyzer

3. **`firmware/src/lora_recon_tool.h`**
   - No changes needed (already has deviceStressTester member)

4. **`firmware/src/lora_recon_tool.cpp`**
   - Update `ensureDeviceStressTesterInitialized()` to pass ProtocolAnalyzer

### Estimated Lines of Code:
- Phase 1: ~200 lines (response monitoring)
- Phase 2: ~150 lines (baseline + timing)
- Phase 3: ~100 lines (refinements)
- **Total**: ~450 lines of new/modified code

## Success Criteria

✅ **Minimum Viable** (Phase 1 complete):
- Attacks can detect when target stops responding
- No false crash reports
- Response counter accurately tracks target replies
- At least Packet Flood attack works correctly

✅ **Production Ready** (Phase 1 + 2 complete):
- Baseline behavior captured before attacks
- Response timing measured accurately
- All 10 attack types work with monitoring
- Results distinguish real failures from normal behavior

✅ **Fully Featured** (All phases):
- Attack results saved to LittleFS
- Proper Meshtastic packet structure
- Comprehensive vulnerability reports
- Export functionality for analysis

## Priority Recommendation

**START WITH**: Phase 1, Task 1.1 + 1.2 + 1.3 (critical response monitoring)
**ESTIMATED TIME**: 4-6 hours
**OUTCOME**: Working packet flood attack that actually detects target behavior

Once Phase 1 is complete, the framework will be functional and can be tested with real devices.
