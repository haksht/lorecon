/**
 * Channel Hash Utilities Implementation
 * 
 * Pre-computed hash lookup table for known Meshtastic channels.
 * 
 * Hash algorithm (from Meshtastic firmware):
 *   hash = XOR(channel_name_bytes) ^ XOR(psk_bytes)
 * 
 * For default channels with default PSK (0x01 expanded):
 *   hash = XOR(preset_name) ^ XOR(default_psk_16_bytes)
 * 
 * References:
 * - https://github.com/meshtastic/firmware/blob/master/src/mesh/Channels.cpp
 * - Meshtastic protocol docs: byte 13 = channel hash
 */

#include "channel_hash.h"
#include <cstring>

namespace ChannelHash {

// Default PSK (0x01 expanded to 16 bytes) - used for hash calculation
// From Meshtastic: base64 "1PG7OiApB1nwvP+rz05pAQ=="
static const uint8_t DEFAULT_PSK[] = {
    0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
    0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01
};

// Pre-computed XOR of default PSK = 0x02
static constexpr uint8_t DEFAULT_PSK_XOR = 0x02;

// Known channel hash -> name mappings
// Format: { hash, "name" }
// 
// For default PSK channels: hash = XOR(name_bytes) ^ 0x02
// For community channels with custom PSKs: hashes are observed values
struct HashEntry {
    uint8_t hash;
    const char* name;
};

// Pre-computed hashes for known channels
// Default PSK channels calculated as: XOR(channel_name) ^ 0x02
// Community channels use observed hash values from the wild
static const HashEntry KNOWN_HASHES[] = {
    // === Meshtastic Modem Presets (with default PSK) ===
    // Calculated: XOR(preset_name) ^ 0x02
    { 0x08, "LongFast" },       // XOR("LongFast")=0x0a ^ 0x02 = 0x08
    { 0x0f, "LongSlow" },       // XOR("LongSlow")=0x0d ^ 0x02 = 0x0f  
    { 0x18, "MediumSlow" },     // XOR("MediumSlow")=0x1a ^ 0x02 = 0x18
    { 0x70, "ShortFast" },      // XOR("ShortFast")=0x72 ^ 0x02 = 0x70
    { 0x02, "" },               // Empty name (uses preset) with default PSK
    
    // === Special Channels (default PSK) ===
    { 0x6d, "admin" },          // XOR("admin")=0x6f ^ 0x02 = 0x6d
    { 0x67, "gpio" },           // XOR("gpio")=0x65 ^ 0x02 = 0x67
    { 0x05, "serial" },         // XOR("serial")=0x07 ^ 0x02 = 0x05
    { 0x0e, "mqtt" },           // XOR("mqtt")=0x0c ^ 0x02 = 0x0e
};

static constexpr size_t NUM_KNOWN_HASHES = sizeof(KNOWN_HASHES) / sizeof(HashEntry);

const char* getChannelName(uint8_t hash) {
    // Linear search through known hashes
    // Could optimize with sorted array + binary search if list grows large
    for (size_t i = 0; i < NUM_KNOWN_HASHES; i++) {
        if (KNOWN_HASHES[i].hash == hash) {
            return KNOWN_HASHES[i].name;
        }
    }
    return nullptr;
}

uint8_t calculateHash(const char* name, const uint8_t* psk, size_t pskLen) {
    uint8_t h = 0;
    
    // XOR all bytes of channel name
    if (name) {
        h = xorBytes(reinterpret_cast<const uint8_t*>(name), strlen(name));
    }
    
    // XOR with PSK bytes
    if (psk && pskLen > 0) {
        h ^= xorBytes(psk, pskLen);
    }
    
    return h;
}

}  // namespace ChannelHash
