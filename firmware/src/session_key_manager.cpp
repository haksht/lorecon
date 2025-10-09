/**
 * Session Key Manager Implementation
 */

#include "session_key_manager.h"
#include <mbedtls/aes.h>
#include <mbedtls/base64.h>
#include <string.h>

// Meshtastic protocol constants
#define MESHTASTIC_HEADER 0xFFFFFFFF
#define PORTNUM_ADMIN_APP 0x07           // Admin/key management port
#define ADMIN_MSG_SESSION_KEY_REQUEST 0x10
#define ADMIN_MSG_SESSION_KEY_RESPONSE 0x11

SessionKeyManager::SessionKeyManager() 
    : radio(nullptr)
    , channelPSKLen(0)
    , numSessionKeys(0)
    , lastRequestTime(0)
    , requestRetries(0) {
    memset(channelPSK, 0, sizeof(channelPSK));
    memset(sessionKeys, 0, sizeof(sessionKeys));
}

void SessionKeyManager::initialize(SX1262* radioRef, const uint8_t* psk, size_t pskLen) {
    radio = radioRef;
    
    if (pskLen > sizeof(channelPSK)) pskLen = sizeof(channelPSK);
    memcpy(channelPSK, psk, pskLen);
    channelPSKLen = pskLen;
    
    // Clear cache
    clearAllKeys();
    
    Serial.println("[SESSION] Session key manager initialized");
    Serial.printf("[SESSION] Channel PSK: %d bytes\n", pskLen);
}

bool SessionKeyManager::requestSessionKey(uint8_t channelIndex, uint32_t nodeId) {
    if (!radio) {
        Serial.println("[SESSION] ❌ Radio not initialized");
        return false;
    }
    
    // Check backoff timer
    uint32_t now = millis();
    if (now - lastRequestTime < SESSION_KEY_REQUEST_BACKOFF_MS) {
        Serial.printf("[SESSION] ⏱️  Request backoff: %d ms remaining\n",
                      SESSION_KEY_REQUEST_BACKOFF_MS - (now - lastRequestTime));
        return false;
    }
    
    // Check retry limit
    if (requestRetries >= SESSION_KEY_REQUEST_MAX_RETRIES) {
        Serial.println("[SESSION] ⚠️  Max retries reached, resetting counter");
        requestRetries = 0;
    }
    
    uint8_t packet[64];
    size_t packetLen = buildKeyRequestPacket(packet, channelIndex, nodeId);
    
    if (packetLen == 0) {
        Serial.println("[SESSION] ❌ Failed to build key request packet");
        return false;
    }
    
    Serial.println("\n[SESSION] 📡 Transmitting session key request...");
    Serial.printf("[SESSION] Channel: %d, Node ID: 0x%08X\n", channelIndex, nodeId);
    Serial.printf("[SESSION] Packet size: %d bytes\n", packetLen);
    
    // Temporarily increase TX power for request
    radio->setOutputPower(10);
    
    // Transmit (RadioLib requires non-const buffer)
    uint8_t txBuffer[64];
    memcpy(txBuffer, packet, packetLen);
    int state = radio->transmit(txBuffer, packetLen);
    
    // Back to RX mode
    radio->setOutputPower(0);
    radio->startReceive();
    
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("[SESSION] ✅ Request transmitted successfully");
        lastRequestTime = now;
        requestRetries++;
        return true;
    } else {
        Serial.printf("[SESSION] ❌ Transmission failed (error %d)\n", state);
        return false;
    }
}

size_t SessionKeyManager::buildKeyRequestPacket(uint8_t* buffer, uint8_t channelIndex, uint32_t nodeId) {
    /*
     * Meshtastic packet structure (unencrypted key request):
     * 
     * Header (4 bytes):    0xFF 0xFF 0xFF 0xFF
     * From Node (4 bytes): Our node ID (big-endian)
     * Type/Flags (2 bytes): Packet type nibble + flags
     * Packet ID (4 bytes):  Random ID (little-endian)
     * Dest Node (4 bytes):  0xFFFFFFFF (broadcast)
     * 
     * Payload (unencrypted):
     *   Field 1 (portnum): 0x08 0x07 (ADMIN_APP)
     *   Field 2 (payload): 0x12 <len> 0x08 <session_key_request_type>
     *   Field 3 (channel): 0x18 <channel_index>
     */
    
    size_t pos = 0;
    
    // Header
    buffer[pos++] = 0xFF;
    buffer[pos++] = 0xFF;
    buffer[pos++] = 0xFF;
    buffer[pos++] = 0xFF;
    
    // From Node (big-endian)
    buffer[pos++] = (nodeId >> 24) & 0xFF;
    buffer[pos++] = (nodeId >> 16) & 0xFF;
    buffer[pos++] = (nodeId >> 8) & 0xFF;
    buffer[pos++] = nodeId & 0xFF;
    
    // Type/Flags: 0x01 = data packet
    buffer[pos++] = 0x01;
    buffer[pos++] = 0x00;
    
    // Packet ID (random, little-endian)
    uint32_t packetId = random(1, 0xFFFFFFF);
    buffer[pos++] = packetId & 0xFF;
    buffer[pos++] = (packetId >> 8) & 0xFF;
    buffer[pos++] = (packetId >> 16) & 0xFF;
    buffer[pos++] = (packetId >> 24) & 0xFF;
    
    // Destination (broadcast)
    buffer[pos++] = 0xFF;
    buffer[pos++] = 0xFF;
    buffer[pos++] = 0xFF;
    buffer[pos++] = 0xFF;
    
    // Payload: Admin message with session key request
    // Field 1: portnum = ADMIN_APP (0x07)
    buffer[pos++] = 0x08;  // Field 1, wire type 0 (varint)
    buffer[pos++] = PORTNUM_ADMIN_APP;
    
    // Field 2: payload (Admin message)
    buffer[pos++] = 0x12;  // Field 2, wire type 2 (length-delimited)
    size_t payloadLenPos = pos++;  // Reserve byte for length
    size_t payloadStart = pos;
    
    // Admin message content:
    // Field 1: session_key_request_type
    buffer[pos++] = 0x08;  // Field 1, wire type 0
    buffer[pos++] = ADMIN_MSG_SESSION_KEY_REQUEST;
    
    // Field 2: channel_index
    buffer[pos++] = 0x10;  // Field 2, wire type 0
    buffer[pos++] = channelIndex;
    
    // Fill in payload length
    buffer[payloadLenPos] = pos - payloadStart;
    
    Serial.print("[SESSION] Request packet hex: ");
    for (size_t i = 0; i < pos; i++) {
        Serial.printf("%02X ", buffer[i]);
    }
    Serial.println();
    
    return pos;
}

bool SessionKeyManager::processKeyAnnouncement(const uint8_t* data, size_t length) {
    // Check for Meshtastic header
    if (length < 20) return false;
    
    bool hasHeader = (data[0] == 0xFF && data[1] == 0xFF && 
                      data[2] == 0xFF && data[3] == 0xFF);
    
    if (!hasHeader) {
        // Try to find header within packet (for routed packets)
        bool found = false;
        for (size_t i = 0; i < length - 4; i++) {
            if (data[i] == 0xFF && data[i+1] == 0xFF && 
                data[i+2] == 0xFF && data[i+3] == 0xFF) {
                data += i;
                length -= i;
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    
    // Extract packet metadata
    uint32_t nodeId = (uint32_t(data[4]) << 24) | (uint32_t(data[5]) << 16) | 
                      (uint32_t(data[6]) << 8) | uint32_t(data[7]);
    uint32_t packetId = ((uint32_t)data[10]) | ((uint32_t)data[11] << 8) | 
                        ((uint32_t)data[12] << 16) | ((uint32_t)data[13] << 24);
    
    const uint8_t* encryptedData = data + 14;
    size_t encryptedLen = length - 14;
    
    // Decrypt with channel PSK
    return parseKeyAnnouncement(encryptedData, encryptedLen, nodeId, packetId);
}

bool SessionKeyManager::parseKeyAnnouncement(const uint8_t* encryptedData, size_t length,
                                             uint32_t nodeId, uint32_t packetId) {
    if (channelPSKLen == 0) {
        Serial.println("[SESSION] ❌ No channel PSK configured");
        return false;
    }
    
    // Construct AES-CTR nonce
    uint8_t nonce[16];
    memset(nonce, 0, sizeof(nonce));
    nonce[0] = (packetId) & 0xFF;
    nonce[1] = (packetId >> 8) & 0xFF;
    nonce[2] = (packetId >> 16) & 0xFF;
    nonce[3] = (packetId >> 24) & 0xFF;
    nonce[8] = (nodeId >> 24) & 0xFF;
    nonce[9] = (nodeId >> 16) & 0xFF;
    nonce[10] = (nodeId >> 8) & 0xFF;
    nonce[11] = nodeId & 0xFF;
    
    // Decrypt
    uint8_t decrypted[256];
    if (length > sizeof(decrypted)) return false;
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    if (mbedtls_aes_setkey_enc(&aes, channelPSK, channelPSKLen * 8) != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }
    
    uint8_t nonce_counter[16];
    uint8_t stream_block[16];
    memcpy(nonce_counter, nonce, 16);
    memset(stream_block, 0, 16);
    size_t nc_off = 0;
    
    int result = mbedtls_aes_crypt_ctr(&aes, length, &nc_off,
                                       nonce_counter, stream_block,
                                       encryptedData, decrypted);
    mbedtls_aes_free(&aes);
    
    if (result != 0) return false;
    
    // Parse decrypted admin message
    // Expected structure:
    // Field 1 (portnum): 0x08 0x07 (ADMIN_APP)
    // Field 2 (payload): 0x12 <len> ...
    //   Inside payload:
    //     Field X: session_key_response with key bytes
    
    size_t pos = 0;
    
    // Check portnum
    if (pos + 2 > length || decrypted[pos] != 0x08) return false;
    pos++;
    
    uint8_t portnum = decrypted[pos++];
    if (portnum != PORTNUM_ADMIN_APP) {
        return false;  // Not an admin message
    }
    
    Serial.println("[SESSION] 🔍 Found admin message, parsing for session key...");
    
    // Field 2: payload
    if (pos >= length || decrypted[pos] != 0x12) return false;
    pos++;
    
    // Payload length (varint)
    uint32_t payloadLen = 0;
    uint8_t shift = 0;
    while (pos < length) {
        uint8_t byte = decrypted[pos++];
        payloadLen |= ((uint32_t)(byte & 0x7F)) << shift;
        if ((byte & 0x80) == 0) break;
        shift += 7;
    }
    
    if (pos + payloadLen > length) return false;
    
    // Parse admin message payload
    const uint8_t* payloadStart = decrypted + pos;
    size_t payloadPos = 0;
    
    SessionKey newKey;
    memset(&newKey, 0, sizeof(newKey));
    newKey.timestamp = millis();
    
    bool foundKey = false;
    
    while (payloadPos < payloadLen) {
        // Read field tag
        uint8_t tag = payloadStart[payloadPos++];
        uint8_t fieldNum = tag >> 3;
        uint8_t wireType = tag & 0x07;
        
        if (wireType == 2) {  // Length-delimited (could be key bytes)
            // Read length
            uint32_t fieldLen = 0;
            uint8_t shift = 0;
            while (payloadPos < payloadLen) {
                uint8_t byte = payloadStart[payloadPos++];
                fieldLen |= ((uint32_t)(byte & 0x7F)) << shift;
                if ((byte & 0x80) == 0) break;
                shift += 7;
            }
            
            if (payloadPos + fieldLen > payloadLen) break;
            
            // Check if this looks like a session key (32 bytes)
            if (fieldLen == 32) {
                memcpy(newKey.keyBytes, payloadStart + payloadPos, 32);
                foundKey = true;
                Serial.println("[SESSION] 🔑 Found 32-byte session key in response!");
            } else if (fieldLen == 16) {
                // Some firmware uses 16-byte session keys
                memcpy(newKey.keyBytes, payloadStart + payloadPos, 16);
                foundKey = true;
                Serial.println("[SESSION] 🔑 Found 16-byte session key in response!");
            }
            
            payloadPos += fieldLen;
            
        } else if (wireType == 0) {  // Varint (could be session ID)
            uint32_t value = 0;
            uint8_t shift = 0;
            while (payloadPos < payloadLen) {
                uint8_t byte = payloadStart[payloadPos++];
                value |= ((uint32_t)(byte & 0x7F)) << shift;
                if ((byte & 0x80) == 0) break;
                shift += 7;
            }
            
            // Might be session ID
            if (newKey.sessionId == 0) {
                newKey.sessionId = value;
            }
        } else {
            // Skip other wire types
            break;
        }
    }
    
    if (foundKey) {
        newKey.valid = true;
        newKey.channelIndex = 0;  // Assume primary channel
        addSessionKey(newKey);
        
        Serial.println("[SESSION] ✅ Session key harvested successfully!");
        Serial.printf("[SESSION] Session ID: 0x%08X\n", newKey.sessionId);
        Serial.print("[SESSION] Key (hex): ");
        for (int i = 0; i < 32; i++) {
            Serial.printf("%02X", newKey.keyBytes[i]);
            if ((i + 1) % 16 == 0) Serial.println();
        }
        Serial.println();
        
        return true;
    }
    
    return false;
}

void SessionKeyManager::addSessionKey(const SessionKey& key) {
    // Check if we already have this session ID
    for (uint8_t i = 0; i < numSessionKeys; i++) {
        if (sessionKeys[i].channelIndex == key.channelIndex &&
            sessionKeys[i].sessionId == key.sessionId) {
            // Update existing
            sessionKeys[i] = key;
            Serial.println("[SESSION] 🔄 Updated existing session key");
            return;
        }
    }
    
    // Add new key
    if (numSessionKeys < MAX_SESSION_KEYS) {
        sessionKeys[numSessionKeys++] = key;
        Serial.printf("[SESSION] ➕ Added session key (total: %d)\n", numSessionKeys);
    } else {
        // Replace oldest key
        uint8_t oldestIdx = 0;
        uint32_t oldestTime = sessionKeys[0].timestamp;
        for (uint8_t i = 1; i < MAX_SESSION_KEYS; i++) {
            if (sessionKeys[i].timestamp < oldestTime) {
                oldestTime = sessionKeys[i].timestamp;
                oldestIdx = i;
            }
        }
        sessionKeys[oldestIdx] = key;
        Serial.printf("[SESSION] 🔄 Replaced oldest session key (slot %d)\n", oldestIdx);
    }
}

const SessionKey* SessionKeyManager::getSessionKey(uint8_t channelIndex, uint32_t sessionId) {
    // If sessionId is 0, return most recent key for channel
    if (sessionId == 0) {
        const SessionKey* latest = nullptr;
        uint32_t latestTime = 0;
        
        for (uint8_t i = 0; i < numSessionKeys; i++) {
            if (sessionKeys[i].valid && 
                sessionKeys[i].channelIndex == channelIndex &&
                isKeyValid(sessionKeys[i]) &&
                sessionKeys[i].timestamp > latestTime) {
                latest = &sessionKeys[i];
                latestTime = sessionKeys[i].timestamp;
            }
        }
        
        return latest;
    }
    
    // Look for specific session ID
    for (uint8_t i = 0; i < numSessionKeys; i++) {
        if (sessionKeys[i].valid &&
            sessionKeys[i].channelIndex == channelIndex &&
            sessionKeys[i].sessionId == sessionId &&
            isKeyValid(sessionKeys[i])) {
            return &sessionKeys[i];
        }
    }
    
    return nullptr;
}

bool SessionKeyManager::hasSessionKey(uint8_t channelIndex) {
    return getSessionKey(channelIndex, 0) != nullptr;
}

bool SessionKeyManager::isKeyValid(const SessionKey& key) {
    uint32_t age = millis() - key.timestamp;
    return age < SESSION_KEY_VALIDITY_MS;
}

void SessionKeyManager::clearAllKeys() {
    memset(sessionKeys, 0, sizeof(sessionKeys));
    numSessionKeys = 0;
    Serial.println("[SESSION] 🗑️  All session keys cleared");
}

void SessionKeyManager::printStatus() {
    Serial.println("\n[SESSION] 📊 SESSION KEY STATUS:");
    Serial.printf("   Cached Keys: %d/%d\n", numSessionKeys, MAX_SESSION_KEYS);
    
    if (numSessionKeys == 0) {
        Serial.println("   No session keys cached.");
        Serial.println("   Use 'q' command to request session keys from network.");
        return;
    }
    
    for (uint8_t i = 0; i < numSessionKeys; i++) {
        const SessionKey& key = sessionKeys[i];
        if (!key.valid) continue;
        
        uint32_t ageSeconds = (millis() - key.timestamp) / 1000;
        bool valid = isKeyValid(key);
        
        Serial.printf("\n   Key #%d:\n", i + 1);
        Serial.printf("     Channel: %d\n", key.channelIndex);
        Serial.printf("     Session ID: 0x%08X\n", key.sessionId);
        Serial.printf("     Age: %u seconds\n", (unsigned)ageSeconds);
        Serial.printf("     Status: %s\n", valid ? "✅ VALID" : "⚠️  EXPIRED");
        Serial.print("     Key: ");
        for (int j = 0; j < 16; j++) {
            Serial.printf("%02X", key.keyBytes[j]);
        }
        Serial.println("...");
    }
    Serial.println();
}
