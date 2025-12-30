/**
 * Device Archiver - SD Card Offload for Memory Management
 * 
 * STATUS: Production ready (Dec 2025)
 * REQUIRES: SD card hardware for full functionality; works without SD in rotation-only mode
 * 
 * Purpose:
 * Manages memory pressure during long reconnaissance sessions (48+ hours) by:
 * 1. Monitoring heap fragmentation
 * 2. Archiving "cold" (inactive) devices to SD card when fragmentation exceeds threshold
 * 3. Rotating out oldest devices when SD unavailable (graceful degradation)
 * 4. Restoring devices from archive when they reappear (preserves packet history)
 * 5. Rotating archive files when they exceed 512KB (prevents SD fill during long sessions)
 * 
 * Integration:
 * - Uses DeviceRepository via its public interface
 * - Called periodically from LoRaReconTool::update() (every 5 minutes)
 * - SD archival writes JSON Lines format to /recon/devices_archive.jsonl
 * - Call tryRestoreOnPacket() from packet processor when new device seen
 * 
 * Memory Strategy:
 * - Fragmentation threshold: 35% (configurable via Config::Archiver)
 * - Inactivity threshold: 5 minutes (device not seen)
 * - Minimum active devices: 5 (always keep some devices in RAM)
 * - No-SD rotation: drops oldest 20% when memory critical
 * 
 * File Format (archive.jsonl):
 * {"type":"device","nodeId":123456,"protocol":"Meshtastic",...,"archivedAt":12345678}
 * 
 * Archive Rotation:
 * - Archive files rotated when exceeding MAX_ARCHIVE_SIZE_BYTES (512KB)
 * - Keeps up to 3 backup files: devices_archive.1.jsonl, .2.jsonl, .3.jsonl
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
 *   archiver.checkAndArchive(deviceRepo);
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
     * @return true if archival was performed
     */
    bool checkAndArchive(DeviceRepository& deviceRepo);
    
    /**
     * Force archival of inactive devices (manual trigger)
     * @param deviceRepo Reference to device repository
     * @return true if any devices were archived
     */
    bool archiveInactiveDevices(DeviceRepository& deviceRepo);
    
    /**
     * Attempt to restore device from archive
     * @param nodeId Device ID to restore
     * @param device Output: restored device data
     * @return true if device was found and restored
     */
    bool restoreDevice(uint32_t nodeId, TargetableDevice& device);
    
    /**
     * Try to restore a device when we see a packet from an unknown nodeId
     * Call this from packet processor when a new device is seen.
     * If device was previously archived, restores its history.
     * @param nodeId Device ID from packet
     * @param deviceRepo Repository to restore into
     * @return Pointer to restored device, or nullptr if not in archive
     */
    TargetableDevice* tryRestoreOnPacket(uint32_t nodeId, DeviceRepository& deviceRepo);
    
    /**
     * Rotate archive file if it exceeds size limit
     * Creates backup and starts fresh file for long sessions
     * @return true if rotation occurred
     */
    bool rotateArchiveIfNeeded();
    
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
    void logArchiveOperation(uint8_t archivedCount, float fragBefore, float fragAfter);
    
    // No-SD fallback: rotation instead of archival
    bool rotateOldestDevices(DeviceRepository& deviceRepo, uint8_t removeCount);
};

#endif // DEVICE_ARCHIVER_H
