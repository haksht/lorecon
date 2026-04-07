/**
 * Device Repository Implementation
 * 
 * Manages storage and retrieval of targetable devices.
 */

#include "device_repository.h"
#include "../utils/format_utils.h"
#include "../logger.h"
#include <cstring>
#include <cmath>

// Static member initialization
TargetableDevice DeviceRepository::emptyDevice;
bool DeviceRepository::emptyDeviceInitialized = false;

DeviceRepository::DeviceRepository() 
    : deviceCount(0)
    , deviceIdentifier(nullptr)
{
    initialize();
}

void DeviceRepository::setDeviceIdentifier(DeviceIdentifierFunc identifier) {
    deviceIdentifier = identifier;
}

void DeviceRepository::initialize() {
    deviceCount = 0;
    memset(devices, 0, sizeof(devices));
    
    // Initialize empty device once
    if (!emptyDeviceInitialized) {
        memset(&emptyDevice, 0, sizeof(emptyDevice));
        strncpy(emptyDevice.deviceType, "Unknown", sizeof(emptyDevice.deviceType) - 1);
        emptyDevice.deviceType[sizeof(emptyDevice.deviceType) - 1] = '\0';
        strncpy(emptyDevice.firmwareVersion, "Unknown", sizeof(emptyDevice.firmwareVersion) - 1);
        emptyDevice.firmwareVersion[sizeof(emptyDevice.firmwareVersion) - 1] = '\0';
        emptyDeviceInitialized = true;
    }
}

void DeviceRepository::clear() {
    deviceCount = 0;
    memset(devices, 0, sizeof(devices));
}

TargetableDevice* DeviceRepository::addOrUpdate(
    uint32_t nodeId, 
    uint8_t configIndex, 
    float rssi,
    const char* protocol,
    const uint8_t* packetData,
    size_t packetLength,
    uint8_t hopCount
) {
    if (nodeId == 0) return nullptr;
    
    // Try to find existing device
    TargetableDevice* device = findByNodeId(nodeId);
    
    if (device) {
        // Update existing device
        updateExistingDevice(device, configIndex, rssi, hopCount);
        return device;
    }
    
    // Create new device if we have capacity
    if (deviceCount >= Config::Tracking::MAX_DEVICES) {
        // Try to evict oldest inactive device to make room
        if (!evictOldestInactive()) {
            LOG_WARN("Device repository full, cannot add node 0x%08X", nodeId);
            return nullptr;
        }
    }
    
    device = &devices[deviceCount++];
    initializeNewDevice(device, nodeId, configIndex, rssi, protocol, packetData, packetLength, hopCount);
    
    LOG_INFO("[DEVICE] New: 0x%08X (%s) %.1f dBm", nodeId, device->deviceType, rssi);
    
    return device;
}

void DeviceRepository::initializeNewDevice(
    TargetableDevice* device,
    uint32_t nodeId,
    uint8_t configIndex,
    float rssi,
    const char* protocol,
    const uint8_t* packetData,
    size_t packetLength,
    uint8_t hopCount
) {
    memset(device, 0, sizeof(TargetableDevice));
    
    device->nodeId = nodeId;
    device->configIndex = configIndex;
    device->packetCount = 1;
    device->avgRSSI = rssi;
    device->bestRSSI = rssi;
    device->lastRSSI = rssi;
    device->rssiStdDev = 0.0f;
    device->rssiM2 = 0.0f;
    device->avgPacketInterval = 0;
    device->lastPacketInterval = 0;
    device->periodicityScore = 0;
    device->batteryLevel = -1;
    device->batteryVoltage = 0.0f;
    device->lastSNR = 0.0f;
    device->meshCoreChannel[0] = '\0';
    device->senderName[0] = '\0';
    device->lastMessage[0] = '\0';
    
    // Track first packet as originated or relayed based on hop count.
    // Meshtastic: countdown (3=fresh, decrements per relay). hopCount==maxHopCount → originated.
    // MeshCore:   upcount  (0=fresh, increments per relay). hopCount==0 → originated.
    // Others (LoRaWAN, RadioHead): no hop field; treat as originated.
    if (hopCount != 0xFF) {
        bool isMeshCore = (protocol && strcmp(protocol, "MeshCore") == 0);
        if (isMeshCore) {
            device->maxHopCount = hopCount;
            if (hopCount == 0) {
                device->originatedPackets = 1;
                device->relayedPackets = 0;
            } else {
                device->originatedPackets = 0;
                device->relayedPackets = 1;
            }
        } else {
            // Meshtastic and others: first packet baseline → originated
            device->maxHopCount = hopCount;
            device->originatedPackets = 1;
            device->relayedPackets = 0;
        }
    } else {
        device->maxHopCount = 0;
        device->originatedPackets = 0;
        device->relayedPackets = 0;
    }
    
    device->firstSeen = millis();
    device->lastSeen = millis();
    device->powerClass = FormatUtils::estimatePowerClass(rssi);
    device->isRouter = false;
    
    // Copy protocol
    if (protocol) {
        strncpy(device->protocol, protocol, sizeof(device->protocol) - 1);
        device->protocol[sizeof(device->protocol) - 1] = '\0';
    }
    
    // Default device type
    strncpy(device->deviceType, "Unknown Device", sizeof(device->deviceType) - 1);
    device->deviceType[sizeof(device->deviceType) - 1] = '\0';
    
    // Default firmware version
    strncpy(device->firmwareVersion, "Unknown", sizeof(device->firmwareVersion) - 1);
    device->firmwareVersion[sizeof(device->firmwareVersion) - 1] = '\0';
    
    // Use device identifier callback if available
    if (deviceIdentifier && packetData && packetLength > 0) {
        DeviceIdentification id = deviceIdentifier(packetData, packetLength, protocol, rssi);
        
        if (id.deviceType) {
            strncpy(device->deviceType, id.deviceType, sizeof(device->deviceType) - 1);
            device->deviceType[sizeof(device->deviceType) - 1] = '\0';
        }
        if (id.firmwareVersion) {
            strncpy(device->firmwareVersion, id.firmwareVersion, sizeof(device->firmwareVersion) - 1);
            device->firmwareVersion[sizeof(device->firmwareVersion) - 1] = '\0';
        }
        device->isRouter = id.isRouter;
        device->powerClass = id.powerClass;
    }
}

void DeviceRepository::updateExistingDevice(
    TargetableDevice* device,
    uint8_t configIndex,
    float rssi,
    uint8_t hopCount
) {
    // Cap packetCount at UINT32_MAX to prevent overflow (Welford's freezes after saturation)
    if (device->packetCount < UINT32_MAX) {
        device->packetCount++;
    }

    // Track originated vs relayed packets based on hop count.
    // Protocols differ in hop count semantics:
    //   Meshtastic: countdown — fresh packet has hop_count = hop_limit (e.g. 3),
    //               each relay decrements it. hopCount >= maxHopCount → originated.
    //   MeshCore:   upcount  — fresh packet has hop_count = 0 (no path entries),
    //               each relay appends its hash and increments it. hopCount == 0 → originated.
    //   Others:     no hop field (hopCount == 0xFF); skip classification.
    if (hopCount != 0xFF) {
        bool isMeshCore = (strcmp(device->protocol, "MeshCore") == 0);
        if (isMeshCore) {
            // MeshCore upcount: 0 = direct from originator, >0 = traversed N relays
            if (hopCount > device->maxHopCount) {
                device->maxHopCount = hopCount;
            }
            if (hopCount == 0) {
                if (device->originatedPackets < UINT16_MAX) device->originatedPackets++;
            } else {
                if (device->relayedPackets < UINT16_MAX) device->relayedPackets++;
                if (!device->isRouter && device->relayedPackets >= 2) {
                    device->isRouter = true;
                }
            }
        } else {
            // Meshtastic countdown: hopCount >= maxHopCount means full hop_limit → originated
            if (hopCount > device->maxHopCount) {
                device->maxHopCount = hopCount;
            }
            if (hopCount >= device->maxHopCount) {
                if (device->originatedPackets < UINT16_MAX) device->originatedPackets++;
            } else {
                if (device->relayedPackets < UINT16_MAX) device->relayedPackets++;
                if (!device->isRouter && device->relayedPackets >= 2) {
                    device->isRouter = true;
                }
            }
        }
    }
    
    // Always record the most recent RSSI
    device->lastRSSI = rssi;

    // Update RSSI statistics with running variance calculation (Welford's algorithm)
    float oldAvg = device->avgRSSI;
    device->avgRSSI = oldAvg + (rssi - oldAvg) / device->packetCount;
    
    // Update RSSI standard deviation
    if (device->packetCount > 1) {
        float delta = rssi - oldAvg;
        float delta2 = rssi - device->avgRSSI;
        device->rssiM2 += delta * delta2;
        device->rssiStdDev = sqrt(fabs(device->rssiM2) / (device->packetCount - 1));
    }
    
    // Update best RSSI and associated config
    if (rssi > device->bestRSSI) {
        device->bestRSSI = rssi;
        device->configIndex = configIndex;
    }
    
    device->lastSeen = millis();
}

TargetableDevice* DeviceRepository::findByNodeId(uint32_t nodeId) {
    for (uint8_t i = 0; i < deviceCount; i++) {
        if (devices[i].nodeId == nodeId) {
            return &devices[i];
        }
    }
    return nullptr;
}

const TargetableDevice* DeviceRepository::findByNodeId(uint32_t nodeId) const {
    for (uint8_t i = 0; i < deviceCount; i++) {
        if (devices[i].nodeId == nodeId) {
            return &devices[i];
        }
    }
    return nullptr;
}

uint8_t DeviceRepository::findIndexByNodeId(uint32_t nodeId) const {
    for (uint8_t i = 0; i < deviceCount; i++) {
        if (devices[i].nodeId == nodeId) {
            return i;
        }
    }
    return UINT8_MAX;
}

const TargetableDevice& DeviceRepository::getByIndex(uint8_t index) const {
    if (index >= deviceCount) {
        return emptyDevice;
    }
    return devices[index];
}

TargetableDevice* DeviceRepository::getByIndexMutable(uint8_t index) {
    if (index >= deviceCount) {
        return nullptr;
    }
    return &devices[index];
}

bool DeviceRepository::removeByNodeId(uint32_t nodeId) {
    uint8_t index = findIndexByNodeId(nodeId);
    if (index == UINT8_MAX) {
        return false;  // Not found
    }
    
    // Shift remaining devices down to fill the gap
    for (uint8_t i = index; i < deviceCount - 1; i++) {
        devices[i] = devices[i + 1];
    }
    
    deviceCount--;
    
    // Clear the last slot
    memset(&devices[deviceCount], 0, sizeof(TargetableDevice));
    
    return true;
}

bool DeviceRepository::evictOldestInactive() {
    if (deviceCount == 0) return false;
    
    uint32_t now = millis();
    uint32_t oldestTime = UINT32_MAX;
    uint8_t oldestIndex = UINT8_MAX;
    
    // Find the device with the oldest lastSeen timestamp
    for (uint8_t i = 0; i < deviceCount; i++) {
        if (devices[i].lastSeen < oldestTime) {
            oldestTime = devices[i].lastSeen;
            oldestIndex = i;
        }
    }
    
    if (oldestIndex == UINT8_MAX) return false;
    
    uint32_t inactiveMs = now - oldestTime;
    uint32_t nodeId = devices[oldestIndex].nodeId;
    
    LOG_INFO("[DEVICE] Evicting 0x%08X (inactive %lu ms) to make room", nodeId, inactiveMs);
    
    // Shift remaining devices down
    for (uint8_t i = oldestIndex; i < deviceCount - 1; i++) {
        devices[i] = devices[i + 1];
    }
    
    deviceCount--;
    memset(&devices[deviceCount], 0, sizeof(TargetableDevice));
    
    return true;
}
