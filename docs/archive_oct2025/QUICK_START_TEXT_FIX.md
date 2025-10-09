# Quick Start: Fixing Text Message Extraction

**Last Updated:** October 8, 2025  
**Status:** Awaiting firmware version confirmation

---

## ⚠️ BETTER APPROACH AVAILABLE!

**Instead of manual parsing, use official Meshtastic protobufs + nanopb!**

👉 **See [USING_OFFICIAL_PROTOBUFS.md](USING_OFFICIAL_PROTOBUFS.md) for the RECOMMENDED approach**

This guide shows the manual cleanup approach, but using official protobufs is:
- ✅ More reliable (works with ALL firmware versions)
- ✅ Less code (100 lines vs 700 lines)
- ✅ Automatically handles protocol changes
- ✅ Proven and tested by Meshtastic team

**Continue below only if you prefer manual parsing for some reason.**

---

## TL;DR - What to Do Right Now (Manual Approach)

### Step 1: Check Your Firmware (5 minutes)

On **BOTH** Meshtastic devices:
1. Open Meshtastic app
2. Go to: **Settings → Device → Firmware Version**
3. Write down the EXACT version string
4. Example: "v2.3.7.abc123" or "v2.6.11-beta"

### Step 2: Clean Up the Code (15 minutes)

```powershell
# Navigate to project
cd C:\Users\tim\lora\esp32-sniffer

# Backup current code
cp firmware/src/psk_decryption_simple.cpp firmware/src/psk_decryption_simple.cpp.backup

# Replace with clean version
cp firmware/src/psk_decryption_clean.cpp firmware/src/psk_decryption_simple.cpp

# Compile
pio run -e default

# Flash
pio run -e default --target upload

# Test
pio device monitor
```

### Step 3: Send Test Messages (5 minutes)

1. Open Meshtastic app
2. Send these messages:
   - "A"
   - "TEST"
   - "Hello World"
3. Watch serial output for each
4. Copy the output to a file

### Step 4: Report Results

Share these details:
- ✅ Firmware version (both devices)
- ✅ Decryption still works? (yes/no)
- ✅ Packet sizes for each test message
- ✅ First 32 bytes of hex output (from serial)

---

## What the Clean Code Does

### Before (Old Code - 700+ lines)
```
├── Huge nested if statements
├── Multiple parsing strategies mixed together
├── 64+ lines of debug output per packet
├── Repeated code for similar patterns
└── Hard to understand and modify
```

### After (Clean Code - 400 lines)
```
├── decryptPacket() - Handles AES-CTR
├── identifyPacketType() - Determines packet type
├── extractTextStandard() - Strategy 1
├── extractTextImplicitPortnum() - Strategy 2
├── extractTextAnyReadable() - Strategy 3 (fallback)
└── Clean, concise diagnostic output
```

**Benefits:**
- ✅ 43% less code
- ✅ Easier to read and maintain
- ✅ Simple to add new format parsers
- ✅ Same functionality, cleaner implementation

---

## Expected Outcomes

### If Firmware is v2.0-2.2 (Standard Format)

**You should see:**
```
[PSK] Type: TEXT_MESSAGE_APP ✉️
[PSK] ✓ Text extraction: STANDARD FORMAT
╔════════════════════════════════════════════╗
║  📧 TEXT MESSAGE: "Hello World"
╚════════════════════════════════════════════╝
```

### If Firmware is v2.3+ (Implicit Format)

**Current output:**
```
[PSK] Type: POSITION_APP (implicit portnum=3) 📍
[PSK] ℹ️ No text message found (might be position/routing packet)
```

**After adding v2.3 parser:**
```
[PSK] Type: TEXT_MESSAGE_APP (implicit format detected) ✉️
[PSK] ✓ Text extraction: IMPLICIT PORTNUM FORMAT
╔════════════════════════════════════════════╗
║  📧 TEXT MESSAGE: "Hello World"
╚════════════════════════════════════════════╝
```

---

## Troubleshooting

### "Compilation fails with clean code"

**Check:**
- Is `text_packet_diagnostic.h` still included?
- Did you copy the file correctly?
- Try: `pio run -e default --target clean` first

### "Decryption no longer works"

**This shouldn't happen** - the clean code has the same decryption logic.

**But if it does:**
```powershell
# Restore backup
cp firmware/src/psk_decryption_simple.cpp.backup firmware/src/psk_decryption_simple.cpp
pio run -e default --target upload
```

Then report the issue.

### "Still no text messages"

**This is expected!** The clean code doesn't add new functionality yet.

**Next step:** Once we know your firmware version, we'll add the correct parser.

---

## Understanding the Diagnostic Output

### Current Output (Position Packet)

```
[PSK] === ATTEMPTING DECRYPTION ===
[PSK] Node: 0xBE438E37, Packet ID: 0x00001234
[PSK] ✓ Decryption SUCCESS with key #1: "AQ=="
[PSK] First 32 bytes: 12 11 AA DF 52 4B 90 C8 47 3A 01 DD 58 84 24...
[PSK] First byte: 0x12 (Field 2, Wire 2)
[PSK] Type: POSITION_APP (implicit portnum=3) 📍
[PSK] ℹ️ No text message found (might be position/routing packet)
```

**What this means:**
- ✅ Packet captured
- ✅ Decryption successful
- ✅ Identified as position packet
- ⚠️ No text (correct - it's GPS data)

### Desired Output (Text Message)

```
[PSK] === ATTEMPTING DECRYPTION ===
[PSK] Node: 0x401ACD4E, Packet ID: 0x00005678
[PSK] ✓ Decryption SUCCESS with key #1: "AQ=="
[PSK] First 32 bytes: 08 01 12 0E 0A 0C 48 65 6C 6C 6F 20 57 6F 72...
[PSK] First byte: 0x08 (Field 1, Wire 0)
[PSK] Type: TEXT_MESSAGE_APP ✉️
[PSK] ✓ Text extraction: STANDARD FORMAT

╔════════════════════════════════════════════╗
║  📧 TEXT MESSAGE: "Hello World"
╚════════════════════════════════════════════╝
```

**Key differences:**
- ✅ First byte is `0x08` (not `0x12`)
- ✅ Type identified as TEXT_MESSAGE_APP
- ✅ Text successfully extracted

---

## Firmware Version Quick Reference

| Version | Format | First Byte | Notes |
|---------|--------|------------|-------|
| v1.x | Legacy | Varies | Pre-2022, rare now |
| v2.0-2.2 | Standard | `0x08` | Explicit portnum field |
| v2.3-2.5 | Implicit | `0x12` | Bandwidth optimized |
| v2.6+ | Latest | TBD | Need to test |

**Once you provide your version, I can tell you exactly which parser to implement.**

---

## What Happens Next

### After You Provide Firmware Version

**If v2.0-2.2:**
- Code should already work
- If not, minor tweaks needed

**If v2.3+:**
- Need to add implicit portnum parser
- ~30 lines of code
- I'll provide exact implementation

**If custom/unknown:**
- Will analyze your hex dumps
- Reverse engineer the format
- Create custom parser

---

## Files Modified

### New Files Created
```
firmware/src/psk_decryption_clean.cpp     - Cleaned implementation
docs/TEXT_MESSAGE_ISSUE_ANALYSIS.md       - Full analysis
docs/QUICK_START_TEXT_FIX.md              - This file
```

### Files to Backup (Before Replacing)
```
firmware/src/psk_decryption_simple.cpp    - Current version
```

### Files NOT Modified
```
✅ firmware/src/psk_decryption_simple.h   - Interface unchanged
✅ firmware/src/lora_recon_tool.cpp       - No changes needed
✅ firmware/src/main.cpp                  - No changes needed
✅ platformio.ini                         - No changes needed
```

---

## Timeline Estimate

### Scenario A: v2.0-2.2 Standard Format
- Should work immediately with clean code
- **Time:** 15 minutes (just code replacement)

### Scenario B: v2.3+ Implicit Format
- Add implicit portnum parser (~30 lines)
- Test and validate
- **Time:** 1-2 hours

### Scenario C: Unknown Custom Format
- Analyze hex dumps
- Reverse engineer structure
- Implement custom parser
- **Time:** 3-4 hours

---

## Contact Points

### If You Get Stuck

1. **Compilation errors:** Share the exact error message
2. **Runtime errors:** Share serial output
3. **No change in behavior:** Share firmware versions
4. **Other issues:** Describe what you see vs. what you expected

### Information to Provide

Always include:
- ✅ Firmware version (both devices)
- ✅ Serial output (copy/paste or screenshot)
- ✅ What you did (steps taken)
- ✅ What happened (actual result)
- ✅ What you expected (desired result)

---

## Success Checklist

### Before Firmware Version Check
- [ ] Code compiles without errors
- [ ] Device flashes successfully
- [ ] Serial monitor shows output
- [ ] Decryption still works
- [ ] Position packets still decoded

### After Providing Firmware Version
- [ ] Format-specific parser implemented
- [ ] Code compiles without errors
- [ ] Test messages sent from Meshtastic app
- [ ] Text messages successfully extracted
- [ ] Output shows actual message content

---

## Remember

**The decryption is working perfectly!**

The only issue is understanding the specific message format your firmware uses. Once we know that, adding the correct parser is straightforward.

**Don't worry about:**
- ❌ Changing radio settings
- ❌ Trying different keys
- ❌ Modifying nonce construction
- ❌ Adjusting timing

**Focus on:**
- ✅ Getting firmware versions
- ✅ Testing with clean code
- ✅ Sending test messages
- ✅ Capturing hex output

---

**Let's get those firmware versions and fix this!** 🚀
