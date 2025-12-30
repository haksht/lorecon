/**
 * Device Archiver Implementation
 * 
 * Refactored Dec 2025 to use repository pattern instead of raw arrays.
 * See device_archiver.h for full documentation.
 */

#include "device_archiver.h"
#include "repositories/device_repository.h"
#include "logger.h"
#include "utils/sd_utils.h"
#include <esp_heap_caps.h>
#include <ArduinoJson.h>
#include <vector>
#include <algorithm>

DeviceArchiver::DeviceArchiver() 
    : stats()
    , sdInitialized(false) {
}

void DeviceArchiver::initializeSD() {
    if (sdInitialized) return;
    
    if (SDUtils::initialize()) {
        sdInitialized = true;
        LOG_INFO("[ARCHIVE] SD card initialized for device archival");
        SDUtils::ensureDirectory("/recon");
    } else {
        LOG_WARN("[ARCHIVE] SD card not available - using rotation-only mode");
    }
}

bool DeviceArchiver::isSDAvailable() const {
    return sdInitialized || SDUtils::isAvailable();
}

float DeviceArchiver::getCurrentFragmentation() const {
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t maxBlock = ESP.getMaxAllocHeap();
    
    if (freeHeap == 0) return 0.0f;
    
    // Fragmentation = how much of free heap is unusable due to fragmentation
    float fragmentation = 100.0f * (1.0f - (float)maxBlock / (float)freeHeap);
    return fragmentation;
}

bool DeviceArchiver::shouldArchive() const {
    float frag = getCurrentFragmentation();
    return frag >= Config::Archiver::FRAGMENTATION_THRESHOLD;
}

bool DeviceArchiver::checkAndArchive(DeviceRepository& deviceRepo) {
    if (!shouldArchive()) {
        return false;  // Fragmentation is acceptable
    }
    
    LOG_INFO("[ARCHIVE] Fragmentation %.1f%% exceeds threshold %.1f%%",
             getCurrentFragmentation(), Config::Archiver::FRAGMENTATION_THRESHOLD);
    
    initializeSD();
    
    if (isSDAvailable()) {
        return archiveInactiveDevices(deviceRepo);
    } else {
        // No SD card - use rotation to free memory
        LOG_INFO("[ARCHIVE] No SD - rotating oldest devices to free memory");
        
        // Remove ~20% of devices to relieve memory pressure
        uint8_t deviceRemoveCount = max((uint8_t)2, (uint8_t)(deviceRepo.count() / 5));
        
        return rotateOldestDevices(deviceRepo, deviceRemoveCount);
    }
}

bool DeviceArchiver::archiveInactiveDevices(DeviceRepository& deviceRepo) {
    // Check if we need to rotate the archive file first
    rotateArchiveIfNeeded();
    
    float fragBefore = getCurrentFragmentation();
    uint32_t heapBefore = ESP.getFreeHeap();
    uint32_t now = millis();
    
    // Keep at least MIN_ACTIVE_DEVICES in RAM
    if (deviceRepo.count() <= Config::Archiver::MIN_ACTIVE_DEVICES) {
        LOG_INFO("[ARCHIVE] Only %d devices - skipping archival", deviceRepo.count());
        return false;
    }
    
    // Find inactive devices (not seen for DEVICE_INACTIVITY_MS)
    // We need to identify them first, then remove (can't modify while iterating)
    std::vector<uint32_t> coldDeviceIds;
    coldDeviceIds.reserve(deviceRepo.count());
    
    for (uint8_t i = 0; i < deviceRepo.count(); i++) {
        const TargetableDevice& device = deviceRepo.getByIndex(i);
        uint32_t inactiveMs = now - device.lastSeen;
        
        if (inactiveMs > Config::Archiver::DEVICE_INACTIVITY_MS) {
            coldDeviceIds.push_back(device.nodeId);
        }
    }
    
    // Keep minimum devices even if inactive
    size_t maxToArchive = deviceRepo.count() - Config::Archiver::MIN_ACTIVE_DEVICES;
    if (coldDeviceIds.size() > maxToArchive) {
        coldDeviceIds.resize(maxToArchive);
    }
    
    if (coldDeviceIds.empty()) {
        LOG_INFO("[ARCHIVE] No inactive devices to archive");
        return false;
    }
    
    LOG_INFO("[ARCHIVE] Archiving %d inactive devices...", coldDeviceIds.size());
    
    // Write devices to SD and remove from repository
    uint8_t archived = 0;
    for (uint32_t nodeId : coldDeviceIds) {
        const TargetableDevice* device = deviceRepo.findByNodeId(nodeId);
        if (device && writeDeviceToArchive(*device)) {
            archived++;
        }
    }
    
    // Remove archived devices from repository
    for (uint32_t nodeId : coldDeviceIds) {
        deviceRepo.removeByNodeId(nodeId);
    }
    
    // Update stats
    uint32_t heapAfter = ESP.getFreeHeap();
    float fragAfter = getCurrentFragmentation();
    
    stats.totalArchived += archived;
    stats.lastArchiveTime = now;
    stats.bytesFreed = (heapAfter > heapBefore) ? (heapAfter - heapBefore) : 0;
    stats.fragmentationBefore = fragBefore;
    stats.fragmentationAfter = fragAfter;
    
    LOG_INFO("[ARCHIVE] ✓ Archived %d devices", archived);
    LOG_INFO("[ARCHIVE] Fragmentation: %.1f%% → %.1f%% (freed %u bytes)", 
             fragBefore, fragAfter, stats.bytesFreed);
    
    logArchiveOperation(archived, fragBefore, fragAfter);
    
    return archived > 0;
}

bool DeviceArchiver::rotateOldestDevices(DeviceRepository& deviceRepo, uint8_t removeCount) {
    if (deviceRepo.count() <= Config::Archiver::MIN_ACTIVE_DEVICES) {
        return false;  // Don't remove below minimum
    }
    
    // Cap removal to not go below minimum
    uint8_t maxRemove = deviceRepo.count() - Config::Archiver::MIN_ACTIVE_DEVICES;
    removeCount = min(removeCount, maxRemove);
    
    if (removeCount == 0) return false;
    
    float fragBefore = getCurrentFragmentation();
    
    // Find oldest devices by lastSeen
    struct DeviceAge {
        uint32_t nodeId;
        uint32_t lastSeen;
    };
    
    std::vector<DeviceAge> ages;
    ages.reserve(deviceRepo.count());
    
    for (uint8_t i = 0; i < deviceRepo.count(); i++) {
        const TargetableDevice& dev = deviceRepo.getByIndex(i);
        ages.push_back({dev.nodeId, dev.lastSeen});
    }
    
    // Sort by lastSeen (oldest first)
    std::sort(ages.begin(), ages.end(), [](const DeviceAge& a, const DeviceAge& b) {
        return a.lastSeen < b.lastSeen;
    });
    
    // Remove oldest N
    uint32_t now = millis();
    for (uint8_t i = 0; i < removeCount && i < ages.size(); i++) {
        uint32_t inactiveMs = now - ages[i].lastSeen;
        LOG_INFO("[ARCHIVE] Rotating out device 0x%08X (inactive %lu ms)", 
                ages[i].nodeId, inactiveMs);
        deviceRepo.removeByNodeId(ages[i].nodeId);
    }
    
    float fragAfter = getCurrentFragmentation();
    stats.totalRotated += removeCount;
    
    LOG_INFO("[ARCHIVE] ✓ Rotated %d devices (frag %.1f%% → %.1f%%)",
            removeCount, fragBefore, fragAfter);
    
    return true;
}

bool DeviceArchiver::writeDeviceToArchive(const TargetableDevice& device) {
    File archive = SD.open(Config::Archiver::ARCHIVE_FILE, FILE_APPEND);
    if (!archive) {
        LOG_ERROR("[ARCHIVE] Failed to open archive file");
        return false;
    }
    
    JsonDocument doc;
    
    doc["type"] = "device";
    doc["nodeId"] = device.nodeId;
    doc["configIndex"] = device.configIndex;
    doc["bestRSSI"] = device.bestRSSI;
    doc["avgRSSI"] = device.avgRSSI;
    doc["packetCount"] = device.packetCount;
    doc["lastSeen"] = device.lastSeen;
    doc["firstSeen"] = device.firstSeen;
    doc["protocol"] = device.protocol;
    doc["deviceType"] = device.deviceType;
    doc["firmwareVersion"] = device.firmwareVersion;
    doc["isRouter"] = device.isRouter;
    doc["powerClass"] = device.powerClass;
    doc["archivedAt"] = millis();
    
    serializeJson(doc, archive);
    archive.println();  // Newline separator (JSON Lines format)
    archive.close();
    
    return true;
}

void DeviceArchiver::logArchiveOperation(uint8_t archivedCount, float fragBefore, float fragAfter) {
    File statsFile = SD.open(Config::Archiver::STATS_FILE, FILE_APPEND);
    if (!statsFile) return;
    
    JsonDocument doc;
    doc["timestamp"] = millis();
    doc["archived"] = archivedCount;
    doc["fragBefore"] = fragBefore;
    doc["fragAfter"] = fragAfter;
    doc["heapFree"] = ESP.getFreeHeap();
    
    serializeJson(doc, statsFile);
    statsFile.println();
    statsFile.close();
}

bool DeviceArchiver::restoreDevice(uint32_t nodeId, TargetableDevice& device) {
    if (!isSDAvailable()) {
        return false;  // Can't restore without SD
    }
    
    File archive = SD.open(Config::Archiver::ARCHIVE_FILE, FILE_READ);
    if (!archive) {
        return false;  // No archive file
    }
    
    bool found = false;
    String line;
    
    // Scan JSON Lines file for matching nodeId
    while (archive.available() && !found) {
        line = archive.readStringUntil('\n');
        if (line.length() == 0) continue;
        
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, line);
        if (err) continue;
        
        // Check if this is a device record with matching nodeId
        if (doc["type"] == "device" && doc["nodeId"] == nodeId) {
            // Restore device fields
            device.nodeId = nodeId;
            device.configIndex = doc["configIndex"] | 0;
            device.bestRSSI = doc["bestRSSI"] | -120.0f;
            device.avgRSSI = doc["avgRSSI"] | -120.0f;
            device.packetCount = doc["packetCount"] | 0;
            device.lastSeen = millis();  // Update to now since we're restoring on activity
            device.firstSeen = doc["firstSeen"] | millis();
            device.isRouter = doc["isRouter"] | false;
            device.powerClass = doc["powerClass"] | 0;
            
            // Copy string fields safely
            strncpy(device.protocol, doc["protocol"] | "Unknown", sizeof(device.protocol) - 1);
            device.protocol[sizeof(device.protocol) - 1] = '\0';
            
            strncpy(device.deviceType, doc["deviceType"] | "Unknown", sizeof(device.deviceType) - 1);
            device.deviceType[sizeof(device.deviceType) - 1] = '\0';
            
            strncpy(device.firmwareVersion, doc["firmwareVersion"] | "", sizeof(device.firmwareVersion) - 1);
            device.firmwareVersion[sizeof(device.firmwareVersion) - 1] = '\0';
            
            // Initialize RSSI tracking fields for Welford's algorithm
            device.rssiStdDev = 0.0f;
            device.rssiM2 = 0.0f;
            device.originatedPackets = 0;
            device.relayedPackets = 0;
            device.periodicityScore = 0;
            device.avgPacketInterval = 0;
            device.lastPacketInterval = 0;
            
            found = true;
            stats.totalRestored++;
            
            LOG_INFO("[ARCHIVE] ✓ Restored device 0x%08X from archive", nodeId);
        }
    }
    
    archive.close();
    return found;
}

void DeviceArchiver::printStats() const {
    Serial.println("\n[ARCHIVE STATS]");
    Serial.printf("  Total archived to SD: %u devices\n", stats.totalArchived);
    Serial.printf("  Total rotated (no-SD): %u devices\n", stats.totalRotated);
    Serial.printf("  Total restored: %u devices\n", stats.totalRestored);
    Serial.printf("  Last operation freed: %u bytes\n", stats.bytesFreed);
    Serial.printf("  Last fragmentation: %.1f%% → %.1f%%\n", 
                  stats.fragmentationBefore, stats.fragmentationAfter);
    Serial.printf("  Current fragmentation: %.1f%%\n", getCurrentFragmentation());
    Serial.printf("  Current free heap: %u bytes\n", ESP.getFreeHeap());
}

TargetableDevice* DeviceArchiver::tryRestoreOnPacket(uint32_t nodeId, DeviceRepository& deviceRepo) {
    // Only try restore if SD is available and we have archive
    if (!isSDAvailable()) {
        return nullptr;
    }
    
    // Check if device is already in repository
    if (deviceRepo.findByNodeId(nodeId) != nullptr) {
        return nullptr;  // Already in memory, no need to restore
    }
    
    // Check if repository has room
    if (deviceRepo.isFull()) {
        return nullptr;  // No room to restore
    }
    
    // Try to find in archive
    TargetableDevice restoredDevice;
    if (restoreDevice(nodeId, restoredDevice)) {
        // Add restored device to repository
        // Use addOrUpdate which will handle the insert
        TargetableDevice* device = deviceRepo.addOrUpdate(
            restoredDevice.nodeId,
            restoredDevice.configIndex,
            restoredDevice.avgRSSI,
            restoredDevice.protocol,
            nullptr,  // No packet data needed for restore
            0,
            0xFF  // Unknown hop count
        );
        
        if (device) {
            // Copy over archived stats
            device->packetCount = restoredDevice.packetCount;
            device->firstSeen = restoredDevice.firstSeen;
            device->isRouter = restoredDevice.isRouter;
            device->powerClass = restoredDevice.powerClass;
            strncpy(device->deviceType, restoredDevice.deviceType, sizeof(device->deviceType) - 1);
            device->deviceType[sizeof(device->deviceType) - 1] = '\0';
            strncpy(device->firmwareVersion, restoredDevice.firmwareVersion, sizeof(device->firmwareVersion) - 1);
            device->firmwareVersion[sizeof(device->firmwareVersion) - 1] = '\0';
            
            LOG_INFO("[ARCHIVE] Device 0x%08X restored from archive with %u prior packets",
                    nodeId, device->packetCount);
            return device;
        }
    }
    
    return nullptr;
}

bool DeviceArchiver::rotateArchiveIfNeeded() {
    if (!isSDAvailable()) {
        return false;
    }
    
    // Check archive file size
    File archive = SD.open(Config::Archiver::ARCHIVE_FILE, FILE_READ);
    if (!archive) {
        return false;  // No archive to rotate
    }
    
    size_t fileSize = archive.size();
    archive.close();
    
    if (fileSize < Config::Archiver::MAX_ARCHIVE_SIZE_BYTES) {
        return false;  // File is small enough
    }
    
    LOG_INFO("[ARCHIVE] Archive file %u bytes exceeds limit, rotating...", fileSize);
    
    // Remove oldest backup if it exists
    char backupPath[64];
    for (int i = Config::Archiver::MAX_BACKUP_FILES; i >= 1; i--) {
        snprintf(backupPath, sizeof(backupPath), "/recon/devices_archive.%d.jsonl", i);
        
        if (i == Config::Archiver::MAX_BACKUP_FILES) {
            // Delete oldest
            if (SD.exists(backupPath)) {
                SD.remove(backupPath);
                LOG_INFO("[ARCHIVE] Removed oldest backup: %s", backupPath);
            }
        } else {
            // Rename N to N+1
            char newPath[64];
            snprintf(newPath, sizeof(newPath), "/recon/devices_archive.%d.jsonl", i + 1);
            if (SD.exists(backupPath)) {
                SD.rename(backupPath, newPath);
            }
        }
    }
    
    // Rename current archive to .1.jsonl
    snprintf(backupPath, sizeof(backupPath), "/recon/devices_archive.1.jsonl");
    if (SD.exists(Config::Archiver::ARCHIVE_FILE)) {
        SD.rename(Config::Archiver::ARCHIVE_FILE, backupPath);
        LOG_INFO("[ARCHIVE] ✓ Rotated archive to %s", backupPath);
    }
    
    return true;
}
