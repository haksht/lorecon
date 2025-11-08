/**
 * Session Key Manager for Meshtastic
 * 
 * Handles requesting, harvesting, and managing session keys for decrypting
 * encrypted user messages on the primary channel.
 * 
 * Background:
 * - Channel PSK only encrypts control/routing/admin packets
 * - User messages (TEXT_MESSAGE_APP) use ephemeral session keys
 * - Session keys rotate periodically and are announced on the channel
 * - We can request current session keys or listen for announcements
 */

#ifndef SESSION_KEY_MANAGER_H
#define SESSION_KEY_MANAGER_H

#include <Arduino.h>
#include <RadioLib.h>

// Maximum number of session keys to cache (one per channel)
#define MAX_SESSION_KEYS 8

// Session key validity (30 days in milliseconds)
#define SESSION_KEY_VALIDITY_MS (30UL * 24UL * 60UL * 60UL * 1000UL)

// Request retry timing
#define SESSION_KEY_REQUEST_TIMEOUT_MS 5000
#define SESSION_KEY_REQUEST_MAX_RETRIES 3
#define SESSION_KEY_REQUEST_BACKOFF_MS 2000

/**
 * Session Key Entry
 */
struct SessionKey {
    uint8_t channelIndex;           // Channel this key belongs to (0 = Primary)
    uint8_t keyBytes[32];           // AES-256 key bytes (Meshtastic uses 256-bit for sessions)
    uint32_t sessionId;             // Session identifier/epoch
    uint32_t timestamp;             // When we received this key
    bool valid;                     // Is this entry populated?
};

/**
 * Session Key Manager
 * 
 * Handles:
 * 1. Building and transmitting session key requests
 * 2. Listening for and parsing key announcements
 * 3. Caching session keys by channel and epoch
 * 4. Providing keys for message decryption
 */
class SessionKeyManager {
public:
    SessionKeyManager();
    
    /**
     * Initialize with radio reference and channel PSK
     * @param radio Reference to SX1262 radio
     * @param channelPSK Base64-encoded channel PSK (for decrypting key announcements)
     * @param channelPSKLen Length of decoded PSK (typically 16 bytes for AES-128)
     */
    void initialize(SX1262* radio, const uint8_t* channelPSK, size_t channelPSKLen);
    
    /**
     * Request session key for a specific channel
     * @param channelIndex Channel to request key for (0 = Primary)
     * @param nodeId Our node ID (use random or sniffed ID)
     * @return true if request transmitted successfully
     */
    bool requestSessionKey(uint8_t channelIndex, uint32_t nodeId);
    
    /**
     * Process a received packet that might be a key announcement
     * @param data Packet data
     * @param length Packet length
     * @return true if this was a key announcement and we extracted it
     */
    bool processKeyAnnouncement(const uint8_t* data, size_t length);
    
    /**
     * Get session key for decryption
     * @param channelIndex Channel index
     * @param sessionId Session ID (if known, otherwise 0 for latest)
     * @return Pointer to session key, or nullptr if not found
     */
    const SessionKey* getSessionKey(uint8_t channelIndex, uint32_t sessionId = 0);
    
    /**
     * Check if we have a valid session key for a channel
     * @param channelIndex Channel index
     * @return true if we have a valid key
     */
    bool hasSessionKey(uint8_t channelIndex);
    
    /**
     * Clear all cached session keys
     */
    void clearAllKeys();
    
    /**
     * Print current session key cache status
     */
    void printStatus();
    
private:
    SX1262* radio;
    uint8_t channelPSK[32];
    size_t channelPSKLen;
    
    SessionKey sessionKeys[MAX_SESSION_KEYS];
    uint8_t numSessionKeys;
    
    // Request tracking
    uint32_t lastRequestTime;
    uint8_t requestRetries;
    
    /**
     * Build a Meshtastic key request packet
     * @param buffer Output buffer (at least 64 bytes)
     * @param channelIndex Channel to request key for
     * @param nodeId Our node ID
     * @return Packet length, or 0 on error
     */
    size_t buildKeyRequestPacket(uint8_t* buffer, uint8_t channelIndex, uint32_t nodeId);
    
    /**
     * Decrypt and parse a key announcement packet
     * @param encryptedData Encrypted payload
     * @param length Payload length
     * @param nodeId Source node ID
     * @param packetId Packet ID (for nonce)
     * @return true if successfully parsed
     */
    bool parseKeyAnnouncement(const uint8_t* encryptedData, size_t length,
                             uint32_t nodeId, uint32_t packetId, const uint8_t* rawPacket);    /**
     * Add or update a session key in cache
     * @param key Session key to add
     */
    void addSessionKey(const SessionKey& key);
    
    /**
     * Check if session key is still valid
     * @param key Session key to check
     * @return true if valid (not expired)
     */
    bool isKeyValid(const SessionKey& key);
};

#endif // SESSION_KEY_MANAGER_H
