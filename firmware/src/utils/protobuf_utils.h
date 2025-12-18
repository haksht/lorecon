/**
 * Protobuf Utilities - Shared protobuf decoding functions
 * 
 * Common protobuf parsing used by PSK decryption, GPS extraction,
 * and protocol analysis.
 */

#ifndef PROTOBUF_UTILS_H
#define PROTOBUF_UTILS_H

#include <Arduino.h>

namespace ProtobufUtils {

// Maximum bytes in a varint encoding
constexpr uint8_t MAX_VARINT_BYTES = 5;

/**
 * Decode a protobuf varint from a buffer
 * 
 * Varints are 7 bits of data per byte with MSB as continuation flag.
 * Used throughout Meshtastic packet structures.
 * 
 * @param data Pointer to start of varint
 * @param dataLen Total bytes available
 * @param pos Starting position in buffer
 * @param value Output: decoded value
 * @param bytesRead Output: number of bytes consumed
 * @return true if successfully decoded, false on error
 */
inline bool decodeVarint(const uint8_t* data, size_t dataLen, size_t pos, 
                         uint32_t& value, size_t& bytesRead) {
    value = 0;
    bytesRead = 0;
    uint32_t shift = 0;
    
    while (pos + bytesRead < dataLen && bytesRead < MAX_VARINT_BYTES) {
        uint8_t byte = data[pos + bytesRead];
        value |= ((uint32_t)(byte & 0x7F)) << shift;
        bytesRead++;
        if ((byte & 0x80) == 0) return true;  // MSB clear = last byte
        shift += 7;
    }
    return false;  // Incomplete or too long
}

/**
 * Decode a signed varint (zigzag encoding)
 * 
 * Protobuf uses zigzag encoding for signed integers:
 * 0 -> 0, -1 -> 1, 1 -> 2, -2 -> 3, etc.
 * 
 * @param data Pointer to start of varint
 * @param maxLen Maximum bytes available
 * @param bytesRead Output: number of bytes consumed
 * @return Decoded signed value
 */
inline int32_t decodeSignedVarint(const uint8_t* data, size_t maxLen, size_t& bytesRead) {
    uint32_t value = 0;
    if (!decodeVarint(data, maxLen, 0, value, bytesRead)) {
        return 0;
    }
    // Zigzag decode: (n >> 1) ^ -(n & 1)
    return (int32_t)((value >> 1) ^ (~(value & 1) + 1));
}

/**
 * Decode a raw varint (simpler signature for geo_intelligence compatibility)
 * 
 * @param data Pointer to start of varint  
 * @param maxLen Maximum bytes available
 * @param bytesRead Output: number of bytes consumed
 * @return Decoded unsigned value (cast to int32_t for coordinate math)
 */
inline int32_t decodeVarintSimple(const uint8_t* data, size_t maxLen, size_t& bytesRead) {
    uint32_t value = 0;
    bytesRead = 0;
    uint32_t shift = 0;
    
    while (bytesRead < maxLen && bytesRead < MAX_VARINT_BYTES) {
        uint8_t byte = data[bytesRead];
        value |= ((uint32_t)(byte & 0x7F)) << shift;
        bytesRead++;
        if ((byte & 0x80) == 0) return (int32_t)value;
        shift += 7;
    }
    bytesRead = 0;  // Error indicator
    return 0;
}

/**
 * Get protobuf field number from wire byte
 * 
 * @param wireByte First byte of a field
 * @return Field number (1-based)
 */
inline uint8_t getFieldNumber(uint8_t wireByte) {
    return wireByte >> 3;
}

/**
 * Get protobuf wire type from wire byte
 * 
 * Wire types:
 * 0 = Varint (int32, int64, uint32, uint64, sint32, sint64, bool, enum)
 * 1 = 64-bit (fixed64, sfixed64, double)
 * 2 = Length-delimited (string, bytes, embedded messages, packed repeated fields)
 * 5 = 32-bit (fixed32, sfixed32, float)
 * 
 * @param wireByte First byte of a field
 * @return Wire type (0-5, 6/7 are invalid)
 */
inline uint8_t getWireType(uint8_t wireByte) {
    return wireByte & 0x07;
}

/**
 * Check if a wire type is valid
 * 
 * @param wireType Wire type value
 * @return true if valid (0, 1, 2, or 5)
 */
inline bool isValidWireType(uint8_t wireType) {
    return wireType == 0 || wireType == 1 || wireType == 2 || wireType == 5;
}

} // namespace ProtobufUtils

#endif // PROTOBUF_UTILS_H
