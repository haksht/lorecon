/**
 * Reconnaissance State Management Implementation
 */

#include "recon_state.h"
#include "config.h"
#include "utils/format_utils.h"
#include "logger.h"
#include <time.h>

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
  
  // Helium Network US915 (uplink - catch sensor transmissions)
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

ReconState::ReconState() : repoMutex_(nullptr) {
    // Verify scanConfigs matches NUM_CONFIGURATIONS (catches adding configs without updating config.h)
    static_assert(sizeof(scanConfigs) / sizeof(ScanConfig) <= Config::Scanning::NUM_CONFIGURATIONS,
                  "More scanConfigs entries than NUM_CONFIGURATIONS - increase it in config.h");

    // Create mutex for thread-safe repository access
    repoMutex_ = xSemaphoreCreateMutex();
    if (!repoMutex_) {
        Serial.println("[ERROR] Failed to create ReconState mutex!");
    }
    initialize();
}

bool ReconState::lock(uint32_t timeoutMs) const {
    if (!repoMutex_) return false;
    return xSemaphoreTake(repoMutex_, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void ReconState::unlock() const {
    if (repoMutex_) {
        xSemaphoreGive(repoMutex_);
    }
}

void ReconState::initialize() {
    // Clear all arrays
    memset(rfActivity, 0, sizeof(rfActivity));
    
    // Clear repositories (lock if mutex exists, safe for constructor call)
    if (repoMutex_) lock();
    deviceRepo_.clear();
    packetStore_.clear();
    if (repoMutex_) unlock();
    
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
        LOG_ERROR("updateRFActivity: Invalid config index %d (max: %d)", 
                  configIndex, NUM_CONFIGS - 1);
        return;
    }
    
    // Thread safety: Use short lock to protect rfActivity array
    if (!lock(10)) {
        return;  // Skip update rather than block - RF activity is non-critical
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
    
    unlock();
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

// Device management - delegates storage to DeviceRepository, identification stays here
// Thread-safe: Protected by mutex
void ReconState::addTargetableDevice(uint32_t nodeId, uint8_t configIndex, float rssi, 
                                    const char* protocol, const uint8_t* packetData, 
                                    size_t packetLength, uint8_t hopCount) {
    if (nodeId == 0 || configIndex >= NUM_CONFIGS) return;
    
    if (!lock(50)) {  // 50ms timeout - don't block packet processing too long
        Serial.println("[WARN] addTargetableDevice: Failed to acquire lock");
        return;
    }
    
    // Check if device already exists
    TargetableDevice* existing = deviceRepo_.findByNodeId(nodeId);
    if (existing) {
        // Delegate update to repository with hop count for originated/relayed tracking
        deviceRepo_.addOrUpdate(nodeId, configIndex, rssi, protocol, nullptr, 0, hopCount);
        unlock();
        return;
    }
    
    // New device - do identification here, then add via repository
    DeviceIdentification id;
    id.deviceType = "Unknown Device";
    id.firmwareVersion = "Unknown";
    id.isRouter = false;
    id.powerClass = FormatUtils::estimatePowerClass(rssi);
    
    if (packetData && packetLength > 0) {
        id.deviceType = identifyDeviceType(packetData, packetLength, protocol, rssi);
        id.isRouter = isRoutingDevice(packetData, packetLength, protocol);
        id.firmwareVersion = estimateFirmwareVersion(packetData, packetLength, protocol);
    }
    
    // Set up identifier callback with our computed values
    // (DeviceRepository will use this for new devices)
    static DeviceIdentification cachedId;
    cachedId = id;
    deviceRepo_.setDeviceIdentifier([](const uint8_t*, size_t, const char*, float) {
        return cachedId;
    });
    
    TargetableDevice* device = deviceRepo_.addOrUpdate(nodeId, configIndex, rssi, protocol, 
                                                        packetData, packetLength, hopCount);
    
    if (device) {
        // Increment total detections counter when new device is added
        scanState.totalDetections++;
        
        LOG_INFO("New targetable device: 0x%08X (%s) on %s (%.1f dBm)", 
                 nodeId, device->deviceType, getScanConfig(configIndex).protocol, rssi);
    }
    unlock();
}

// Thread-safe: Returns copy to avoid reference-after-unlock race
TargetableDevice ReconState::getTargetableDevice(uint8_t index) const {
    if (!lock(100)) {
        LOG_WARN("getTargetableDevice: Failed to acquire lock");
        return TargetableDevice{};  // Return empty device
    }
    TargetableDevice copy = deviceRepo_.getByIndex(index);  // Copy while locked
    unlock();
    return copy;
}

// Thread-safe: Protected by mutex
// WARNING: Returns mutable pointer - caller MUST NOT hold reference across yield points
// Safe in current design because ESP32 main loop is single-threaded (ISR only sets atomic flag)
TargetableDevice* ReconState::getTargetableDeviceMutable(uint8_t index) {
    if (!lock(100)) {
        LOG_WARN("getTargetableDeviceMutable: Failed to acquire lock");
        return nullptr;
    }
    TargetableDevice* dev = deviceRepo_.getByIndexMutable(index);
    unlock();
    return dev;
}

// Thread-safe: Protected by mutex
// WARNING: Returns pointer that could become stale if repository is modified after unlock.
// This is safe in current ESP32 architecture because:
// 1. Main loop is single-threaded (no preemption between lock/unlock and pointer use)
// 2. ISR only sets atomic flag, never modifies repository directly
// 3. All repository mutations happen in main loop context
// If multi-threading is added, change to return copy or use RAII lock guard pattern.
TargetableDevice* ReconState::findTargetableDevice(uint32_t nodeId) {
    if (!lock(100)) {
        Serial.println("[WARN] findTargetableDevice: Failed to acquire lock");
        return nullptr;
    }
    TargetableDevice* dev = deviceRepo_.findByNodeId(nodeId);
    unlock();
    return dev;
}

// Thread-safe: Protected by mutex
void ReconState::clearTargetableDevices() {
    if (!lock(100)) {
        Serial.println("[WARN] clearTargetableDevices: Failed to acquire lock");
        return;
    }
    deviceRepo_.clear();
    unlock();
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
            return "~v2.2+ (est)";
        }

        // Firmware 2.1+ has longer packets
        if (length > 50) {
            return "~v2.1+ (est)";
        }

        // Short packets = older firmware or beacons
        if (length <= 16) {
            return "~v1.x/beacon (est)";
        }

        return "~v2.0-2.2 (est)";
    }

    if (strcmp(protocol, "LoRaWAN") == 0) {
        return "~LoRaWAN 1.0.x (est)";
    }

    return "Unknown";
}

void ReconState::printStateSummary() const {
    Serial.println("\n=== RECONNAISSANCE STATE SUMMARY ===");
    Serial.printf("Mode: %d, Current Config: %d/%d\n", 
                  scanState.mode, scanState.currentConfig, NUM_CONFIGS);
    Serial.printf("Total packets: %d, Total detections: %d\n",
                  scanState.totalPackets.load(), scanState.totalDetections);
    Serial.printf("Targetable devices: %d\n", deviceRepo_.count());
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

// Packet replay management - delegates to PacketStore
// Thread-safe: Protected by mutex
bool ReconState::capturePacketForReplay(const uint8_t* data, size_t length, uint8_t configIndex,
                                        float rssi, float snr, const char* protocol, const char* decryptedText,
                                        uint32_t nodeId, uint32_t packetId, uint8_t hopCount,
                                        uint32_t destId, uint8_t channel, bool wantAck,
                                        bool viaMqtt, uint8_t priority) {
    if (!lock(50)) {  // 50ms timeout for capture operations
        Serial.println("[WARN] capturePacketForReplay: Failed to acquire lock, dropping packet");
        return false;
    }
    bool result = packetStore_.capturePacket(data, length, configIndex, 
                                       static_cast<int16_t>(rssi), snr,
                                       nodeId, packetId, hopCount,
                                       destId, channel, wantAck, viaMqtt, priority,
                                       protocol, decryptedText);
    unlock();
    return result;
}

// Thread-safe: Returns copy to avoid use-after-unlock race condition
CapturedPacket ReconState::getReplayPacket(uint8_t index) const {
    if (!lock(Config::System::MUTEX_TIMEOUT_MS)) {
        LOG_WARN("getReplayPacket: Failed to acquire lock");
        return CapturedPacket{};
    }
    CapturedPacket pkt = packetStore_.getPacket(index);  // Copy while locked
    unlock();
    return pkt;
}

// Thread-safe: Protected by mutex
void ReconState::clearReplaySlots() {
    if (!lock(Config::System::MUTEX_TIMEOUT_MS)) {
        LOG_WARN("clearReplaySlots: Failed to acquire lock");
        return;
    }
    packetStore_.clear();
    unlock();
}

// Calculate network intelligence statistics
void ReconState::updateNetworkIntel() {
    if (!lock(100)) {
        LOG_WARN("updateNetworkIntel: Failed to acquire lock");
        return;
    }

    networkIntel.activeTransmitters = 0;
    networkIntel.relayOnlyNodes = 0;
    networkIntel.mixedNodes = 0;
    networkIntel.floodingEvents = 0;
    networkIntel.encryptedNetworks = 0;
    networkIntel.beaconDevices = 0;

    const uint8_t numDevices = deviceRepo_.count();
    const uint8_t numPackets = packetStore_.count();
    
    // Analyze transmission patterns from targetable devices
    for (uint8_t i = 0; i < numDevices; i++) {
        const TargetableDevice& dev = deviceRepo_.getByIndex(i);
        
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
    
    for (uint8_t i = 0; i < numPackets; i++) {
        const CapturedPacket& pkt = packetStore_.getPacket(i);
        if (pkt.packetId == 0) continue;
        
        // Check if we've seen this packet ID
        bool found = false;
        for (uint8_t j = 0; j < uniquePacketIds; j++) {
            if (seenPacketIds[j] == pkt.packetId) {
                packetIdCounts[j]++;
                found = true;
                break;
            }
        }
        
        if (!found && uniquePacketIds < Config::Replay::MAX_SLOTS) {
            seenPacketIds[uniquePacketIds] = pkt.packetId;
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
    for (uint8_t i = 0; i < numDevices; i++) {
        const TargetableDevice& dev = deviceRepo_.getByIndex(i);
        // Check if device has packets but we have no successful decryptions
        bool hasEncryptedOnly = true;
        for (uint8_t j = 0; j < numPackets; j++) {
            const CapturedPacket& pkt = packetStore_.getPacket(j);
            if (pkt.nodeId == dev.nodeId) {
                if (pkt.decryptedText[0] != '\0') {
                    hasEncryptedOnly = false;
                    break;
                }
            }
        }
        if (hasEncryptedOnly && dev.packetCount > 0) {
            encryptedDevices++;
        }
    }
    
    // Estimate number of encrypted networks (assume 2-4 devices per network)
    if (encryptedDevices > 0) {
        networkIntel.encryptedNetworks = (encryptedDevices + 2) / 3;  // Rough estimate
    }

    networkIntel.lastUpdate = millis();
    unlock();
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

    // Hold lock across entire check to prevent stale pointer from clearDevices race
    if (!lock(50)) return;
    TargetableDevice* device = deviceRepo_.findByNodeId(nodeId);
    if (!device) { unlock(); return; }
    
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

    unlock();
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
    // Use wall clock time from NTP (time() returns epoch seconds, 0 if not synced)
    time_t now = time(nullptr);
    uint8_t currentHour;

    if (now > 1700000000) {  // NTP synced (after ~2023)
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        currentHour = timeinfo.tm_hour;
    } else {
        // Fallback to uptime hours if NTP not yet synced
        currentHour = (millis() / 3600000) % 24;
    }

    if (currentHour < 24) {
        trafficHist.hourlyPackets[currentHour]++;
    }
    trafficHist.lastHourChange = millis();
}

/**
 * Update temporal metrics for a device (periodicity, beacon detection)
 */
void ReconState::updateDeviceTemporalMetrics(uint32_t nodeId) {
    if (nodeId == 0) return;

    // Hold lock across entire update to prevent stale pointer from clearDevices race
    if (!lock(50)) return;
    TargetableDevice* device = deviceRepo_.findByNodeId(nodeId);
    if (!device) { unlock(); return; }

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

    unlock();
}