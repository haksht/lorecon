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
    size_t packetLength
) {
    if (nodeId == 0) return nullptr;
    
    // Try to find existing device
    TargetableDevice* device = findByNodeId(nodeId);
    
    if (device) {
        // Update existing device
        updateExistingDevice(device, configIndex, rssi);
        return device;
    }
    
    // Create new device if we have capacity
    if (deviceCount >= Config::Tracking::MAX_DEVICES) {
        LOG_WARN("Device repository full, cannot add node 0x%08X", nodeId);
        return nullptr;
    }
    
    device = &devices[deviceCount++];
    initializeNewDevice(device, nodeId, configIndex, rssi, protocol, packetData, packetLength);
    
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
    size_t packetLength
) {
    memset(device, 0, sizeof(TargetableDevice));
    
    device->nodeId = nodeId;
    device->configIndex = configIndex;
    device->packetCount = 1;
    device->avgRSSI = rssi;
    device->bestRSSI = rssi;
    device->rssiStdDev = 0.0f;
    device->rssiM2 = 0.0f;
    device->avgPacketInterval = 0;
    device->lastPacketInterval = 0;
    device->periodicityScore = 0;
    device->originatedPackets = 0;
    device->relayedPackets = 0;
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
    float rssi
) {
    // Cap packetCount at UINT16_MAX to prevent overflow
    if (device->packetCount < UINT16_MAX) {
        device->packetCount++;
    }
    
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
