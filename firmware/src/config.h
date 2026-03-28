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
// VERSION
// ============================================================================
constexpr const char* VERSION = "2.4.0";

// ============================================================================
// HARDWARE CONFIGURATION
// ============================================================================
namespace Hardware {

    // Sentinel value for unused/unavailable pins
    constexpr uint8_t PIN_UNUSED = 0xFF;

    // Board-specific pin definitions
    #if defined(BOARD_T3_S3)
        // ========================================================================
        // LilyGO T3-S3 V1.2/V1.3 Pin Configuration
        // ========================================================================
        // Board: ESP32-S3FH4R2 (4MB flash, 2MB PSRAM) + SX1262 + SSD1306 OLED (I2C) + SD Card
        // Display: 0.96" OLED on I2C (avoids SPI conflicts)
        // SD Card: Native slot on separate SPI pins

        // SX1262 LoRa Radio Pins
        constexpr uint8_t LORA_NSS = 7;      // SPI chip select
        constexpr uint8_t LORA_DIO1 = 33;    // Interrupt pin (DIO1)
        constexpr uint8_t LORA_RST = 8;      // Reset pin
        constexpr uint8_t LORA_BUSY = 34;    // Busy indicator

        // LoRa SPI Bus Pins
        constexpr uint8_t SPI_SCK = 5;       // Clock
        constexpr uint8_t SPI_MISO = 3;      // Master In Slave Out
        constexpr uint8_t SPI_MOSI = 6;      // Master Out Slave In

        // SD Card Pins (separate from LoRa SPI)
        constexpr uint8_t SD_CS = 13;        // SD card chip select
        constexpr uint8_t SD_SCK = 14;       // SD card clock
        constexpr uint8_t SD_MISO = 2;       // SD card MISO
        constexpr uint8_t SD_MOSI = 11;      // SD card MOSI

        // I2C Display Pins (SSD1306 OLED)
        constexpr uint8_t OLED_SDA = 18;     // I2C data
        constexpr uint8_t OLED_SCL = 17;     // I2C clock
        constexpr uint8_t OLED_RST = PIN_UNUSED;  // No reset pin (software reset)

        // User Interface
        constexpr uint8_t USER_BUTTON = 0;   // BOOT button (active low)
        constexpr uint8_t USER_LED = 37;     // Onboard LED

        // Battery Monitoring
        constexpr uint8_t VBAT_ADC_PIN = 1;  // GPIO 1 - battery voltage ADC
        constexpr uint8_t VBAT_CTRL_PIN = PIN_UNUSED; // No control pin needed
        constexpr float VBAT_SCALE = 2.0f;   // Voltage divider scaling factor

    #elif defined(BOARD_TBEAM_SUPREME)
        // ========================================================================
        // LilyGo T-Beam Supreme Pin Configuration
        // ========================================================================
        // Board: ESP32-S3FN8 (8MB QIO flash, 8MB Quad PSRAM) + SX1262 + SH1106 OLED
        //        + SD Card + GPS (L76K/MAX-M10S) + AXP2101 PMIC
        // GPIO 33-37 are safe (Quad PSRAM, NOT Octal — no OPI conflict)
        // AXP2101 powers all peripherals; must initialize before radio/SD/GPS.

        // SX1262 LoRa Radio Pins (FSPI/SPI2 — default FSPI pins on ESP32-S3)
        constexpr uint8_t LORA_NSS = 10;     // SPI chip select
        constexpr uint8_t LORA_DIO1 = 1;     // Interrupt pin (DIO1)
        constexpr uint8_t LORA_RST = 5;      // Reset pin
        constexpr uint8_t LORA_BUSY = 4;     // Busy indicator

        // LoRa SPI Bus Pins (FSPI/SPI2)
        constexpr uint8_t SPI_SCK = 12;      // Clock
        constexpr uint8_t SPI_MISO = 13;     // Master In Slave Out
        constexpr uint8_t SPI_MOSI = 11;     // Master Out Slave In

        // SD Card Pins (HSPI/SPI3 — separate bus to avoid LoRa SPI conflict)
        constexpr uint8_t SD_CS = 47;        // SD card chip select
        constexpr uint8_t SD_SCK = 36;       // SD card clock
        constexpr uint8_t SD_MISO = 37;      // SD card MISO
        constexpr uint8_t SD_MOSI = 35;      // SD card MOSI

        // I2C Display Bus (SH1106 OLED 128×64 @ 0x3C)
        constexpr uint8_t OLED_SDA = 17;     // I2C data
        constexpr uint8_t OLED_SCL = 18;     // I2C clock
        constexpr uint8_t OLED_RST = PIN_UNUSED;

        // GPS UART (L76K or MAX-M10S)
        constexpr uint8_t GPS_RX = 9;        // GPS TX → ESP32 RX
        constexpr uint8_t GPS_TX = 8;        // ESP32 TX → GPS RX
        constexpr uint8_t GPS_EN = 7;        // Pull HIGH to enable GPS module
        constexpr uint8_t GPS_EN_LEVEL = HIGH; // Active HIGH on T-Beam Supreme

        // AXP2101 PMIC (separate I2C bus from display)
        constexpr uint8_t PMU_SDA = 42;      // PMU I2C data
        constexpr uint8_t PMU_SCL = 41;      // PMU I2C clock
        constexpr uint8_t PMU_IRQ = 40;      // PMU interrupt (active low)

        // User Interface
        constexpr uint8_t USER_BUTTON = 0;   // BOOT button (active low)
        constexpr uint8_t USER_LED = PIN_UNUSED;  // No dedicated user LED

        // Battery Monitoring (via AXP2101 PMIC over I2C, not ADC)
        // Actual reading handled by pmu_controller.h
        constexpr uint8_t VBAT_ADC_PIN = PIN_UNUSED;
        constexpr uint8_t VBAT_CTRL_PIN = PIN_UNUSED;
        constexpr float VBAT_SCALE = 1.0f;   // Not used (PMIC reads directly)

    #elif defined(BOARD_HELTEC_V3)
        // ========================================================================
        // Heltec WiFi LoRa 32 V3 / V4 Pin Configuration
        // ========================================================================
        // Board: ESP32-S3 + SX1262 + SSD1306 OLED (I2C)
        // V3 and V4 share identical LoRa/OLED/button pins — both use this block.
        // V4 adds a dedicated SH1.25-8P GPS connector (L76K module); use the
        // heltec_v4 PlatformIO environment to enable GPS support.
        // Display: 0.96" OLED on I2C
        // Note: SD card requires external module (not natively supported)

        // SX1262 LoRa Radio Pins
        constexpr uint8_t LORA_NSS = 8;      // SPI chip select
        constexpr uint8_t LORA_DIO1 = 14;    // Interrupt pin
        constexpr uint8_t LORA_RST = 12;     // Reset pin
        constexpr uint8_t LORA_BUSY = 13;    // Busy indicator

        // LoRa SPI Bus Pins
        constexpr uint8_t SPI_SCK = 9;       // Clock
        constexpr uint8_t SPI_MISO = 11;     // Master In Slave Out
        constexpr uint8_t SPI_MOSI = 10;     // Master Out Slave In

        // SD Card Pins (external module, if connected)
        // Note: SD card support on Heltec V3 requires hardware modification
        constexpr uint8_t SD_CS = 5;         // SD card chip select (GPIO 5, conflicts with OLED_RST avoided)
        constexpr uint8_t SD_SCK = 9;        // Shares LoRa SPI bus
        constexpr uint8_t SD_MISO = 11;      // Shares LoRa SPI bus
        constexpr uint8_t SD_MOSI = 10;      // Shares LoRa SPI bus

        // I2C Display Pins (SSD1306 OLED) - implicit, using Wire library defaults
        constexpr uint8_t OLED_SDA = PIN_UNUSED;  // Uses default I2C pins
        constexpr uint8_t OLED_SCL = PIN_UNUSED;  // Uses default I2C pins
        constexpr uint8_t OLED_RST = PIN_UNUSED;  // No reset pin (software reset)

        // User Interface
        constexpr uint8_t USER_BUTTON = 0;   // PRG button (active low)
        constexpr uint8_t USER_LED = 35;     // Onboard LED (if present)

        // GPS UART — V4 only (dedicated SH1.25-8P connector; not present on V3)
        // Enable this block by building with the heltec_v4 environment.
        constexpr uint8_t GPS_RX = 38;       // GPS TX → ESP32 RX
        constexpr uint8_t GPS_TX = 39;       // ESP32 TX → GPS RX
        constexpr uint8_t GPS_EN = 34;       // Pull LOW to enable L76K module (active low)
        constexpr uint8_t GPS_EN_LEVEL = LOW; // Active LOW on Heltec V4

        // Battery Monitoring
        constexpr uint8_t VBAT_ADC_PIN = 1;  // GPIO 1 - battery voltage ADC
        constexpr uint8_t VBAT_CTRL_PIN = 37; // GPIO 37 - ADC control (set HIGH to enable)
        constexpr float VBAT_SCALE = 4.9f;   // Voltage divider scaling factor

    #else
        #error "No board type defined! Define BOARD_HELTEC_V3 (V3/V4 no GPS), BOARD_T3_S3, or BOARD_TBEAM_SUPREME in platformio.ini. For Heltec V4 with GPS use the heltec_v4 environment (which defines BOARD_HELTEC_V3 + HAS_GPS)."
    #endif

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
    constexpr size_t MAX_PACKET_SIZE = 255;
    
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
// DEVICE TRACKING
// ============================================================================
namespace Tracking {
    // Maximum targetable devices to track
    // Each device ~100 bytes, so 50 devices = ~5KB RAM
    // Increased from 20 to handle busy environments (conferences, urban areas)
    constexpr uint8_t MAX_DEVICES = 50;
    
    // Maximum RF activity entries (non-device signals)
    constexpr uint8_t MAX_RF_ACTIVITIES = 16;
    
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
    
    // Health check interval (milliseconds)
    // Network intel update and device archival check
    constexpr uint32_t HEALTH_CHECK_INTERVAL_MS = 300000;  // 5 minutes
    
    // Mutex lock timeout (milliseconds)
    // Used for thread-safe repository access
    constexpr uint32_t MUTEX_TIMEOUT_MS = 100;
}

// ============================================================================
// LOGGING
// ============================================================================
namespace Logging {
    // Enable PCAP export for offline analysis with Wireshark/LoRa_Craft
    // PCAP files preserve RF metadata (RSSI/SNR/frequency) in pseudo-header
    // Set to false to save SD space if you only need CSV logs
    constexpr bool PCAP_EXPORT_ENABLED = true;

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
    // Includes: Current defaults, legacy single-byte, admin channel leaks,
    // named channel derivations, test keys, and regional defaults
    // See psk_decryption_simple.cpp for full list with history
    constexpr uint8_t NUM_DEFAULT_KEYS = 23;
    
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

// ============================================================================
// WIFI CONFIGURATION
// ============================================================================
namespace WiFi {
    // NVS (Non-Volatile Storage) settings for secure credential storage
    // Credentials are stored in ESP32's NVS partition, not as plaintext files
    constexpr const char* NVS_NAMESPACE = "wifi_creds";
    constexpr const char* NVS_KEY_SSID = "ssid";
    constexpr const char* NVS_KEY_PASSWORD = "pass";
    constexpr const char* NVS_KEY_AP_PASSWORD = "ap_pass";
    
    // Legacy JSON file paths (for backward compatibility migration)
    // These files will be automatically migrated to NVS and deleted
    constexpr const char* LEGACY_CREDENTIALS_FILE = "/wifi_config.json";
    constexpr const char* LEGACY_PREPROVISIONED_FILE = "/wifi_creds.json";
    
    // Default AP mode settings (used when no credentials stored)
    // SSID will be "LoRa-XXYYZZ" where XXYYZZ is last 3 bytes of MAC address
    // This ensures unique SSIDs at conferences with many devices
    constexpr const char* AP_SSID_PREFIX = "LoRa-";
    // Default AP password includes device ID suffix for uniqueness
    // Actual password will be "recon-XXYYZZ" where XXYYZZ is last 3 bytes of MAC
    constexpr const char* DEFAULT_AP_PASSWORD_PREFIX = "recon-";
    
    // mDNS hostname will be "lora-xxyyzz.local" (unique per device)
    constexpr const char* MDNS_PREFIX = "lora-";
    
    // Station mode connection timeout (milliseconds)
    constexpr uint32_t STA_CONNECT_TIMEOUT_MS = 15000;
    
    // Number of retries before falling back to AP mode
    constexpr uint8_t STA_MAX_RETRIES = 3;
}

// ============================================================================
// API SECURITY CONFIGURATION
// ============================================================================
namespace Security {
    // Enable/disable API authentication (set false for open demo mode)
    constexpr bool AUTH_ENABLED = true;
    
    // NVS storage for API token
    constexpr const char* NVS_NAMESPACE = "api_auth";
    constexpr const char* NVS_KEY_TOKEN = "token";
    
    // Token length (hex characters)
    constexpr uint8_t TOKEN_LENGTH = 32;
    
    // Protected endpoints requiring authentication
    // Read-only endpoints (devices, status, etc.) are public
    // Write/dangerous endpoints require token
    
    // HTTP header name for token authentication
    constexpr const char* AUTH_HEADER = "X-API-Token";
    
    // Maximum replay count per request (prevents RF flooding)
    constexpr uint8_t MAX_REPLAY_COUNT = 10;
    
    // Maximum replay delay (prevents long blocking)
    constexpr uint16_t MAX_REPLAY_DELAY_MS = 5000;
    
    // Rate limiting configuration
    constexpr uint16_t RATE_LIMIT_REQUESTS = 10;      // Max requests per window
    constexpr uint16_t RATE_LIMIT_WINDOW_MS = 1000;   // Window size (1 second)
    constexpr uint8_t RATE_LIMIT_BUCKETS = 8;         // IP tracking buckets
}

// ANOMALY DETECTION
// ============================================================================
namespace Anomaly {
    // Maximum anomaly records to store
    constexpr uint8_t MAX_ANOMALIES = 50;
    
    // Detection thresholds
    constexpr uint8_t RATE_LIMIT_PACKETS_PER_MIN = 20;  // Max packets per minute per device
    constexpr uint8_t EXCESSIVE_RELAY_HOPS = 5;         // Max relay chain length
    constexpr float RSSI_STDDEV_THRESHOLD = 15.0f;      // RSSI variation indicating spoof
    constexpr uint32_t REPLAY_TIME_WINDOW_MS = 300000;  // 5 minutes
    
    // Temporal analysis
    constexpr uint32_t BEACON_INTERVAL_MIN_MS = 30000;    // 30 seconds
    constexpr uint32_t BEACON_INTERVAL_MAX_MS = 3600000;  // 1 hour (requires uint32_t)
    constexpr float BEACON_JITTER_TOLERANCE = 0.10f;      // ±10% interval variation
    constexpr uint8_t MIN_PACKETS_FOR_PERIODICITY = 3;    // Min packets to detect beacon
    constexpr uint8_t MIN_BEACON_CONFIDENCE = 70;         // Min score to classify as beacon
}

} // namespace Config

// ============================================================================
// COMPILE-TIME SAFETY CHECKS
// ============================================================================

// Verify LoRa protocol limits
static_assert(Config::PacketProcessing::MAX_PACKET_SIZE == 255,
              "LoRa protocol maximum packet size is 255 bytes");

// Verify index types can hold maximum values
static_assert(Config::Tracking::MAX_DEVICES <= 255,
              "Device indices use uint8_t, must fit in 0-255 range");
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
