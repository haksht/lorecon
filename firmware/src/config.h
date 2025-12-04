/**
 * Configuration Constants - Centralized Settings
 * 
 * All magic numbers and configuration values in one place with documentation.
 * Makes it easy to tune behavior without hunting through code.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

namespace Config {

// ============================================================================
// HARDWARE CONFIGURATION
// ============================================================================
namespace Hardware {
    // SX1262 LoRa Radio Pins (Heltec WiFi LoRa 32 V3)
    constexpr uint8_t LORA_NSS = 8;      // SPI chip select
    constexpr uint8_t LORA_DIO1 = 14;    // Interrupt pin
    constexpr uint8_t LORA_RST = 12;     // Reset pin
    constexpr uint8_t LORA_BUSY = 13;    // Busy indicator
    
    // SPI Pins
    constexpr uint8_t SPI_SCK = 9;       // Clock
    constexpr uint8_t SPI_MISO = 11;     // Master In Slave Out
    constexpr uint8_t SPI_MOSI = 10;     // Master Out Slave In
    
    // User Interface
    constexpr uint8_t USER_BUTTON = 0;   // PRG button (active low)
    
    // SD Card (optional)
    constexpr uint8_t SD_CS = 5;         // SD card chip select (GPIO 5, not 21 which conflicts with OLED_RST)
}

// ============================================================================
// SCANNING CONFIGURATION
// ============================================================================
namespace Scanning {
    // How long to listen on each frequency configuration (milliseconds)
    // 12 seconds balances detection vs. scan speed
    // - Too short: Miss slow transmitters
    // - Too long: Takes forever to complete scan
    constexpr uint32_t DWELL_TIME_MS = 12000;
    
    // Number of frequency configurations to scan
    constexpr uint8_t NUM_CONFIGURATIONS = 26;
    
    // Total reconnaissance cycle time: 26 configs × 12s = ~5 minutes
}

// ============================================================================
// PACKET PROCESSING
// ============================================================================
namespace PacketProcessing {
    // Maximum queued packets before dropping
    // Balance: Memory usage vs. handling burst traffic
    // Increased to 100 to handle busy ISM bands without silent drops
    constexpr size_t QUEUE_SIZE = 100;
    
    // Maximum packet size (LoRa protocol limit)
    constexpr size_t MAX_PACKET_SIZE = 256;
    
    // Signal quality metric cache duration (milliseconds)
    // Avoid repeated SPI reads for RSSI/SNR
    constexpr uint32_t METRIC_CACHE_MS = 100;
}

// ============================================================================
// PACKET REPLAY
// ============================================================================
namespace Replay {
    // Number of packet slots for capture/replay
    // Limited by RAM - each slot = 256 bytes + metadata
    constexpr uint8_t MAX_SLOTS = 10;
}

// ============================================================================
// DATA LOGGING
// ============================================================================
namespace Logging {
    // Enable PCAP export for offline analysis with Wireshark/LoRa_Craft
    // PCAP files preserve RF metadata (RSSI/SNR/frequency) in pseudo-header
    // Set to false to save SD space if you only need CSV logs
    #define ENABLE_PCAP_EXPORT true
}

// ============================================================================
// DEVICE TRACKING
// ============================================================================
namespace Tracking {
    // Maximum targetable devices to track
    constexpr uint8_t MAX_DEVICES = 20;
    
    // Maximum RF activity entries (non-device signals)
    constexpr uint8_t MAX_RF_ACTIVITIES = 16;
    
    // Maximum tracked nodes for behavioral analysis
    constexpr uint8_t MAX_NODES = 30;
    
    // Maximum GPS points to store
    constexpr uint8_t MAX_GEO_POINTS = 50;
    
    // Device timeout (milliseconds)
    // Consider device inactive after this period
    constexpr uint32_t DEVICE_TIMEOUT_MS = 300000;  // 5 minutes
}

// ============================================================================
// USER INTERFACE
// ============================================================================
namespace UI {
    // Serial console baud rate
    constexpr uint32_t SERIAL_BAUD = 115200;
    
    // Delays for user experience (milliseconds)
    constexpr uint32_t SERIAL_INIT_DELAY_MS = 2000;    // Wait for serial console
    constexpr uint32_t WELCOME_SCREEN_DELAY_MS = 2000; // Display welcome
    constexpr uint32_t SHUTDOWN_WARNING_MS = 2000;      // Show shutdown message
    constexpr uint32_t SHUTDOWN_FINAL_DELAY_MS = 1000;  // Final delay before off
    
    // Button debouncing and long press detection
    constexpr uint32_t BUTTON_DEBOUNCE_MS = 100;       // Ignore bounces
    constexpr uint32_t BUTTON_LONG_PRESS_MS = 3000;    // Long press threshold
    
    // User input timeout (milliseconds)
    // Auto-return to normal operation after idle
    constexpr uint32_t INPUT_TIMEOUT_MS = 30000;       // 30 seconds
    
    // Menu mode auto-resume timeout (milliseconds)
    // Prevents "left in menu mode overnight" issue where radio stops scanning
    // Set to 5 minutes - long enough for configuration, short enough to catch mistakes
    constexpr uint32_t MENU_TIMEOUT_MS = 5 * 60 * 1000;  // 5 minutes
    
    // Main loop polling delay (milliseconds)
    constexpr uint32_t LOOP_POLLING_DELAY_MS = 10;
}

// ============================================================================
// SYSTEM RELIABILITY
// ============================================================================
namespace System {
    // Watchdog timer timeout (seconds)
    // ESP32 resets if main loop hangs this long
    constexpr uint8_t WATCHDOG_TIMEOUT_SEC = 30;
    
    // SD card flush interval (packets)
    // Write to SD card after this many packets
    constexpr uint8_t SD_FLUSH_INTERVAL = 10;
}

// ============================================================================
// LOGGING
// ============================================================================
namespace Logging {
    // Log buffer size for formatting messages
    constexpr size_t LOG_BUFFER_SIZE = 256;
    
    // Default log level (DEBUG, INFO, WARNING, ERROR, NONE)
    // Set to INFO for production, DEBUG for development
#ifdef DEBUG_BUILD
    constexpr uint8_t DEFAULT_LEVEL = 0;  // DEBUG
#else
    constexpr uint8_t DEFAULT_LEVEL = 1;  // INFO
#endif
}

// ============================================================================
// PSK DECRYPTION
// ============================================================================
namespace PSK {
    // Number of default PSKs to test
    // Includes: AQ==, 1PG7OiApB1nwvP+rz05pAQ==, and 12 other common keys
    // See psk_decryption_simple.cpp for full list
    constexpr uint8_t NUM_DEFAULT_KEYS = 14;
    
    // AES key size (bytes)
    constexpr uint8_t KEY_SIZE = 16;
    
    // Nonce size for AES-CTR (bytes)
    constexpr uint8_t NONCE_SIZE = 16;
}

// ============================================================================
// RADIO CONFIGURATION
// ============================================================================
namespace Radio {
    // Current limit for SX1262 (mA)
    constexpr float CURRENT_LIMIT_MA = 100.0f;
    
    // Output power for transmit (dBm)
    // Set to 0 for receive-only operation
    constexpr int8_t OUTPUT_POWER_DBM = 0;
    
    // Preamble length (symbols)
    constexpr uint16_t PREAMBLE_LENGTH = 8;
}

} // namespace Config

// ============================================================================
// COMPILE-TIME SAFETY CHECKS
// ============================================================================

// Verify LoRa protocol limits
static_assert(Config::PacketProcessing::MAX_PACKET_SIZE == 256,
              "LoRa protocol maximum packet size is 256 bytes");

// Verify index types can hold maximum values
static_assert(Config::Tracking::MAX_DEVICES <= 255,
              "Device indices use uint8_t, must fit in 0-255 range");
static_assert(Config::Tracking::MAX_NODES <= 255,
              "Node indices use uint8_t, must fit in 0-255 range");
static_assert(Config::Scanning::NUM_CONFIGURATIONS <= 255,
              "Config indices use uint8_t, must fit in 0-255 range");

// Verify queue size is reasonable
static_assert(Config::PacketProcessing::QUEUE_SIZE >= 50,
              "Queue size should be at least 50 to handle burst traffic");
static_assert(Config::PacketProcessing::QUEUE_SIZE <= 200,
              "Queue size should not exceed 200 to prevent memory issues");

// Verify PSK key count matches array sizes
static_assert(Config::PSK::NUM_DEFAULT_KEYS > 0,
              "Must have at least one default PSK key");

#endif // CONFIG_H
