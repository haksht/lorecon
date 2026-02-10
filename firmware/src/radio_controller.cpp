/**
 * Radio Controller Implementation
 */

#include "radio_controller.h"
#include "logger.h"
#include "config.h"
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
    : radio(nullptr)  // Constructed in initialize() after SPI.begin()
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
    // Note: Don't delete radio - RadioLib's SX1262 has non-virtual destructor
    // Let it persist (singleton pattern anyway)
    g_radioController = nullptr;
}

// Initialize radio hardware
bool RadioController::initialize() {
    LOG_INFO("Initializing SX1262...");
    
    // Initialize SPI bus with custom pins BEFORE creating Module
    // Note: SPI.begin(SCK, MISO, MOSI, SS) - don't pass LORA_NSS here,
    // RadioLib handles chip select separately
    SPI.begin(Config::Hardware::SPI_SCK, Config::Hardware::SPI_MISO, 
              Config::Hardware::SPI_MOSI);
    
    LOG_INFO("SPI initialized (SCK:%d MISO:%d MOSI:%d)",
              Config::Hardware::SPI_SCK, Config::Hardware::SPI_MISO, 
              Config::Hardware::SPI_MOSI);
    
    // Now create radio object (after SPI is configured with correct pins)
    // LilyGO factory code: uses default SPI (no explicit pass)
    radio = new SX1262(new Module(Config::Hardware::LORA_NSS, Config::Hardware::LORA_DIO1, 
                                   Config::Hardware::LORA_RST, Config::Hardware::LORA_BUSY));
    
    #if defined(BOARD_T3_S3)
        // T3-S3: SX1262 with TCXO on DIO3 at 1.8V
        // CRITICAL: tcxoVoltage MUST be passed to begin() so it's configured
        // BEFORE the internal "calibrate all blocks" step. Setting TCXO after
        // begin() causes PLL lock failures (-2) on setFrequency().
        // Ref: Meshtastic variant.h uses SX126X_DIO3_TCXO_VOLTAGE = 1.8
        LOG_INFO("T3-S3: Initializing SX1262 (TCXO 1.8V)...");
        int state = radio->begin(
            868.0,    // freq (initial, changed later by applyConfig)
            125.0,    // bw (kHz)
            9,        // sf
            7,        // cr
            RADIOLIB_SX126X_SYNC_WORD_PRIVATE,  // syncWord
            10,       // power (dBm)
            8,        // preambleLength
            1.8,      // tcxoVoltage - set here so calibration uses correct reference
            false     // useRegulatorLDO (false = DCDC)
        );
        if (state != RADIOLIB_ERR_NONE) {
            LOG_ERROR("SX1262 begin failed (error: %d)", state);
            return false;
        }

        LOG_INFO("T3-S3: begin() succeeded");

        // Enable DIO2 as RF switch control (required for T3-S3 antenna switching)
        state = radio->setDio2AsRfSwitch(true);
        if (state != RADIOLIB_ERR_NONE) {
            LOG_WARN("setDio2AsRfSwitch: %d", state);
        } else {
            LOG_INFO("DIO2 RF switch enabled");
        }
    #else
        // Non-T3-S3 boards: just call begin()
        int state = radio->begin();
        if (state != RADIOLIB_ERR_NONE) {
            LOG_ERROR("SX1262 initialization failed (error: %d)", state);
            return false;
        }
    #endif
    
    LOG_INFO("SX1262 initialized successfully");
    
    // Configure interrupt pin and handler
    pinMode(Config::Hardware::LORA_DIO1, INPUT);
    radio->setDio1Action(radioISR);
    LOG_DEBUG("Interrupt configured on GPIO %d", Config::Hardware::LORA_DIO1);
    
    // Set receive-only mode (no transmission by default)
    radio->setCurrentLimit(Config::Radio::CURRENT_LIMIT_MA);
    radio->setOutputPower(Config::Radio::OUTPUT_POWER_DBM);
    
    return true;
}

// Shutdown radio
void RadioController::shutdown() {
    stopReceive();
    // Radio will be cleaned up by destructor
}

// Run radio diagnostics (call when USB serial is known to be active)
void RadioController::runDiagnostics() {
    LOG_INFO("=== RADIO DIAGNOSTICS ===");
    LOG_INFO("Radio pointer: %s", radio ? "OK" : "NULL");
    if (!radio) return;

    // Verify SPI bus is still configured correctly for LoRa pins
    LOG_INFO("SPI pins: SCK=%d MISO=%d MOSI=%d NSS=%d",
             Config::Hardware::SPI_SCK, Config::Hardware::SPI_MISO,
             Config::Hardware::SPI_MOSI, Config::Hardware::LORA_NSS);

    int state = radio->standby();
    LOG_INFO("standby(): %d", state);

    if (state == RADIOLIB_ERR_NONE) {
        state = radio->setFrequency(906.875);
        LOG_INFO("setFrequency(906.875): %d", state);
    }

    if (state != RADIOLIB_ERR_NONE) {
        LOG_WARN("Radio not responding (error %d), attempting recovery...", state);

        // Re-assert SPI bus pins (in case something reconfigured them)
        SPI.end();
        SPI.begin(Config::Hardware::SPI_SCK, Config::Hardware::SPI_MISO,
                  Config::Hardware::SPI_MOSI);
        LOG_INFO("SPI bus re-initialized");

        // Hardware reset the radio
        pinMode(Config::Hardware::LORA_RST, OUTPUT);
        digitalWrite(Config::Hardware::LORA_RST, LOW);
        delay(10);
        digitalWrite(Config::Hardware::LORA_RST, HIGH);
        delay(50);

        state = radio->begin(906.875, 125.0, 9, 7,
                             RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                             10, 8, 1.8, false);
        LOG_INFO("Re-init begin(906.875, tcxo=1.8): %d", state);

        if (state == RADIOLIB_ERR_NONE) {
            radio->setDio2AsRfSwitch(true);
            state = radio->setFrequency(902.125);
            LOG_INFO("After re-init, setFrequency(902.125): %d", state);
        }
    }
    LOG_INFO("=== END DIAGNOSTICS ===");
}

// Apply complete scan configuration
bool RadioController::applyConfig(const ScanConfig& config) {
    LOG_DEBUG("Scanning: %s (%.3f MHz, SF%d, BW%.0f, SW:0x%02X)",
              config.protocol, config.frequency, config.spreadingFactor,
              config.bandwidth, config.syncWord);

    // SX1262 requires STDBY mode before changing frequency/modulation params
    radio->standby();

    // Apply frequency
    int state = radio->setFrequency(config.frequency);
    if (state != RADIOLIB_ERR_NONE) {
        LOG_ERROR("setFrequency(%.3f) failed: %d", config.frequency, state);
        return false;
    }
    
    // Apply bandwidth
    state = radio->setBandwidth(config.bandwidth);
    if (state != RADIOLIB_ERR_NONE) {
        LOG_ERROR("setBandwidth failed: %d", state);
        return false;
    }
    
    // Apply spreading factor
    state = radio->setSpreadingFactor(config.spreadingFactor);
    if (state != RADIOLIB_ERR_NONE) {
        LOG_ERROR("setSpreadingFactor failed: %d", state);
        return false;
    }
    
    // Apply sync word
    state = radio->setSyncWord(config.syncWord);
    if (state != RADIOLIB_ERR_NONE) {
        LOG_ERROR("setSyncWord failed: %d", state);
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
        radio->setCodingRate(5);         // 4/5 for message content
        radio->setPreambleLength(8);     // Standard for messages
        radio->setCRC(false);            // Promiscuous mode for better capture
    } else if (strstr(protocol, "Meshtastic_MF") != nullptr) {
        radio->setCodingRate(8);         // 4/8 for MediumFast
        radio->setPreambleLength(12);    // Longer preamble
        radio->setCRC(false);            // Promiscuous mode
    } else {
        radio->setCodingRate(5);         // 4/5 for routing packets
        radio->setPreambleLength(8);     // Standard
        radio->setCRC(false);            // Promiscuous mode
    }
    
    radio->explicitHeader();
}
// Set frequency
bool RadioController::setFrequency(float frequency) {
    int state = radio->setFrequency(frequency);
    return (state == RADIOLIB_ERR_NONE);
}

// Set bandwidth
bool RadioController::setBandwidth(float bandwidth) {
    int state = radio->setBandwidth(bandwidth);
    return (state == RADIOLIB_ERR_NONE);
}

// Set spreading factor
bool RadioController::setSpreadingFactor(uint8_t sf) {
    int state = radio->setSpreadingFactor(sf);
    return (state == RADIOLIB_ERR_NONE);
}

// Set sync word
bool RadioController::setSyncWord(uint8_t sw) {
    int state = radio->setSyncWord(sw);
    return (state == RADIOLIB_ERR_NONE);
}

// Start receiving packets
bool RadioController::startReceive() {
    int state = radio->startReceive();
    return (state == RADIOLIB_ERR_NONE);
}

// Stop receiving packets
bool RadioController::stopReceive() {
    int state = radio->standby();
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
    int state = radio->readData(buffer, maxLength);
    
    if (state == RADIOLIB_ERR_NONE) {
        // Cache signal metrics
        cachedRSSI = radio->getRSSI();
        cachedSNR = radio->getSNR();
        lastMetricUpdate = millis();
        
        return radio->getPacketLength(false);
    }
    
    return state; // Return error code
}

// Get packet length
size_t RadioController::getPacketLength() {
    return radio->getPacketLength(false);
}

// Get RSSI
float RadioController::getRSSI(bool useCache) {
    if (useCache && (millis() - lastMetricUpdate < Config::PacketProcessing::METRIC_CACHE_MS)) {
        return cachedRSSI;
    }
    
    cachedRSSI = radio->getRSSI();
    lastMetricUpdate = millis();
    return cachedRSSI;
}

// Get SNR
float RadioController::getSNR(bool useCache) {
    if (useCache && (millis() - lastMetricUpdate < Config::PacketProcessing::METRIC_CACHE_MS)) {
        return cachedSNR;
    }
    
    cachedSNR = radio->getSNR();
    lastMetricUpdate = millis();
    return cachedSNR;
}
