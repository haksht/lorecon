/**
 * Device Archiver - SD Card Offload for Memory Management
 * 
 * STATUS: Ready for use with repository pattern (refactored Dec 2025)
 * REQUIRES: SD card hardware for full functionality; works without SD in rotation-only mode
 * 
 * Purpose:
 * Manages memory pressure during long reconnaissance sessions by:
 * 1. Monitoring heap fragmentation
 * 2. Archiving "cold" (inactive) devices to SD card when fragmentation exceeds threshold
 * 3. Rotating out oldest devices when SD unavailable (graceful degradation)
 * 4. Optionally restoring devices from archive if they reappear
 * 
 * Integration:
 * - Uses DeviceRepository and NodeTracker via their public interfaces
 * - Called periodically from LoRaReconTool::update() (every 5 minutes)
 * - SD archival writes JSON Lines format to /recon/archive.jsonl
 * 
 * Memory Strategy:
 * - Fragmentation threshold: 30% (configurable via Config::Archiver)
 * - Inactivity threshold: 10 minutes (device not seen)
 * - Minimum active devices: 5 (always keep some devices in RAM)
 * - No-SD rotation: drops oldest 30% when memory critical
 * 
 * File Format (archive.jsonl):
 * {"type":"device","nodeId":123456,"protocol":"Meshtastic",...,"archivedAt":12345678}
 * {"type":"node","nodeId":789012,"protocol":"LoRaWAN",...,"archivedAt":12345679}
 * 
 * Future Enhancements:
 * - TODO: Implement restoreDevice() for full round-trip archival
 * - TODO: Add archive file rotation/cleanup for long sessions
 * - TODO: Consider LRU cache for recently archived devices
 */

#ifndef DEVICE_ARCHIVER_H
#define DEVICE_ARCHIVER_H

#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "data_structures.h"
#include "config.h"

// Forward declarations for repository pattern
class DeviceRepository;
class NodeTracker;

/**
 * Statistics tracking for archival operations
 */
struct ArchiveStats {
    uint32_t totalArchived;      // Total devices written to SD
    uint32_t totalRotated;       // Total devices dropped (no-SD mode)
    uint32_t totalRestored;      // Total devices restored from archive
    uint32_t lastArchiveTime;    // millis() of last archive operation
    uint32_t bytesFreed;         // Estimated bytes freed (last operation)
    float fragmentationBefore;   // Fragmentation % before last operation
    float fragmentationAfter;    // Fragmentation % after last operation
    
    ArchiveStats() 
        : totalArchived(0)
        , totalRotated(0)
        , totalRestored(0)
        , lastArchiveTime(0)
        , bytesFreed(0)
        , fragmentationBefore(0.0f)
        , fragmentationAfter(0.0f) {}
};

/**
 * DeviceArchiver - Memory management via SD card offload
 * 
 * Usage:
 *   DeviceArchiver archiver;
 *   // In periodic update (every 5 min):
 *   archiver.checkAndArchive(deviceRepo, nodeTracker);
 */
class DeviceArchiver {
public:
    DeviceArchiver();
    
    //--------------------------------------------------------------------------
    // Primary API (Repository Pattern)
    //--------------------------------------------------------------------------
    
    /**
     * Check fragmentation and archive if needed
     * @param deviceRepo Reference to device repository
     * @param nodeTracker Reference to node tracker
     * @return true if archival was performed
     */
    bool checkAndArchive(DeviceRepository& deviceRepo, NodeTracker& nodeTracker);
    
    /**
     * Force archival of inactive devices (manual trigger)
     * @param deviceRepo Reference to device repository
     * @param nodeTracker Reference to node tracker  
     * @return true if any devices were archived
     */
    bool archiveInactiveDevices(DeviceRepository& deviceRepo, NodeTracker& nodeTracker);
    
    /**
     * Attempt to restore device from archive
     * @param nodeId Device ID to restore
     * @param device Output: restored device data
     * @return true if device was found and restored
     * @note Not yet implemented - returns false
     */
    bool restoreDevice(uint32_t nodeId, TargetableDevice& device);
    
    //--------------------------------------------------------------------------
    // Statistics & Diagnostics
    //--------------------------------------------------------------------------
    
    const ArchiveStats& getStats() const { return stats; }
    void printStats() const;
    bool isSDAvailable() const;
    float getCurrentFragmentation() const;
    
private:
    ArchiveStats stats;
    bool sdInitialized;
    
    // SD card operations
    void initializeSD();
    bool shouldArchive() const;
    
    // Archive write operations
    bool writeDeviceToArchive(const TargetableDevice& device);
    bool writeNodeToArchive(const TrackedNode& node);
    void logArchiveOperation(uint8_t archivedCount, float fragBefore, float fragAfter);
    
    // No-SD fallback: rotation instead of archival
    bool rotateOldestDevices(DeviceRepository& deviceRepo, uint8_t removeCount);
    bool rotateOldestNodes(NodeTracker& nodeTracker, uint8_t removeCount);
};

#endif // DEVICE_ARCHIVER_H
