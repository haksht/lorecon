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
    prefs.end();
    
    // Validate persisted mode
    if (savedMode == MODE_TARGETED_CAPTURE) {
        if (reconState.isValidConfigIndex(savedTargetCfg)) {
            LOG_INFO("🔄 Restoring persisted targeting mode (config %d)", savedTargetCfg);
            outMode = MODE_TARGETED_CAPTURE;
            outTargetConfig = savedTargetCfg;
            outByDevice = savedByDevice;
            return true;
        } else {
            LOG_WARN("Persisted mode invalid (config %d out of range), clearing", savedTargetCfg);
            clearPersistedMode();
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

void ModeManager::clearPersistedMode() {
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        LOG_WARN("Could not open mode preferences for clearing");
        return;
    }
    
    prefs.clear();
    prefs.end();
    
    LOG_INFO("Persisted mode cleared");
}

bool ModeManager::hasPersistedMode() {
    if (!prefs.begin(NVS_NAMESPACE, true)) {  // Read-only
        return false;
    }
    
    uint8_t savedMode = prefs.getUChar(KEY_MODE, MODE_RECONNAISSANCE);
    prefs.end();
    
    return savedMode == MODE_TARGETED_CAPTURE;
}
