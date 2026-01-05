/**
 * Channel Hash Utilities
 * 
 * Maps Meshtastic channel hashes (byte 13) to known channel names.
 * 
 * The channel hash is: XOR(channel_name_bytes) XOR XOR(psk_bytes)
 * Since hashes can collide, we return the most likely match.
 * 
 * Known regional/community channels and Meshtastic presets are included.
 */

#ifndef CHANNEL_HASH_H
#define CHANNEL_HASH_H

#include <cstdint>
#include <cstddef>

namespace ChannelHash {

/**
 * Get the likely channel name for a given hash value.
 * Returns nullptr if hash is not in our known list.
 * 
 * Note: Multiple channel name+PSK combinations can produce the same hash.
 * We return the most commonly used match.
 * 
 * @param hash The channel hash byte (byte 13 of Meshtastic packet)
 * @return Channel name string or nullptr if unknown
 */
const char* getChannelName(uint8_t hash);

/**
 * Calculate channel hash from name and PSK
 * Uses Meshtastic's XOR algorithm: XOR(name_bytes) ^ XOR(psk_bytes)
 * 
 * @param name Channel name (can be empty string for default)
 * @param psk PSK bytes (expanded to 16/32 bytes as needed)
 * @param pskLen Length of PSK
 * @return Calculated hash byte
 */
uint8_t calculateHash(const char* name, const uint8_t* psk, size_t pskLen);

/**
 * XOR all bytes in a buffer (Meshtastic hash algorithm)
 */
inline uint8_t xorBytes(const uint8_t* data, size_t len) {
    uint8_t result = 0;
    for (size_t i = 0; i < len; i++) {
        result ^= data[i];
    }
    return result;
}

}  // namespace ChannelHash

#endif // CHANNEL_HASH_H
