/**
 * Radio Controller Implementation
 */

#include "radio_controller.h"
#include "error_handler.h"
#include <SPI.h>

// Global pointer for ISR access
RadioController* g_radioController = nullptr;

// Interrupt service routine
void IRAM_ATTR radioISR() {
    if (g_radioController) {
        g_radioController->markPacketReceived();
    }
}

// Constructor
RadioController::RadioController()
    : radio(new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY))
    , packetAvailable(false)
    , cachedRSSI(0)
    , cachedSNR(0)
    , lastMetricUpdate(0)
{
    g_radioController = this;
}

// Destructor
RadioController::~RadioController() {
    shutdown();
    g_radioController = nullptr;
}

// Initialize radio hardware
bool RadioController::initialize() {
    Serial.println("[RADIO] Initializing SX1262...");
    
    // Initialize SPI bus
    SPI.begin(SCK, MISO, MOSI, LORA_NSS);
    Serial.printf("[RADIO] SPI initialized (SCK:%d MISO:%d MOSI:%d NSS:%d)\n", 
                  SCK, MISO, MOSI, LORA_NSS);
    
    // Initialize radio module
    int state = radio.begin();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[RADIO] Initialization FAILED (error: %d)\n", state);
        LOG_RADIO_ERROR(ErrorCodes::RADIO_INIT_FAILED, "SX1262 initialization failed");
        return false;
    }
    
    Serial.println("[RADIO] SX1262 initialized successfully");
    
    // Configure interrupt pin and handler
    pinMode(LORA_DIO1, INPUT);
    radio.setDio1Action(radioISR);
    Serial.printf("[RADIO] Interrupt configured on GPIO %d\n", LORA_DIO1);
    
    // Set receive-only mode (no transmission by default)
    radio.setCurrentLimit(100);
    radio.setOutputPower(0);
    
    return true;
}

// Shutdown radio
void RadioController::shutdown() {
    stopReceive();
    // Radio will be cleaned up by destructor
}

// Apply complete scan configuration
bool RadioController::applyConfig(const ScanConfig& config) {
    Serial.printf("[RADIO] Configuring: %s (%.3f MHz, SF%d, BW%.0f, SW:0x%02X)\n", 
                  config.protocol, config.frequency, config.spreadingFactor, 
                  config.bandwidth, config.syncWord);
    
    // Apply frequency
    int state = radio.setFrequency(config.frequency);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[RADIO] setFrequency failed: %d\n", state);
        return false;
    }
    
    // Apply bandwidth
    state = radio.setBandwidth(config.bandwidth);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[RADIO] setBandwidth failed: %d\n", state);
        return false;
    }
    
    // Apply spreading factor
    state = radio.setSpreadingFactor(config.spreadingFactor);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[RADIO] setSpreadingFactor failed: %d\n", state);
        return false;
    }
    
    // Apply sync word
    state = radio.setSyncWord(config.syncWord);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[RADIO] setSyncWord failed: %d\n", state);
        return false;
    }
    
    // Apply protocol-specific parameters
    applyProtocolParameters(config.protocol);
    
    return true;
}

// Apply protocol-specific radio parameters
void RadioController::applyProtocolParameters(const char* protocol) {
    // Try different parameters based on protocol type
    if (strstr(protocol, "Meshtastic_MSG") != nullptr) {
        radio.setCodingRate(5);         // 4/5 for message content
        radio.setPreambleLength(8);     // Standard for messages
        radio.setCRC(false);            // Promiscuous mode for better capture
    } else if (strstr(protocol, "Meshtastic_MF") != nullptr) {
        radio.setCodingRate(8);         // 4/8 for MediumFast
        radio.setPreambleLength(12);    // Longer preamble
        radio.setCRC(false);            // Promiscuous mode
    } else {
        radio.setCodingRate(5);         // 4/5 for routing packets
        radio.setPreambleLength(8);     // Standard
        radio.setCRC(false);            // Promiscuous mode
    }
    
    radio.explicitHeader();
}

// Set frequency
bool RadioController::setFrequency(float frequency) {
    int state = radio.setFrequency(frequency);
    return (state == RADIOLIB_ERR_NONE);
}

// Set bandwidth
bool RadioController::setBandwidth(float bandwidth) {
    int state = radio.setBandwidth(bandwidth);
    return (state == RADIOLIB_ERR_NONE);
}

// Set spreading factor
bool RadioController::setSpreadingFactor(uint8_t sf) {
    int state = radio.setSpreadingFactor(sf);
    return (state == RADIOLIB_ERR_NONE);
}

// Set sync word
bool RadioController::setSyncWord(uint8_t sw) {
    int state = radio.setSyncWord(sw);
    return (state == RADIOLIB_ERR_NONE);
}

// Start receiving packets
bool RadioController::startReceive() {
    int state = radio.startReceive();
    return (state == RADIOLIB_ERR_NONE);
}

// Stop receiving packets
bool RadioController::stopReceive() {
    int state = radio.standby();
    return (state == RADIOLIB_ERR_NONE);
}

// Check if packet is available
bool RadioController::hasPacket() const {
    return packetAvailable.load(std::memory_order_acquire);
}

// Read packet from radio
int RadioController::readPacket(uint8_t* buffer, size_t maxLength) {
    // Clear flag first
    packetAvailable.store(false, std::memory_order_release);
    
    // Read packet data
    int state = radio.readData(buffer, maxLength);
    
    if (state == RADIOLIB_ERR_NONE) {
        // Cache signal metrics
        cachedRSSI = radio.getRSSI();
        cachedSNR = radio.getSNR();
        lastMetricUpdate = millis();
        
        return radio.getPacketLength(false);
    }
    
    return state; // Return error code
}

// Get packet length
size_t RadioController::getPacketLength() {
    return radio.getPacketLength(false);
}

// Get RSSI
float RadioController::getRSSI(bool useCache) {
    if (useCache && (millis() - lastMetricUpdate < METRIC_CACHE_MS)) {
        return cachedRSSI;
    }
    
    cachedRSSI = radio.getRSSI();
    lastMetricUpdate = millis();
    return cachedRSSI;
}

// Get SNR
float RadioController::getSNR(bool useCache) {
    if (useCache && (millis() - lastMetricUpdate < METRIC_CACHE_MS)) {
        return cachedSNR;
    }
    
    cachedSNR = radio.getSNR();
    lastMetricUpdate = millis();
    return cachedSNR;
}
