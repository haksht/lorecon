/**
 * Reconnaissance State Management Implementation
 */

#include "recon_state.h"
#include "config.h"

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
    scanState.lastScanSwitch = millis();
    scanState.totalPackets = 0;
    scanState.totalDetections = 0;
    scanState.packetPending = false;
    scanState.reconStartTime = millis();
    scanState.waitingForUserInput = false;
    
    #ifdef ENABLE_PSK_TESTING
    pskStats = PSKStats();  // Use constructor instead of memset
    #endif
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
        device->firstSeen = millis();
        device->powerClass = estimatePowerClass(rssi);
        device->isRouter = false;
        
        strncpy(device->protocol, protocol, sizeof(device->protocol) - 1);
        device->protocol[sizeof(device->protocol) - 1] = '\0';
        
        // Initialize device type
        strcpy(device->deviceType, "Unknown Device");
        
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
        
        Serial.printf("[TARGET] New targetable device: 0x%08X (%s) on %s (%.1f dBm)\n", 
                      nodeId, device->deviceType, getScanConfig(configIndex).protocol, rssi);
    }
    
    if (device) {
        device->packetCount++;
        device->avgRSSI = (device->avgRSSI * (device->packetCount - 1) + rssi) / device->packetCount;
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
        strcpy(emptyDevice.deviceType, "Unknown");
        strcpy(emptyDevice.firmwareVersion, "Unknown");
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
    
    if (!node && nodeCount < Config::Tracking::MAX_NODES) {
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
        node->packetCount++;
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

uint8_t ReconState::estimatePowerClass(float rssi) const {
    if (rssi > -70) return 2;  // High power (>100mW)
    if (rssi > -90) return 1;  // Medium power (10-100mW)
    return 0;                  // Low power (<10mW)
}

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
    
    Serial.println("=====================================\n");
}

// Packet replay management
bool ReconState::capturePacketForReplay(const uint8_t* data, size_t length, uint8_t configIndex,
                                        float rssi, const char* protocol, const char* decryptedText,
                                        uint32_t nodeId) {
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