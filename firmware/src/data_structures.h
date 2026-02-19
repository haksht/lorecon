/**
 * Shared Data Structures
 * 
 * Common data structures used across multiple modules in the ESP32 LoRa 
 * Reconnaissance Tool. This header provides the complete struct definitions
 * needed by both main.cpp and user_interface.cpp.
 */

#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <Arduino.h>
#include <atomic>
#include "config.h"

// Operation modes
enum OperationMode : uint8_t {
  MODE_RECONNAISSANCE,  // Scan all frequencies, detect devices
  MODE_TARGETED_CAPTURE,  // Focus on specific device/frequency
  MODE_INTERACTIVE_MENU,  // Show devices and await user selection
  MODE_PACKET_REPLAY  // Replay captured packets
};

// Captured packet for replay
struct CapturedPacket {
  uint8_t data[Config::PacketProcessing::MAX_PACKET_SIZE];
  size_t length;
  uint8_t configIndex;  // Which radio config to use for replay
  float originalRSSI;
  float snr;            // Signal-to-noise ratio (dB)
  uint32_t captureTime;
  uint32_t nodeId;      // From field - original sender (0 if unknown)
  uint32_t destId;      // To field - destination (0xFFFFFFFF = broadcast)
  uint32_t packetId;    // Packet ID for deduplication (0 if unknown)
  uint8_t hopCount;     // Hop count from flags (lower 3 bits of byte 12)
  uint8_t channel;      // Channel index (byte 13)
  bool wantAck;         // Sender wants acknowledgment (flags bit 3)
  bool viaMqtt;         // Packet came via MQTT gateway (flags bit 4)
  uint8_t priority;     // Packet priority 0-3 (flags bits 5-6)
  char protocol[16];
  char decryptedText[256];  // Stores decrypted message text if available
  bool valid;
};

// Queued packet for interrupt-driven reception
struct QueuedPacket {
    uint8_t data[Config::PacketProcessing::MAX_PACKET_SIZE];
    size_t length;
    float rssi;
    float snr;
    uint32_t timestamp;
    uint8_t configIndex;
    float frequencyMHz;
};

// Frequency scan configuration
struct ScanConfig {
  float frequency;
  float bandwidth;
  uint8_t spreadingFactor;
  uint8_t syncWord;
  const char* protocol;
};

// RF Activity monitoring - tracks signal activity without creating targetable entries
struct RFActivity {
  uint8_t configIndex;
  float avgRSSI;
  float peakRSSI;
  uint16_t activityCount;
  uint32_t lastActivity;
  uint32_t firstSeen;
  const char* activityLevel;  // "LOW", "MEDIUM", "HIGH"
};

// Targetable devices - only devices with confirmed node IDs from decoded packets
struct TargetableDevice {
  uint32_t nodeId;          // Real node ID from decoded packets (required)
  uint8_t configIndex;      // Which scan config works best for this device
  float bestRSSI;
  float avgRSSI;
  float rssiStdDev;         // RSSI standard deviation (for spoofing detection)
  float rssiM2;             // Welford's M2 for variance calculation (internal use)
  uint32_t packetCount;     // Number of successfully decoded packets
  uint16_t originatedPackets; // Packets originated by this device (not relays)
  uint16_t relayedPackets;    // Packets relayed by this device
  uint32_t lastSeen;
  uint32_t firstSeen;
  uint32_t lastPacketInterval; // Time since previous packet (for periodicity detection)
  uint32_t avgPacketInterval;  // Average interval between packets (0 = not periodic)
  uint8_t periodicityScore;    // 0-100, confidence this is a beacon
  char protocol[16];
  char deviceType[24];      // Identified device type
  char firmwareVersion[32]; // Detected firmware version
  bool isRouter;            // Device appears to be routing traffic
  uint8_t powerClass;       // Estimated power class (0=low, 1=med, 2=high)
  uint8_t maxHopCount;      // Max hop count seen (approximates sender's hop_limit)
};

// System state management
struct ScanState {
  OperationMode mode;
  uint8_t currentConfig;
  uint8_t targetConfig;     // For targeted capture mode
  bool targetedByDevice;    // true = device targeting, false = frequency targeting
  uint32_t lastScanSwitch;
  // Counters written from the main loop (packet_processor) and read from the
  // AsyncWebServer task (api_controller, json_builders). Use std::atomic to
  // prevent data races on dual-core ESP32-S3. Implicit load()/store() via
  // assignment/conversion operators keep call sites unchanged.
  std::atomic<uint32_t> totalPackets;
  uint32_t totalDetections;
  std::atomic<uint32_t> droppedPackets;
  std::atomic<uint32_t> peakQueueSize;
  bool packetPending;
  char lastPacket[Config::PacketProcessing::MAX_PACKET_SIZE];  // Fixed buffer instead of String
  size_t lastPacketLength;           // Track actual length
  uint32_t reconStartTime;
  bool waitingForUserInput;
};

// PSK decryption statistics
struct PSKStats {
  uint16_t attempts;
  uint16_t successes;
  uint8_t hitCount[Config::PSK::NUM_DEFAULT_KEYS];
  
  PSKStats() : attempts(0), successes(0) {
    memset(hitCount, 0, sizeof(hitCount));
  }
};

// Geographic intelligence data
struct GeoPoint {
  float latitude;
  float longitude;
  float altitude;
  uint32_t timestamp;
  uint32_t nodeId;
  int8_t precision;  // GPS precision indicator
  bool valid;
  
  GeoPoint() : latitude(0), longitude(0), altitude(0), timestamp(0), 
               nodeId(0), precision(0), valid(false) {}
};

// Anomaly types
enum class AnomalyType : uint8_t {
    PACKET_SIZE_OUTLIER,      // Packet size >2σ from mean
    EXCESSIVE_RELAY_HOPS,     // >5 relay hops detected
    RATE_VIOLATION,           // >20 packets/min
    RSSI_INCONSISTENCY,       // Same node, wildly different RSSI
    REPLAY_ATTACK,            // Old packet ID reappearing
    TIMING_ANOMALY            // Unexpected interval variation
};

// Anomaly detection record
struct AnomalyRecord {
    uint32_t nodeId;          // Device that triggered anomaly
    AnomalyType type;
    uint32_t timestamp;
    float severity;           // 0.0-1.0
    char description[64];     // Human-readable explanation
    bool acknowledged;        // For UI dismissal
    
    AnomalyRecord() : nodeId(0), type(AnomalyType::PACKET_SIZE_OUTLIER), 
                      timestamp(0), severity(0.0f), acknowledged(false) {
        description[0] = '\0';
    }
};

// Temporal traffic histogram (24 hours, 1-hour buckets)
struct TrafficHistogram {
    uint16_t hourlyPackets[24];  // Rolling 24-hour window
    uint8_t currentHour;         // Current bucket index
    uint32_t lastHourChange;     // Timestamp of last hour rollover
    
    TrafficHistogram() : currentHour(0), lastHourChange(0) {
        memset(hourlyPackets, 0, sizeof(hourlyPackets));
    }
};

// Network intelligence statistics
struct NetworkIntel {
    uint8_t activeTransmitters;   // Devices originating messages
    uint8_t relayOnlyNodes;       // Devices only relaying
    uint8_t mixedNodes;           // Both originate and relay
    uint16_t floodingEvents;      // Detected relay chains
    uint8_t encryptedNetworks;    // Distinct encrypted groups detected
    uint8_t beaconDevices;        // Devices transmitting periodically
    uint16_t anomalyCount;        // Active unacknowledged anomalies (uint16_t to prevent overflow)
    uint32_t lastUpdate;          // Timestamp of last calculation
    
    NetworkIntel() : activeTransmitters(0), relayOnlyNodes(0), mixedNodes(0),
                     floodingEvents(0), encryptedNetworks(0), beaconDevices(0),
                     anomalyCount(0), lastUpdate(0) {}
};

#endif // DATA_STRUCTURES_H