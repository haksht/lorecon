/**
 * Reconnaissance State Management Implementation
 */

#include "recon_state.h"
#include "config.h"
#include "utils/format_utils.h"

// Compile-time check: Ensure rfActivity array size matches NUM_CONFIGS
static_assert(sizeof(((ReconState*)0)->rfActivity) / sizeof(RFActivity) >= 16, 
              "rfActivity array must be large enough for all scan configs");

// Define the static scan configurations
const ScanConfig ReconState::scanConfigs[] = {
  // Meshtastic US frequencies - Primary routing
  {906.875, 250.0, 11, 0x2b, "Meshtastic_LF_906"},
  {902.125, 250.0, 11, 0x2b, "Meshtastic_LF_902"},
  {903.875, 250.0, 10, 0x2b, "Meshtastic_MF"},
  {904.375, 250.0,  9, 0x2b, "Meshtastic_SF"},
  
  // Focus on 902.125 MHz variations (where T-Deck reports it's configured)
  {902.125, 250.0, 10, 0x2b, "Meshtastic_902_SF10"},
  {902.125, 250.0,  9, 0x2b, "Meshtastic_902_SF9"},
  {902.125, 250.0,  8, 0x2b, "Meshtastic_902_SF8"},
  {902.125, 250.0, 11, 0x12, "Meshtastic_902_SW12"},
  {902.125, 250.0, 10, 0x12, "Meshtastic_902_SF10_SW12"},
  {902.125, 250.0,  9, 0x12, "Meshtastic_902_SF9_SW12"},
  
  // TTN/LoRaWAN US channels
  {903.9,   125.0,  7, 0x12, "TTN_CH0_SF7"},
  {904.1,   125.0,  8, 0x12, "TTN_CH1_SF8"},
  {904.3,   125.0,  9, 0x12, "TTN_CH2_SF9"},
  {904.5,   125.0, 10, 0x12, "TTN_CH3_SF10"},
  
  // LoRaWAN US915 uplink channels (commercial deployments)
  {902.3,   125.0,  7, 0x12, "LoRaWAN_US_UP0"},
  {902.5,   125.0,  8, 0x12, "LoRaWAN_US_UP1"},
  {911.9,   125.0,  9, 0x12, "LoRaWAN_US_UP63"},
  
  // Meshtastic LongSlow preset (maximum range)
  {906.875, 125.0, 12, 0x48, "Meshtastic_LongSlow"},
  
  // Helium Network US915 (uplink - catch sensor transmissions)or transmissions)
  {904.3,   125.0,  9, 0x12, "Helium_US_UL1"},
  {904.5,   125.0, 10, 0x12, "Helium_US_UL2"},
  
  // Helium Network US915 (downlink - hotspot transmissions)
  {923.3,   500.0, 10, 0x12, "Helium_US_DL1"},
  {923.9,   500.0, 10, 0x12, "Helium_US_DL2"},
  {924.5,   500.0,  9, 0x12, "Helium_US_DL3"},
  {925.1,   500.0,  8, 0x12, "Helium_US_DL4"},
  
  // ISM band exploration
  {915.0,   125.0,  7, 0x12, "ISM_915_SF7"},
  {920.0,   250.0,  8, 0x12, "ISM_920_SF8"},
};

const uint8_t ReconState::NUM_CONFIGS = sizeof(scanConfigs) / sizeof(ScanConfig);

// Global instance
ReconState reconState;

ReconState::ReconState() {
    initialize();
}

void ReconState::initialize() {
    // Initialize counters
    numTargetableDevices = 0;
    nodeCount = 0;
    numCapturedPackets = 0;
    
    // Clear all arrays
    memset(rfActivity, 0, sizeof(rfActivity));
    memset(targetableDevices, 0, sizeof(targetableDevices));
    memset(trackedNodes, 0, sizeof(trackedNodes));
    memset(replaySlots, 0, sizeof(replaySlots));
    
    // Initialize scan state
    scanState.mode = MODE_RECONNAISSANCE;
    scanState.currentConfig = 0;
    scanState.targetConfig = 0;
    scanState.targetedByDevice = false;
    scanState.lastScanSwitch = millis();
    scanState.totalPackets = 0;
    scanState.totalDetections = 0;
    scanState.droppedPackets = 0;
    scanState.peakQueueSize = 0;
    scanState.packetPending = false;
    scanState.reconStartTime = millis();
    scanState.waitingForUserInput = false;
    
    pskStats = PSKStats();  // Use constructor instead of memset
}

void ReconState::reset() {
    initialize();
}

const ScanConfig& ReconState::getScanConfig(uint8_t index) const {
    if (index >= NUM_CONFIGS) {
        // Return first config as fallback
        return scanConfigs[0];
    }
    return scanConfigs[index];
}

uint8_t ReconState::getNumConfigs() const {
    return NUM_CONFIGS;
}

bool ReconState::isValidConfigIndex(uint8_t index) const {
    return index < NUM_CONFIGS;
}

void ReconState::updateRFActivity(uint8_t configIndex, float rssi) {
    if (configIndex >= NUM_CONFIGS) {
        Serial.printf("[ERROR] updateRFActivity: Invalid config index %d (max: %d)\n", 
                      configIndex, NUM_CONFIGS - 1);
        return;
    }
    
    RFActivity* activity = &rfActivity[configIndex];
    
    if (activity->activityCount == 0) {
        // First detection on this config
        activity->configIndex = configIndex;
        activity->avgRSSI = rssi;
        activity->peakRSSI = rssi;
        activity->activityCount = 1;
        activity->firstSeen = millis();
        activity->lastActivity = millis();
    } else {
        // Update existing activity
        activity->activityCount++;
        activity->avgRSSI = (activity->avgRSSI * (activity->activityCount - 1) + rssi) / activity->activityCount;
        if (rssi > activity->peakRSSI) activity->peakRSSI = rssi;
        activity->lastActivity = millis();
    }
    
    // Determine activity level
    if (rssi > -60) {
        activity->activityLevel = "HIGH";
    } else if (rssi > -80 || activity->activityCount > 5) {
        activity->activityLevel = "MEDIUM";
    } else {
        activity->activityLevel = "LOW";
    }
}

const RFActivity& ReconState::getRFActivity(uint8_t configIndex) const {
    static RFActivity emptyActivity;
    static bool initialized = false;
    if (!initialized) {
        memset(&emptyActivity, 0, sizeof(emptyActivity));
        emptyActivity.activityLevel = "NONE";
        initialized = true;
    }
    if (configIndex >= NUM_CONFIGS) return emptyActivity;
    return rfActivity[configIndex];
}

void ReconState::clearRFActivity() {
    memset(rfActivity, 0, sizeof(rfActivity));
}

void ReconState::addTargetableDevice(uint32_t nodeId, uint8_t configIndex, float rssi, 
                                    const char* protocol, const uint8_t* packetData, 
                                    size_t packetLength) {
    if (nodeId == 0 || configIndex >= NUM_CONFIGS) return;
    
    // Find existing device or create new entry
    TargetableDevice* device = findTargetableDevice(nodeId);
    
    if (!device && numTargetableDevices < Config::Tracking::MAX_DEVICES) {
        device = &targetableDevices[numTargetableDevices++];
        device->nodeId = nodeId;
        device->configIndex = configIndex;
        device->packetCount = 0;
        device->avgRSSI = rssi;
        device->bestRSSI = rssi;
        device->rssiStdDev = 0.0f;  // Initialize variance tracking
        device->rssiM2 = 0.0f;      // Initialize Welford's M2
        device->avgPacketInterval = 0;
        device->lastPacketInterval = 0;
        device->periodicityScore = 0;
        device->originatedPackets = 0;
        device->relayedPackets = 0;
        device->firstSeen = millis();
        device->powerClass = FormatUtils::estimatePowerClass(rssi);
        device->isRouter = false;
        
        strncpy(device->protocol, protocol, sizeof(device->protocol) - 1);
        device->protocol[sizeof(device->protocol) - 1] = '\0';
        
        // Initialize device type
        strncpy(device->deviceType, "Unknown Device", sizeof(device->deviceType) - 1);
        device->deviceType[sizeof(device->deviceType) - 1] = '\0';
        
        // Perform device type identification
        if (packetData && packetLength > 0) {
            const char* detectedType = identifyDeviceType(packetData, packetLength, protocol, rssi);
            strncpy(device->deviceType, detectedType, sizeof(device->deviceType) - 1);
            device->deviceType[sizeof(device->deviceType) - 1] = '\0';
            device->isRouter = isRoutingDevice(packetData, packetLength, protocol);
            
            // Add firmware version detection
            const char* fwVersion = estimateFirmwareVersion(packetData, packetLength, protocol);
            strncpy(device->firmwareVersion, fwVersion, sizeof(device->firmwareVersion) - 1);
            device->firmwareVersion[sizeof(device->firmwareVersion) - 1] = '\0';
        }
        
        // Increment total detections counter when new device is added
        scanState.totalDetections++;
        
        Serial.printf("[TARGET] New targetable device: 0x%08X (%s) on %s (%.1f dBm)\n", 
                      nodeId, device->deviceType, getScanConfig(configIndex).protocol, rssi);
    }
    
    if (device) {
        // Cap packetCount at UINT16_MAX to prevent overflow (65,536 packets is ~18 hrs at 1 pkt/sec)
        if (device->packetCount < UINT16_MAX) {
            device->packetCount++;
        }
        
        // Update RSSI statistics with running variance calculation
        float oldAvg = device->avgRSSI;
        // Use incremental formula to avoid precision loss: newAvg = oldAvg + (x - oldAvg)/n
        device->avgRSSI = oldAvg + (rssi - oldAvg) / device->packetCount;
        
        // Update RSSI standard deviation using Welford's online algorithm
        if (device->packetCount > 1) {
            float delta = rssi - oldAvg;
            float delta2 = rssi - device->avgRSSI;
            // Update M2 (sum of squared differences)
            device->rssiM2 += delta * delta2;
            // Calculate standard deviation from M2 (use fabs to handle floating point precision errors)
            device->rssiStdDev = sqrt(fabs(device->rssiM2) / (device->packetCount - 1));
        }
        
        if (rssi > device->bestRSSI) {
            device->bestRSSI = rssi;
            device->configIndex = configIndex;  // Update to best config
        }
        device->lastSeen = millis();
    }
}

const TargetableDevice& ReconState::getTargetableDevice(uint8_t index) const {
    static TargetableDevice emptyDevice;
    static bool initialized = false;
    if (!initialized) {
        memset(&emptyDevice, 0, sizeof(emptyDevice));
        strncpy(emptyDevice.deviceType, "Unknown", sizeof(emptyDevice.deviceType) - 1);
        emptyDevice.deviceType[sizeof(emptyDevice.deviceType) - 1] = '\0';
        strncpy(emptyDevice.firmwareVersion, "Unknown", sizeof(emptyDevice.firmwareVersion) - 1);
        emptyDevice.firmwareVersion[sizeof(emptyDevice.firmwareVersion) - 1] = '\0';
        initialized = true;
    }
    if (index >= numTargetableDevices) return emptyDevice;
    return targetableDevices[index];
}

TargetableDevice* ReconState::findTargetableDevice(uint32_t nodeId) {
    for (uint8_t i = 0; i < numTargetableDevices; i++) {
        if (targetableDevices[i].nodeId == nodeId) {
            return &targetableDevices[i];
        }
    }
    return nullptr;
}

void ReconState::clearTargetableDevices() {
    numTargetableDevices = 0;
    memset(targetableDevices, 0, sizeof(targetableDevices));
}

void ReconState::updateNode(uint32_t nodeId, const char* protocol, float rssi) {
    if (nodeId == 0) return;
    
    TrackedNode* node = findNode(nodeId);
    
    if (!node && nodeCount < Config::Tracking::MAX_HOT_NODES) {
        node = &trackedNodes[nodeCount++];
        node->nodeId = nodeId;
        node->protocol = protocol;
        node->packetCount = 0;
        node->avgRSSI = rssi;
        node->bestRSSI = rssi;
        node->firstSeen = millis();
        Serial.printf("[NODE] New: 0x%08X (%s)\n", nodeId, protocol);
    }
    
    if (node) {
        // Cap packetCount at UINT16_MAX to prevent overflow and division by zero
        if (node->packetCount < UINT16_MAX) {
            node->packetCount++;
        }
        node->avgRSSI = (node->avgRSSI * (node->packetCount - 1) + rssi) / node->packetCount;
        if (rssi > node->bestRSSI) node->bestRSSI = rssi;
        node->lastSeen = millis();
        node->active = true;
    }
}

TrackedNode* ReconState::findNode(uint32_t nodeId) {
    for (uint8_t i = 0; i < nodeCount; i++) {
        if (trackedNodes[i].nodeId == nodeId) {
            return &trackedNodes[i];
        }
    }
    return nullptr;
}

void ReconState::clearNodes() {
    nodeCount = 0;
    memset(trackedNodes, 0, sizeof(trackedNodes));
}

bool ReconState::hasRFActivity() const {
    for (uint8_t i = 0; i < NUM_CONFIGS; i++) {
        if (rfActivity[i].activityCount > 0) {
            return true;
        }
    }
    return false;
}

uint32_t ReconState::getReconDuration() const {
    return (millis() - scanState.reconStartTime) / 1000;
}

// Helper methods
const char* ReconState::identifyDeviceType(const uint8_t* data, size_t length, 
                                          const char* protocol, float rssi) const {
    // Meshtastic device type detection
    if (strcmp(protocol, "Meshtastic") == 0) {
        if (rssi > -50) return "Meshtastic Base/Solar";
        
        if (length >= 16) {
            uint8_t hopCount = data[8] & 0x07;
            if (hopCount > 0) return "Meshtastic Router";
        }
        
        if (rssi > -80) return "Meshtastic Mobile";
        if (rssi > -110) return "Meshtastic Handheld";
        return "Meshtastic Low-Power";
    }
    
    // LoRaWAN device classification
    if (strcmp(protocol, "LoRaWAN") == 0) {
        uint8_t mtype = (data[0] >> 5) & 0x07;
        
        switch (mtype) {
            case 0x00: return "LoRaWAN Join Request";
            case 0x01: return "LoRaWAN Join Accept";
            case 0x02: return "LoRaWAN Unconfirmed Up";
            case 0x03: return "LoRaWAN Unconfirmed Down";
            case 0x04: return "LoRaWAN Confirmed Up";
            case 0x05: return "LoRaWAN Confirmed Down";
            case 0x06: return "LoRaWAN RejoinReq";
            case 0x07: return "LoRaWAN Proprietary";
        }
    }
    
    if (strcmp(protocol, "Beacon") == 0) {
        if (rssi > -60) return "Beacon High-Power";
        return "Beacon Standard";
    }
    
    return "Unknown Device";
}

// estimatePowerClass moved to utils/format_utils.h as FormatUtils::estimatePowerClass()

bool ReconState::isRoutingDevice(const uint8_t* data, size_t length, const char* protocol) const {
    if (strcmp(protocol, "Meshtastic") == 0 && length >= 12) {
        uint8_t hopCount = data[8] & 0x07;
        uint8_t routingFlags = data[9];
        return (hopCount > 0 || (routingFlags & 0x01));
    }
    return false;
}

const char* ReconState::estimateFirmwareVersion(const uint8_t* data, size_t length, const char* protocol) const {
    if (strcmp(protocol, "Meshtastic") == 0 && length >= 12) {
        // Firmware 2.2+ uses encryption flag
        if (length >= 9 && (data[8] & 0x80)) {
            return "v2.2+";
        }
        
        // Firmware 2.1+ has longer packets
        if (length > 50) {
            return "v2.1+";
        }
        
        // Short packets = older firmware or beacons
        if (length <= 16) {
            return "v1.x/beacon";
        }
        
        return "v2.0-2.2";
    }
    
    if (strcmp(protocol, "LoRaWAN") == 0) {
        return "LoRaWAN 1.0.x";
    }
    
    return "Unknown";
}

void ReconState::printStateSummary() const {
    Serial.println("\n=== RECONNAISSANCE STATE SUMMARY ===");
    Serial.printf("Mode: %d, Current Config: %d/%d\n", 
                  scanState.mode, scanState.currentConfig, NUM_CONFIGS);
    Serial.printf("Total packets: %d, Total detections: %d\n", 
                  scanState.totalPackets, scanState.totalDetections);
    Serial.printf("Targetable devices: %d, Tracked nodes: %d\n", 
                  numTargetableDevices, nodeCount);
    Serial.printf("Recon duration: %u seconds\n", (unsigned int)getReconDuration());
    
    if (hasRFActivity()) {
        Serial.println("\nActive configurations:");
        for (uint8_t i = 0; i < NUM_CONFIGS; i++) {
            if (rfActivity[i].activityCount > 0) {
                Serial.printf("  %s: %d detections, %.1f dBm avg\n",
                              getScanConfig(i).protocol, 
                              rfActivity[i].activityCount, 
                              rfActivity[i].avgRSSI);
            }
        }
    }
    
    // Network intelligence summary
    if (networkIntel.activeTransmitters > 0 || networkIntel.floodingEvents > 0) {
        Serial.println("\n[NETWORK INTEL]");
        Serial.printf("  Active transmitters: %d\n", networkIntel.activeTransmitters);
        Serial.printf("  Relay-only nodes: %d\n", networkIntel.relayOnlyNodes);
        Serial.printf("  Mixed nodes: %d\n", networkIntel.mixedNodes);
        Serial.printf("  Flooding events: %d\n", networkIntel.floodingEvents);
        if (networkIntel.encryptedNetworks > 0) {
            Serial.printf("  Encrypted networks: ~%d\n", networkIntel.encryptedNetworks);
        }
    }
    
    Serial.println("=====================================\n");
}

// Packet replay management
bool ReconState::capturePacketForReplay(const uint8_t* data, size_t length, uint8_t configIndex,
                                        float rssi, const char* protocol, const char* decryptedText,
                                        uint32_t nodeId, uint32_t packetId) {
    if (numCapturedPackets >= Config::Replay::MAX_SLOTS) {
        return false;  // Slots full
    }
    
    if (length > Config::PacketProcessing::MAX_PACKET_SIZE) {
        return false;  // Packet too large
    }
    
    CapturedPacket& slot = replaySlots[numCapturedPackets];
    memcpy(slot.data, data, length);
    slot.length = length;
    slot.configIndex = configIndex;
    slot.originalRSSI = rssi;
    slot.captureTime = millis();
    slot.nodeId = nodeId;
    slot.packetId = packetId;
    strncpy(slot.protocol, protocol, sizeof(slot.protocol) - 1);
    slot.protocol[sizeof(slot.protocol) - 1] = '\0';
    
    // Store decrypted text if available
    if (decryptedText != nullptr && decryptedText[0] != '\0') {
        strncpy(slot.decryptedText, decryptedText, sizeof(slot.decryptedText) - 1);
        slot.decryptedText[sizeof(slot.decryptedText) - 1] = '\0';
    } else {
        slot.decryptedText[0] = '\0';  // Empty string if no decryption
    }
    
    slot.valid = true;
    
    numCapturedPackets++;
    
    Serial.printf("[REPLAY] Packet captured to slot #%d (%d bytes, %s)\n",
                  numCapturedPackets, length, protocol);
    
    return true;
}

const CapturedPacket& ReconState::getReplayPacket(uint8_t index) const {
    static CapturedPacket emptyPacket;
    static bool initialized = false;
    if (!initialized) {
        memset(&emptyPacket, 0, sizeof(emptyPacket));
        initialized = true;
    }
    if (index >= numCapturedPackets) return emptyPacket;
    return replaySlots[index];
}

void ReconState::clearReplaySlots() {
    numCapturedPackets = 0;
    memset(replaySlots, 0, sizeof(replaySlots));
    Serial.println("[REPLAY] All replay slots cleared");
}

// Calculate network intelligence statistics
void ReconState::updateNetworkIntel() {
    networkIntel.activeTransmitters = 0;
    networkIntel.relayOnlyNodes = 0;
    networkIntel.mixedNodes = 0;
    networkIntel.floodingEvents = 0;
    networkIntel.encryptedNetworks = 0;
    networkIntel.beaconDevices = 0;
    
    // Analyze transmission patterns from targetable devices
    for (uint8_t i = 0; i < numTargetableDevices; i++) {
        const TargetableDevice& dev = targetableDevices[i];
        
        if (dev.originatedPackets > 0 && dev.relayedPackets > 0) {
            networkIntel.mixedNodes++;
        } else if (dev.originatedPackets > 0) {
            networkIntel.activeTransmitters++;
        } else if (dev.relayedPackets > 0) {
            networkIntel.relayOnlyNodes++;
        }
        
        // Count devices with high periodicity score as beacons
        if (dev.periodicityScore >= Config::Anomaly::MIN_BEACON_CONFIDENCE) {
            networkIntel.beaconDevices++;
        }
    }
    
    // Count flooding events from replay slots (same packet ID, different nodes)
    uint32_t seenPacketIds[Config::Replay::MAX_SLOTS];
    uint8_t packetIdCounts[Config::Replay::MAX_SLOTS] = {0};
    uint8_t uniquePacketIds = 0;
    
    for (uint8_t i = 0; i < numCapturedPackets; i++) {
        if (replaySlots[i].packetId == 0) continue;
        
        // Check if we've seen this packet ID
        bool found = false;
        for (uint8_t j = 0; j < uniquePacketIds; j++) {
            if (seenPacketIds[j] == replaySlots[i].packetId) {
                packetIdCounts[j]++;
                found = true;
                break;
            }
        }
        
        if (!found && uniquePacketIds < Config::Replay::MAX_SLOTS) {
            seenPacketIds[uniquePacketIds] = replaySlots[i].packetId;
            packetIdCounts[uniquePacketIds] = 1;
            uniquePacketIds++;
        }
    }
    
    // Count flooding events (packet ID seen more than once)
    for (uint8_t i = 0; i < uniquePacketIds; i++) {
        if (packetIdCounts[i] > 1) {
            networkIntel.floodingEvents++;
        }
    }
    
    // Estimate encrypted networks (groups of devices with failed decryption)
    // This is a rough heuristic - devices with no decrypted text but valid packets
    uint8_t encryptedDevices = 0;
    for (uint8_t i = 0; i < numTargetableDevices; i++) {
        // Check if device has packets but we have no successful decryptions
        bool hasEncryptedOnly = true;
        for (uint8_t j = 0; j < numCapturedPackets; j++) {
            if (replaySlots[j].nodeId == targetableDevices[i].nodeId) {
                if (replaySlots[j].decryptedText[0] != '\0') {
                    hasEncryptedOnly = false;
                    break;
                }
            }
        }
        if (hasEncryptedOnly && targetableDevices[i].packetCount > 0) {
            encryptedDevices++;
        }
    }
    
    // Estimate number of encrypted networks (assume 2-4 devices per network)
    if (encryptedDevices > 0) {
        networkIntel.encryptedNetworks = (encryptedDevices + 2) / 3;  // Rough estimate
    }
    
    networkIntel.lastUpdate = millis();
}

/**
 * Record an anomaly detection event
 */
void ReconState::recordAnomaly(uint32_t nodeId, AnomalyType type, float severity, const char* description) {
    // Use modulo for circular buffer (keeps growing counter)
    uint8_t index = numAnomalies % Config::Anomaly::MAX_ANOMALIES;
    
    AnomalyRecord& anomaly = anomalies[index];
    anomaly.nodeId = nodeId;
    anomaly.type = type;
    anomaly.timestamp = millis();
    anomaly.severity = severity;
    anomaly.acknowledged = false;
    
    // Safe string copy with guaranteed null termination
    strncpy(anomaly.description, description, sizeof(anomaly.description) - 1);
    anomaly.description[sizeof(anomaly.description) - 1] = '\0';
    
    numAnomalies++;
    // Cap anomalyCount at buffer size (circular buffer overwrites old anomalies)
    if (networkIntel.anomalyCount < Config::Anomaly::MAX_ANOMALIES) {
        networkIntel.anomalyCount++;
    }
}

/**
 * Check for anomalies in received packet
 */
void ReconState::checkForAnomalies(const uint8_t* data, size_t length, uint32_t nodeId, float rssi) {
    if (!data || length == 0 || nodeId == 0) return;
    
    // Find device
    TargetableDevice* device = findTargetableDevice(nodeId);
    if (!device) return;
    
    // Cooldown: prevent spam by checking if we recently logged this anomaly type for this device
    // Uses a simple check: skip if same device+type already in recent anomalies
    auto shouldSkip = [this, nodeId](AnomalyType type) -> bool {
        unsigned long now = millis();
        uint8_t checkCount = std::min((uint16_t)numAnomalies, (uint16_t)Config::Anomaly::MAX_ANOMALIES);
        // Check last 10 anomalies for same device+type within cooldown period
        for (uint8_t i = 0; i < std::min(checkCount, (uint8_t)10); i++) {
            uint8_t idx = (numAnomalies - 1 - i) % Config::Anomaly::MAX_ANOMALIES;
            if (anomalies[idx].nodeId == nodeId && 
                anomalies[idx].type == type &&
                (now - anomalies[idx].timestamp) < 300000) {  // 5 minute cooldown
                return true;
            }
        }
        return false;
    };
    
    // Check 1: Packet size outlier (very rare, likely protocol error)
    if (length > 255 && !shouldSkip(AnomalyType::PACKET_SIZE_OUTLIER)) {
        char desc[64];
        snprintf(desc, sizeof(desc), "Packet size %u exceeds typical LoRa MTU", length);
        recordAnomaly(nodeId, AnomalyType::PACKET_SIZE_OUTLIER, 0.5f, desc);
    }
    
    // Check 2: Rate violation (packets per minute)
    // Raise threshold: 60 packets/min = 1/sec is more reasonable for some protocols
    if (device->avgPacketInterval > 100 && device->packetCount >= 5 && 
        !shouldSkip(AnomalyType::RATE_VIOLATION)) {
        float packetsPerMin = 60000.0f / device->avgPacketInterval;
        // Use 60 pkt/min threshold (1 per second) - more permissive than before
        if (packetsPerMin > 60) {
            char desc[64];
            snprintf(desc, sizeof(desc), "Rate %.1f pkt/min (>1/sec)", packetsPerMin);
            recordAnomaly(nodeId, AnomalyType::RATE_VIOLATION, 0.8f, desc);
        }
    }
    
    // Check 3: RSSI inconsistency (possible spoofing or mobile device)
    // Raise threshold: 20 dBm variance is more reasonable for mobile devices
    if (device->rssiStdDev > 20.0f && device->packetCount >= 5 &&
        !shouldSkip(AnomalyType::RSSI_INCONSISTENCY)) {
        char desc[64];
        snprintf(desc, sizeof(desc), "RSSI stddev %.1f dBm (possible mobile/spoof)", device->rssiStdDev);
        recordAnomaly(nodeId, AnomalyType::RSSI_INCONSISTENCY, 0.7f, desc);
    }
    
    // Check 4: Replay attack detection (packet ID seen before with old timestamp)
    // This requires packet ID tracking - implementation deferred to packet processor
}

/**
 * Get count of unacknowledged anomalies
 */
uint8_t ReconState::getUnacknowledgedAnomalies() const {
    uint8_t count = 0;
    uint8_t maxIndex = std::min((uint16_t)numAnomalies, (uint16_t)Config::Anomaly::MAX_ANOMALIES);
    for (uint8_t i = 0; i < maxIndex; i++) {
        if (!anomalies[i].acknowledged) {
            count++;
        }
    }
    return count;
}

/**
 * Update traffic histogram (called once per packet)
 */
void ReconState::updateTrafficHistogram() {
    unsigned long now = millis();
    uint8_t currentHour = (now / 3600000) % 24;  // Hour of day (0-23)
    
    // Safety: should always be 0-23, but guarantee bounds
    if (currentHour < 24) {
        trafficHist.hourlyPackets[currentHour]++;
    }
    trafficHist.lastHourChange = now;
}

/**
 * Update temporal metrics for a device (periodicity, beacon detection)
 */
void ReconState::updateDeviceTemporalMetrics(uint32_t nodeId) {
    if (nodeId == 0) return;
    
    TargetableDevice* device = findTargetableDevice(nodeId);
    if (!device) return;
    
    unsigned long now = millis();
    
    // Calculate packet interval (protect against millis() rollover)
    if (device->lastSeen > 0 && device->lastSeen <= now) {
        unsigned long interval = now - device->lastSeen;
        
        // Update average interval using exponential moving average
        if (device->avgPacketInterval == 0) {
            device->avgPacketInterval = interval;
        } else {
            // EMA with alpha = 0.2 (weight new samples less to smooth out jitter)
            // Use 64-bit intermediate to prevent overflow with large intervals (e.g., hourly beacons)
            uint64_t numerator = (uint64_t)device->avgPacketInterval * 4 + interval;
            device->avgPacketInterval = (uint32_t)(numerator / 5);
        }
        
        device->lastPacketInterval = interval;
        
        // Calculate periodicity score (0-100)
        // High score = consistent intervals (beacon-like)
        if (device->packetCount >= Config::Anomaly::MIN_PACKETS_FOR_PERIODICITY && 
            device->avgPacketInterval > 0) {
            // Calculate absolute difference safely (avoid overflow from unsigned subtraction)
            uint32_t jitter = (interval > device->avgPacketInterval) ? 
                              (interval - device->avgPacketInterval) : 
                              (device->avgPacketInterval - interval);
            float jitterRatio = (float)jitter / device->avgPacketInterval;
            
            if (jitterRatio < Config::Anomaly::BEACON_JITTER_TOLERANCE) {
                // Within jitter tolerance - likely a beacon
                device->periodicityScore = 100 - (int)(jitterRatio * 100.0f);
                device->periodicityScore = std::min((uint8_t)100, device->periodicityScore);
            } else {
                // Not periodic - decay score (prevent uint8_t underflow)
                if (device->periodicityScore >= 10) {
                    device->periodicityScore -= 10;
                } else {
                    device->periodicityScore = 0;
                }
            }
        }
    }
}