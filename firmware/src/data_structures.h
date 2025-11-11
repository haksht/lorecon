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

// Operation modes
enum OperationMode : uint8_t {
  MODE_RECONNAISSANCE,  // Scan all frequencies, detect devices
  MODE_TARGETED_CAPTURE,  // Focus on specific device/frequency
  MODE_INTERACTIVE_MENU,  // Show devices and await user selection
  MODE_PACKET_REPLAY  // Replay captured packets
};

// Captured packet for replay
#define MAX_REPLAY_SLOTS 10
#define MAX_PACKET_SIZE 256

struct CapturedPacket {
  uint8_t data[MAX_PACKET_SIZE];
  size_t length;
  uint8_t configIndex;  // Which radio config to use for replay
  float originalRSSI;
  uint32_t captureTime;
  char protocol[16];
  bool valid;
};

// Queued packet for interrupt-driven reception
struct QueuedPacket {
    uint8_t data[256];
    size_t length;
    float rssi;
    float snr;
    uint32_t timestamp;
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
  uint16_t packetCount;     // Number of successfully decoded packets
  uint32_t lastSeen;
  uint32_t firstSeen;
  char protocol[16];
  char deviceType[24];      // Identified device type
  char firmwareVersion[32]; // Detected firmware version
  bool isRouter;            // Device appears to be routing traffic
  uint8_t powerClass;       // Estimated power class (0=low, 1=med, 2=high)
};

// Node tracking for behavioral analysis
struct TrackedNode {
  uint32_t nodeId;
  String protocol;
  uint16_t packetCount;
  float avgRSSI;
  float bestRSSI;
  uint32_t lastSeen;
  uint32_t firstSeen;
  bool active;
};

// System state management
struct ScanState {
  OperationMode mode;
  uint8_t currentConfig;
  uint8_t targetConfig;     // For targeted capture mode
  uint32_t lastScanSwitch;
  uint32_t totalPackets;
  uint32_t totalDetections;
  bool packetPending;
  char lastPacket[MAX_PACKET_SIZE];  // Fixed buffer instead of String
  size_t lastPacketLength;           // Track actual length
  uint32_t reconStartTime;
  bool waitingForUserInput;
};

#ifdef ENABLE_PSK_TESTING
struct PSKStats {
  uint16_t attempts;
  uint16_t successes;
  uint8_t hitCount[5]; // NUM_DEFAULT_PSKS
  
  PSKStats() : attempts(0), successes(0) {
    memset(hitCount, 0, sizeof(hitCount));
  }
};
#endif

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

#define MAX_GEO_POINTS 50

#endif // DATA_STRUCTURES_H