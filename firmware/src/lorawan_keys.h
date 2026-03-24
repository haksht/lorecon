/**
 * LoRaWAN Key Testing Module
 * 
 * Tests default/common AppKeys against captured Join Requests.
 * LoRaWAN Join Requests include a MIC (Message Integrity Code) calculated
 * with the AppKey - if we guess the right key, the MIC will match.
 * 
 * This catches misconfigured IoT devices using:
 * - Test/development keys left in production
 * - Manufacturer default keys
 * - Common example keys from tutorials
 */

#ifndef LORAWAN_KEYS_H
#define LORAWAN_KEYS_H

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace LoRaWANKeys {

// Join Request structure (parsed from packet)
struct JoinRequest {
    uint8_t appEUI[8];      // Application EUI (little-endian)
    uint8_t devEUI[8];      // Device EUI (little-endian)
    uint16_t devNonce;      // Device nonce
    uint8_t mic[4];         // Message Integrity Code
    bool valid;             // Successfully parsed
    
    JoinRequest() : devNonce(0), valid(false) {
        memset(appEUI, 0, sizeof(appEUI));
        memset(devEUI, 0, sizeof(devEUI));
        memset(mic, 0, sizeof(mic));
    }
};

// Result of key testing
struct KeyTestResult {
    bool success;           // Key found
    uint8_t keyIndex;       // Index of matching key (0-based)
    const char* keyName;    // Human-readable key description
    uint8_t appKey[16];     // The matching AppKey
    
    KeyTestResult() : success(false), keyIndex(0), keyName(nullptr) {
        memset(appKey, 0, sizeof(appKey));
    }
};

// Statistics
struct LoRaWANStats {
    uint16_t joinRequestsSeen;
    uint16_t joinRequestsDecoded;
    uint16_t keysFound;
    uint8_t keyHits[16];    // Hit count per key index
    
    LoRaWANStats() : joinRequestsSeen(0), joinRequestsDecoded(0), keysFound(0) {
        memset(keyHits, 0, sizeof(keyHits));
    }
};

/**
 * Parse a LoRaWAN Join Request packet
 * @param data Raw packet data (must start with MHDR byte)
 * @param length Packet length
 * @return Parsed JoinRequest (check .valid field)
 */
JoinRequest parseJoinRequest(const uint8_t* data, size_t length);

/**
 * Test default AppKeys against a Join Request
 * @param jr Parsed Join Request
 * @param data Original packet data (for MIC calculation)
 * @param length Original packet length
 * @return KeyTestResult (check .success field)
 */
KeyTestResult testDefaultKeys(const JoinRequest& jr, const uint8_t* data, size_t length);

/**
 * Analyze a LoRaWAN packet - auto-detects type and tests keys if Join Request
 * @param data Raw packet data
 * @param length Packet length
 * @return true if successfully analyzed (key found or packet parsed)
 */
bool analyzePacket(const uint8_t* data, size_t length);

/**
 * Get current statistics
 */
const LoRaWANStats& getStats();

/**
 * Get number of default keys
 */
uint8_t getKeyCount();

/**
 * Print summary to Serial
 */
void printSummary();

/**
 * Format EUI as string (XX:XX:XX:XX:XX:XX:XX:XX)
 * @param eui 8-byte EUI array
 * @param buffer Output buffer (must be at least 24 bytes)
 */
void formatEUI(const uint8_t* eui, char* buffer);

} // namespace LoRaWANKeys

#endif // LORAWAN_KEYS_H
