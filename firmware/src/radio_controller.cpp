/**
 * Radio Controller Implementation
 */

#include "radio_controller.h"
#include "logger.h"
#include "config.h"
#include <SPI.h>

// Global pointer for ISR access
RadioController* g_radioController = nullptr;

// -- Board-specific radio init helpers ----------------------------------------

// T3-S3 and T-Beam Supreme both use SX1262 with an external TCXO at 1.8 V and
// DIO2 as the RF-switch control line.  The only difference is the log tag.
static int initTcxoRadio(SX1262* radio, const char* boardName) {
    LOG_INFO("%s: Initializing SX1262 (TCXO 1.8V via DIO3)...", boardName);
    int state = radio->begin(
        868.0,   // initial freq  -  overridden by applyConfig()
        125.0,   // bw kHz
        9,       // sf
        7,       // cr
        RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
        10,      // power dBm
        8,       // preamble length
        1.8,     // TCXO voltage  -  must be passed to begin() so calibration uses it
        false    // useRegulatorLDO (false = DCDC)
    );
    if (state != RADIOLIB_ERR_NONE) {
        LOG_ERROR("%s: SX1262 begin() failed (error: %d)", boardName, state);
        return state;
    }
    LOG_INFO("%s: begin() succeeded", boardName);
    state = radio->setDio2AsRfSwitch(true);
    if (state != RADIOLIB_ERR_NONE) LOG_WARN("setDio2AsRfSwitch: %d", state);
    else                             LOG_INFO("DIO2 RF switch enabled");
    return RADIOLIB_ERR_NONE;
}

#if defined(BOARD_HELTEC_V4)
// Heltec V4 adds an external FEM (GC1109 on V4.2, KCT8103L on V4.3) that must
// be explicitly enabled via GPIO, plus DIO2 RF-switch control.
static void enableHeltecV4Fem(SX1262* radio) {
    pinMode(Config::Hardware::V4_FEM_EN, OUTPUT);
    digitalWrite(Config::Hardware::V4_FEM_EN, HIGH);
    pinMode(Config::Hardware::V4_FEM_PA_CTX, OUTPUT);
    digitalWrite(Config::Hardware::V4_FEM_PA_CTX, LOW);
    LOG_INFO("V4 FEM enabled (EN=GPIO%u HIGH, PA_CTX=GPIO%u LOW)",
             Config::Hardware::V4_FEM_EN, Config::Hardware::V4_FEM_PA_CTX);
    int state = radio->setDio2AsRfSwitch(true);
    if (state != RADIOLIB_ERR_NONE) LOG_WARN("setDio2AsRfSwitch: %d", state);
    else                             LOG_INFO("V4 DIO2 RF switch enabled");
}
#endif

// -----------------------------------------------------------------------------

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

    // Explicit hardware reset of SX1262 before SPI init.
    // After a full flash erase, GPIO pin states may be undefined  -  the radio
    // can be in a stuck state where it won't respond to SPI until reset.
    pinMode(Config::Hardware::LORA_RST, OUTPUT);
    digitalWrite(Config::Hardware::LORA_RST, LOW);
    delay(10);
    digitalWrite(Config::Hardware::LORA_RST, HIGH);
    delay(20);  // SX1262 datasheet: 10ms max reset time
    LOG_INFO("SX1262 hardware reset complete (RST pin %d)", Config::Hardware::LORA_RST);

    // Initialize SPI bus with custom pins BEFORE creating Module
    // Use explicit FSPI (SPI2) bus  -  after a full flash erase, the default SPI
    // object may not reliably map to FSPI on ESP32-S3.
    static SPIClass loraSPI(FSPI);
    loraSPI.begin(Config::Hardware::SPI_SCK, Config::Hardware::SPI_MISO,
                  Config::Hardware::SPI_MOSI, Config::Hardware::LORA_NSS);

    LOG_INFO("SPI initialized on FSPI bus (SCK:%d MISO:%d MOSI:%d NSS:%d)",
              Config::Hardware::SPI_SCK, Config::Hardware::SPI_MISO,
              Config::Hardware::SPI_MOSI, Config::Hardware::LORA_NSS);

    // Create radio with explicit SPI bus reference
    Module* module = new Module(Config::Hardware::LORA_NSS, Config::Hardware::LORA_DIO1,
                                Config::Hardware::LORA_RST, Config::Hardware::LORA_BUSY,
                                loraSPI);
    if (!module) {
        LOG_ERROR("Failed to allocate radio Module (OOM)");
        return false;
    }
    radio = new SX1262(module);
    if (!radio) {
        delete module;
        LOG_ERROR("Failed to allocate SX1262 (OOM)");
        return false;
    }
    
    #if defined(BOARD_T3_S3)
        // T3-S3: TCXO on DIO3 at 1.8V.  tcxoVoltage MUST be in begin() so
        // calibration uses the correct oscillator reference (not crystal mode).
        if (initTcxoRadio(radio, "T3-S3") != RADIOLIB_ERR_NONE) return false;
    #elif defined(BOARD_TBEAM_SUPREME)
        // T-Beam Supreme: TCXO powered continuously by AXP2101 ALDO3.
        // DIO3 does not switch TCXO power; we still pass tcxoVoltage=1.8 so
        // RadioLib puts the SX1262 in TCXO mode (not crystal mode) for calibration.
        if (initTcxoRadio(radio, "T-Beam Supreme") != RADIOLIB_ERR_NONE) return false;
    #else
        // Heltec V3/V4: crystal oscillator  -  no tcxoVoltage argument.
        int state = radio->begin();
        if (state != RADIOLIB_ERR_NONE) {
            LOG_ERROR("SX1262 initialization failed (error: %d)", state);
            return false;
        }
        #if defined(BOARD_HELTEC_V4)
            // V4 has an external FEM that gates the LNA/PA  -  must be enabled
            // before reception is possible.  GPIO 46 is a strapping pin that
            // defaults LOW (GC1109 RX mode) so it is left as INPUT.
            enableHeltecV4Fem(radio);
        #endif
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
        LOG_WARN("Radio not responding (error %d), attempting hardware reset recovery...", state);

        // Hardware reset  -  only reliable recovery path.
        // Note: SPI bus is on the explicit loraSPI(FSPI) instance created in initialize().
        // Re-initializing the default SPI object here would have no effect on the radio.
        pinMode(Config::Hardware::LORA_RST, OUTPUT);
        digitalWrite(Config::Hardware::LORA_RST, LOW);
        delay(10);
        digitalWrite(Config::Hardware::LORA_RST, HIGH);
        delay(50);

        #if defined(BOARD_T3_S3) || defined(BOARD_TBEAM_SUPREME)
            state = initTcxoRadio(radio, "recovery");
        #else
            state = radio->begin();
            #if defined(BOARD_HELTEC_V4)
                if (state == RADIOLIB_ERR_NONE) enableHeltecV4Fem(radio);
            #endif
        #endif
        LOG_INFO("Re-init after reset: %d", state);
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
// All protocols use the same promiscuous-mode settings for passive capture.
// (Previous per-protocol branches for "Meshtastic_MSG" / "Meshtastic_MF" were
// dead code  -  identifyProtocol() returns "Meshtastic", never those strings.)
void RadioController::applyProtocolParameters(const char* /*protocol*/) {
    radio->setCodingRate(5);    // 4/5 coding rate
    radio->setPreambleLength(8);
    radio->setCRC(false);       // Promiscuous mode: accept frames regardless of CRC
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

// Poll SX1262 IRQ status register directly (fallback for DIO1 pin issues).
// If the RxDone flag is set, marks a packet as available and clears the flag.
void RadioController::pollIrqStatus() {
    if (!radio) return;
    // Skip if a packet is already pending  -  readData() will clear the IRQ when consumed.
    if (packetAvailable.load(std::memory_order_acquire)) return;
    uint16_t irq = radio->getIrqStatus();
    if (irq & RADIOLIB_SX126X_IRQ_RX_DONE) {
        irqPollCount.fetch_add(1, std::memory_order_relaxed);
        packetAvailable.store(true, std::memory_order_release);
    }
}

// Start receiving packets
bool RadioController::startReceive() {
    int state = radio->startReceive();
    lastRxError = state;
    if (state == RADIOLIB_ERR_NONE) {
        LOG_INFO("startReceive() OK  -  radio in continuous RX mode");
    } else {
        LOG_ERROR("startReceive() FAILED (error: %d)", state);
    }
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

// Set transmit output power
void RadioController::setOutputPower(int dBm) {
    radio->setOutputPower(dBm);
}
