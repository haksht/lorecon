/**
 * Device Archiver Implementation
 */

#include "device_archiver.h"
#include "logger.h"
#include <esp_heap_caps.h>
#include <ArduinoJson.h>

DeviceArchiver::DeviceArchiver() 
    : stats()
    , sdInitialized(false) {
}

void DeviceArchiver::initializeSD() {
    if (sdInitialized) return;
    
    if (SD.begin(Config::Hardware::SD_CS)) {
        sdInitialized = true;
        LOG_INFO("[ARCHIVE] SD card initialized");
    } else {
        LOG_WARN("[ARCHIVE] SD card not available - archival disabled");
    }
}

bool DeviceArchiver::isSDAvailable() const {
    return sdInitialized || SD.begin(Config::Hardware::SD_CS);
}

float DeviceArchiver::getCurrentFragmentation() const {
    // Use ESP.getHeapFragmentation() instead of low-level heap_caps API
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t maxBlock = ESP.getMaxAllocHeap();
    
    if (freeHeap == 0) return 0.0f;
    
    float fragmentation = 100.0f * (1.0f - (float)maxBlock / (float)freeHeap);
    return fragmentation;
}

bool DeviceArchiver::shouldArchive() const {
    float frag = getCurrentFragmentation();
    return frag >= Config::Archiver::FRAGMENTATION_THRESHOLD;
}

bool DeviceArchiver::checkAndArchive(TargetableDevice* devices, uint8_t& deviceCount,
                                     TrackedNode* hotNodes, uint8_t& hotNodeCount) {
    if (!isSDAvailable()) {
        // No SD card - use aggressive rotation instead
        return rotateDevicesNoSD(devices, deviceCount, Config::Tracking::MAX_DEVICES);
    }
    
    initializeSD();
    
    if (!shouldArchive()) {
        return false;  // Fragmentation is acceptable
    }
    
    LOG_INFO("[ARCHIVE] Fragmentation threshold exceeded, triggering archival...");
    return archiveInactiveDevices(devices, deviceCount, hotNodes, hotNodeCount);
}

uint8_t DeviceArchiver::identifyColdDevices(const TargetableDevice* devices, uint8_t deviceCount,
                                            uint8_t* coldIndices, uint8_t maxCold) const {
    uint32_t now = millis();
    uint8_t coldCount = 0;
    
    // Keep at least MIN_ACTIVE_DEVICES in RAM
    if (deviceCount <= Config::Archiver::MIN_ACTIVE_DEVICES) {
        return 0;
    }
    
    for (uint8_t i = 0; i < deviceCount && coldCount < maxCold; i++) {
        uint32_t inactiveMs = now - devices[i].lastSeen;
        if (inactiveMs > Config::Archiver::DEVICE_INACTIVITY_MS) {
            coldIndices[coldCount++] = i;
        }
    }
    
    return coldCount;
}

bool DeviceArchiver::rotateDevicesNoSD(TargetableDevice* devices, uint8_t& deviceCount,
                                       uint8_t maxDevices, float fragmentationThreshold) {
    if (deviceCount < maxDevices) return false;  // No need to rotate
    
    float currentFrag = getCurrentFragmentation();
    if (currentFrag < fragmentationThreshold) return false;  // Fragmentation acceptable
    
    LOG_INFO("[ARCHIVE] No SD card - aggressive rotation mode");
    LOG_INFO("[ARCHIVE] Devices: %u/%u, Fragmentation: %.1f%%", deviceCount, maxDevices, currentFrag);
    
    // Find oldest 30% of devices to remove (minimum 2, maximum 10)
    uint8_t removeCount = max(2, min(10, deviceCount * 3 / 10));
    uint32_t now = millis();
    
    // Create sorted list of devices by lastSeen (oldest first)
    struct DeviceAge {
        uint8_t index;
        uint32_t lastSeen;
    };
    DeviceAge ages[Config::Tracking::MAX_DEVICES];
    
    for (uint8_t i = 0; i < deviceCount; i++) {
        ages[i].index = i;
        ages[i].lastSeen = devices[i].lastSeen;
    }
    
    // Simple bubble sort (small array, embedded-friendly)
    for (uint8_t i = 0; i < deviceCount - 1; i++) {
        for (uint8_t j = i + 1; j < deviceCount; j++) {
            if (ages[i].lastSeen > ages[j].lastSeen) {
                DeviceAge temp = ages[i];
                ages[i] = ages[j];
                ages[j] = temp;
            }
        }
    }
    
    // Remove oldest devices
    uint8_t oldestIndices[10];
    for (uint8_t i = 0; i < removeCount; i++) {
        oldestIndices[i] = ages[i].index;
        uint32_t inactiveMs = now - devices[ages[i].index].lastSeen;
        LOG_INFO("[ARCHIVE] Dropping device 0x%08X (inactive %lu ms)", 
                devices[ages[i].index].nodeId, inactiveMs);
    }
    
    // Compact array
    compactDeviceArray(devices, deviceCount, oldestIndices, removeCount);
    
    float fragAfter = getCurrentFragmentation();
    LOG_INFO("[ARCHIVE] ✓ Dropped %u devices, %u remain (frag %.1f%% → %.1f%%)",
            removeCount, deviceCount, currentFrag, fragAfter);
    
    stats.totalArchived += removeCount;  // Track as "archived" even though not saved
    
    return true;
}

bool DeviceArchiver::writeDeviceToArchive(const TargetableDevice& device) {
    File archive = SD.open(Config::Archiver::ARCHIVE_FILE, FILE_APPEND);
    if (!archive) {
        LOG_ERROR("[ARCHIVE] Failed to open archive file");
        return false;
    }
    
    // Create JSON document (allocate on heap temporarily)
    JsonDocument doc;
    
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
    
    // Write as single-line JSON
    serializeJson(doc, archive);
    archive.println();  // Newline separator
    archive.close();
    
    return true;
}

bool DeviceArchiver::writeNodeToArchive(const TrackedNode& node) {
    File archive = SD.open(Config::Archiver::ARCHIVE_FILE, FILE_APPEND);
    if (!archive) {
        return false;
    }
    
    JsonDocument doc;
    
    doc["type"] = "node";
    doc["nodeId"] = node.nodeId;
    doc["protocol"] = node.protocol;
    doc["packetCount"] = node.packetCount;
    doc["avgRSSI"] = node.avgRSSI;
    doc["bestRSSI"] = node.bestRSSI;
    doc["lastSeen"] = node.lastSeen;
    doc["firstSeen"] = node.firstSeen;
    doc["archivedAt"] = millis();
    
    serializeJson(doc, archive);
    archive.println();
    archive.close();
    
    return true;
}

bool DeviceArchiver::compactDeviceArray(TargetableDevice* devices, uint8_t& deviceCount,
                                        const uint8_t* removeIndices, uint8_t removeCount) {
    if (removeCount == 0) return true;
    
    // Mark devices for removal
    bool toRemove[Config::Tracking::MAX_DEVICES] = {false};
    for (uint8_t i = 0; i < removeCount; i++) {
        toRemove[removeIndices[i]] = true;
    }
    
    // Compact array by shifting kept devices down
    uint8_t writePos = 0;
    for (uint8_t readPos = 0; readPos < deviceCount; readPos++) {
        if (!toRemove[readPos]) {
            if (writePos != readPos) {
                devices[writePos] = devices[readPos];
            }
            writePos++;
        }
    }
    
    deviceCount = writePos;
    return true;
}

bool DeviceArchiver::compactNodeArray(TrackedNode* nodes, uint8_t& nodeCount,
                                     const uint8_t* removeIndices, uint8_t removeCount) {
    if (removeCount == 0) return true;
    
    bool toRemove[Config::Tracking::MAX_HOT_NODES] = {false};
    for (uint8_t i = 0; i < removeCount; i++) {
        toRemove[removeIndices[i]] = true;
    }
    
    uint8_t writePos = 0;
    for (uint8_t readPos = 0; readPos < nodeCount; readPos++) {
        if (!toRemove[readPos]) {
            if (writePos != readPos) {
                nodes[writePos] = nodes[readPos];
            }
            writePos++;
        }
    }
    
    nodeCount = writePos;
    return true;
}

bool DeviceArchiver::archiveInactiveDevices(TargetableDevice* devices, uint8_t& deviceCount,
                                            TrackedNode* hotNodes, uint8_t& hotNodeCount) {
    float fragBefore = getCurrentFragmentation();
    uint32_t heapBefore = ESP.getFreeHeap();
    
    // Identify cold devices
    uint8_t coldIndices[Config::Tracking::MAX_DEVICES];
    uint8_t coldCount = identifyColdDevices(devices, deviceCount, coldIndices, Config::Tracking::MAX_DEVICES);
    
    if (coldCount == 0) {
        LOG_INFO("[ARCHIVE] No inactive devices to archive");
        return false;
    }
    
    LOG_INFO("[ARCHIVE] Archiving %d inactive devices...", coldCount);
    
    // Write devices to SD
    uint8_t archived = 0;
    for (uint8_t i = 0; i < coldCount; i++) {
        uint8_t idx = coldIndices[i];
        if (writeDeviceToArchive(devices[idx])) {
            archived++;
        }
    }
    
    // Compact array to remove archived devices
    compactDeviceArray(devices, deviceCount, coldIndices, archived);
    
    // Also archive cold nodes
    uint32_t now = millis();
    uint8_t coldNodeIndices[Config::Tracking::MAX_HOT_NODES];
    uint8_t coldNodeCount = 0;
    
    for (uint8_t i = 0; i < hotNodeCount; i++) {
        uint32_t inactiveMs = now - hotNodes[i].lastSeen;
        if (inactiveMs > Config::Archiver::DEVICE_INACTIVITY_MS) {
            if (writeNodeToArchive(hotNodes[i])) {
                coldNodeIndices[coldNodeCount++] = i;
            }
        }
    }
    
    compactNodeArray(hotNodes, hotNodeCount, coldNodeIndices, coldNodeCount);
    
    // Update stats
    uint32_t heapAfter = ESP.getFreeHeap();
    float fragAfter = getCurrentFragmentation();
    
    stats.totalArchived += archived;
    stats.lastArchiveTime = millis();
    stats.bytesFreed = heapAfter - heapBefore;
    stats.fragmentationBefore = fragBefore;
    stats.fragmentationAfter = fragAfter;
    
    LOG_INFO("[ARCHIVE] ✓ Archived %d devices, %d nodes", archived, coldNodeCount);
    LOG_INFO("[ARCHIVE] Fragmentation: %.1f%% -> %.1f%% (freed %u bytes)", 
             fragBefore, fragAfter, stats.bytesFreed);
    
    logArchiveOperation(archived, fragBefore, fragAfter);
    
    return archived > 0;
}

void DeviceArchiver::logArchiveOperation(uint8_t archivedCount, float fragBefore, float fragAfter) {
    File statsFile = SD.open(Config::Archiver::STATS_FILE, FILE_WRITE);
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
    // TODO: Implement if needed - search archive file for nodeId and restore
    // For now, archival is one-way to keep it simple
    return false;
}

void DeviceArchiver::printStats() const {
    Serial.println("\n[ARCHIVE STATS]");
    Serial.printf("  Total archived: %u devices\n", stats.totalArchived);
    Serial.printf("  Total restored: %u devices\n", stats.totalRestored);
    Serial.printf("  Bytes freed: %u\n", stats.bytesFreed);
    Serial.printf("  Last fragmentation: %.1f%% -> %.1f%%\n", 
                  stats.fragmentationBefore, stats.fragmentationAfter);
}
