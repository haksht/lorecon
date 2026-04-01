/**
 * Mode Manager Implementation
 * 
 * Operation mode persistence using ESP32 NVS (Non-Volatile Storage).
 */

#include "mode_manager.h"
#include "logger.h"
#include "recon_state.h"

ModeManager::ModeManager() {
}

bool ModeManager::loadPersistedMode(OperationMode& outMode, uint8_t& outTargetConfig, bool& outByDevice) {
    // Default values
    outMode = MODE_RECONNAISSANCE;
    outTargetConfig = 0;
    outByDevice = false;
    
    // Try to open NVS namespace (read-write to create if missing)
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        LOG_WARN("Could not open mode preferences - fresh start");
        return false;
    }
    
    // Read persisted values
    uint8_t savedMode = prefs.getUChar(KEY_MODE, MODE_RECONNAISSANCE);
    uint8_t savedTargetCfg = prefs.getUChar(KEY_TARGET_CFG, 0);
    bool savedByDevice = prefs.getBool(KEY_BY_DEVICE, false);
    uint32_t changeCount = prefs.getULong(KEY_CHANGE_COUNT, 0);
    prefs.end();
    
    // Log change counter for debugging long-running tests
    if (changeCount > 0) {
        LOG_INFO(" Mode change counter: %lu (since last firmware flash)", changeCount);
    }
    
    // Validate persisted mode
    if (savedMode == MODE_TARGETED_CAPTURE) {
        if (reconState.isValidConfigIndex(savedTargetCfg)) {
            LOG_INFO(">> Restoring persisted targeting mode (config %d)", savedTargetCfg);
            outMode = MODE_TARGETED_CAPTURE;
            outTargetConfig = savedTargetCfg;
            outByDevice = savedByDevice;
            return true;
        } else {
            LOG_WARN("Persisted mode invalid (config %d out of range), clearing", savedTargetCfg);
            clearPersistedMode("Boot:invalidConfig");
            return false;
        }
    }
    
    return false;
}

void ModeManager::saveMode(OperationMode mode, uint8_t targetConfig, bool byDevice) {
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        LOG_ERROR("Failed to open mode preferences for writing");
        return;
    }
    
    prefs.putUChar(KEY_MODE, mode);
    prefs.putUChar(KEY_TARGET_CFG, targetConfig);
    prefs.putBool(KEY_BY_DEVICE, byDevice);
    prefs.end();
    
    LOG_INFO("Mode persisted: mode=%d, config=%d, byDevice=%d", mode, targetConfig, byDevice);
}

void ModeManager::clearPersistedMode(const char* source) {
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        LOG_WARN("Could not open mode preferences for clearing");
        return;
    }
    
    // Increment and persist change counter (survives reboots for debugging)
    uint32_t changeCount = prefs.getULong(KEY_CHANGE_COUNT, 0) + 1;
    prefs.putULong(KEY_CHANGE_COUNT, changeCount);
    
    // Clear mode but keep counter
    prefs.remove(KEY_MODE);
    prefs.remove(KEY_TARGET_CFG);
    prefs.remove(KEY_BY_DEVICE);
    prefs.end();
    
    LOG_WARN("[!] Mode CLEARED by [%s] (total clears: %lu)", source, changeCount);
}

uint32_t ModeManager::getModeChangeCount() {
    if (!prefs.begin(NVS_NAMESPACE, true)) {  // Read-only
        return 0;
    }
    uint32_t count = prefs.getULong(KEY_CHANGE_COUNT, 0);
    prefs.end();
    return count;
}

uint32_t ModeManager::getLastModeChangeUptime() {
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        return 0;
    }
    uint32_t uptime = prefs.getULong(KEY_LAST_UPTIME, 0);
    prefs.end();
    return uptime;
}

void ModeManager::logModeTransition(OperationMode from, OperationMode to, const char* source) {
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        LOG_ERROR("Failed to open NVS for mode transition log");
        return;
    }
    
    uint32_t uptimeSec = millis() / 1000;
    prefs.putULong(KEY_LAST_UPTIME, uptimeSec);
    prefs.putUChar(KEY_LAST_FROM, from);
    prefs.putUChar(KEY_LAST_TO, to);
    prefs.end();
    
    // Critical log - this helps debug unexpected mode changes
    LOG_WARN(">> MODE TRANSITION: %d->%d at uptime %lu sec by [%s]", from, to, uptimeSec, source);
}

bool ModeManager::hasPersistedMode() {
    if (!prefs.begin(NVS_NAMESPACE, true)) {  // Read-only
        return false;
    }
    
    uint8_t savedMode = prefs.getUChar(KEY_MODE, MODE_RECONNAISSANCE);
    prefs.end();
    
    return savedMode == MODE_TARGETED_CAPTURE;
}
