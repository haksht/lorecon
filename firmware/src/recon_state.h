/**
 * Reconnaissance State Management
 * 
 * Encapsulates all global state variables into a single managed class
 * to improve testability, reduce coupling, and enable better state management.
 */

#ifndef RECON_STATE_H
#define RECON_STATE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "data_structures.h"
#include "config.h"
#include "repositories/packet_store.h"
#include "repositories/device_repository.h"

class ReconState {
private:
    // Scan configurations - made const and part of state
    static const ScanConfig scanConfigs[];
    static const uint8_t NUM_CONFIGS;
    
    // Repository delegates
    PacketStore packetStore_;
    DeviceRepository deviceRepo_;

    // Thread safety mutex for repository access
    // Protects packetStore_ and deviceRepo_ from concurrent access
    // between main loop (packet capture) and AsyncWebServer task (API handlers)
    mutable SemaphoreHandle_t repoMutex_;

public:
    // Core reconnaissance state
    // RFActivity array sized to handle all scanning configurations
    RFActivity rfActivity[Config::Scanning::NUM_CONFIGURATIONS];
    
    // System state
    ScanState scanState;
    
    // PSK decryption statistics
    PSKStats pskStats;
    
    // Network intelligence
    NetworkIntel networkIntel;
    
    // Anomaly detection
    AnomalyRecord anomalies[Config::Anomaly::MAX_ANOMALIES];
    uint16_t numAnomalies;  // uint16_t to prevent overflow (circular buffer uses modulo)
    
    // Traffic histogram
    TrafficHistogram trafficHist;
    
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
                            size_t packetLength = 0, uint8_t hopCount = 0xFF);
    TargetableDevice getTargetableDevice(uint8_t index) const;  // Returns copy for thread safety
    void clearTargetableDevices();
    uint8_t getNumTargetableDevices() const { return deviceRepo_.count(); }
    
    // Repository access — caller must hold lock when accessing directly
    DeviceRepository& getDeviceRepository() { return deviceRepo_; }
    
    // Thread-safe lock/unlock for API handlers accessing repositories
    // Use RAII guard pattern: ReconState::ScopedLock lock(reconState);
    bool lock(uint32_t timeoutMs = 100) const;
    void unlock() const;
    
    // RAII helper for scoped locking
    class ScopedLock {
    public:
        explicit ScopedLock(const ReconState& state, uint32_t timeoutMs = 100)
            : state_(state), acquired_(state.lock(timeoutMs)) {}
        ~ScopedLock() { if (acquired_) state_.unlock(); }
        bool acquired() const { return acquired_; }
        operator bool() const { return acquired_; }
    private:
        const ReconState& state_;
        bool acquired_;
    };
    
    // State queries
    bool hasTargetableDevices() const { return deviceRepo_.count() > 0; }
    bool hasRFActivity() const;
    uint32_t getReconDuration() const;
    
    // Packet replay management
    bool capturePacketForReplay(const uint8_t* data, size_t length, uint8_t configIndex, 
                                float rssi, float snr, const char* protocol, const char* decryptedText = nullptr,
                                uint32_t nodeId = 0, uint32_t packetId = 0, uint8_t hopCount = 0,
                                uint32_t destId = 0xFFFFFFFF, uint8_t channel = 0,
                                bool wantAck = false, bool viaMqtt = false, uint8_t priority = 0);
    CapturedPacket getReplayPacket(uint8_t index) const;  // Returns copy for thread safety
    void clearReplaySlots();
    uint8_t getNumCapturedPackets() const { return packetStore_.count(); }
    
    // Direct repository access (caller MUST hold lock via ScopedLock first)
    const PacketStore& getPacketStore() const { return packetStore_; }
    
    // Statistics and reporting
    void printStateSummary() const;
    void exportStateToJSON(String& output) const;
    
    // Network intelligence
    void updateNetworkIntel();
    const NetworkIntel& getNetworkIntel() const { return networkIntel; }
    
    // Anomaly detection
    void recordAnomaly(uint32_t nodeId, AnomalyType type, float severity, const char* description);
    void checkForAnomalies(const uint8_t* data, size_t length, uint32_t nodeId, float rssi);
    uint8_t getUnacknowledgedAnomalies() const;
    
    // Temporal analysis
    void updateTrafficHistogram();
    void updateDeviceTemporalMetrics(uint32_t nodeId);
    
private:
    // Helper methods
    void initializeScanConfigs();
};

// Global instance - eventually we'll pass this around instead of using globals
extern ReconState reconState;

#endif // RECON_STATE_H