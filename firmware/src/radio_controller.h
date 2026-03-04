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
#include "config.h"

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
    void runDiagnostics();
    
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

    // Transmit power (dBm). Use before transmit; restore to OUTPUT_POWER_DBM after.
    void setOutputPower(int dBm);
    
    // Interrupt handling (public for ISR access)
    void markPacketReceived() {
        isrCount.fetch_add(1, std::memory_order_relaxed);
        packetAvailable.store(true, std::memory_order_release);
    }

    // Diagnostic: total number of times the DIO1 ISR has fired since boot
    uint32_t getISRCount() const { return isrCount.load(std::memory_order_relaxed); }

    // Diagnostic: RadioLib error code from last startReceive() call (0 = success)
    int getLastRxError() const { return lastRxError; }

    // Polling fallback: read SX1262 IRQ status register directly via SPI.
    // Sets packetAvailable if RxDone is set. Increments irqPollCount each time
    // a packet is detected this way (vs via DIO1 interrupt). Used to diagnose
    // DIO1 pin mapping issues: irqPollCount>0 + isrCount==0 = DIO1 not on GPIO 14.
    void pollIrqStatus();
    uint32_t getIrqPollCount() const { return irqPollCount.load(std::memory_order_relaxed); }
    
    // Radio access (for advanced operations like packet replay)
    SX1262& getRadio() { return *radio; }
    
private:
    SX1262* radio;  // Constructed after SPI.begin() in initialize()
    std::atomic<bool> packetAvailable;
    std::atomic<uint32_t> isrCount{0};     // Total DIO1 ISR firings since boot
    std::atomic<uint32_t> irqPollCount{0}; // Packets detected via SPI IRQ polling (not DIO1)
    int lastRxError{1};  // Result of last startReceive() call; 1 = never called, 0 = success
    
    // Cached signal metrics (avoid repeated SPI reads)
    float cachedRSSI;
    float cachedSNR;
    uint32_t lastMetricUpdate;
    
    // Apply protocol-specific parameters
    void applyProtocolParameters(const char* protocol);
};

// Global instance for interrupt handler access
extern RadioController* g_radioController;

// Interrupt service routine (must be global)
void IRAM_ATTR radioISR();

#endif // RADIO_CONTROLLER_H
