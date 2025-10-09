#ifndef LORA_RECON_TOOL_H
#define LORA_RECON_TOOL_H

#include <Arduino.h>
#include <RadioLib.h>
#include <atomic>
#include <queue>
#include "data_structures.h"

// Forward declarations
class CommandHandler;
class OLEDDisplay;

// Packet queue structure for buffering received packets
struct QueuedPacket {
    uint8_t data[256];
    size_t length;
    float rssi;
    float snr;
    uint32_t timestamp;
};

#include "recon_state.h"
#include "protocol_analyzer.h"
#include "geo_intelligence.h"
#include "command_handler.h"
#include "oled_display.h"
#include "session_key_manager.h"

#ifdef ENABLE_STRESS_TESTING
#include "hardware_stress_tester.h"
#endif

// Hardware configuration for Heltec WiFi LoRa 32 V3
#define LORA_NSS    8
#define LORA_DIO1   14
#define LORA_RST    12
#define LORA_BUSY   13
#define USER_BUTTON 0   // PRG button (active low)

// Scanning configuration
#define SCAN_DWELL_TIME     12000  // 12 seconds per frequency
#define PACKET_BUFFER_SIZE  256
#define SERIAL_TIMEOUT_MS   30000  // 30 second timeout for user input

/**
 * LoRa Reconnaissance Tool - Main application class
 * 
 * Encapsulates all reconnaissance logic, radio control, and state management.
 * Eliminates global variables and provides clean interface for the application.
 */
class LoRaReconTool {
public:
    LoRaReconTool();
    
    // Lifecycle methods
    bool initialize();
    void update();
    
    // User interaction
    void handleUserInput(char cmd);
    
    // Device targeting (public for UI module access)
    void startTargetedCapture(uint8_t deviceIndex);
    void startFrequencyTargeting(uint8_t configIndex);
    
    // Packet replay (public for command handler)
    void showReplayMenu();
    void replayPacket(uint8_t slotIndex);
    
    // Access to radio for external needs (like stress testing)
    SX1262& getRadio() { return radio; }
    
    // Public config application for command handler
    bool applyConfigPublic(uint8_t configIndex) { return applyConfig(configIndex); }
    
    // Public receiver start for command handler
    void startReceiving() { radio.startReceive(); }
    
    // Packet received flag for interrupt handler
    void markPacketReceived() { packetReceived.store(true, std::memory_order_release); }
    
    // Display access
    OLEDDisplay* getDisplay() { return oledDisplay; }
    
    // Session key manager access (for command handler)
    SessionKeyManager& getSessionKeyManager() { return sessionKeyManager; }
    
#ifdef ENABLE_STRESS_TESTING
    HardwareStressTester* getStressTester() { return stressTester; }
#endif

private:
    // Hardware instances
    SX1262 radio;
    ProtocolAnalyzer protocolAnalyzer;
    CommandHandler* commandHandler;
    OLEDDisplay* oledDisplay;
    
    // Session key management
    SessionKeyManager sessionKeyManager;
    
    // Interrupt flag and packet queue
    std::atomic<bool> packetReceived;
    std::queue<QueuedPacket> packetQueue;
    static const size_t MAX_QUEUE_SIZE = 10;  // Buffer up to 10 packets
    
    // Button state tracking
    bool buttonPressed;
    uint32_t buttonPressStart;
    bool shutdownInitiated;
    
#ifdef ENABLE_STRESS_TESTING
    HardwareStressTester* stressTester;
#endif
    
    // Button handler
    void handleButtonPress(uint32_t now);
    
    // Radio configuration
    bool applyConfig(uint8_t configIndex);
    
    // Mode handlers
    void handleReconnaissanceMode(uint32_t now);
    void handleTargetedCaptureMode(uint32_t now);
    void handleReconActivityMonitoring(uint32_t now);
    
    // Packet processing
    void handlePacketReception();
    void processQueuedPackets();
    
    // Tracking functions
    void trackNode(uint32_t nodeId, const char* protocol, float rssi);
    void trackRFActivity(uint8_t configIndex, float rssi);
    void trackTargetableDevice(uint32_t nodeId, uint8_t configIndex, float rssi, 
                               const char* protocol, const uint8_t* packetData, size_t packetLength);
    
    // Session key decryption
    void trySessionKeyDecryption(const uint8_t* data, size_t length, 
                                  uint32_t nodeId, uint32_t packetId);
    bool tryDecryptWithKey(const uint8_t* encryptedData, size_t encryptedLen,
                           const uint8_t* nonce, const uint8_t* key, 
                           uint16_t keyBits, uint8_t* decrypted);
    void extractAndPrintTextMessage(const uint8_t* decrypted, size_t encryptedLen, 
                                    const char* keyType);
    
    // Logging
    void logPacket(const uint8_t* data, size_t length, float rssi, float snr, const char* protocol);
    
    // Stress testing
    void initializeStressTesting();
};

// Global interrupt handler needs access to tool instance
extern LoRaReconTool* g_reconTool;

#endif // LORA_RECON_TOOL_H
