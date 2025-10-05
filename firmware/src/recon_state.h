/**
 * Reconnaissance State Management
 * 
 * Encapsulates all global state variables into a single managed class
 * to improve testability, reduce coupling, and enable better state management.
 */

#ifndef RECON_STATE_H
#define RECON_STATE_H

#include <Arduino.h>
#include "data_structures.h"

// Configuration constants
#define SCAN_DWELL_TIME     12000  // 12 seconds per frequency
#define MAX_TRACKED_NODES   20
#define PACKET_BUFFER_SIZE  256

class ReconState {
private:
    // Scan configurations - made const and part of state
    static const ScanConfig scanConfigs[];
    static const uint8_t NUM_CONFIGS;

public:
    // Core reconnaissance state
    // RFActivity array sized exactly to NUM_CONFIGS (no waste)
    RFActivity rfActivity[16];  // Will be initialized to correct size based on NUM_CONFIGS
    TargetableDevice targetableDevices[MAX_TRACKED_NODES];
    uint8_t numTargetableDevices;
    
    // System state
    ScanState scanState;
    
    // Node tracking
    TrackedNode trackedNodes[MAX_TRACKED_NODES];
    uint8_t nodeCount;
    
    // Packet replay storage
    CapturedPacket replaySlots[MAX_REPLAY_SLOTS];
    uint8_t numCapturedPackets;
    
    // Optional modules state
    #ifdef ENABLE_PSK_TESTING
    PSKStats pskStats;
    #endif
    
    // Constructor and initialization
    ReconState();
    void initialize();
    void reset();
    
    // Configuration access
    const ScanConfig& getScanConfig(uint8_t index) const;
    uint8_t getNumConfigs() const;
    bool isValidConfigIndex(uint8_t index) const;
    
    // RF Activity management
    void updateRFActivity(uint8_t configIndex, float rssi);
    const RFActivity& getRFActivity(uint8_t configIndex) const;
    void clearRFActivity();
    
    // Targetable device management
    void addTargetableDevice(uint32_t nodeId, uint8_t configIndex, float rssi, 
                            const char* protocol, const uint8_t* packetData = nullptr, 
                            size_t packetLength = 0);
    const TargetableDevice& getTargetableDevice(uint8_t index) const;
    TargetableDevice* findTargetableDevice(uint32_t nodeId);
    void clearTargetableDevices();
    
    // Node tracking management
    void updateNode(uint32_t nodeId, const char* protocol, float rssi);
    TrackedNode* findNode(uint32_t nodeId);
    void clearNodes();
    
    // State queries
    bool hasTargetableDevices() const { return numTargetableDevices > 0; }
    bool hasRFActivity() const;
    uint32_t getReconDuration() const;
    
    // Packet replay management
    bool capturePacketForReplay(const uint8_t* data, size_t length, uint8_t configIndex, 
                                float rssi, const char* protocol);
    const CapturedPacket& getReplayPacket(uint8_t index) const;
    void clearReplaySlots();
    uint8_t getNumCapturedPackets() const { return numCapturedPackets; }
    
    // Statistics and reporting
    void printStateSummary() const;
    void exportStateToJSON(String& output) const;
    
private:
    // Helper methods
    void initializeScanConfigs();
    const char* identifyDeviceType(const uint8_t* data, size_t length, 
                                   const char* protocol, float rssi) const;
    uint8_t estimatePowerClass(float rssi) const;
    bool isRoutingDevice(const uint8_t* data, size_t length, const char* protocol) const;
    const char* estimateFirmwareVersion(const uint8_t* data, size_t length, const char* protocol) const;
};

// Global instance - eventually we'll pass this around instead of using globals
extern ReconState reconState;

#endif // RECON_STATE_H