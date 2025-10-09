# Session Key Harvesting - Integration Checklist

## Quick Integration Guide

### Files Already Created ✅
- [x] `firmware/src/session_key_manager.h` - Header file
- [x] `firmware/src/session_key_manager.cpp` - Implementation
- [x] `docs/SESSION_KEY_HARVESTING_GUIDE.md` - Complete documentation

### Integration Steps

#### Step 1: Add Include to `lora_recon_tool.h`
**File:** `firmware/src/lora_recon_tool.h`  
**Location:** After existing includes (~line 10)

```cpp
#include "session_key_manager.h"  // ADD THIS LINE
```

**Location:** Inside `LoRaReconTool` class private section (~line 80)

```cpp
private:
    // ... existing members ...
    
    // NEW: Session key management
    SessionKeyManager sessionKeyManager;
    
    // Helper functions
    void trySessionKeyDecryption(const uint8_t* data, size_t length, 
                                  uint32_t nodeId, uint32_t packetId);
```

---

#### Step 2: Initialize in `lora_recon_tool.cpp`
**File:** `firmware/src/lora_recon_tool.cpp`  
**Function:** `LoRaReconTool::initialize()`  
**Location:** After PSK initialization (~line 95)

```cpp
bool LoRaReconTool::initialize() {
    // ... existing initialization code ...
    
    #ifdef ENABLE_PSK_TESTING
    PSKDecryption::initialize();
    #endif
    
    // ADD THIS BLOCK:
    // Initialize session key manager with channel PSK
    uint8_t channelPSK[16];
    size_t pskLen = 0;
    // Decode your channel PSK - replace with your actual PSK!
    const char* myChannelPSK = "1PG7OiApB1nwvP+rz05pAQ==";  // User confirmed this one works
    
    int result = mbedtls_base64_decode(channelPSK, sizeof(channelPSK), &pskLen,
                                       (const unsigned char*)myChannelPSK, 
                                       strlen(myChannelPSK));
    
    if (result == 0 && pskLen > 0) {
        sessionKeyManager.initialize(&radio, channelPSK, pskLen);
        Serial.println("\n📊 SESSION KEY HARVESTING ENABLED");
        Serial.println("   Press 'k' in menu to request session keys");
        Serial.println("   Or wait for passive key announcements\n");
    } else {
        Serial.println("[WARNING] Failed to decode channel PSK for session key manager");
    }
    
    return true;
}
```

---

#### Step 3: Add Packet Processing in `processQueuedPackets()`
**File:** `firmware/src/lora_recon_tool.cpp`  
**Function:** `LoRaReconTool::processQueuedPackets()`  
**Location:** Inside main packet processing loop (~line 450)

Find this section:
```cpp
// Analyze packet using ProtocolAnalyzer
PacketInfo info = protocolAnalyzer.analyze(data, length, rssi);
```

**Add after it:**

```cpp
// NEW: Check if this is a session key announcement
if (sessionKeyManager.processKeyAnnouncement(packetBuffer, packetLength)) {
    Serial.println("[HARVEST] 🎉 Session key harvested from announcement!");
    sessionKeyManager.printStatus();
}

// NEW: Try session key decryption for large packets
if (packetLength >= 40 && strcmp(info.protocol, "Meshtastic") == 0) {
    trySessionKeyDecryption(packetBuffer, packetLength, info.nodeId, qp.timestamp);
}
```

---

#### Step 4: Add Helper Function to `lora_recon_tool.cpp`
**File:** `firmware/src/lora_recon_tool.cpp`  
**Location:** At end of file, before closing (~line 900)

```cpp
// NEW FUNCTION: Try decrypting with session key
void LoRaReconTool::trySessionKeyDecryption(const uint8_t* data, size_t length,
                                            uint32_t nodeId, uint32_t packetId) {
    // Check if we have a session key for primary channel
    const SessionKey* sessionKey = sessionKeyManager.getSessionKey(0);
    if (!sessionKey) {
        return;  // No session key yet
    }
    
    // Extract encrypted payload
    if (length < 20) return;
    
    bool hasHeader = (data[0] == 0xFF && data[1] == 0xFF && 
                      data[2] == 0xFF && data[3] == 0xFF);
    
    if (!hasHeader) return;
    
    const uint8_t* encryptedData = data + 14;
    size_t encryptedLen = length - 14;
    
    // Construct nonce (same as channel PSK decryption)
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
    
    // Decrypt with session key (256-bit)
    uint8_t decrypted[256];
    if (encryptedLen > sizeof(decrypted)) return;
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    // Session keys are 256-bit (32 bytes)
    if (mbedtls_aes_setkey_enc(&aes, sessionKey->keyBytes, 256) != 0) {
        mbedtls_aes_free(&aes);
        return;
    }
    
    uint8_t nonce_counter[16];
    uint8_t stream_block[16];
    memcpy(nonce_counter, nonce, 16);
    memset(stream_block, 0, 16);
    size_t nc_off = 0;
    
    int result = mbedtls_aes_crypt_ctr(&aes, encryptedLen, &nc_off,
                                       nonce_counter, stream_block,
                                       encryptedData, decrypted);
    mbedtls_aes_free(&aes);
    
    if (result != 0) return;
    
    // Check if this looks like a valid TEXT_MESSAGE_APP protobuf
    if (decrypted[0] == 0x08 && decrypted[1] == 0x01) {
        Serial.println("\n[SESSION] 🎯 Successfully decrypted with SESSION KEY!");
        Serial.println("[SESSION] This is a TEXT_MESSAGE_APP packet!");
        
        // Extract the text message
        if (decrypted[2] == 0x12) {
            size_t pos = 3;
            uint32_t payloadLen = 0;
            uint8_t shift = 0;
            
            // Decode payload length (varint)
            while (pos < encryptedLen) {
                uint8_t byte = decrypted[pos++];
                payloadLen |= ((uint32_t)(byte & 0x7F)) << shift;
                if ((byte & 0x80) == 0) break;
                shift += 7;
            }
            
            // Look for text field (0x0A)
            if (pos < encryptedLen && decrypted[pos] == 0x0A) {
                pos++;
                uint32_t textLen = 0;
                shift = 0;
                
                // Decode text length (varint)
                while (pos < encryptedLen) {
                    uint8_t byte = decrypted[pos++];
                    textLen |= ((uint32_t)(byte & 0x7F)) << shift;
                    if ((byte & 0x80) == 0) break;
                    shift += 7;
                }
                
                if (pos + textLen <= encryptedLen && textLen > 0 && textLen < 200) {
                    Serial.println("\n╔════════════════════════════════════════════╗");
                    Serial.print("║  📧 TEXT MESSAGE (SESSION KEY): \"");
                    
                    for (uint32_t i = 0; i < textLen; i++) {
                        char c = decrypted[pos + i];
                        if (c >= 32 && c <= 126) {
                            Serial.print(c);
                        } else if (c == '\n' || c == '\r') {
                            Serial.print(" ");  // Replace newlines with space
                        }
                    }
                    
                    Serial.println("\"");
                    Serial.println("╚════════════════════════════════════════════╝\n");
                }
            }
        }
    }
}
```

---

#### Step 5: Add Menu Commands in `handleUserInput()`
**File:** `firmware/src/lora_recon_tool.cpp`  
**Function:** `LoRaReconTool::handleUserInput()`  
**Location:** Add before the device selection code (~line 750)

```cpp
void LoRaReconTool::handleUserInput(char cmd) {
    // ... existing commands ...
    
    // NEW: Session key commands
    if (cmd == 'k') {
        // Request session key
        Serial.println("\n[SESSION] 🔑 Requesting session key from mesh...");
        
        // Use a node ID we've seen, or random
        uint32_t nodeId = 0xDEADBEEF;  // Default random
        
        if (reconState.numTargetableDevices > 0) {
            // Use ID of first discovered device
            nodeId = reconState.getTargetableDevice(0).nodeId;
            Serial.printf("[SESSION] Using node ID: 0x%08X\n", nodeId);
        } else {
            Serial.println("[SESSION] Using random node ID (no devices discovered yet)");
        }
        
        if (sessionKeyManager.requestSessionKey(0, nodeId)) {
            Serial.println("[SESSION] ✅ Request transmitted");
            Serial.println("[SESSION] 📡 Listening for response...");
            Serial.println("[SESSION] (Should arrive within 5-10 seconds)\n");
        } else {
            Serial.println("[SESSION] ❌ Request transmission failed\n");
        }
        
        return;
    }
    
    if (cmd == 'K') {
        // Show session key status
        sessionKeyManager.printStatus();
        return;
    }
    
    // ... rest of existing commands ...
}
```

---

#### Step 6: Update Menu Display
**File:** `firmware/src/user_interface.cpp` (or wherever your menu is)  
**Function:** `showReconResults()` or similar

```cpp
Serial.println("\n📋 COMMANDS:");
Serial.println("  1-9 : Target specific device");
Serial.println("  f   : Frequency targeting");
Serial.println("  k   : Request session key from mesh");  // ADD THIS
Serial.println("  K   : Show session key status");         // ADD THIS
Serial.println("  m   : Show this menu");
Serial.println("  r   : Restart reconnaissance");
// ... existing commands ...
```

---

## Compilation & Testing

### Build
```powershell
pio run -e default
```

**Expected:** Should compile cleanly. If errors, check:
- All includes added correctly
- Function declarations match
- mbedtls library available

### Upload
```powershell
pio run -e default --target upload
```

### Monitor
```powershell
pio device monitor
```

### Test Sequence

1. **Start device** - should see:
   ```
   📊 SESSION KEY HARVESTING ENABLED
      Press 'k' in menu to request session keys
      Or wait for passive key announcements
   ```

2. **Wait for recon** (~3 minutes)

3. **Press 'm'** - should see new commands in menu:
   ```
   k   : Request session key from mesh
   K   : Show session key status
   ```

4. **Press 'k'** - should see:
   ```
   [SESSION] 🔑 Requesting session key from mesh...
   [SESSION] Using node ID: 0x401ACD4E
   [SESSION] Request packet hex: FF FF FF FF ...
   [SESSION] ✅ Request transmitted
   [SESSION] 📡 Listening for response...
   ```

5. **Wait 5-10 seconds** - look for:
   ```
   [SESSION] 🔍 Found admin message, parsing for session key...
   [SESSION] 🔑 Found 32-byte session key in response!
   [HARVEST] 🎉 Session key harvested from announcement!
   ```

6. **Send test message** from Meshtastic app

7. **Look for:**
   ```
   [SESSION] 🎯 Successfully decrypted with SESSION KEY!
   ╔════════════════════════════════════════════╗
   ║  📧 TEXT MESSAGE (SESSION KEY): "test"
   ╚════════════════════════════════════════════╝
   ```

---

## Troubleshooting

### Compile Error: "mbedtls/base64.h not found"
**Solution:** Already included in your project (you use it for PSK decryption)

### Compile Error: "SessionKey undefined"
**Solution:** Check that `session_key_manager.h` is included in `lora_recon_tool.h`

### No Response to Key Request
**Options:**
1. Try again after 30 seconds (rate limiting)
2. Move closer to mesh devices
3. Try passive listening (just wait, don't press 'k')

### Key Harvested But No Text Messages
**Check:**
1. Are you actually sending text messages? (not position/telemetry)
2. Is packet size >40 bytes? (small packets are routing, not text)
3. Press 'K' to verify session key is cached

---

## Success Criteria

### You'll know it's working when:

1. ✅ Compiles without errors
2. ✅ Device boots and shows session key manager initialized
3. ✅ Can press 'k' to request key
4. ✅ See transmission confirmation
5. ✅ Receive key announcement after 5-10 seconds
6. ✅ Text messages decrypt and display with "SESSION KEY" label

---

## Estimated Integration Time

- **Code changes:** 30 minutes
- **Testing:** 15 minutes
- **Troubleshooting:** 15 minutes
- **Total:** ~1 hour

---

## What This Gives You

After integration, you'll be able to:

1. **Request session keys** actively from the mesh
2. **Harvest session keys** passively from announcements
3. **Decrypt text messages** sent on the primary channel
4. **See actual user messages** (not just routing packets)
5. **Monitor session key rotation** and cache multiple keys

This solves your main problem: **Why am I not seeing text messages?**

Answer: Because they use session keys, not channel PSKs. Now you can get those session keys!

---

**Status:** Ready to integrate ✅  
**Difficulty:** Medium (requires careful editing)  
**Impact:** HIGH - This is the solution to your text message problem!

**Go for it!** 🚀
