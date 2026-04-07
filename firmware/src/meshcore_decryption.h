#ifndef MESHCORE_DECRYPTION_H
#define MESHCORE_DECRYPTION_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * MeshCore channel decryption.
 *
 * Supports:
 *   - Public channel (PSK "izOH6cXN6mrJ5e26oRXNcg==")
 *   - Common hashtag rooms (key = SHA256("#roomname")[0:16])
 *
 * Algorithm: AES-128-ECB decrypt, HMAC-SHA256 MAC verification (2 bytes).
 * Channel hash (1 byte) used for fast key selection before HMAC check.
 *
 * Packet payload format (group message):
 *   [channel_hash 1B][mac 2B][ciphertext N*16B]
 * Plaintext format after decryption:
 *   [timestamp 4B LE][flags 1B][message text...]
 */
class MeshCoreDecryption {
public:
    // Call once during initialization (creates mutex, derives hashtag keys).
    static void initialize();

    // Try all known channel keys against a MeshCore packet.
    // Returns true if decrypted successfully; call getLastMessageSafe() for the text.
    static bool tryDecrypt(const uint8_t* data, size_t length);

    static void getLastMessageSafe(char* buffer, size_t bufferSize);
    static void clearLastMessage();

    // Channel name that successfully decrypted the last packet.
    // "public" for key[0], "#roomname" for hashtag rooms, "" if decryption failed.
    static void getLastChannelName(char* buffer, size_t bufferSize);

    // 1 public channel + 12 common hashtag rooms
    static constexpr uint8_t NUM_KEYS = 13;

private:
    // Returns byte offset to payload within a MeshCore packet, or -1 if malformed.
    static int getPayloadOffset(const uint8_t* data, size_t length);

    // Try one 16-byte key. Checks channel hash byte, verifies HMAC, decrypts AES-ECB.
    static bool tryKey(const uint8_t key[16], const uint8_t* payload, size_t payloadLen);

    static constexpr size_t MAX_MESSAGE_LEN = 256;
    static constexpr size_t MAX_CHANNEL_LEN = 24;
    static char lastMessage[MAX_MESSAGE_LEN];
    static char lastChannelName[MAX_CHANNEL_LEN];
    static SemaphoreHandle_t messageMutex;
    static uint8_t channelKeys[NUM_KEYS][16];
    static bool initialized;
};

#endif // MESHCORE_DECRYPTION_H
