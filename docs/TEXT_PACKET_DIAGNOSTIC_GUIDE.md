# Text Packet Diagnostic Tool

## Problem Statement

Your ESP32 LoRa sniffer successfully:
- ✅ Captures and decrypts routing/ACK/position packets from Meshtastic devices
- ✅ PSK "AQ==" works perfectly (proven by 30+ successful decryptions)
- ✅ AES-CTR decryption with correct nonce structure (NodeID at offset 8)

But:
- ❌ **Text message DATA packets are never captured**
- ❌ Only routing/control/position packets appear in capture
- ❌ Pattern `0x08 0x01` (TEXT_MESSAGE_APP) never found in any decrypted packet

Messages **DO** transmit successfully between your devices (you confirmed delivery), but the sniffer is missing the actual text message packets.

## What I've Added

I've created a **comprehensive diagnostic tool** to help identify WHY text packets aren't being captured. This tool analyzes:

1. **Timing Gaps** - Measures gaps between packet reception
   - Identifies if text packets are transmitted while sniffer is busy processing
   - Reports gaps >500ms that could cause missed packets

2. **Encryption Analysis** - Detects encrypted vs plaintext packets
   - Checks entropy (randomness) to identify encrypted payloads
   - Identifies if text messages might be sent as plaintext
   - Searches for protobuf markers in raw packets

3. **Packet Type Statistics** - Tracks what you're actually capturing
   - Counts TEXT vs POSITION vs ROUTING packets
   - Shows size ranges for each packet type
   - Identifies if text messages are completely absent

4. **Automatic Conclusions** - Diagnoses the most likely root cause
   - Timing issues (most likely given your setup)
   - Frequency/modulation mismatches
   - Plaintext vs encrypted packets
   - Channel configuration problems

## Files Added

```
firmware/src/
├── text_packet_diagnostic.h     - Header with function declarations
├── text_packet_diagnostic.cpp   - Diagnostic implementation
└── (modifications to existing files)
```

## Files Modified

1. **psk_decryption_simple.cpp**
   - Added `#include "text_packet_diagnostic.h"`
   - Calls `TextPacketDiagnostic::analyzeDecryptedPacket()` after successful decryption
   - Provides detailed analysis of decrypted packet contents

2. **lora_recon_tool.cpp**
   - Added `#include "text_packet_diagnostic.h"`
   - Calls `TextPacketDiagnostic::analyzePacket()` immediately after receiving each packet
   - Analyzes RAW packets before decryption attempt

3. **command_handler.h** and **command_handler.cpp**
   - Added new command 'x' to print diagnostic report
   - Modified `cmdRestartRecon()` to reset diagnostic counters
   - Updated help menu to show diagnostic command

## How to Use

### Step 1: Build and Flash

No configuration changes needed - just build and flash as normal:

```powershell
cd c:\Users\tim\lora\esp32-sniffer
pio run -t upload && pio device monitor
```

### Step 2: Start Targeting Mode

Once the device boots, target the frequency where your devices are communicating:

```
Press '8' for SF8 targeting (if using 902.125 MHz SF8)
  OR
Press 'f' for frequency targeting menu, then select your frequency
  OR  
Press '1' to target a specific discovered device
```

### Step 3: Send Test Messages

With the sniffer in targeting mode and actively monitoring:

1. Send a short message from Device A to Device B: **"hi"**
2. Wait 3 seconds
3. Send another message: **"test message"**
4. Wait 3 seconds
5. Send a longer message: **"this is a longer test message to see if size matters"**

### Step 4: Review Diagnostic Output

For **EACH** packet captured, you'll see detailed analysis:

```
[DIAG] ═══════ RAW PACKET ANALYSIS ═══════
[DIAG] Length: 34 bytes, RSSI: -11.0 dBm, SNR: 10.5 dB
[DIAG] Node ID: 0x44D7A39E
[DIAG] Payload start (bytes 12-27): 5A B2 C4 8F 3D 12 A5 ...
[DIAG] Entropy analysis: 85.3% non-zero, 14 high bytes
[DIAG] 🔒 HIGH ENTROPY - Likely ENCRYPTED packet
[DIAG] Packet size: 34 bytes (medium - could be text message)
[DIAG] ═══════════════════════════════════

[PSK] === MESHTASTIC MESSAGE ANALYSIS ===
[PSK] 🔓 DECRYPTION SUCCESS with default key #1!

[DIAG] ─── DECRYPTED CONTENT ANALYSIS ───
[DIAG] ✓ Valid portnum field found: 0x05 = ROUTING_APP 🔄
```

### Step 5: Generate Diagnostic Report

After capturing several packets (including during when you sent text messages), press:

```
'x' - Show text packet diagnostic report
```

This will print a comprehensive report like:

```
╔═══════════════════════════════════════════════════════╗
║       TEXT PACKET DIAGNOSTIC REPORT                  ║
╚═══════════════════════════════════════════════════════╝

📊 TIMING ANALYSIS:
  Total packet gaps: 15
  Maximum gap: 1243 ms
  Large gaps (>500ms): 5

  ⚠️  WARNING: Large gaps detected!
     Text messages might be transmitted during these gaps.
     Your sniffer is NOT in continuous receive mode.

🔒 ENCRYPTION ANALYSIS:
  Encrypted packets: 12
  Plaintext packets: 0
  Unknown packets: 0

📦 PACKET TYPE STATISTICS:
  TEXT messages: NONE ❌
  POSITION messages: 3 packets
    Size range: 145 - 151 bytes (avg: 148.0)
  ROUTING messages: 9 packets
    Size range: 24 - 34 bytes (avg: 27.3)
  OTHER messages: NONE

🔍 DIAGNOSTIC CONCLUSIONS:

  ❌ NO TEXT MESSAGES CAPTURED
     Possible reasons:
     1. ⚠️  TIMING ISSUE (most likely)
        Text packets transmitted during processing gaps
        Solution: Implement interrupt-driven buffering

     2. ⚠️  FREQUENCY/MODULATION MISMATCH
        Text packets on different channel parameters
        Solution: Verify channel settings match exactly

     3. ⚠️  TEXT MIGHT BE IN PLAINTEXT
        Check plaintext packets above for text content
        Solution: Modify code to check plaintext packets
```

## What the Report Tells You

### Scenario 1: Large Timing Gaps Detected

**Diagnosis:** Your sniffer is missing packets because it's not in continuous receive mode.

**What's happening:**
- Sniffer receives packet → processes it → calls `radio.startReceive()` → waits for next packet
- During processing (1-2 seconds), radio is NOT listening
- Text messages transmitted during this gap are **completely missed**

**Solution:**
This is a **radio hardware limitation** with your current synchronous design. To fix:

1. **Short-term workaround:** Have someone send text messages REPEATEDLY (every 2 seconds) while you monitor. Eventually one should arrive during a listening window.

2. **Proper fix:** Implement interrupt-driven receive with a circular buffer:
   - RadioLib DIO1 interrupt triggers on packet arrival
   - ISR copies packet to buffer without processing
   - Main loop processes buffered packets
   - Radio IMMEDIATELY restarts receive after interrupt

### Scenario 2: Plaintext Packets Detected

**Diagnosis:** Text messages might be sent UNENCRYPTED despite PSK configuration.

**What's happening:**
- Meshtastic firmware shows packets can be `encrypted_tag` OR `decoded_tag` (plaintext)
- Your routing/position packets are encrypted (high entropy)
- But text messages might use `decoded_tag` variant

**Solution:**
Look at the "RAW PACKET ANALYSIS" output for plaintext packets (LOW ENTROPY). Check if any contain readable text. If so, text messages are unencrypted and need different handling.

### Scenario 3: No Gaps, Only Encrypted Packets, Still No Text

**Diagnosis:** Text packets on different frequency or modulation parameters.

**What's happening:**
- Meshtastic might use **different SF/BW for data vs control packets**
- Your SF8 (902.125 MHz) might be for control only
- Text might be on different channel slot

**Solution:**
1. Check **both devices' channel configuration** - screenshot or serial output
2. Verify they show IDENTICAL frequency, SF, BW, sync word
3. Try targeting ALL Meshtastic frequencies in sequence while sending text
4. Use device channel scanner to see actual transmission parameters

## Next Steps

1. **Run the diagnostic** with the new code
2. **Send multiple test messages** while monitoring
3. **Review the diagnostic report** (press 'x')
4. **Share the report output** - it will pinpoint the exact issue

The report will definitively tell us if this is:
- ✅ Timing (most likely) → needs interrupt-driven buffering
- ✅ Plaintext packets → needs plaintext processing
- ✅ Frequency mismatch → needs channel verification
- ✅ Something else entirely

## Example Output to Expect

When you run this, you should see:

**For routing packets (currently working):**
```
[DIAG] 🔒 HIGH ENTROPY - Likely ENCRYPTED packet
[PSK] 🔓 DECRYPTION SUCCESS with default key #1!
[DIAG] ✓ Valid portnum field found: 0x05 = ROUTING_APP 🔄
```

**For text packets (if captured):**
```
[DIAG] 🔒 HIGH ENTROPY - Likely ENCRYPTED packet
[PSK] 🔓 DECRYPTION SUCCESS with default key #1!
[DIAG] ✓ Valid portnum field found: 0x01 = TEXT_MESSAGE_APP ✉️
[DIAG]   Found text field at offset 4, length 11: "hello world"
```

**For timing gaps (explaining missing packets):**
```
[DIAG] ⚠️ Large gap detected: 1523 ms (packet might have been missed during this time)
```

## Technical Details

### Entropy Analysis

The tool checks if a packet is encrypted by analyzing the payload:
- **High entropy** (>70% non-zero bytes, many >0x80) → Encrypted
- **Low entropy** (structured, many zeros) → Plaintext or structured data

### Protobuf Detection

Searches for these markers in decrypted data:
- `0x08` (field 1) = portnum field
  - `0x01` = TEXT_MESSAGE_APP
  - `0x03` = POSITION_APP
  - `0x05`/`0x07` = ROUTING_APP
- `0x0A` = Text string marker (when preceded by field 2)

### Timing Analysis

Tracks time between `radio.readData()` calls:
- <100ms = Normal sequential packets
- 100-500ms = Processing delay but acceptable
- \>500ms = **CRITICAL GAP** - packets likely missed

## Troubleshooting

**Q: I'm not seeing any [DIAG] output**
A: Make sure you've built and flashed the new code. The diagnostic runs automatically on every packet.

**Q: Report shows "Total packet gaps: 0"**
A: Send more test messages. The gap counter only starts after the first packet.

**Q: I see encrypted packets but decryption fails**
A: This is normal for packets encrypted with different PSKs. The diagnostic focuses on successfully decrypted packets.

**Q: How do I reset the diagnostic counters?**
A: Press 'r' to restart reconnaissance, or power cycle the device.

---

**Bottom Line:** This diagnostic will definitively identify why text packets aren't being captured. The most likely culprit is timing gaps, but the tool will prove it with data rather than assumptions.
