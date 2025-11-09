/**
 * Attack Scenarios - Pre-configured attack sequences
 * Common attack patterns for security research
 */

#include "device_stress_tester.h"

// Scenario 1: Denial of Service
AttackConfig AttackScenarios::denialOfService(uint32_t targetNode) {
    AttackConfig config;
    config.type = ATTACK_PACKET_FLOOD;
    config.targetNodeId = targetNode;
    config.targetFrequency = 906.875;  // Meshtastic US primary
    config.duration = 60000;  // 1 minute
    config.packetRate = 50;   // 50 packets/sec
    config.power = 17;        // Standard Meshtastic power
    config.destructive = true;
    config.logAllActions = true;
    config.description = "Denial of Service - sustained packet flood to test resilience";
    
    return config;
}

// Scenario 2: Protocol Fuzzing
AttackConfig AttackScenarios::protocolFuzzing(uint32_t targetNode) {
    AttackConfig config;
    config.type = ATTACK_MALFORMED_PACKETS;
    config.targetNodeId = targetNode;
    config.targetFrequency = 906.875;
    config.duration = 120000;  // 2 minutes
    config.packetRate = 10;    // 10 malformed packets/sec
    config.power = 10;
    config.destructive = false;
    config.logAllActions = true;
    config.description = "Protocol Fuzzing - systematic malformed packet testing";
    
    return config;
}

// Scenario 3: Mesh Disruption
AttackConfig AttackScenarios::meshDisruption(uint32_t targetNode) {
    AttackConfig config;
    config.type = ATTACK_ROUTING_MANIPULATION;
    config.targetNodeId = targetNode;
    config.targetFrequency = 906.875;
    config.duration = 180000;  // 3 minutes
    config.packetRate = 5;
    config.power = 20;
    config.destructive = true;
    config.logAllActions = true;
    config.description = "Mesh Disruption - routing table manipulation and topology poisoning";
    
    return config;
}

// Scenario 4: Privacy Violation
AttackConfig AttackScenarios::privacyViolation(uint32_t targetNode) {
    AttackConfig config;
    config.type = ATTACK_TIMING_ANALYSIS;
    config.targetNodeId = targetNode;
    config.targetFrequency = 906.875;
    config.duration = 300000;  // 5 minutes
    config.packetRate = 2;     // 2 probes/sec
    config.power = 10;
    config.destructive = false;
    config.logAllActions = true;
    config.description = "Privacy Violation - timing side-channel analysis for state inference";
    
    return config;
}

// Scenario 5: Reliability Test
AttackConfig AttackScenarios::reliabilityTest(uint32_t targetNode) {
    AttackConfig config;
    config.type = ATTACK_PROTOCOL_VIOLATION;
    config.targetNodeId = targetNode;
    config.targetFrequency = 906.875;
    config.duration = 60000;  // 1 minute
    config.packetRate = 10;
    config.power = 10;
    config.destructive = false;
    config.logAllActions = true;
    config.description = "Reliability Test - edge case protocol violations";
    
    return config;
}
