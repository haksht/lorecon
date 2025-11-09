

# Offensive Security Testing Module - Complete Documentation

**ESP32 LoRa Sniffer - Adversarial Testing Framework**

**Version:** 1.0  
**Date:** November 9, 2025  
**Purpose:** Security research, vulnerability assessment, and device hardening

---

## 📚 Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Attack Types](#attack-types)
4. [Usage Examples](#usage-examples)
5. [Vulnerability Scanner](#vulnerability-scanner)
6. [Integration Guide](#integration-guide)
7. [Ethical Guidelines](#ethical-guidelines)
8. [API Reference](#api-reference)

---

## Overview

The Offensive Security Testing Module transforms the ESP32 LoRa Sniffer from a passive reconnaissance tool into an active security assessment platform. This enables security researchers to:

- **Test device resilience** under adversarial conditions
- **Discover vulnerabilities** before malicious actors do
- **Validate security claims** of mesh network devices
- **Develop defensive countermeasures** based on findings
- **Perform authorized penetration testing** of LoRa/Meshtastic networks

**Key Principle:** All testing follows ethical guidelines and requires explicit authorization.

---

## Architecture

### Component Overview

```
┌─────────────────────────────────────────────────────────┐
│                 User Interface / Commands                │
└───────────────────┬─────────────────────────────────────┘
                    │
┌───────────────────▼─────────────────────────────────────┐
│            DeviceStressTester (Core Engine)              │
│  • Target management                                     │
│  • Attack execution                                      │
│  • Result reporting                                      │
│  • Ethical safeguards                                    │
└────┬──────────────────────────────────┬────────────────┘
     │                                   │
     ▼                                   ▼
┌─────────────────────┐         ┌──────────────────────┐
│  AttackScenarios    │         │ VulnerabilityScanner │
│  (Pre-configured)   │         │  (Automated Testing) │
└─────────────────────┘         └──────────────────────┘
     │                                   │
     ▼                                   ▼
┌─────────────────────────────────────────────────────────┐
│                    PacketFuzzer                          │
│             (Malformed packet generation)                │
└─────────────────────────────────────────────────────────┘
```

### Class Hierarchy

```cpp
DeviceStressTester
├── Target management
│   ├── selectTarget()
│   ├── lockOntoTarget()
│   └── clearTarget()
│
├── Attack execution
│   ├── executeAttack()
│   ├── executePacketFlood()
│   ├── executeMalformedPackets()
│   ├── executeFrequencyJamming()
│   ├── executeTimingAnalysis()
│   ├── executeProtocolViolation()
│   ├── executePowerAnalysis()
│   ├── executeReplayAttack()
│   ├── executeInjectionAttack()
│   ├── executeRoutingManipulation()
│   └── executeIdentitySpoofing()
│
├── Safety mechanisms
│   ├── requestUserConfirmation()
│   ├── validateAttackParameters()
│   └── logAttackAction()
│
└── Reporting
    ├── getLastResult()
    ├── printAttackReport()
    └── exportResults()
```

---

## Attack Types

### 1. Packet Flood (DoS)

**Purpose:** Test target's ability to handle high packet rates

**How It Works:**
- Sends valid Meshtastic packets at configurable rate
- Monitors target responsiveness during flood
- Detects if target crashes or becomes unresponsive

**Typical Configuration:**
```cpp
AttackConfig config;
config.type = ATTACK_PACKET_FLOOD;
config.targetNodeId = 0x12345678;
config.targetFrequency = 906.875;
config.duration = 60000;  // 1 minute
config.packetRate = 50;   // 50 packets/sec
config.power = 17;        // Standard Meshtastic
config.destructive = true;
```

**Detection Criteria:**
- ✅ **Pass:** Target continues responding normally
- ⚠️ **Degradation:** Target slows but doesn't crash
- ❌ **Fail:** Target stops responding (DoS vulnerability)

**Recommendations:**
- Implement per-source rate limiting (10-20 pps)
- Queue excess packets instead of dropping
- Add watchdog timer to detect flooding

### 2. Malformed Packets (Fuzzing)

**Purpose:** Find parser bugs and input validation issues

**How It Works:**
- Generates 100+ malformed packet variants
- Tests different corruption types (header, length, payload, protobuf)
- Monitors for crashes or unexpected behavior

**Fuzz Types:**
1. Invalid header (wrong magic bytes)
2. Oversized packets (>256 bytes)
3. Undersized packets (<16 bytes)
4. Corrupted length fields
5. Invalid protobuf structures
6. Out-of-range values
7. Reserved bits set
8. Random mutations

**Detection Criteria:**
- ✅ **Pass:** All malformed packets handled gracefully
- ⚠️ **Warning:** Some packets cause errors but recovery works
- ❌ **Fail:** Target crashes or hangs (critical bug)

**Recommendations:**
- Add bounds checking on all input fields
- Validate protobuf structures before parsing
- Implement error recovery mechanisms
- Use fuzzing in development testing

### 3. Frequency Jamming

**Purpose:** Test interference resilience

**How It Works:**
- Transmits continuous RF energy on target frequency
- Blocks legitimate communications
- Observes recovery behavior after jamming stops

**⚠️ WARNING:** Highly destructive - blocks ALL traffic on frequency

**Detection Criteria:**
- ✅ **Pass:** Target detects jamming and switches frequency
- ⚠️ **Degradation:** Target continues trying on jammed frequency
- ❌ **Fail:** Target never recovers from jamming

**Recommendations:**
- Implement frequency hopping
- Add channel quality assessment
- Detect jamming and blacklist bad channels
- Use multiple frequencies in mesh

### 4. Timing Analysis

**Purpose:** Detect timing-based information leakage

**How It Works:**
- Sends test packets and measures response times
- Analyzes variance in response patterns
- Looks for correlation with internal state

**What It Can Reveal:**
- Encryption vs. plaintext processing time
- Authentication success vs. failure timing
- Queue depth or resource availability
- Presence of rate limiting

**Detection Criteria:**
- ✅ **Pass:** Response times consistent with jitter
- ⚠️ **Warning:** Some variance but not exploitable
- ❌ **Fail:** Significant timing differences (>50% variance)

**Recommendations:**
- Add constant-time crypto operations
- Introduce random delays (timing jitter)
- Rate limit responses uniformly

### 5. Protocol Violation

**Purpose:** Test protocol parser edge case handling

**How It Works:**
- Sends packets violating protocol rules
- Tests hop count limits, invalid flags, etc.
- Checks if target crashes or behaves oddly

**Violations Tested:**
- Hop count > 7 (max allowed)
- Reserved flags set to 1
- Out-of-sequence packets
- Invalid channel indices
- Malformed routing headers

**Detection Criteria:**
- ✅ **Pass:** All violations rejected gracefully
- ⚠️ **Warning:** Some violations processed (should be rejected)
- ❌ **Fail:** Target crashes on violations

**Recommendations:**
- Validate all protocol fields
- Reject out-of-spec packets early
- Don't trust packet contents

### 6. Power Analysis

**Purpose:** Test for RSSI-based information leakage

**How It Works:**
- Sends packets at different power levels
- Analyzes RSSI of responses
- Looks for patterns revealing internal state

**What It Might Reveal:**
- Whether packet was successfully decrypted
- Internal routing decisions
- Priority levels of different packet types

**Detection Criteria:**
- ✅ **Pass:** RSSI varies naturally with distance/conditions
- ⚠️ **Warning:** Patterns exist but hard to exploit
- ❌ **Fail:** Clear correlation between RSSI and state

**Recommendations:**
- Normalize response power levels
- Add random power variations
- Don't leak state via RF characteristics

### 7. Replay Attack

**Purpose:** Test anti-replay mechanisms

**How It Works:**
- Captures valid packet from target
- Replays same packet multiple times
- Checks if target accepts replays

**Detection Criteria:**
- ✅ **Pass:** Only first packet accepted, replays rejected
- ⚠️ **Warning:** Some replays accepted (time window issue)
- ❌ **Fail:** All replays accepted (no anti-replay)

**Recommendations:**
- Implement packet sequence numbers
- Use cryptographic nonces
- Track recently-seen packet IDs
- Reject duplicates within time window

### 8. Packet Injection

**Purpose:** Test if crafted packets accepted into mesh

**How It Works:**
- Crafts packets appearing to be from legitimate nodes
- Injects into mesh network
- Monitors if packets are routed/accepted

**Detection Criteria:**
- ✅ **Pass:** Injected packets rejected (authentication working)
- ⚠️ **Warning:** Packets accepted but not routed
- ❌ **Fail:** Packets fully accepted and routed

**Recommendations:**
- Implement source authentication (PKI, HMAC)
- Verify sender identity cryptographically
- Don't trust source address in packet

### 9. Routing Manipulation

**Purpose:** Test routing protocol security

**How It Works:**
- Sends packets with manipulated routing headers
- Attempts to poison routing tables
- Observes mesh topology changes

**Attacks Tested:**
- Fake shortest paths
- Route loops
- Blackhole routes
- Sybil attacks (fake multiple nodes)

**Detection Criteria:**
- ✅ **Pass:** Routing tables remain correct
- ⚠️ **Warning:** Temporary confusion but recovery
- ❌ **Fail:** Attacker controls routing

**Recommendations:**
- Authenticate routing updates
- Validate routing logic (loop prevention)
- Rate limit routing changes
- Implement secure routing protocols

### 10. Identity Spoofing

**Purpose:** Test node authentication

**How It Works:**
- Attempts to impersonate other nodes
- Sends packets with spoofed source addresses
- Checks if mesh accepts fake identity

**Detection Criteria:**
- ✅ **Pass:** Spoofed packets rejected
- ⚠️ **Warning:** Some spoofing works temporarily
- ❌ **Fail:** Full impersonation possible

**Recommendations:**
- Use PKI for node identity
- Implement MAC or digital signatures
- Verify every packet's source cryptographically

---

## Usage Examples

### Basic Attack Execution

```cpp
#include "device_stress_tester.h"

// Initialize
DeviceStressTester tester(&radio);
tester.initialize();
tester.setEthicalMode(true);  // Require confirmations

// Select target
tester.selectTarget(0x12345678, 906.875);

// Configure attack
AttackConfig config;
config.type = ATTACK_PACKET_FLOOD;
config.targetNodeId = 0x12345678;
config.targetFrequency = 906.875;
config.duration = 30000;     // 30 seconds
config.packetRate = 20;      // 20 pps
config.destructive = true;
config.description = "Testing DoS resilience";

// Execute
if (tester.executeAttack(config)) {
    tester.printAttackReport();
}
```

### Using Pre-configured Scenarios

```cpp
#include "device_stress_tester.h"

// Get pre-configured attack
AttackConfig dosAttack = AttackScenarios::denialOfService(0x12345678);

// Execute
DeviceStressTester tester(&radio);
tester.initialize();
tester.executeAttack(dosAttack);
```

### Attack Sequence

```cpp
// Run multiple attacks in sequence
AttackType sequence[] = {
    ATTACK_PROTOCOL_VIOLATION,  // Start gentle
    ATTACK_MALFORMED_PACKETS,   // Fuzzing
    ATTACK_TIMING_ANALYSIS,     // Side-channel
    ATTACK_PACKET_FLOOD         // Stress test
};

tester.executeAttackSequence(sequence, 4);
```

### Quick Testing Functions

```cpp
// Quick flood test
quickFloodTest(&radio, 0x12345678, 60000);

// Quick fuzz test  
quickFuzzTest(&radio, 0x12345678, 100);

// Quick vulnerability scan
quickScanTarget(&radio, 0x12345678);
```

---

## Vulnerability Scanner

### Automated Assessment

The vulnerability scanner runs a comprehensive suite of tests automatically:

```cpp
#include "device_stress_tester.h"

VulnerabilityScanner scanner(&radio, &tester);

// Full scan (15-20 minutes)
scanner.runFullScan(targetNodeId);
scanner.printFindings();

// Quick scan (5 minutes)
scanner.runQuickScan(targetNodeId);

// Export results
scanner.exportFindings("JSON");
scanner.exportFindings("CSV");
scanner.exportFindings("Markdown");

// Generate CVE draft
scanner.generateCVEDraft();
```

### Tests Performed

1. **Buffer Overflow** - Oversized packets
2. **Integer Overflow** - Boundary value testing
3. **Rate Limiting** - Flood detection
4. **Input Validation** - Malformed data handling
5. **Authentication Bypass** - Spoofing attempts
6. **Encryption Weakness** - Weak key detection
7. **Memory Exhaustion** - Resource consumption
8. **Crash Conditions** - Fault injection

### Severity Levels

- **🔴 CRITICAL:** Immediate exploitation, severe impact
- **🟠 HIGH:** Exploitation likely, significant impact
- **🟡 MEDIUM:** Exploitation possible, moderate impact
- **🟢 LOW:** Difficult to exploit, minimal impact
- **ℹ️ INFO:** Security observation, no immediate risk

---

## Integration Guide

### Adding to Main Application

```cpp
// In main.cpp or similar

#include "device_stress_tester.h"

// Global instance
DeviceStressTester* deviceTester = nullptr;
VulnerabilityScanner* vulnScanner = nullptr;

void setup() {
    // ... existing setup ...
    
    // Initialize offensive testing
    #ifdef ENABLE_OFFENSIVE_TESTING
    deviceTester = new DeviceStressTester(&radio);
    if (deviceTester->initialize()) {
        Serial.println("✅ Offensive testing available");
        
        vulnScanner = new VulnerabilityScanner(&radio, deviceTester);
    }
    #endif
}

void loop() {
    // ... existing loop ...
    
    // Check for offensive testing commands
    if (Serial.available()) {
        char cmd = Serial.read();
        
        switch (cmd) {
            case 'A':  // Attack menu
                showAttackMenu();
                break;
                
            case 'V':  // Vulnerability scan
                runVulnerabilityScan();
                break;
                
            // ... other commands ...
        }
    }
}

void showAttackMenu() {
    Serial.println("\n╔════════════════════════════════════════════╗");
    Serial.println("║          OFFENSIVE TESTING MENU            ║");
    Serial.println("╚════════════════════════════════════════════╝");
    Serial.println("1. Packet Flood (DoS)");
    Serial.println("2. Malformed Packets (Fuzzing)");
    Serial.println("3. Frequency Jamming");
    Serial.println("4. Timing Analysis");
    Serial.println("5. Protocol Violations");
    Serial.println("6. Power Analysis");
    Serial.println("7. Replay Attack");
    Serial.println("8. Packet Injection");
    Serial.println("9. Routing Manipulation");
    Serial.println("0. Identity Spoofing");
    Serial.println("F. Full Vulnerability Scan");
    Serial.println("Q. Quick Scan");
    Serial.println("X. Exit");
    Serial.println("\nSelect option:");
}
```

### Build Configuration

Add to `platformio.ini`:

```ini
[env:offensive]
platform = espressif32@^6.4.0
board = esp32-s3-devkitc-1
framework = arduino
build_flags = 
    -DENABLE_OFFENSIVE_TESTING
    -DENABLE_PSK_TESTING
    -DPRODUCTION_BUILD
build_src_filter = 
    +<*> 
    -<main_realistic.cpp>
lib_deps = 
    jgromes/RadioLib@^6.4.2
    bblanchon/ArduinoJson@^7.0.4
```

Build with:
```bash
pio run -e offensive --target upload
```

---

## Ethical Guidelines

**BEFORE using offensive testing capabilities:**

1. ✅ Read [ETHICAL_TESTING_GUIDELINES.md](ETHICAL_TESTING_GUIDELINES.md)
2. ✅ Obtain written authorization
3. ✅ Define clear scope and limitations
4. ✅ Establish emergency contacts
5. ✅ Enable audit logging
6. ✅ Prepare rollback procedures

**Golden Rules:**
- **Authorization is mandatory** - Never test without permission
- **Minimize harm** - Use least invasive tests first
- **Disclose responsibly** - 90-day vendor notification
- **Document everything** - Maintain audit trail
- **Follow the law** - Research local regulations

---

## API Reference

### DeviceStressTester Class

#### Methods

**Initialization:**
```cpp
bool initialize()
void setEthicalMode(bool enabled)
bool isReady()
```

**Target Management:**
```cpp
bool selectTarget(uint32_t nodeId, float frequency)
bool lockOntoTarget()
TargetDevice getTarget()
void clearTarget()
```

**Attack Execution:**
```cpp
bool executeAttack(const AttackConfig& config)
bool executeAttackSequence(AttackType* attacks, size_t count)
void abortAttack()
```

**Results:**
```cpp
AttackResult getLastResult()
void printAttackReport()
void exportResults(const String& filename)
```

### VulnerabilityScanner Class

#### Methods

**Scanning:**
```cpp
bool runFullScan(uint32_t targetNode)
bool runQuickScan(uint32_t targetNode)
bool runCustomScan(uint32_t targetNode, String* tests, size_t count)
```

**Reporting:**
```cpp
void printFindings()
void exportFindings(const String& format)  // JSON, CSV, Markdown
void generateCVEDraft()
```

### AttackScenarios Class

#### Pre-configured Attacks

```cpp
static AttackConfig denialOfService(uint32_t targetNode)
static AttackConfig protocolFuzzing(uint32_t targetNode)
static AttackConfig meshDisruption(uint32_t targetNode)
static AttackConfig privacyViolation(uint32_t targetNode)
static AttackConfig reliabilityTest(uint32_t targetNode)
```

### PacketFuzzer Class

#### Methods

```cpp
void fuzzHeader(uint8_t* packet)
void fuzzPayload(uint8_t* packet, size_t length)
void fuzzLength(size_t* length)
void fuzzProtobuf(uint8_t* packet, size_t length)
void generateRandomPacket(uint8_t* packet, size_t* length)
void mutatePacket(uint8_t* original, uint8_t* mutated, size_t length)
```

---

## Best Practices

### 1. Start Non-Destructive

Always begin with passive or low-impact tests:
1. Protocol violation (safe)
2. Timing analysis (safe)
3. Power analysis (safe)
4. Malformed packets (mostly safe)
5. Packet flood (destructive)
6. Frequency jamming (very destructive)

### 2. Monitor Continuously

During testing:
- Watch serial output for target responses
- Monitor RSSI levels
- Track packet counts
- Note any anomalies immediately

### 3. Document Everything

Required documentation:
- Authorization agreement
- Testing plan
- Real-time logs
- Findings report
- Disclosure timeline

### 4. Test Your Own First

Before testing someone else's device:
- Test your own devices first
- Understand expected behavior
- Know what "crash" looks like
- Verify your methodology

### 5. Have Rollback Ready

Be prepared to:
- Stop testing immediately
- Restore service if disrupted
- Provide logs if asked
- Assist with recovery

---

## Troubleshooting

### "Attack not executing"

Check:
- Is `initialize()` called?
- Is ethical mode blocking? (requires "CONFIRM")
- Are parameters valid? (frequency, node ID)
- Is radio initialized properly?

### "Target not responding to anything"

Possible causes:
- Target is off or out of range
- Wrong frequency selected
- Radio not transmitting (check antenna)
- Target already crashed from previous test

### "Ethical confirmation not showing"

Check:
- Is `setEthicalMode(true)` called?
- Is serial monitor connected?
- Is baud rate correct (115200)?
- Check timeout (30 seconds)

### "Scan takes too long"

Solutions:
- Use `runQuickScan()` instead (5 minutes)
- Run individual tests manually
- Reduce test durations in code
- Test during off-peak hours

---

## Security Considerations

### Protecting This Tool

This tool has powerful capabilities. Protect it:
- Don't share compiled binaries publicly
- Keep source code access controlled
- Require authentication to use
- Log all usage for audit trail
- Consider encrypting findings

### Operational Security

When conducting tests:
- Use dedicated testing hardware
- Isolate testing networks
- Encrypt communications about findings
- Secure test data storage
- Follow NDAs and agreements

---

## Contributing

To contribute new attack types or improvements:

1. Fork the repository
2. Create feature branch (`feature/new-attack-type`)
3. Follow ethical guidelines
4. Add documentation
5. Include test cases
6. Submit pull request

**Requirements:**
- All new attacks must have ethical safeguards
- Documentation must be complete
- Code must compile without warnings
- Testing methodology must be sound

---

## License

This code is provided for educational and authorized security research purposes only. Misuse may result in criminal prosecution. See [ETHICAL_TESTING_GUIDELINES.md](ETHICAL_TESTING_GUIDELINES.md) for terms of use.

---

## Support

**Questions?**
- GitHub Issues: [Project issues page]
- Documentation: See `docs/` directory
- Ethics: [ETHICAL_TESTING_GUIDELINES.md]

**Found a vulnerability?**
- Follow responsible disclosure
- See vulnerability scanner CVE draft template
- Allow 90 days for patch development

---

**Last Updated:** November 9, 2025  
**Version:** 1.0  
**Maintainer:** [Project maintainer]

