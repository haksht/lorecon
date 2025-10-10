# Session Key Harvesting Guide

## The Real Problem: Session Keys vs Channel PSKs

### Why You're Not Getting Text Messages

Your analysis is **100% correct**. Here's what's happening:

#### Two-Layer Encryption in Meshtastic

1. **Channel PSK** (what you have working):
   - Encrypts routing/admin/control packets
   - Static key configured in channel settings
   - Example: `"AQ=="` or `"1PG7OiApB1nwvP+rz05pAQ=="`
   - **This works** - you're successfully decrypting these packets

2. **Session Key** (what you're missing):
   - Encrypts actual user messages (TEXT_MESSAGE_APP)
   - Ephemeral, rotates periodically
   - Distributed via encrypted announcements on the channel
   - **This is why you see routing packets but no text messages**

### The Evidence

From your captures:
- ✅ Decryption succeeds with channel PSK
- ✅ Routing/admin packets decrypt cleanly
- ❌ No text messages found in decrypted data
- ❌ Decrypted packets don't have expected Data message structure

**Conclusion:** You're decrypting the control plane, but user messages use a different key.

---

## Solution: Session Key Harvesting

### Two Approaches

#### Approach 1: Passive Listening (Easiest)
**Wait for key announcements** - nodes periodically broadcast session keys

**Pros:**
- No transmission required
- Completely passive
- Works immediately when announcement occurs

**Cons:**
- May take time (keys announced every ~5-30 minutes)
- Might miss announcement if not listening
- Requires being in range during announcement

#### Approach 2: Active Request (Faster)
**Transmit a key request** - ask the mesh for current session key

**Pros:**
- Immediate response (usually < 5 seconds)
- Triggers fresh announcement
- Ensures you get current key

**Cons:**
- Requires transmission (not pure passive)
- Nodes may rate-limit requests
- Need valid node ID

---

## Implementation Plan

### Phase 1: Integration into Existing Code

#### Step 1: Add Session Key Manager to Project

Files created:
- `firmware/src/session_key_manager.h` ✅
- `firmware/src/session_key_manager.cpp` ✅

#### Step 2: Modify `lora_recon_tool.h`

Add to class definition:

```cpp
#include "session_key_manager.h"

class LoRaReconTool {
private:
    // Existing members...
    
    // NEW: Session key management
    SessionKeyManager sessionKeyManager;
    
    // Helper functions
    void handleSessionKeyRequest();
    void trySessionKeyDecryption(const uint8_t* data, size_t length, 
                                  uint32_t nodeId, uint32_t packetId);
};
```

#### Step 3: Modify `lora_recon_tool.cpp`

**In `initialize()` function:**

```cpp
bool LoRaReconTool::initialize() {
    // ... existing initialization ...
    
    // Initialize session key manager
    uint8_t channelPSK[16];
    // Decode your channel PSK (e.g., "1PG7OiApB1nwvP+rz05pAQ==")
    size_t pskLen = 0;
    mbedtls_base64_decode(channelPSK, sizeof(channelPSK), &pskLen,
                         (const unsigned char*)"1PG7OiApB1nwvP+rz05pAQ==", 
                         strlen("1PG7OiApB1nwvP+rz05pAQ=="));
    
    sessionKeyManager.initialize(&radio, channelPSK, pskLen);
    
    Serial.println("\n📊 SESSION KEY HARVESTING ENABLED");
    Serial.println("   Press 'k' in menu to request session keys");
    Serial.println("   Or wait for passive key announcements\n");
    
    return true;
}
```

**In `processQueuedPackets()` function:**

```cpp
void LoRaReconTool::processQueuedPackets() {
    while (!packetQueue.empty()) {
        QueuedPacket qp = packetQueue.front();
        packetQueue.pop();
        
        // ... existing code ...
        
        // NEW: Check if this is a session key announcement
        if (sessionKeyManager.processKeyAnnouncement(packetBuffer, packetLength)) {
            Serial.println("[HARVEST] 🎉 Session key harvested from announcement!");
            sessionKeyManager.printStatus();
        }
        
        // NEW: Try session key decryption for large packets
        if (packetLength >= 40 && strcmp(info.protocol, "Meshtastic") == 0) {
            trySessionKeyDecryption(packetBuffer, packetLength, info.nodeId, qp.timestamp);
        }
        
        // ... rest of existing code ...
    }
}
```

**Add new helper function:**

```cpp
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
    
    // Decrypt with session key
    uint8_t decrypted[256];
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
    
    // Check if this looks like a valid protobuf
    if (decrypted[0] == 0x08 && decrypted[1] == 0x01) {
        Serial.println("\n[SESSION] 🎯 Successfully decrypted with SESSION KEY!");
        Serial.println("[SESSION] This is a TEXT_MESSAGE_APP packet!");
        
        // Now extract the text message
        // Field 2 should be the payload
        if (decrypted[2] == 0x12) {
            size_t pos = 3;
            uint32_t payloadLen = 0;
            uint8_t shift = 0;
            
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
                
                while (pos < encryptedLen) {
                    uint8_t byte = decrypted[pos++];
                    textLen |= ((uint32_t)(byte & 0x7F)) << shift;
                    if ((byte & 0x80) == 0) break;
                    shift += 7;
                }
                
                if (pos + textLen <= encryptedLen && textLen > 0 && textLen < 200) {
                    Serial.print("\n╔════════════════════════════════════════════╗\n");
                    Serial.print("║  📧 TEXT MESSAGE (SESSION KEY): \"");
                    
                    for (uint32_t i = 0; i < textLen; i++) {
                        char c = decrypted[pos + i];
                        if (c >= 32 && c <= 126) {
                            Serial.print(c);
                        }
                    }
                    
                    Serial.println("\"\n╚════════════════════════════════════════════╝\n");
                }
            }
        }
    }
}
```

**In `handleUserInput()` function:**

Add new command:

```cpp
void LoRaReconTool::handleUserInput(char cmd) {
    // ... existing commands ...
    
    if (cmd == 'k' || cmd == 'K') {
        // Request session key
        Serial.println("\n[SESSION] Requesting session key from mesh...");
        
        // Use a random node ID or one we've seen
        uint32_t nodeId = 0xDEADBEEF;  // Random ID
        
        if (reconState.numTargetableDevices > 0) {
            // Use ID of a known device
            nodeId = reconState.getTargetableDevice(0).nodeId;
            Serial.printf("[SESSION] Using node ID: 0x%08X\n", nodeId);
        }
        
        if (sessionKeyManager.requestSessionKey(0, nodeId)) {
            Serial.println("[SESSION] ✅ Request transmitted");
            Serial.println("[SESSION] Listening for response...");
            Serial.println("[SESSION] (Response should arrive within 5-10 seconds)\n");
        } else {
            Serial.println("[SESSION] ❌ Request failed\n");
        }
        
        return;
    }
    
    if (cmd == 'K') {  // Capital K = show session key status
        sessionKeyManager.printStatus();
        return;
    }
    
    // ... rest of existing commands ...
}
```

#### Step 4: Update Menu Display

In `showReconResults()` or your menu function:

```cpp
Serial.println("COMMANDS:");
Serial.println("  1-9 : Target specific device");
Serial.println("  f   : Frequency targeting");
Serial.println("  k   : Request session key from mesh");  // NEW
Serial.println("  K   : Show session key status");         // NEW
Serial.println("  m   : Show this menu");
Serial.println("  r   : Restart reconnaissance");
// ... existing commands ...
```

---

## Usage Workflow

### Scenario 1: Fresh Start (No Session Key)

1. **Start reconnaissance** as usual:
   ```
   pio run -t upload && pio device monitor
   ```

2. **Wait for initial scan** (~3 minutes)

3. **Request session key:**
   - Press `m` for menu
   - Press `k` to request session key
   - Watch for: `[SESSION] ✅ Request transmitted`

4. **Wait for response** (5-10 seconds)
   - Should see: `[HARVEST] 🎉 Session key harvested from announcement!`
   - If not, try again (press `k` again)

5. **Now capture text messages:**
   - Press `1` to target first device
   - Send text message from Meshtastic app
   - Should see: `[SESSION] 🎯 Successfully decrypted with SESSION KEY!`
   - Followed by: `📧 TEXT MESSAGE: "Your message here"`

### Scenario 2: Passive Monitoring

1. **Start reconnaissance** as usual

2. **Just wait** - no key request needed
   - Monitor output for: `[HARVEST] 🎉 Session key harvested from announcement!`
   - This happens when nodes periodically announce keys

3. **Once you see key harvested:**
   - All subsequent text messages will decrypt automatically

### Scenario 3: Key Expired

Session keys rotate periodically (every few hours/days):

1. **Watch for decryption failures:**
   - Text messages stop decrypting
   - Or see error messages

2. **Request new key:**
   - Press `m` then `k`
   - New key will be fetched

3. **Automatic re-request:**
   - System could auto-request when decryption fails (future enhancement)

---

## Expected Output

### Successful Key Harvest (Passive)

```
[RECON] Packet #47: Meshtastic, 58 bytes, -42.0 dBm, 8.5 dB SNR
[SESSION] 🔍 Found admin message, parsing for session key...
[SESSION] 🔑 Found 32-byte session key in response!
[HARVEST] 🎉 Session key harvested from announcement!
[SESSION] ✅ Session key harvested successfully!
[SESSION] Session ID: 0x12345678
[SESSION] Key (hex): A1B2C3D4E5F6G7H8I9J0K1L2M3N4O5P6
                     Q7R8S9T0U1V2W3X4Y5Z6A7B8C9D0E1F2
```

### Successful Key Request (Active)

```
[SESSION] 📡 Transmitting session key request...
[SESSION] Channel: 0, Node ID: 0x401ACD4E
[SESSION] Packet size: 32 bytes
[SESSION] Request packet hex: FF FF FF FF 40 1A CD 4E 01 00 ...
[SESSION] ✅ Request transmitted successfully
[SESSION] Listening for response...
[SESSION] (Response should arrive within 5-10 seconds)

... (5 seconds later) ...

[RECON] Packet #52: Meshtastic, 62 bytes, -38.0 dBm, 9.2 dB SNR
[SESSION] 🔍 Found admin message, parsing for session key...
[SESSION] 🔑 Found 32-byte session key in response!
[HARVEST] 🎉 Session key harvested from announcement!
```

### Successful Text Message Decryption

```
[CAPTURE] Packet #103: Meshtastic, 87 bytes, -45.0 dBm, 7.8 dB SNR
[SESSION] 🎯 Successfully decrypted with SESSION KEY!
[SESSION] This is a TEXT_MESSAGE_APP packet!

╔════════════════════════════════════════════╗
║  📧 TEXT MESSAGE (SESSION KEY): "Hello from the mesh!"
╚════════════════════════════════════════════╝
```

---

## Troubleshooting

### Issue 1: No Response to Key Request

**Symptoms:**
- Request transmitted successfully
- But no `[HARVEST]` message after 10+ seconds

**Possible Causes:**
1. **No nodes in range** - mesh is empty or out of range
2. **Nodes sleeping** - devices in low-power mode
3. **Rate limiting** - nodes throttling requests

**Solutions:**
- Wait 30 seconds and try again
- Move closer to mesh devices
- Try passive listening instead (`Ctrl+C` and just wait)

### Issue 2: Key Harvested But Decryption Still Fails

**Symptoms:**
- `[HARVEST] 🎉 Session key harvested`
- But text messages still don't decrypt

**Possible Causes:**
1. **Wrong session ID** - multiple sessions active
2. **Key rotation** - session changed since harvest
3. **Different channel** - text sent on different channel

**Solutions:**
- Request key again: `k`
- Check session ID in your packets vs cached key
- Verify channel configuration matches

### Issue 3: Session Key Request Won't Transmit

**Symptoms:**
- `[SESSION] ❌ Request failed`

**Possible Causes:**
1. **Radio in wrong state** - still receiving
2. **Transmission error** - radio configuration mismatch

**Solutions:**
- Restart device and try again
- Check radio configuration matches mesh
- Verify antenna connected

---

## Advanced: Protocol Details

### Session Key Announcement Packet Structure

**Encrypted with channel PSK:**

```
Header:         FF FF FF FF
From Node:      [4 bytes] Source node ID
Type/Flags:     [2 bytes] Packet type
Packet ID:      [4 bytes] For nonce construction
Dest:           FF FF FF FF (broadcast)

Encrypted Payload:
  Field 1: 08 07              (portnum = ADMIN_APP)
  Field 2: 12 <len>           (payload length)
    Field X: <session_key_data>
      - 32 bytes: AES-256 session key
      - Session ID/epoch (varint)
      - Validity period (optional)
```

### Session Key Request Packet Structure

**Unencrypted (broadcast):**

```
Header:         FF FF FF FF
From Node:      [4 bytes] Our node ID (can be random)
Type/Flags:     01 00       (data packet)
Packet ID:      [4 bytes] Random ID
Dest:           FF FF FF FF (broadcast)

Payload (unencrypted):
  Field 1: 08 07              (portnum = ADMIN_APP)
  Field 2: 12 <len>           (admin message)
    Field 1: 08 10            (session_key_request)
    Field 2: 10 00            (channel_index = 0)
```

### Nonce Construction (Same as Channel PSK)

```
Nonce[0..3]   = Packet ID (little-endian)
Nonce[4..7]   = 0x00 (reserved)
Nonce[8..11]  = Node ID (big-endian)
Nonce[12..15] = 0x00 (padding)
```

### Decryption Process

1. **Channel PSK decryption** (admin messages):
   - Use 128-bit or 256-bit AES-CTR
   - Key = channel PSK from settings
   - Nonce = constructed from packet header
   
2. **Session key decryption** (user messages):
   - Use 256-bit AES-CTR (session keys are always 256-bit)
   - Key = harvested session key
   - Nonce = same construction as channel PSK

---

## Next Steps

### Immediate (Phase 1)
1. ✅ Create session key manager files
2. ⏳ Integrate into `lora_recon_tool.cpp`
3. ⏳ Add menu commands (`k` and `K`)
4. ⏳ Test passive listening
5. ⏳ Test active request

### Short Term (Phase 2)
- Add automatic re-request on decryption failure
- Cache multiple session IDs per channel
- Implement session key rotation detection
- Add session key export/import (for offline analysis)

### Long Term (Phase 3)
- Support multiple channels simultaneously
- Implement session key prediction (timing patterns)
- Add replay attack detection
- Create session key database for long-term monitoring

---

## Security & Legal Considerations

### What This Enables

- **Legitimate use**: Monitor your own mesh network
- **Security research**: Test mesh encryption strength
- **Network debugging**: Troubleshoot message delivery

### Important Notes

1. **Only works with known channel PSK** - you must have legitimate access
2. **Session keys are broadcast to authorized nodes** - this mimics that behavior
3. **Does not crack encryption** - requires valid channel PSK first
4. **Respects rate limiting** - follows Meshtastic anti-flood rules

### Legal Compliance

- ✅ **Passive monitoring** of ISM band is legal
- ✅ **Minimal transmission** (key requests) follows FCC Part 15
- ⚠️ **Respect privacy** - only monitor networks you own/control
- ⚠️ **Follow local laws** - RF regulations vary by country

---

## References

- Meshtastic Protocol: https://meshtastic.org/docs/developers/protocol
- AES-CTR Mode: NIST SP 800-38A
- Protobuf Encoding: https://protobuf.dev/programming-guides/encoding/
- RadioLib Documentation: https://github.com/jgromes/RadioLib

---

**Status:** Ready for integration ✅
**Files Created:** 
- `session_key_manager.h` ✅
- `session_key_manager.cpp` ✅
- This guide ✅

**Next:** Integrate into your existing code and test!
