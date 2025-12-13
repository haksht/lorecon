/**
 * Device Archiver - SD Card Offload for Memory Management
 * 
 * Offloads inactive devices to SD card when memory fragmentation is high.
 * Keeps recently-seen devices hot in RAM for fast access.
 * 
 * Strategy:
 * - Monitor heap fragmentation
 * - When fragmentation > threshold, archive cold devices
 * - Write device data to SD as JSON
 * - Remove from RAM arrays
 * - Optionally reload if device reappears
 */

#ifndef DEVICE_ARCHIVER_H
#define DEVICE_ARCHIVER_H

#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "data_structures.h"
#include "config.h"

struct ArchiveStats {
    uint32_t totalArchived;
    uint32_t totalRestored;
    uint32_t lastArchiveTime;
    uint32_t bytesFreed;
    float fragmentationBefore;
    float fragmentationAfter;
    
    ArchiveStats() 
        : totalArchived(0)
        , totalRestored(0)
        , lastArchiveTime(0)
        , bytesFreed(0)
        , fragmentationBefore(0.0f)
        , fragmentationAfter(0.0f) {}
};

class DeviceArchiver {
public:
    DeviceArchiver();
    
    // Check if archival is needed and perform if necessary
    bool checkAndArchive(TargetableDevice* devices, uint8_t& deviceCount,
                         TrackedNode* hotNodes, uint8_t& hotNodeCount);
    
    // Manual archival trigger
    bool archiveInactiveDevices(TargetableDevice* devices, uint8_t& deviceCount,
                                TrackedNode* hotNodes, uint8_t& hotNodeCount);
    
    // Attempt to restore device from archive if it reappears
    bool restoreDevice(uint32_t nodeId, TargetableDevice& device);
    
    // Aggressive rotation when no SD (drop oldest devices)
    bool rotateDevicesNoSD(TargetableDevice* devices, uint8_t& deviceCount,
                           uint8_t maxDevices, float fragmentationThreshold = 30.0f);
    
    // Get statistics
    const ArchiveStats& getStats() const { return stats; }
    void printStats() const;
    
    // Utility
    bool isSDAvailable() const;
    float getCurrentFragmentation() const;
    
private:
    ArchiveStats stats;
    bool sdInitialized;
    
    // Helper functions
    void initializeSD();
    bool shouldArchive() const;
    uint8_t identifyColdDevices(const TargetableDevice* devices, uint8_t deviceCount,
                                uint8_t* coldIndices, uint8_t maxCold) const;
    bool writeDeviceToArchive(const TargetableDevice& device);
    bool writeNodeToArchive(const TrackedNode& node);
    bool compactDeviceArray(TargetableDevice* devices, uint8_t& deviceCount,
                           const uint8_t* removeIndices, uint8_t removeCount);
    bool compactNodeArray(TrackedNode* nodes, uint8_t& nodeCount,
                         const uint8_t* removeIndices, uint8_t removeCount);
    void logArchiveOperation(uint8_t archivedCount, float fragBefore, float fragAfter);
};

#endif // DEVICE_ARCHIVER_H
