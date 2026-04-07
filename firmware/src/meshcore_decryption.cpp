/**
 * MeshCore Channel Decryption
 *
 * Decrypts MeshCore group messages (public channel + hashtag rooms).
 * Algorithm: AES-128-ECB, HMAC-SHA256 (2-byte MAC), channel hash for key selection.
 *
 * References:
 *   - meshcore-decoder TypeScript library (michaelhart/meshcore-decoder)
 *   - https://docs.meshcore.io/packet_format/
 *   - https://jacksbrain.com/2026/01/a-hitchhiker-s-guide-to-meshcore-cryptography/
 */

#include "meshcore_decryption.h"
#include <mbedtls/aes.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>
#include <mbedtls/base64.h>
#include <string.h>
#include "logger.h"

// --- Static member definitions -----------------------------------------------

char MeshCoreDecryption::lastMessage[MeshCoreDecryption::MAX_MESSAGE_LEN] = {0};
char MeshCoreDecryption::lastChannelName[MeshCoreDecryption::MAX_CHANNEL_LEN] = {0};
SemaphoreHandle_t MeshCoreDecryption::messageMutex = nullptr;
uint8_t MeshCoreDecryption::channelKeys[MeshCoreDecryption::NUM_KEYS][16];
bool MeshCoreDecryption::initialized = false;

// --- Known channel keys -------------------------------------------------------

// Public channel PSK (shipped on every MeshCore device, intentionally public)
static const char PUBLIC_CHANNEL_KEY_B64[] = "izOH6cXN6mrJ5e26oRXNcg==";

// Common hashtag room names. Key = SHA256("#roomname")[0:16].
// Hashtag rooms are insecure by design — key is derivable from the room name alone.
static const char* const HASHTAG_ROOMS[] = {
    "#general", "#emergency", "#local", "#mesh", "#public",
    "#chat", "#help", "#info", "#sos", "#weather", "#news",
    "#test"
};
static_assert(sizeof(HASHTAG_ROOMS) / sizeof(HASHTAG_ROOMS[0]) == MeshCoreDecryption::NUM_KEYS - 1,
              "HASHTAG_ROOMS count must equal NUM_KEYS - 1");

// --- Initialization ----------------------------------------------------------

void MeshCoreDecryption::initialize() {
    if (!messageMutex) {
        messageMutex = xSemaphoreCreateMutex();
    }

    // Decode public channel key from base64 into channelKeys[0]
    size_t outLen = 0;
    mbedtls_base64_decode(channelKeys[0], 16, &outLen,
        (const unsigned char*)PUBLIC_CHANNEL_KEY_B64, strlen(PUBLIC_CHANNEL_KEY_B64));

    // Derive hashtag room keys: SHA256("#roomname")[0:16]
    for (uint8_t i = 0; i < NUM_KEYS - 1; i++) {
        uint8_t hash[32];
        mbedtls_sha256((const unsigned char*)HASHTAG_ROOMS[i],
                       strlen(HASHTAG_ROOMS[i]), hash, 0 /* SHA-256, not SHA-224 */);
        memcpy(channelKeys[i + 1], hash, 16);
    }

    initialized = true;
    LOG_INFO("MeshCore: %d channel keys loaded (public + %d hashtag rooms)",
             NUM_KEYS, NUM_KEYS - 1);
}

// --- Packet parsing ----------------------------------------------------------

/**
 * Returns the byte offset of the payload within a MeshCore packet, or -1 if malformed.
 *
 * Packet layout:
 *   [header 1B]
 *   [transport_codes 4B] -- only for route type 2 (TRANSPORT_FLOOD) or 3 (TRANSPORT_DIRECT)
 *   [path_length 1B]     -- bits 0-5 = hop count, bits 6-7 = hash_size - 1
 *   [path N bytes]       -- hop_count * hash_size bytes
 *   [payload ...]
 */
int MeshCoreDecryption::getPayloadOffset(const uint8_t* data, size_t length) {
    if (length < 2) return -1;

    uint8_t routeType = data[0] & 0x03;

    // Route types 2 (TRANSPORT_FLOOD) and 3 (TRANSPORT_DIRECT) carry 4-byte transport codes
    int pathLenOffset = (routeType == 2 || routeType == 3) ? 5 : 1;

    if ((size_t)(pathLenOffset + 1) > length) return -1;

    uint8_t pathLenByte = data[pathLenOffset];
    uint8_t hopCount = pathLenByte & 0x3F;               // bits 0-5
    uint8_t hashSize = ((pathLenByte >> 6) & 0x03) + 1;  // bits 6-7, value 1-4

    int payloadOffset = pathLenOffset + 1 + (int)(hopCount * hashSize);

    if ((size_t)payloadOffset >= length) return -1;

    return payloadOffset;
}

// --- Decryption --------------------------------------------------------------

/**
 * Try one 16-byte key against a group message payload.
 *
 * payload format: [channel_hash 1B][mac 2B][ciphertext N*16B]
 *
 * Steps:
 *  1. Fast check: SHA256(key)[0] must equal payload[0]
 *  2. HMAC-SHA256(ciphertext, zero_padded_key_32B)[0:2] must equal payload[1:3]
 *  3. AES-128-ECB decrypt each 16-byte block
 *  4. Parse plaintext: [4B LE timestamp][1B flags][text...]
 */
bool MeshCoreDecryption::tryKey(const uint8_t key[16], const uint8_t* payload, size_t payloadLen) {
    // Need at least: channel_hash(1) + mac(2) + one AES block(16)
    if (payloadLen < 19) return false;

    const uint8_t* ciphertext = payload + 3;
    size_t ciphertextLen = payloadLen - 3;

    // Ciphertext must be a non-zero multiple of 16 (AES block size)
    if (ciphertextLen == 0 || (ciphertextLen % 16) != 0) return false;

    // Step 1: channel hash fast-reject (SHA256(key)[0] must match payload[0])
    uint8_t keyHash[32];
    mbedtls_sha256(key, 16, keyHash, 0);
    if (keyHash[0] != payload[0]) return false;

    // Step 2: HMAC-SHA256 verification
    // HMAC key = 16-byte channel key zero-padded to 32 bytes
    uint8_t hmacKey[32] = {0};
    memcpy(hmacKey, key, 16);

    uint8_t mac[32];
    mbedtls_md_context_t mdCtx;
    mbedtls_md_init(&mdCtx);
    const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_setup(&mdCtx, mdInfo, 1 /* HMAC mode */);
    mbedtls_md_hmac_starts(&mdCtx, hmacKey, 32);
    mbedtls_md_hmac_update(&mdCtx, ciphertext, ciphertextLen);
    mbedtls_md_hmac_finish(&mdCtx, mac);
    mbedtls_md_free(&mdCtx);

    if (mac[0] != payload[1] || mac[1] != payload[2]) return false;

    // Step 3: AES-128-ECB decrypt (one 16-byte block at a time)
    if (ciphertextLen > 192) return false;  // Sanity: MeshCore max payload is 184B
    uint8_t plaintext[192] = {0};

    mbedtls_aes_context aesCtx;
    mbedtls_aes_init(&aesCtx);
    mbedtls_aes_setkey_dec(&aesCtx, key, 128);
    for (size_t i = 0; i < ciphertextLen; i += 16) {
        mbedtls_aes_crypt_ecb(&aesCtx, MBEDTLS_AES_DECRYPT, ciphertext + i, plaintext + i);
    }
    mbedtls_aes_free(&aesCtx);

    // Step 4: Parse plaintext — [4B LE timestamp][1B flags][text...]
    // Need at least 5 bytes (timestamp + flags) for a valid frame
    if (ciphertextLen < 5) return false;

    // Extract text starting at byte 5, stop at null or end of block
    const uint8_t* textStart = plaintext + 5;
    size_t maxTextBytes = ciphertextLen - 5;
    size_t textLen = 0;

    for (size_t i = 0; i < maxTextBytes; i++) {
        uint8_t c = textStart[i];
        if (c == 0) break;
        // Accept printable ASCII and UTF-8 continuation bytes (>= 0x80)
        // Reject control characters except tab(9), LF(10), CR(13)
        if (c < 9 || (c > 13 && c < 32)) return false;
        textLen++;
    }

    if (textLen == 0) return false;  // Empty message — not interesting

    // Store message thread-safely
    if (!messageMutex) return false;
    if (xSemaphoreTake(messageMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        size_t copyLen = (textLen < MAX_MESSAGE_LEN - 1) ? textLen : MAX_MESSAGE_LEN - 1;
        memcpy(lastMessage, textStart, copyLen);
        lastMessage[copyLen] = '\0';
        xSemaphoreGive(messageMutex);
    }

    return true;
}

bool MeshCoreDecryption::tryDecrypt(const uint8_t* data, size_t length) {
    if (!initialized) return false;

    int payloadOffset = getPayloadOffset(data, length);
    if (payloadOffset < 0) return false;

    const uint8_t* payload = data + payloadOffset;
    size_t payloadLen = length - payloadOffset;

    // Minimum: channel_hash(1) + mac(2) + one AES block(16)
    if (payloadLen < 19) return false;

    clearLastMessage();
    // Clear channel name for this attempt
    if (messageMutex && xSemaphoreTake(messageMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        lastChannelName[0] = '\0';
        xSemaphoreGive(messageMutex);
    }

    for (uint8_t i = 0; i < NUM_KEYS; i++) {
        if (tryKey(channelKeys[i], payload, payloadLen)) {
            const char* name = (i == 0) ? "public" : HASHTAG_ROOMS[i - 1];
            LOG_INFO("MeshCore: decrypted with key[%d] (%s)", i,
                     i == 0 ? "public channel" : HASHTAG_ROOMS[i - 1]);
            // Store channel name thread-safely
            if (messageMutex && xSemaphoreTake(messageMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                strncpy(lastChannelName, name, MAX_CHANNEL_LEN - 1);
                lastChannelName[MAX_CHANNEL_LEN - 1] = '\0';
                xSemaphoreGive(messageMutex);
            }
            return true;
        }
    }

    return false;
}

void MeshCoreDecryption::getLastChannelName(char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    buffer[0] = '\0';
    if (!messageMutex) return;
    if (xSemaphoreTake(messageMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        strncpy(buffer, lastChannelName, bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
        xSemaphoreGive(messageMutex);
    }
}

// --- Thread-safe message access ----------------------------------------------

void MeshCoreDecryption::getLastMessageSafe(char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    buffer[0] = '\0';
    if (!messageMutex) return;
    if (xSemaphoreTake(messageMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        strncpy(buffer, lastMessage, bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
        xSemaphoreGive(messageMutex);
    }
}

void MeshCoreDecryption::clearLastMessage() {
    if (!messageMutex) return;
    if (xSemaphoreTake(messageMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        lastMessage[0] = '\0';
        xSemaphoreGive(messageMutex);
    }
}
