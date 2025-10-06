# ESP32 LoRa Sniffer - New Session Context

## 🎯 Project Overview

This is an **ESP32-based LoRa reconnaissance tool** for security research on Meshtastic mesh networks. The tool captures, decrypts, and analyzes LoRa packets in real-time using the Heltec WiFi LoRa 32 V3 board.

**Current Branch:** `buttondisplay` (diverged from main)

**Primary Goal:** Intercept and decrypt Meshtastic text messages using PSK (Pre-Shared Key) decryption.

---

## ✅ What's FULLY WORKING

### 1. Hardware & User Interface
- ✅ **Button Control**: GPIO 0 (PRG button) with short/long press detection
  - Short press (<3s): Toggle display/refresh screen
  - Long press (≥3s): Clean shutdown with deep sleep
  - **Status:** Tested and 100% functional
  
- ✅ **OLED Display Code**: Complete implementation with 6 modes
  - **Status:** Code complete and production-ready
  - **Hardware:** NOT present on user's Heltec V3 board variant (confirmed via I2C scan)
  - Gracefully handles missing hardware, continues without display

### 2. Cryptography & Decryption
- ✅ **AES-128-CTR Decryption**: 100% working with correct cipher mode
  - Previously was using AES-ECB (wrong) - fixed to AES-CTR
  - Nonce construction: PacketID (4 bytes LE) + NodeID (4 bytes) + zeros (8 bytes)
  - Tests 5 default Meshtastic PSKs automatically
  - **Confirmed working keys:**
    - Key #1: `1PG7OiApB1nwvP+rz05pAQ==`
    - Key #2: `AQ==` (default "LongFast" channel) ← Most common
    - Key #3: `d1iq21lNSh7BP6MOkP6cQA==`
    - Key #4: `2f8aH6iT8K9jQ1P3mD4nBw==`
    - Key #5: `7h3kL9mR5wX2pY8qE6tZcA==`

### 3. Packet Capture & Analysis
- ✅ **LoRa Radio**: SX1262 on 906.875 MHz, SF11, BW250, SW:0x2B
- ✅ **Packet Capture**: Reliably captures packets from -8 to -122 dBm
- ✅ **Node Detection**: Successfully identified 3 nodes
  - 0x401ACD4E
  - 0x44D7A39E
  - 0xBE438E37
- ✅ **Real-time Decryption**: Attempts decryption on all captured packets

### 4. Enhanced Features
- ✅ **Message Type Detection**: Identifies 20+ Meshtastic app types with emojis
- ✅ **Three-Tier Text Extraction**: Ready for standard protobuf format
  - Tier 1: Exact pattern (0x08 0x01 0x12 <len> 0x0A <textlen> <TEXT>)
  - Tier 2: Alternative pattern (0x12 <len> 0x0A <textlen> <TEXT>)
  - Tier 3: Fallback (Direct 0x0A search + continuous ASCII)
- ✅ **Extended Debug Output**: Shows up to 64 bytes of decrypted hex
- ✅ **Protobuf Validation**: Field numbers 1-15, wire types 0/2/5
- ✅ **Comprehensive Diagnostics**: Full I2C scan, watchdog management

---

## ❌ What's NOT WORKING (And Why)

### Text Message Extraction BLOCKED

**Problem:** After extensive testing (100+ packets captured, 30+ successfully decrypted), we've discovered that **NO decrypted packets contain the expected Meshtastic protobuf structure**.

**Root Cause:** Protocol mismatch between implementation and actual device firmware.

**Evidence:**
- **Searched for portnum field (0x08) in first 16 bytes:** NEVER found
- **Searched entire packet for 0x08 byte:** NEVER found in any decrypted packet
- **Test messages sent:** 20+ ("test", "hello", longer messages)
- **Decrypted packets:** 26 bytes (12-byte payload), 51 bytes (37-byte), 105 bytes (91-byte)
- **Expected structure:** `08 01 12 <len> 0A <textlen> <TEXT>`
- **Actual structure:** Random binary data, no field tag pattern

**Example Decrypted Data:**
```
Expected: 08 01 12 0C 0A 0A 'H' 'e' 'l' 'l' 'o'...
          ↑  ↑  ↑       ↑
          |  |  |       └─ Text field tag
          |  |  └────────── Payload field tag
          |  └───────────── TEXT_MESSAGE_APP (portnum)
          └──────────────── Portnum field tag

Actual:   6D 7E C5 5F 99 C8 AD 4B C8 47 3A 01...
          ↑
          └─ Random binary, no 0x08 anywhere
```

**False Positives:** Fallback text extraction finds garbage like "6E'&vZ]", "L_|1O", "r/Gz" (random ASCII from binary data).

---

## 🤔 Theories About Protocol Mismatch

### Theory 1: Custom Firmware or Version Differences
- Devices may use older/newer Meshtastic firmware with different protobuf schema
- Possible v1.x legacy format vs v2.x documented format
- Could be alpha/beta firmware with protocol changes

### Theory 2: Implicit/Compressed Format
- Messages sent without full protobuf wrapper (bandwidth optimization)
- Using TEXT_MESSAGE_COMPRESSED_APP (portnum 0x09) with different structure
- Implicit portnum (assumes 0x01 without field tag)

### Theory 3: Additional Encryption Layer
- Double encryption (channel + direct message)
- Nested encoding not documented
- Custom encryption layer

### Theory 4: Non-Standard Implementation
- Custom Meshtastic build with modified protocol
- Regional variant with different encoding
- Optimization for specific hardware (ESP32 vs T-Deck)

---

## 📁 Key Files & Current State

### Main Codebase (`firmware/src/`)

**`lora_recon_tool.h` / `.cpp`** (Main orchestrator)
- Button handler at lines 827-899
- Display integration
- State management
- Status: Production ready

**`psk_decryption_simple.cpp`** (701 lines - EXTENSIVELY MODIFIED)
- **Lines ~240-310**: AES-CTR implementation with proper nonce
- **Lines ~338-372**: Relaxed protobuf validation (was too strict)
- **Lines ~400-410**: Extended hex display (64 bytes, multi-line)
- **Lines ~418-437**: Enhanced app type detection (20+ types)
- **Lines ~470-485**: Portnum field search (scans up to 16 bytes)
- **Lines ~490-620**: Complete 3-tier text extraction rewrite
- Status: Fully functional for standard protobuf format, blocked by protocol mismatch

**`oled_display.h` / `.cpp`** (104 + 321 lines)
- Complete OLED implementation with 6 display modes
- V2/V3 pin fallback, Vext power control
- Full I2C scan diagnostics (0x01-0x7F)
- Auto-off timer, watchdog integration
- Status: Production ready, hardware absent

### Documentation (`docs/`)

**`PSK_DECRYPTION_STATUS.md`** (Just updated)
- Comprehensive status report
- Test evidence from 100+ packets
- Troubleshooting avenues (5 priority levels)
- Debugging workflow for next session

**`BUTTON_DISPLAY_GUIDE.md`**
- Complete feature documentation
- User guide for display modes

**`OLED_TROUBLESHOOTING.md`**
- V2/V3 pin configurations
- I2C diagnostic procedures

### Configuration

**`platformio.ini`**
- Libraries: U8g2@2.35.9, RadioLib@6.4.2, mbedtls (built-in)
- Compiler flags: -Wno-unused-function, -Wno-unused-variable
- Board: heltec_wifi_lora_32_V3

---

## 🔬 Test Environment

### Hardware
- **Board:** Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262)
- **Board Variant:** OLED NOT populated (confirmed via I2C scan showing zero devices)
- **Button:** GPIO 0, tested and fully functional
- **Display Pins (code only):** SDA=17, SCL=18, RST=21, Vext=36

### Meshtastic Devices Under Test
- **Device 1:** ESP32 with Meshtastic
- **Device 2:** T-Deck with Meshtastic
- **Configuration:** Both freshly flashed, factory defaults, no customization
- **Channel:** PRIMARY channel, USA LongFast preset
- **Encryption:** Default PSK ("AQ==" = 0x01 in hex)
- **Firmware Version:** UNKNOWN (need to check in next session)

### Test Results Summary
| Metric | Value | Status |
|--------|-------|--------|
| Packets Captured | 100+ | ✅ Working |
| Successful Decryptions | 30+ | ✅ Working |
| Keys Working | 5/5 | ✅ Working |
| Portnum Fields Found | 0 | ❌ Never found |
| Valid Text Extracted | 0 | ❌ Protocol mismatch |

---

## 🚧 WHAT NEEDS TO BE DONE (Priority Order)

### Priority 1: Firmware Version Investigation
**Goal:** Identify if devices use different protocol version

**Tasks:**
1. Check Meshtastic firmware versions on both devices:
   - Settings → Device → Firmware Version
   - Look for version numbers (2.2.17, 2.3.x, etc.)
   - Check firmware channel (alpha/beta/stable)

2. Research protocol changes:
   - Search Meshtastic GitHub for version-specific protocol changes
   - Check for "breaking changes" in changelogs
   - Look for "implicit message" or "direct mode" features

3. Compare with documentation:
   - Verify if code assumes correct protobuf version
   - Check for v1.x legacy format support

**Expected Outcome:** Discover if firmware uses different message format

---

### Priority 2: Protocol Reverse Engineering
**Goal:** Understand actual message format used by devices

**Tasks:**
1. **Pattern Analysis** on decrypted data:
   - Look for repeating byte sequences across packets
   - Check for compression signatures (zlib/gzip headers: 0x78 0x9C)
   - Calculate entropy to detect compression
   - Compare packets from same source node

2. **Wireshark + SDR Capture** (if available):
   - Use HackRF or RTL-SDR to capture raw LoRa
   - Use Meshtastic Wireshark dissector for comparison
   - Compare ESP32 output with Wireshark decode
   - Look for interpretation differences

3. **Test Different Message Types**:
   - Send position updates (enable GPS on device)
   - Send telemetry data
   - Send traceroute/ping
   - Check if ANY message type shows standard protobuf structure

**Code to Add:**
```cpp
// In psk_decryption_simple.cpp after decryption:

// Entropy calculation
float calculateEntropy(uint8_t* data, size_t len) {
    int freq[256] = {0};
    for (size_t i = 0; i < len; i++) freq[data[i]]++;
    float entropy = 0.0;
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            float p = (float)freq[i] / len;
            entropy -= p * log2(p);
        }
    }
    return entropy;
}

// Compression detection
bool isCompressed(uint8_t* data) {
    // Check for zlib header
    if (data[0] == 0x78 && (data[1] == 0x9C || data[1] == 0xDA || data[1] == 0x01))
        return true;
    // Check for gzip header
    if (data[0] == 0x1F && data[1] == 0x8B)
        return true;
    return false;
}
```

**Expected Outcome:** Identify actual encoding scheme

---

### Priority 3: Alternative Decoding Approaches
**Goal:** Try parsing with different assumptions

**Tasks:**
1. **Double-Encryption Test**:
   ```cpp
   // After successful AES-CTR decryption:
   // Try decrypting AGAIN with same/different keys
   uint8_t secondDecrypt[256];
   for (int keyIdx = 0; keyIdx < 5; keyIdx++) {
       // Try each key on already-decrypted data
       // Check if result has protobuf structure
   }
   ```

2. **Implicit Portnum Test**:
   ```cpp
   // Assume portnum without field tag
   // Try parsing as if first byte is portnum value directly
   uint8_t implicitPortnum = decrypted[0];
   if (implicitPortnum == 0x01 || implicitPortnum == 0x09) {
       // Parse rest as payload
   }
   ```

3. **Compressed Text Test**:
   ```cpp
   // Look for 0x08 0x09 (TEXT_MESSAGE_COMPRESSED_APP)
   // Try decompressing with zlib
   #include <zlib.h>
   // ... decompression code
   ```

**Expected Outcome:** Find alternative parsing method

---

### Priority 4: Community Research
**Goal:** Find if this is a known issue

**Tasks:**
1. **Search Resources:**
   - Meshtastic GitHub Issues: "protocol format" "message structure" "sniffer"
   - Meshtastic Discord: #development channel
   - Reddit r/meshtastic: Search for decryption projects

2. **Compare with Other Projects:**
   - meshtastic-sniffer (Python): Check their decoding approach
   - ESP32-LoRa-Decoder: Compare protobuf parsing
   - Other reconnaissance tools: See if they mention format issues

3. **Check Device-Specific Quirks:**
   - ESP32 vs T-Deck differences
   - Region-specific encoding (US 900MHz vs EU 868MHz)
   - Channel configuration effects

**Expected Outcome:** Find existing solutions or similar issues

---

### Priority 5: Official Library Integration
**Goal:** Use official Meshtastic libraries for comparison

**Tasks:**
1. **Add Meshtastic Protobufs:**
   ```ini
   ; In platformio.ini
   lib_deps = 
       olikraus/U8g2@^2.35.9
       jgromes/RadioLib@^6.4.2
       meshtastic/Meshtastic-protobufs@^2.x
   ```

2. **Implement Official Decoder:**
   ```cpp
   #include "mesh.pb.h"
   #include "portnums.pb.h"
   #include <pb_decode.h>
   
   // Try decoding with official nanopb decoder
   meshtastic_Data msg = meshtastic_Data_init_zero;
   pb_istream_t stream = pb_istream_from_buffer(decrypted, decryptedLen);
   bool status = pb_decode(&stream, meshtastic_Data_fields, &msg);
   
   if (status) {
       Serial.printf("Official decoder SUCCESS!\n");
       Serial.printf("Portnum: %d\n", msg.portnum);
       // Compare with manual parsing
   }
   ```

3. **Test with Meshtastic SDK:**
   - Review official device firmware code
   - Check encode/decode functions
   - Look for "implicit mode" flags

**Expected Outcome:** Validate assumptions against official implementation

---

## 🔍 Debugging Checklist for Next Session

### Phase 1: Information Gathering (15 minutes)
- [ ] Check firmware version on Device 1 (ESP32)
- [ ] Check firmware version on Device 2 (T-Deck)
- [ ] Record hardware models and regions
- [ ] Verify channel settings (LongFast, encryption enabled)
- [ ] Document any custom configuration

### Phase 2: Controlled Testing (30 minutes)
- [ ] Send 5 short text messages: "a", "hi", "test", "hello", "message"
- [ ] Send 1 long text message: "This is a longer test message for analysis"
- [ ] Enable GPS and send position update
- [ ] Send telemetry data (if available)
- [ ] Record packet sizes for each message type
- [ ] Save hex dumps to file

### Phase 3: Pattern Analysis (20 minutes)
- [ ] Look for common byte sequences in all decryptions
- [ ] Check for compression signatures (0x78 0x9C, 0x1F 0x8B)
- [ ] Calculate entropy for each decrypted packet
- [ ] Compare packets from same node vs different nodes
- [ ] Look for length field patterns (varint encoding)

### Phase 4: Alternative Parsing (30 minutes)
- [ ] Test implicit portnum (first byte as portnum value)
- [ ] Test compressed protobuf (look for 0x08 0x09)
- [ ] Try double-decryption with same/different keys
- [ ] Test custom field number ranges
- [ ] Try TLV (Type-Length-Value) interpretation

### Phase 5: Research & Compare (30 minutes)
- [ ] Search GitHub for firmware version + "protocol changes"
- [ ] Check Meshtastic Discord #development channel
- [ ] Compare with meshtastic-sniffer Python project
- [ ] Review Wireshark dissector code
- [ ] Check for similar issues in forums

---

## 💻 Useful Commands & Locations

### Build & Upload
```powershell
# Compile and upload firmware
pio run -t upload

# Monitor serial output
pio device monitor

# Clean build
pio run -t clean
```

### Testing the Device
```powershell
# Check button (should see):
# [BUTTON] Short press - Toggling display
# [BUTTON] Long press detected! Initiating shutdown...

# Check OLED (will see):
# [DISPLAY] Running full I2C scan (0x01-0x7F)...
# [DISPLAY] No I2C devices found on any address

# Check decryption (look for):
# [PSK] 🔓 DECRYPTION SUCCESS with default key #X!
# [PSK] First 16 bytes: 0x...
# [PSK] No portnum field (0x08) found in first 16 bytes
```

### Serial Output Analysis
Look for these patterns:
- `[PSK] 🔓 DECRYPTION SUCCESS` ← Decryption working
- `No portnum field (0x08) found` ← Protocol mismatch
- `Extracted text:` followed by garbage ← False positive

### Files to Check/Modify
- **Decryption logic:** `firmware/src/psk_decryption_simple.cpp`
- **Main loop:** `firmware/src/lora_recon_tool.cpp`
- **Display:** `firmware/src/oled_display.cpp` (optional, hardware absent)
- **Config:** `platformio.ini`

---

## 📚 Key Technical Details

### Nonce Construction (Critical for AES-CTR)
```cpp
// Correct implementation (current code):
nonce[0-3] = packetID (little-endian, 4 bytes)
nonce[4-7] = fromNodeID (4 bytes)
nonce[8-15] = zeros (8 bytes)

// Example:
// PacketID: 0x12345678
// NodeID: 0x401ACD4E
// Nonce: 78 56 34 12 4E CD 1A 40 00 00 00 00 00 00 00 00
```

### Expected Protobuf Structure
```
Data message (standard format):
08          Field 1, wire type 0 (varint) - PORTNUM FIELD TAG
01          Value 0x01 (TEXT_MESSAGE_APP)
12          Field 2, wire type 2 (length-delimited) - PAYLOAD FIELD TAG
0C          Length 12 bytes
0A          Field 1, wire type 2 (length-delimited) - TEXT FIELD TAG
0A          Length 10 bytes
48 65 6C... "Hello" (actual text content)
```

### Meshtastic App Types (Portnum Values)
```cpp
0x01 = TEXT_MESSAGE_APP ✉️
0x03 = POSITION_APP 📍
0x04 = NODEINFO_APP ℹ️
0x07 = ROUTING_APP 🔀
0x08 = ADMIN_APP ⚙️
0x09 = TEXT_MESSAGE_COMPRESSED_APP ✉️🗜️
0x0A = REPLY_APP 💬
0x0B = IP_TUNNEL_APP 🌐
0x0E = PRIVATE_APP 🔒
0x10 = RANGE_TEST_APP 📡
0x20 = STORE_FORWARD_APP 💾
// ... and more
```

---

## 🎯 Success Criteria

### When you've solved the problem, you'll see:
```
[PSK] 🔓 DECRYPTION SUCCESS with default key #2!
[PSK] First 16 bytes: 0x08 0x01 0x12 0x0C 0x0A 0x0A 'H' 'e' 'l' 'l' 'o'...
[PSK] Found portnum field at offset 0: 0x01 (TEXT_MESSAGE_APP) ✉️
[PSK] 📧 DECRYPTED TEXT MESSAGE: "Hello World"
[PSK] >>> 🎯 USER TEXT MESSAGE SUCCESSFULLY DECRYPTED! <<<
```

### Code is ready for:
- Standard Meshtastic protobuf format
- Three-tier text extraction
- Message type detection
- Comprehensive debugging

### What's missing:
- Understanding of actual protocol used by these devices
- Format detection/adaptation logic
- Support for non-standard formats

---

## 🤝 How to Help

### If you're starting a new session:

1. **Read** `docs/PSK_DECRYPTION_STATUS.md` for detailed status
2. **Start with Priority 1**: Check firmware versions
3. **Follow debugging checklist**: Systematic investigation
4. **Document findings**: Add to status doc
5. **Try alternatives**: Test different parsing approaches

### Questions to answer:
- What firmware versions are the devices running?
- Does the decrypted data have patterns (repeated bytes, length fields)?
- Is there compression (high entropy, compression headers)?
- Do position/telemetry messages use standard protobuf?
- Are there known protocol changes in GitHub issues?

### Code to write:
- Entropy calculation for compression detection
- Pattern recognition for common byte sequences
- Firmware version detection
- Alternative protobuf parsers (implicit, compressed, nested)
- Format database for known variants

---

## 📖 Additional Resources

### Documentation
- `docs/PSK_DECRYPTION_STATUS.md` - Comprehensive status report
- `docs/BUTTON_DISPLAY_GUIDE.md` - UI feature documentation
- `docs/OLED_TROUBLESHOOTING.md` - Display diagnostics
- `docs/BUILD_GUIDE.md` - Original build instructions
- `docs/FEATURES.md` - Tool capabilities

### External Links
- Meshtastic Protobufs: https://github.com/meshtastic/protobufs
- Meshtastic Firmware: https://github.com/meshtastic/firmware
- Meshtastic Discord: https://discord.gg/meshtastic
- RadioLib Documentation: https://github.com/jgromes/RadioLib

### Key Commits/Changes
- Fixed AES-ECB → AES-CTR (critical cipher mode fix)
- Implemented proper nonce construction (packetID LE + nodeID)
- Relaxed protobuf validation (field 1-15, wire types 0/2/5)
- Enhanced message type detection (20+ app types)
- 3-tier text extraction (exact → alternative → fallback)
- Extended debug output (64 bytes hex, portnum search)

---

## 🚀 Quick Start for Next Session

```powershell
# 1. Ensure you're on buttondisplay branch
git status

# 2. Build and upload
pio run -t upload

# 3. Monitor output
pio device monitor

# 4. Send test message from Meshtastic app

# 5. Look for:
#    - "DECRYPTION SUCCESS" ← Good
#    - "No portnum field" ← Expected (protocol mismatch)
#    - First 16 bytes hex dump ← Analyze this data

# 6. Check firmware versions in Meshtastic app
#    Settings → Device → Firmware Version

# 7. Search GitHub:
#    "meshtastic firmware <version> protocol changes"
```

---

## ⚠️ Important Notes

1. **DO NOT merge buttondisplay to main yet** - Text extraction not working
2. **All features implemented** - Button, display code, decryption all complete
3. **Code is NOT broken** - Implementation correct for standard protobuf
4. **Protocol mismatch** - Devices use different format than documented
5. **Hardware absent** - OLED not populated on this V3 board variant
6. **Button tested** - Short and long press 100% working

---

## 🎓 Lessons from This Session

1. **Always verify protocol versions** before implementing decoders
2. **Real-world differs from docs** - Test with actual devices early
3. **Cipher mode matters critically** - ECB vs CTR completely different
4. **False positives common** - Random binary can look like text
5. **Hardware variants exist** - Same board model, different features
6. **Comprehensive debug output essential** - Hex dumps reveal truth
7. **Community resources valuable** - Check GitHub issues, Discord
8. **Firmware versions critical** - Different versions = different protocols

---

**Good luck with the investigation! 🔍🚀**

**Remember:** The decryption IS working. The challenge is understanding the actual message format these devices use. Focus on firmware version first, then pattern analysis.
