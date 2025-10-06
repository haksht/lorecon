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
    // Note: This is just preliminary analysis of the encrypted packet
    // We'll still attempt decryption below
    if (isTextMessage && !isRoutingPattern) {
      Serial.println("[PSK] ⚠️ Possible text in encrypted data (false positive likely)");
      // Don't return - continue to decryption attempt
    } else if (isRoutingPattern) {
      Serial.println("[PSK] ℹ️ PROTOCOL METADATA (Expected - enables mesh routing)");
      // Still try decryption for protocol packets too
    } else {
      Serial.println("[PSK] 🔧 ENCRYPTED DATA - attempting decryption...");
    }
    // DON'T return here - continue to decryption!
  }
  
  // Only try PSK decryption on longer packets that might be encrypted
  if (length < 20) {
    Serial.println("[PSK] Packet too short for encrypted content");
    return false;
  }
  
  // Meshtastic packet structure:
  // [0-3]   Header (0xFF FF FF FF)
  // [4-7]   From Node ID (4 bytes)
  // [8]     Packet ID / Message Type
  // [9]     Flags
  // [10-13] Packet Counter (4 bytes) - used as part of nonce
  // [14+]   Encrypted payload
  
  Serial.println("[PSK] === ATTEMPTING AES-CTR DECRYPTION ===");
  
  // Extract packet counter for nonce (bytes 10-13, little-endian)
  uint32_t packetId = 0;
  if (length >= 14) {
    packetId = ((uint32_t)data[10]) | 
               ((uint32_t)data[11] << 8) | 
               ((uint32_t)data[12] << 16) | 
               ((uint32_t)data[13] << 24);
    Serial.printf("[PSK] Packet ID/Counter: 0x%08X\n", packetId);
  }
  
  // Encrypted data starts after header (typically byte 14+)
  const uint8_t* encryptedData = data + 14;
  size_t encryptedLen = length - 14;
  
  if (encryptedLen < 1) {
    Serial.println("[PSK] No encrypted payload found");
    return false;
  }
  
  for (uint8_t i = 0; i < NUM_DEFAULT_PSKS; i++) {
    uint8_t key[16];
    if (!decodeBase64(defaultPSKs[i], key, sizeof(key))) {
      Serial.printf("[PSK] Failed to decode PSK #%d\n", i + 1);
      continue;
    }
    
    Serial.printf("[PSK] Trying default key #%d: \"%s\"\n", i + 1, defaultPSKs[i]);
    
    // Construct nonce for AES-CTR mode
    // Meshtastic uses: PacketID (4 bytes) + FromNode (4 bytes) + zeros (8 bytes) = 16 bytes
    uint8_t nonce[16];
    memset(nonce, 0, sizeof(nonce));
    
    // First 4 bytes: packet counter (little-endian)
    nonce[0] = (packetId) & 0xFF;
    nonce[1] = (packetId >> 8) & 0xFF;
    nonce[2] = (packetId >> 16) & 0xFF;
    nonce[3] = (packetId >> 24) & 0xFF;
    
    // Next 4 bytes: from node ID
    nonce[4] = data[4];
    nonce[5] = data[5];
    nonce[6] = data[6];
    nonce[7] = data[7];
    
    // Remaining 8 bytes stay zero
    
    Serial.print("[PSK] Nonce: ");
    for (int j = 0; j < 16; j++) {
      Serial.printf("%02X ", nonce[j]);
    }
    Serial.println();
    
    // Initialize AES context for CTR mode
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    // For CTR mode, use setkey_enc (not setkey_dec)
    if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0) {
      Serial.println("[PSK] Failed to set AES key");
      mbedtls_aes_free(&aes);
      continue;
    }
    
    uint8_t decrypted[256]; // Use fixed size buffer
    uint8_t stream_block[16];
    size_t nc_off = 0;
    
    // Copy nonce to stream_block (will be modified by crypt_ctr)
    memcpy(stream_block, nonce, 16);
    
    // Decrypt using AES-CTR mode
    int result = mbedtls_aes_crypt_ctr(&aes, encryptedLen, &nc_off,
                                       stream_block, stream_block,
                                       encryptedData, decrypted);
    
    mbedtls_aes_free(&aes);
    
    if (result != 0) {
      Serial.printf("[PSK] CTR decryption failed with error %d\n", result);
      continue;
    }
    if (result != 0) {
      Serial.printf("[PSK] CTR decryption failed with error %d\n", result);
      continue;
    }
    
    // DEBUG: Show first 16 bytes of decrypted data before validation
    Serial.print("[PSK] >>> Decrypted (pre-validation): ");
    for (size_t j = 0; j < min(encryptedLen, size_t(16)); j++) {
      Serial.printf("%02X ", decrypted[j]);
    }
    Serial.println();
    
    // Validate decryption by checking for protobuf structure
    // Valid protobuf starts with field tags (varint encoding)
    // Meshtastic protobuf can start with various field numbers
    bool looksLikeProtobuf = false;
    
    if (encryptedLen >= 3) {
      // Check for protobuf wire type patterns
      // Wire types: 0=varint, 1=64bit, 2=length-delimited, 5=32bit
      // Field tags encode as: (field_number << 3) | wire_type
      // Valid first bytes: 0x08, 0x0A, 0x0C, 0x0D, 0x10, 0x12, etc.
      uint8_t firstByte = decrypted[0];
      uint8_t wireType = firstByte & 0x07;
      uint8_t fieldNumber = firstByte >> 3;
      
      // Accept if it looks like a valid field tag:
      // - Wire type 0, 2, or 5 (common in Meshtastic)
      // - Field number 1-15 (most common fields)
      // - OR specific known patterns
      looksLikeProtobuf = (
        // Common field tags
        (fieldNumber >= 1 && fieldNumber <= 15 && 
         (wireType == 0 || wireType == 2 || wireType == 5)) ||
        // Specific known patterns
        (firstByte == 0x08 || firstByte == 0x0A || firstByte == 0x0C || 
         firstByte == 0x0D || firstByte == 0x10 || firstByte == 0x12 || 
         firstByte == 0x1A || firstByte == 0x20)
      );
      
      // Additional check: protobuf shouldn't have long runs of same byte
      bool hasVariety = false;
      for (size_t j = 1; j < min(encryptedLen, size_t(8)); j++) {
        if (decrypted[j] != decrypted[0]) {
          hasVariety = true;
          break;
        }
      }
      looksLikeProtobuf = looksLikeProtobuf && hasVariety;
      
      // DEBUG: Show validation result
      if (!looksLikeProtobuf) {
        Serial.printf("[PSK] Validation failed: firstByte=0x%02X (field=%d, wire=%d), hasVariety=%d\n", 
                     firstByte, fieldNumber, wireType, hasVariety);
      }
    }
    
    // Enhanced validation and message extraction from decrypted content
    if (looksLikeProtobuf) {
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
      size_t displayLen = min(encryptedLen, size_t(64));  // Show more bytes for large packets
      for (size_t j = 0; j < displayLen; j++) {
        Serial.printf("%02X ", decrypted[j]);
        if ((j + 1) % 16 == 0 && j + 1 < displayLen) Serial.print("\n[PSK]                  ");
      }
      if (encryptedLen > displayLen) Serial.print("...");
      Serial.println();
      
      // Look for text messages in decrypted content
      bool foundTextMessage = false;
      String messageText = "";
      
      // Meshtastic protobuf structure (nested):
      // Top level (Data message):
      //   Field 1 (portnum): 0x08 <varint> - Usually 0x01 for TEXT_MESSAGE_APP
      //   Field 2 (payload): 0x12 <length> <nested protobuf data>
      //
      // Nested (for TEXT_MESSAGE_APP):
      //   Field 1 (text): 0x0A <length> <actual text string>
      //
      // So we look for: 0x08 <portnum> 0x12 <len1> 0x0A <len2> <TEXT>
      
      Serial.println("[PSK] Parsing protobuf structure for text...");
      
      // Show first 16 bytes for manual inspection
      Serial.print("[PSK] First 16 bytes: ");
      for (size_t j = 0; j < min(encryptedLen, size_t(16)); j++) {
        if (decrypted[j] >= 32 && decrypted[j] <= 126) {
          Serial.printf("'%c' ", decrypted[j]);
        } else {
          Serial.printf("0x%02X ", decrypted[j]);
        }
      }
      Serial.println();
      
      // First, try to identify the message type (portnum)
      uint8_t portnum = 0xFF; // Unknown
      bool foundPortnum = false;
      
      // Search more aggressively for portnum field (0x08)
      for (size_t pos = 0; pos < min(encryptedLen, size_t(16)); pos++) {
        if (decrypted[pos] == 0x08 && pos + 1 < encryptedLen) {
          portnum = decrypted[pos + 1];
          foundPortnum = true;
          Serial.printf("[PSK] Found portnum field at offset %d: 0x%02X ", pos, portnum);
          // Common Meshtastic port numbers:
          switch(portnum) {
            case 0x01: Serial.println("(TEXT_MESSAGE_APP) ✉️"); break;
            case 0x03: Serial.println("(POSITION_APP) 📍"); break;
            case 0x04: Serial.println("(NODEINFO_APP) ℹ️"); break;
            case 0x07: Serial.println("(ROUTING_APP) 🔀"); break;
            case 0x08: Serial.println("(ADMIN_APP) ⚙️"); break;
            case 0x09: Serial.println("(TEXT_MESSAGE_COMPRESSED_APP) ✉️🗜️"); break;
            case 0x10: Serial.println("(RANGE_TEST_APP) 📡"); break;
            case 0x20: Serial.println("(STORE_FORWARD_APP) 💾"); break;
            case 0x41: Serial.println("(TRACEROUTE_APP) 🗺️"); break;
            case 0x43: Serial.println("(NEIGHBORINFO_APP) 👥"); break;
            case 0x44: Serial.println("(ATAK_PLUGIN) 🎖️"); break;
            case 0x101: Serial.println("(PRIVATE_APP) 🔒"); break;
            case 0x102: Serial.println("(ATAK_FORWARDER) 📨"); break;
            default: Serial.printf("(UNKNOWN/CUSTOM - decimal:%d)\n", portnum); break;
          }
          break;
        }
      }
      
      if (!foundPortnum) {
        Serial.println("[PSK] ⚠️  No portnum field (0x08) found in decrypted data");
        
        // Look for 0x08 anywhere to help debug
        bool found08 = false;
        for (size_t i = 0; i < encryptedLen; i++) {
          if (decrypted[i] == 0x08) {
            Serial.printf("[PSK] Note: Found 0x08 at offset %d (next byte: 0x%02X)\n", 
                         i, (i+1 < encryptedLen) ? decrypted[i+1] : 0x00);
            found08 = true;
            break;
          }
        }
        
        Serial.println("[PSK] This might be:");
        Serial.println("[PSK]   - A non-Data packet (ACK, routing header, etc.)");
        Serial.println("[PSK]   - Wrong decryption key (gibberish)");
        Serial.println("[PSK]   - Different protobuf message type");
      }
      
      // Look for portnum field (0x08) followed by TEXT_MESSAGE_APP (0x01)
      for (size_t pos = 0; pos < encryptedLen - 5; pos++) {
        // Pattern: 0x08 0x01 (portnum=TEXT_MESSAGE_APP)
        if (decrypted[pos] == 0x08 && decrypted[pos + 1] == 0x01) {
          Serial.printf("[PSK] Found TEXT_MESSAGE_APP at offset %d\n", pos);
          
          // Next should be 0x12 (payload field)
          if (pos + 2 < encryptedLen && decrypted[pos + 2] == 0x12) {
            uint8_t payloadLen = decrypted[pos + 3];
            Serial.printf("[PSK] Payload field found, length=%d\n", payloadLen);
            
            // Inside payload, look for 0x0A (text field, wire type 2)
            size_t payloadStart = pos + 4;
            if (payloadStart < encryptedLen && decrypted[payloadStart] == 0x0A) {
              uint8_t textLen = decrypted[payloadStart + 1];
              Serial.printf("[PSK] Text field found, length=%d\n", textLen);
              
              // Extract text
              if (textLen > 0 && textLen < 80 && (payloadStart + 2 + textLen) <= encryptedLen) {
                String text = "";
                bool validText = true;
                
                for (uint8_t i = 0; i < textLen; i++) {
                  uint8_t c = decrypted[payloadStart + 2 + i];
                  if (c >= 32 && c <= 126) {  // Printable ASCII
                    text += (char)c;
                  } else if (c == 0x0A || c == 0x0D) {  // Allow newlines
                    text += '\n';
                  } else if (c == 0) {
                    break;  // Null terminator
                  } else {
                    // Non-printable character
                    validText = false;
                    break;
                  }
                }
                
                if (validText && text.length() >= 1) {
                  messageText = text;
                  foundTextMessage = true;
                  Serial.printf("[PSK] 📧 DECRYPTED TEXT MESSAGE: \"%s\"\n", messageText.c_str());
                  break;
                }
              }
            }
          }
        }
      }
      
      // Alternative: Look for 0x12 (payload) then 0x0A (text) pattern anywhere
      if (!foundTextMessage) {
        Serial.println("[PSK] Trying alternative protobuf parsing...");
        for (size_t pos = 0; pos < encryptedLen - 4; pos++) {
          // Look for: 0x12 <len> 0x0A <textlen> <text>
          if (decrypted[pos] == 0x12 && pos + 1 < encryptedLen) {
            uint8_t payloadLen = decrypted[pos + 1];
            if (payloadLen > 2 && pos + 2 < encryptedLen && decrypted[pos + 2] == 0x0A) {
              uint8_t textLen = decrypted[pos + 3];
              
              if (textLen > 0 && textLen < 80 && (pos + 4 + textLen) <= encryptedLen) {
                String text = "";
                bool validText = true;
                
                for (uint8_t i = 0; i < textLen; i++) {
                  uint8_t c = decrypted[pos + 4 + i];
                  if (c >= 32 && c <= 126) {
                    text += (char)c;
                  } else if (c == 0x0A || c == 0x0D) {
                    text += '\n';
                  } else if (c == 0) {
                    break;
                  } else {
                    validText = false;
                    break;
                  }
                }
                
                if (validText && text.length() >= 1) {
                  messageText = text;
                  foundTextMessage = true;
                  Serial.printf("[PSK] 📧 DECRYPTED TEXT MESSAGE: \"%s\"\n", messageText.c_str());
                  break;
                }
              }
            }
          }
        }
      }
      
      // Last fallback: search for any continuous readable text (min 4 chars)
      if (!foundTextMessage) {
        Serial.println("[PSK] Searching for any readable text...");
        
        // Also try looking for 0x0A (text field) directly
        for (size_t pos = 0; pos < encryptedLen - 3; pos++) {
          if (decrypted[pos] == 0x0A && pos + 1 < encryptedLen) {
            uint8_t textLen = decrypted[pos + 1];
            // Check if length seems reasonable and data exists
            if (textLen > 0 && textLen < 80 && (pos + 2 + textLen) <= encryptedLen) {
              String text = "";
              bool validText = true;
              
              for (uint8_t i = 0; i < textLen; i++) {
                uint8_t c = decrypted[pos + 2 + i];
                if (c >= 32 && c <= 126) {
                  text += (char)c;
                } else if (c == 0x0A || c == 0x0D) {
                  text += '\n';
                } else if (c == 0) {
                  break;
                } else {
                  validText = false;
                  break;
                }
              }
              
              if (validText && text.length() >= 3) {
                messageText = text;
                foundTextMessage = true;
                Serial.printf("[PSK] 📧 FOUND TEXT (0x0A pattern): \"%s\"\n", messageText.c_str());
                break;
              }
            }
          }
        }
        
        // If still nothing, look for continuous ASCII
        if (!foundTextMessage) {
          for (size_t startPos = 0; startPos < encryptedLen - 4; startPos++) {
            String candidate = "";
            
            for (size_t j = startPos; j < min(encryptedLen, size_t(80)); j++) {
              if (decrypted[j] >= 32 && decrypted[j] <= 126) {  // Printable ASCII
                candidate += (char)decrypted[j];
              } else if (decrypted[j] == 0) {
                break;  // Null terminator
              } else {
                break;  // Non-printable, end of text
              }
            }
          
            if (candidate.length() >= 4) {  // Minimum 4 chars for real message
              messageText = candidate;
              foundTextMessage = true;
              Serial.printf("[PSK] 📧 DECRYPTED TEXT MESSAGE: \"%s\"\n", messageText.c_str());
              break;
            }
          }
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