/**
 * Radio Controller - Hardware Interface for SX1262 LoRa Radio
 * 
 * Encapsulates all radio-specific operations including:
 * - Hardware initialization (SPI, pins, interrupts)
 * - Configuration management (frequency, bandwidth, spreading factor, etc.)
 * - Packet reception (interrupt-driven)
 * - Signal quality metrics (RSSI, SNR)
 * 
 * Separates radio hardware concerns from application logic.
 */

#ifndef RADIO_CONTROLLER_H
#define RADIO_CONTROLLER_H

#include <Arduino.h>
#include <RadioLib.h>
#include <atomic>
#include "data_structures.h"

// Hardware configuration for Heltec WiFi LoRa 32 V3
#define LORA_NSS    8
#define LORA_DIO1   14
#define LORA_RST    12
#define LORA_BUSY   13

// SPI pins for Heltec WiFi LoRa 32 V3
#define SCK  9
#define MISO 11
#define MOSI 10

/**
 * RadioController - Manages SX1262 radio hardware
 * 
 * Thread-safe packet reception using atomic flags and interrupt handling.
 * Provides clean interface for radio configuration and packet reading.
 */
class RadioController {
public:
    RadioController();
    ~RadioController();
    
    // Lifecycle
    bool initialize();
    void shutdown();
    
    // Configuration management
    bool applyConfig(const ScanConfig& config);
    bool setFrequency(float frequency);
    bool setBandwidth(float bandwidth);
    bool setSpreadingFactor(uint8_t sf);
    bool setSyncWord(uint8_t sw);
    
    // Reception control
    bool startReceive();
    bool stopReceive();
    
    // Packet handling
    bool hasPacket() const;
    int readPacket(uint8_t* buffer, size_t maxLength);
    size_t getPacketLength();
    
    // Signal metrics
    float getRSSI(bool useCache = true);
    float getSNR(bool useCache = true);
    
    // Interrupt handling (public for ISR access)
    void markPacketReceived() { 
        packetAvailable.store(true, std::memory_order_release); 
    }
    
    // Radio access (for advanced operations like packet replay)
    SX1262& getRadio() { return radio; }
    
private:
    SX1262 radio;
    std::atomic<bool> packetAvailable;
    
    // Cached signal metrics (avoid repeated SPI reads)
    float cachedRSSI;
    float cachedSNR;
    uint32_t lastMetricUpdate;
    static const uint32_t METRIC_CACHE_MS = 100;
    
    // Apply protocol-specific parameters
    void applyProtocolParameters(const char* protocol);
};

// Global instance for interrupt handler access
extern RadioController* g_radioController;

// Interrupt service routine (must be global)
void IRAM_ATTR radioISR();

#endif // RADIO_CONTROLLER_H
