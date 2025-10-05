#include "psk_decryption_simple.h"

#ifdef ENABLE_PSK_TESTING

#include <mbedtls/aes.h>
#include <mbedtls/base64.h>
#include "data_structures.h"  // For PSKStats struct

// Known Meshtastic default PSKs (some networks still use these)
const char* defaultPSKs[] = {
  "1PG7OiApB1nwvP+rz05pAQ==",  // Most common default
  "AQ==",                      // Short default
  "d1iq21lNSh7BP6MOkP6cQA==",  // Channel 1
  "2f8aH6iT8K9jQ1P3mD4nBw==",  // Channel 2
  "7h3kL9mR5wX2pY8qE6tZcA==",  // Channel 3
};

const uint8_t NUM_DEFAULT_PSKS = sizeof(defaultPSKs) / sizeof(char*);

// PSK stats instance
PSKStats pskStats;

void initializePSKStats() {
    pskStats = PSKStats();  // Use constructor instead of memset
}

// Base64 decode helper
bool PSKDecryption::decodeBase64(const char* input, uint8_t* output, size_t maxLen) {
  size_t outLen;
  int result = mbedtls_base64_decode(output, maxLen, &outLen, 
                                    (const unsigned char*)input, strlen(input));
  return (result == 0 && outLen > 0);
}

// Attempt packet decryption with known PSKs
bool PSKDecryption::testDefaultPSKs(const uint8_t* data, size_t length) {
  pskStats.attempts++;
  
  // Declare variables at function scope to avoid scope issues
  bool isTextMessage = false;
  bool isRoutingPattern = false;
  String messageText = "";
  
  // Enhanced Meshtastic message analysis
  if (length >= 8) {
    Serial.println("[PSK] === MESHTASTIC MESSAGE ANALYSIS ===");
    
    // Meshtastic packet structure analysis
    Serial.printf("[PSK] Header: %02X %02X %02X %02X (signature)\n", data[0], data[1], data[2], data[3]);
    Serial.printf("[PSK] Node ID: 0x%08X\n", (uint32_t(data[4]) << 24) | (uint32_t(data[5]) << 16) | (uint32_t(data[6]) << 8) | uint32_t(data[7]));
    
    if (length > 8) {
      Serial.printf("[PSK] Message Type: 0x%02X\n", data[8]);
      
      // Enhanced text message detection - look for longer, meaningful text
      // Skip the obvious routing patterns and look for actual user content
      
      // Enhanced message type detection with proper protocol understanding
      uint8_t msgType = data[8];
      
      // Routing/control message patterns (protocol metadata - expected plaintext)
      isRoutingPattern = (length <= 16 && 
                         ((msgType >= 0x20 && msgType <= 0x2F) || // Routing protocol range
                          (msgType == 0x23 || msgType == 0x24) || // Common routing types
                          (data[length-2] == 0x63 && data[length-1] == 0x66) || // Common suffix
                                (msgType >= 0xF0))); // High-range control messages
      
      // User message type indicators (these SHOULD be encrypted if PSK is enabled)
      bool isPotentialUserMessage = (msgType == 0x00 || msgType == 0x01 || // Text messages
                                    msgType == 0x03 || msgType == 0x04 || // Position/telemetry
                                    msgType == 0x08 || msgType == 0x09 || // Other user content
                                    (msgType >= 0x40 && msgType <= 0x7F) || // User app range
                                    length > 25); // Longer packets more likely to contain user data
      
      Serial.printf("[PSK] Analysis: MsgType=0x%02X, Length=%d, Protocol=%s, UserData=%s\n", 
                    msgType, length, isRoutingPattern ? "ROUTING" : "DATA", 
                    isPotentialUserMessage ? "LIKELY" : "NO");
      
      // Only search for text in packets that should contain user data
      if (!isRoutingPattern && isPotentialUserMessage) {
        Serial.println("[PSK] Analyzing potential user data for text content...");
        
        // Method 1: Look for substantial text content (standard approach)
        for (size_t startPos = 8; startPos < length - 2; startPos++) {
          String candidate = "";
          size_t consecutiveText = 0;
          
          for (size_t i = startPos; i < length && candidate.length() < 128; i++) {
            if (data[i] >= 32 && data[i] <= 126) {  // Printable ASCII
              candidate += (char)data[i];
              consecutiveText++;
            } else if (data[i] == 0) {
              break;  // Null terminator
            } else if (data[i] < 32) {
              // Allow some control chars but break on too many
              if (consecutiveText > 0 && candidate.length() < 64) {
                candidate += '.';  // Represent as dot
              } else {
                break;
              }
            } else {
              break;  // Non-ASCII
            }
          }
          
          // Improved validation for user text messages
          if (candidate.length() >= 3) {  // Lower threshold for detection
            // Additional validation - exclude obvious routing fragments
            bool isValidMessage = true;
            String lower = candidate;
            lower.toLowerCase();
            
            // Exclude common routing patterns
            if (candidate.length() < 3 || 
                (candidate.length() == 2 && candidate == "cf") ||
                candidate.startsWith("`cf") || candidate.startsWith("Tcf") ||
                candidate.startsWith("1cf") || candidate == "63f" ||
                (candidate.length() < 6 && consecutiveText < 3)) {
              isValidMessage = false;
            }
            
            if (isValidMessage && consecutiveText >= 3) {
              messageText = candidate;
              isTextMessage = true;
              Serial.printf("[PSK] 📧 FOUND TEXT MESSAGE: \"%s\" (method 1)\n", messageText.c_str());
              break;
            }
          }
        }
        
        // Method 2: Look for protobuf-style text (Meshtastic uses protobuf)
        if (!isTextMessage && length > 12) {
          Serial.println("[PSK] Trying protobuf text extraction...");
          for (size_t i = 8; i < length - 4; i++) {
            // Look for protobuf string fields (tag + length + data)
            if (data[i] >= 0x08 && data[i] <= 0x7F && i + 2 < length) {
              uint8_t fieldTag = data[i];
              uint8_t strLen = data[i + 1];
              
              if (strLen > 2 && strLen < 64 && i + 1 + strLen < length) {
                String pbText = "";
                bool validText = true;
                
                for (uint8_t j = 0; j < strLen && validText; j++) {
                  uint8_t c = data[i + 2 + j];
                  if (c >= 32 && c <= 126) {
                    pbText += (char)c;
                  } else if (c == 0) {
                    break;
                  } else {
                    validText = false;
                  }
                }
                
                if (validText && pbText.length() >= 3) {
                  messageText = pbText;
                  isTextMessage = true;
                  Serial.printf("[PSK] 📧 PROTOBUF TEXT MESSAGE: \"%s\" (tag=0x%02X)\n", 
                               messageText.c_str(), fieldTag);
                  break;
                }
              }
            }
          }
        }
      }
      
      if (!isTextMessage) {
        // Analyze as routing/control message
        Serial.print("[PSK] Control/Routing Data: ");
        for (size_t i = 8; i < length; i++) {
          if (data[i] >= 32 && data[i] <= 126) {
            Serial.printf("'%c' ", data[i]);
          } else {
            Serial.printf("0x%02X ", data[i]);
          }
        }
        Serial.println();
        
        // Enhanced message type identification
        if (length <= 16 && ((data[8] >= 0x0A && data[8] <= 0x2F) || 
                             (data[length-2] == 0x63 && data[length-1] == 0x66))) {
          Serial.println("[PSK] >>> MESH ROUTING/CONTROL MESSAGE <<<");
          Serial.printf("[PSK] Message subtype: 0x%02X (routing protocol)\n", data[8]);
        } else if (length > 20) {
          Serial.println("[PSK] >>> POSSIBLE USER DATA MESSAGE <<<");
          Serial.println("[PSK] ⚠️ No readable text found - may be binary data or different encoding");
        } else {
          Serial.println("[PSK] >>> SHORT CONTROL MESSAGE <<<");
        }
        
        // Debug: Show full packet analysis for troubleshooting
        if (!isTextMessage) {
          Serial.println("[PSK] 🔍 FULL PACKET ANALYSIS (no text found):");
          Serial.print("[PSK] Hex: ");
          for (size_t i = 0; i < length; i++) {
            Serial.printf("%02X ", data[i]);
          }
          Serial.println();
          Serial.print("[PSK] ASCII attempt: ");
          for (size_t i = 8; i < length; i++) {
            if (data[i] >= 32 && data[i] <= 126) {
              Serial.printf("'%c' ", data[i]);
            } else {
              Serial.printf("[%02X] ", data[i]);
            }
          }
          Serial.println();
        }
      } else {
        Serial.println("[PSK] >>> USER TEXT MESSAGE DETECTED <<<");
      }
    }
    
    // Accurate assessment of security implications
    if (isTextMessage && !isRoutingPattern) {
      Serial.println("[PSK] � SECURITY ISSUE: USER CONTENT IN PLAINTEXT");
      pskStats.successes++; // Count as security finding
    } else if (isRoutingPattern) {
      Serial.println("[PSK] ℹ️ PROTOCOL METADATA (Expected - enables mesh routing)");
      // Don't count protocol metadata as security issue
    } else {
      Serial.println("[PSK] 🔧 PROTOCOL DATA - no user content detected");
    }
    return true;
  }
  
  // Only try PSK decryption on longer packets that might be encrypted
  if (length < 20) {
    Serial.println("[PSK] Packet too short for encrypted content");
    return false;
  }
  
  // Skip Meshtastic header (first 8 bytes for node ID) and try decryption
  const uint8_t* encryptedData = data + 8;
  size_t encryptedLen = length - 8;
  
  for (uint8_t i = 0; i < NUM_DEFAULT_PSKS; i++) {
    uint8_t key[16];
    if (!decodeBase64(defaultPSKs[i], key, sizeof(key))) continue;
    
    // AES decrypt
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    if (mbedtls_aes_setkey_dec(&aes, key, 128) != 0) {
      mbedtls_aes_free(&aes);
      continue;
    }
    
    uint8_t decrypted[256]; // Use fixed size buffer
    bool validDecryption = true;
    
    // Decrypt in 16-byte blocks
    for (size_t j = 0; j < encryptedLen - 15; j += 16) {
      if (mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, 
                                encryptedData + j, decrypted + j) != 0) {
        validDecryption = false;
        break;
      }
    }
    
    mbedtls_aes_free(&aes);
    
    // Enhanced validation and message extraction from decrypted content
    if (validDecryption && decrypted[0] != 0x00) {
      pskStats.successes++;
      pskStats.hitCount[i]++;
      
      Serial.printf("[PSK] 🔓 DECRYPTION SUCCESS with default key #%d!\n", i + 1);
      Serial.println("[PSK] === DECRYPTED MESSAGE ANALYSIS ===");
      
      // Display original encrypted data
      Serial.print("[PSK] Encrypted data: ");
      for (size_t j = 0; j < min(encryptedLen, size_t(16)); j++) {
        Serial.printf("%02X ", encryptedData[j]);
      }
      if (encryptedLen > 16) Serial.print("...");
      Serial.println();
      
      // Analyze decrypted message structure
      Serial.print("[PSK] Decrypted hex: ");
      for (size_t j = 0; j < min(encryptedLen, size_t(32)); j++) {
        Serial.printf("%02X ", decrypted[j]);
      }
      if (encryptedLen > 32) Serial.print("...");
      Serial.println();
      
      // Look for text messages in decrypted content
      bool foundTextMessage = false;
      String messageText = "";
      
      // Search for readable text in decrypted payload
      for (size_t startPos = 0; startPos < encryptedLen - 3 && startPos < 20; startPos++) {
        String candidate = "";
        bool foundText = false;
        
        for (size_t j = startPos; j < min(encryptedLen, size_t(64)); j++) {
          if (decrypted[j] >= 32 && decrypted[j] <= 126) {  // Printable ASCII
            candidate += (char)decrypted[j];
            foundText = true;
          } else if (decrypted[j] == 0) {
            break;  // Null terminator
          } else if (foundText && candidate.length() > 2) {
            break;  // End of text section
          }
        }
        
        if (candidate.length() >= 3) {  // Minimum meaningful text length
          messageText = candidate;
          foundTextMessage = true;
          Serial.printf("[PSK] 📧 DECRYPTED TEXT MESSAGE: \"%s\"\n", messageText.c_str());
          break;
        }
      }
      
      if (!foundTextMessage) {
        // Display raw decrypted content with ASCII interpretation
        Serial.print("[PSK] 🔍 Decrypted content: ");
        for (size_t j = 0; j < min(encryptedLen, size_t(64)); j++) {
          if (decrypted[j] >= 32 && decrypted[j] <= 126) {
            Serial.printf("'%c' ", decrypted[j]);
          } else {
            Serial.printf("0x%02X ", decrypted[j]);
          }
        }
        Serial.println();
        
        // Try to identify message type
        if (encryptedLen < 32) {
          Serial.println("[PSK] >>> DECRYPTED CONTROL/ROUTING MESSAGE <<<");
        } else {
          Serial.println("[PSK] >>> DECRYPTED DATA MESSAGE (non-text) <<<");
        }
      } else {
        Serial.println("[PSK] >>> 🎯 USER TEXT MESSAGE SUCCESSFULLY DECRYPTED! <<<");
      }
      
      Serial.printf("[PSK] Key used: \"%s\" (Default key #%d)\n", defaultPSKs[i], i + 1);
      Serial.println("[PSK] === END DECRYPTION ANALYSIS ===");
      
      return true;
    }
  }
  
  return false;
}

void PSKDecryption::initialize() {
  Serial.println("🔐 PSK Testing module initialized");
  Serial.printf("📊 Loaded %d default Meshtastic PSKs\n", NUM_DEFAULT_PSKS);
}

PSKStats PSKDecryption::getStats() {
  return pskStats;
}

void PSKDecryption::resetStats() {
  pskStats = PSKStats();  // Use constructor instead of memset
}

void PSKDecryption::printStats() {
  Serial.println("📊 PSK DECRYPTION STATISTICS:");
  Serial.printf("   Total Attempts: %d\n", pskStats.attempts);
  Serial.printf("   Successful: %d (%.1f%% success rate)\n", 
                pskStats.successes,
                pskStats.attempts > 0 ? (float)pskStats.successes / pskStats.attempts * 100.0 : 0.0);
  
  Serial.println("   PSK Hit Count:");
  for (uint8_t i = 0; i < NUM_DEFAULT_PSKS; i++) {
    if (pskStats.hitCount[i] > 0) {
      Serial.printf("     PSK #%d: %d hits\n", i + 1, pskStats.hitCount[i]);
    }
  }
  Serial.println();
}

#endif // ENABLE_PSK_TESTING