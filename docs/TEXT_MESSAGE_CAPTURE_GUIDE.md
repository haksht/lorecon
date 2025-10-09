# Text Message Capture Guide

## 🎯 Current Status: DECRYPTION WORKING!

Your ESP32 sniffer is **successfully decrypting Meshtastic packets** with PSK "AQ==". The AES-CTR decryption is working perfectly!

## 📍 What You're Currently Capturing

Based on your terminal output, you're capturing:

1. **POSITION packets** (44 bytes) - GPS coordinates from device 0xBE438E37
2. **Small routing/ACK packets** (25-36 bytes) - Network management

The "text" fragments you see like "P.K", "5KM", "Xd\"" are **NOT user text messages**. They're GPS coordinates and metadata encoded in position packets.

## 🔍 How to Identify Packet Types

### Position Packets (What You're Seeing Now)
```
Decrypted first bytes: 0x12 0x11 0xAA 0xDF...
                       ^^^^
                       Field 2 (payload) - IMPLICIT portnum

→ This means portnum=3 (POSITION_APP)
→ Contains GPS data, not text messages
```

### Text Message Packets (What You Want)
```
Decrypted first bytes: 0x08 0x01 0x12 0x0A...
                       ^^^^ ^^^^
                       Field 1  portnum=1 (TEXT_MESSAGE_APP)

→ This is a text message!
→ Text follows at offset with 0x0A tag
```

### Routing/ACK Packets (Small 25-byte packets)
```
Decrypted first bytes: 0x0F 0x2C 0xF0...
                       ^^^^
                       Unusual field - likely routing

→ Network management, no user data
```

## ✅ How to Capture Text Messages

### Step 1: Send a Text Message
1. Open Meshtastic app on one of your devices (Heltec or LilyGo)
2. Send a text message: "Hello sniffer test"
3. Make sure it's **broadcast** or sent to the other device

### Step 2: Watch Terminal Output
Look for this pattern:
```
[PSK] === DECRYPTED MESSAGE ANALYSIS ===
[PSK] First byte: 0x08 = Field 1, Wire 0
[PSK] ✓ Field 1 (portnum) = 1 (TEXT_MESSAGE_APP) ✉️
[PSK] ✓ Field 2 (payload) length=22, starts at offset 3
[PSK] ✓ Text field (0x0A) length=18, starts at offset 5
[PSK] 🎉 SUCCESS! EXTRACTED TEXT: "Hello sniffer test"
```

### Step 3: What to Expect

**Current captures (Position packets):**
- First byte: `0x12` (Field 2)
- Size: ~44 bytes
- Content: GPS coordinates
- From: Node 0xBE438E37

**Text message captures (when you send one):**
- First byte: `0x08` (Field 1 = portnum)
- Second byte: `0x01` (TEXT_MESSAGE_APP)
- Size: varies (depends on text length)
- Content: Your actual text message

## 🔧 Diagnostic Commands

### Show Current Statistics
Press `x` to see diagnostic report:
```
📦 PACKET TYPE STATISTICS:
  TEXT messages: 0 packets    ← Should increase when you send text
  POSITION messages: 14        ← These are what you're seeing now
  ROUTING messages: 10
  OTHER messages: 5
```

### Enable Quiet Mode
Press `q` to reduce output and minimize timing gaps (helps capture more packets)

## 💡 Why Aren't You Seeing Text Messages?

**Possible reasons:**

1. **No text messages being sent**
   - Position packets broadcast automatically every 15-30 minutes
   - Text messages only send when user explicitly sends one
   - **Solution:** Send a text message from the Meshtastic app!

2. **Timing gaps missing the text message**
   - Your diagnostic showed 84 out of 85 gaps >500ms
   - If a text message transmitted during a gap, it was missed
   - **Solution:** Enable quiet mode (`q` command) and send multiple messages

3. **Text messages on different channel**
   - Verify both devices are on **same channel** (channel 0, "LongFast")
   - Check PSK matches: "AQ=="
   - **Solution:** Use Meshtastic app to verify channel settings

## 📊 Success Criteria

You'll know text message capture is working when you see:

```
[PSK] ✓ Field 1 (portnum) = 1 (TEXT_MESSAGE_APP) ✉️
[PSK] 🎉 SUCCESS! EXTRACTED TEXT: "your message here"

════════════════════════════════════════════════════
║  🎯 TEXT MESSAGE CAPTURED!                       ║
║  From: 0x401ACD4E                                ║
║  Text: "your message here"                       ║
════════════════════════════════════════════════════
```

## 🧪 Testing Protocol

1. **Prepare:** Enable quiet mode with `q` command
2. **Send:** Send text message from Meshtastic app: "TEST 1"
3. **Wait:** 2-3 seconds for capture
4. **Send:** Another message: "TEST 2"
5. **Check:** Press `x` to see diagnostic report
6. **Verify:** TEXT messages counter should be > 0

## 📈 Expected Results

**Before sending text messages:**
```
TEXT messages: 0 packets
POSITION messages: 14 packets  ← You have these!
```

**After sending text messages:**
```
TEXT messages: 3 packets       ← Success!
POSITION messages: 14 packets
```

## 🔬 Technical Details

### Meshtastic Data Message Structure (Encrypted Portion)

**Text Message:**
```
0x08 0x01        - Field 1 (portnum) = TEXT_MESSAGE_APP (1)
0x12 <len>       - Field 2 (payload) = length-delimited
  0x0A <len>     - Text field tag
    <text_bytes> - UTF-8 text string
```

**Position Message (what you're seeing):**
```
0x12 <len>       - Field 2 (payload) - IMPLICIT portnum=3
  <position_data> - Latitude, longitude, altitude, etc.
```

### Why Position Packets Have Implicit Portnum

Meshtastic v2.6.x optimizes bandwidth by using **implicit field encoding** for common packet types:
- Position packets are so common that portnum field (0x08 0x03) is omitted
- Decoder knows if first byte is 0x12, it's a position packet
- This saves 2 bytes per transmission

### Why You're Not Seeing 0x08 0x01 Pattern

**Simple answer:** You haven't sent any text messages yet! The devices are only broadcasting position updates automatically.

**To fix:** Send a text message from the Meshtastic app and watch the terminal output change from `0x12` (position) to `0x08 0x01` (text).

---

## Summary

✅ **Decryption works perfectly**
✅ **Capturing position packets successfully**
❌ **No text messages sent yet**

**Next step:** Open Meshtastic app and send a text message to see it decrypted!
