/**
 * Mode Manager - Operation Mode Persistence
 * 
 * Manages operation mode state persistence across reboots using NVS.
 * Extracted from lora_recon_tool.cpp to improve modularity.
 */

#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include <Preferences.h>
#include "data_structures.h"

/**
 * ModeManager - Handles mode persistence via NVS
 * 
 * Encapsulates the logic for saving and restoring operation modes
 * across device reboots. Targeted capture mode is persisted so the
 * device resumes monitoring the same frequency after power cycles.
 */
class ModeManager {
public:
    ModeManager();
    
    /**
     * Load persisted mode from NVS
     * 
     * @param outMode Output: restored operation mode
     * @param outTargetConfig Output: target config index (if targeted mode)
     * @param outByDevice Output: whether targeting is by device (vs frequency)
     * @return true if valid persisted mode was found, false for fresh start
     */
    bool loadPersistedMode(OperationMode& outMode, uint8_t& outTargetConfig, bool& outByDevice);
    
    /**
     * Save current mode to NVS
     * 
     * @param mode Current operation mode
     * @param targetConfig Target config index (for targeted mode)
     * @param byDevice Whether targeting is by device
     */
    void saveMode(OperationMode mode, uint8_t targetConfig, bool byDevice);
    
    /**
     * Clear persisted mode (return to reconnaissance on next boot)
     * @param source Caller identification for debugging (e.g., "API:startScan")
     */
    void clearPersistedMode(const char* source = "unknown");
    
    /**
     * Check if there's a valid persisted mode
     */
    bool hasPersistedMode();
    
    /**
     * Get the number of times mode was cleared (persisted counter for debugging)
     */
    uint32_t getModeChangeCount();
    
    /**
     * Get the last recorded uptime when mode was changed (persisted for debugging)
     */
    uint32_t getLastModeChangeUptime();
    
    /**
     * Log a mode transition with uptime (for debugging unexplained mode changes)
     */
    void logModeTransition(OperationMode from, OperationMode to, const char* source);

private:
    static constexpr const char* NVS_NAMESPACE = "mode";
    static constexpr const char* KEY_MODE = "mode";
    static constexpr const char* KEY_TARGET_CFG = "targetCfg";
    static constexpr const char* KEY_BY_DEVICE = "byDevice";
    static constexpr const char* KEY_CHANGE_COUNT = "changeCount";
    static constexpr const char* KEY_LAST_UPTIME = "lastUptime";
    static constexpr const char* KEY_LAST_FROM = "lastFrom";
    static constexpr const char* KEY_LAST_TO = "lastTo";
    
    Preferences prefs;
};

#endif // MODE_MANAGER_H
