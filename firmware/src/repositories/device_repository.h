/**
 * Device Repository
 * 
 * Encapsulates all targetable device storage and retrieval operations.
 * Part of Phase 1 refactoring to implement Repository pattern.
 * 
 * Responsibilities:
 * - Store and retrieve TargetableDevice records
 * - Handle device lookup by nodeId or index
 * - Manage device lifecycle (add, update, clear)
 * - Provide iteration interface
 * 
 * Does NOT handle:
 * - JSON serialization (that's the API layer's job)
 * - Mode management (that's CaptureService's job)
 * - Device type identification (delegated to helper)
 */

#ifndef DEVICE_REPOSITORY_H
#define DEVICE_REPOSITORY_H

#include <Arduino.h>
#include "../data_structures.h"
#include "../config.h"

/**
 * Device identification helper - passed to repository for new device setup
 */
struct DeviceIdentification {
    const char* deviceType;
    const char* firmwareVersion;
    bool isRouter;
    uint8_t powerClass;
};

/**
 * Callback for device type identification
 * Returns identification info based on packet data
 */
typedef DeviceIdentification (*DeviceIdentifierFunc)(
    const uint8_t* packetData, 
    size_t packetLength, 
    const char* protocol, 
    float rssi
);

class DeviceRepository {
public:
    DeviceRepository();
    
    /**
     * Set the device identifier callback (for device type detection)
     */
    void setDeviceIdentifier(DeviceIdentifierFunc identifier);
    
    /**
     * Add or update a device
     * @param nodeId Unique device identifier
     * @param configIndex Radio configuration index
     * @param rssi Signal strength
     * @param protocol Protocol name
     * @param packetData Optional packet data for device identification
     * @param packetLength Length of packet data
     * @return Pointer to the device (new or updated), nullptr if storage full
     */
    TargetableDevice* addOrUpdate(
        uint32_t nodeId, 
        uint8_t configIndex, 
        float rssi,
        const char* protocol,
        const uint8_t* packetData = nullptr,
        size_t packetLength = 0
    );
    
    /**
     * Find device by node ID
     * @return Pointer to device or nullptr if not found
     */
    TargetableDevice* findByNodeId(uint32_t nodeId);
    const TargetableDevice* findByNodeId(uint32_t nodeId) const;
    
    /**
     * Find device index by node ID
     * @return Index or UINT8_MAX if not found
     */
    uint8_t findIndexByNodeId(uint32_t nodeId) const;
    
    /**
     * Get device by index (bounds-checked)
     * @return Reference to device or empty device if out of bounds
     */
    const TargetableDevice& getByIndex(uint8_t index) const;
    TargetableDevice* getByIndexMutable(uint8_t index);
    
    /**
     * Get current device count
     */
    uint8_t count() const { return deviceCount; }
    
    /**
     * Check if repository has any devices
     */
    bool isEmpty() const { return deviceCount == 0; }
    
    /**
     * Check if repository is at capacity
     */
    bool isFull() const { return deviceCount >= Config::Tracking::MAX_DEVICES; }
    
    /**
     * Get maximum capacity
     */
    uint8_t capacity() const { return Config::Tracking::MAX_DEVICES; }
    
    /**
     * Clear all devices
     */
    void clear();
    
    /**
     * Initialize/reset the repository
     */
    void initialize();
    
    /**
     * Iteration support - use with range-based for loops
     * Example: for (const auto& device : repository) { ... }
     */
    const TargetableDevice* begin() const { return devices; }
    const TargetableDevice* end() const { return devices + deviceCount; }
    TargetableDevice* begin() { return devices; }
    TargetableDevice* end() { return devices + deviceCount; }

private:
    TargetableDevice devices[Config::Tracking::MAX_DEVICES];
    uint8_t deviceCount;
    DeviceIdentifierFunc deviceIdentifier;
    
    // Empty device returned for invalid index access
    static TargetableDevice emptyDevice;
    static bool emptyDeviceInitialized;
    
    /**
     * Initialize a new device entry
     */
    void initializeNewDevice(
        TargetableDevice* device,
        uint32_t nodeId,
        uint8_t configIndex,
        float rssi,
        const char* protocol,
        const uint8_t* packetData,
        size_t packetLength
    );
    
    /**
     * Update existing device with new packet data
     */
    void updateExistingDevice(
        TargetableDevice* device,
        uint8_t configIndex,
        float rssi
    );
};

#endif // DEVICE_REPOSITORY_H
