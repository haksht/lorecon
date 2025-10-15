# PSK Decryption Troubleshooting Guide

**Problem**: Not seeing successful PSK decryption on primary channel broadcasts with default "AQ==" key

---

## ✅ Prerequisites Check

### 1. Build Flag Enabled
Check `platformio.ini`:
```ini
build_flags = 
    -DENABLE_PSK_TESTING    # <-- This must be present!
```

**Verify**: On startup, you should see:
```
🔐 PSK Testing initialized
📊 Testing 5 default keys
```

If you don't see this, PSK testing is disabled.

### 2. Correct Firmware Upload
```powershell
# Clean and rebuild
pio run --target clean
pio run --target upload
pio device monitor
```

---

## 🔍 Diagnostic Steps

### Step 1: Verify Packet Capture

**Expected output**:
```
[RECON] Packet #1: Meshtastic, 35 bytes, -45.0 dBm, 8.2 dB SNR
```

**If you see**:
- ❌ `Protocol: Unknown` → Protocol detection issue
- ❌ Very small packets (< 20 bytes) → Not full Meshtastic packets
- ❌ No packets at all → Radio/frequency issue

### Step 2: Check Hex Data

**Expected** (Meshtastic packet):
```
Data: FF FF FF FF 40 1A CD 4E 01 00 12 34 56 78 AB CD EF ...
       ^^^^^^^^^^ ^^^^^^^^^^^ Header + Node ID
```

**If you see**:
- ❌ Doesn't start with `FF FF FF FF` → Not standard Meshtastic format
- ❌ All zeros or random data → Reception issue

### Step 3: Verify PSK Attempt

In **targeted mode** (`press '8'` or select device), you should see:
```
[PSK] Analyzing 35-byte packet (text message size)
```
Or for smaller packets:
```
Decrypting...
```

**If you DON'T see these**:
- ❌ PSK testing not compiled in
- ❌ Packets too small (< 20 bytes)
- ❌ Wrong mode (need targeted mode for verbose output)

### Step 4: Check Packet Structure

The code checks for valid protobuf structure after decryption:
```cpp
uint8_t firstByte = decrypted[0];
uint8_t fieldNum = firstByte >> 3;   // Should be 1-31
uint8_t wireType = firstByte & 0x07; // Should be 0-5
```

**Position packets** typically decrypt to:
- `0x08 0x03` (field 1 = portnum, value 3 = POSITION_APP)

---

## 🧪 Manual Test Procedure

### Test 1: Verify Build

```powershell
# Check if PSK testing is compiled
pio run --target upload 2>&1 | grep "ENABLE_PSK_TESTING"
```

Should show: `-DENABLE_PSK_TESTING` in build flags

### Test 2: Force Targeted Mode

```
1. Start ESP32: pio device monitor
2. Wait for recon to find devices (~3 min)
3. Press '8' to target SF8 frequency (906.875 MHz)
4. Watch for position broadcasts
```

**Expected output every 30-900 seconds**:
```
🎯 [CAPTURE] Packet #X: Meshtastic, 35 bytes, -45.0 dBm
[PSK] Analyzing 35-byte packet (text message size)
[PSK] ✓ Decrypted with key #1 ("AQ==")
[PSK] Type: POSITION_APP (portnum 0x03)
```

### Test 3: Check Actual Packet Contents

Add this temporary debug output in `lora_recon_tool.cpp` line ~445:

```cpp
// After packet capture, before PSK testing
Serial.println("\n=== PACKET DEBUG ===");
Serial.printf("Length: %d bytes\n", length);
Serial.printf("First 20 bytes: ");
for (size_t i = 0; i < min(length, size_t(20)); i++) {
    Serial.printf("%02X ", data[i]);
}
Serial.println("\n===================\n");
```

This shows you exactly what the radio is receiving.

---

## 🐛 Common Issues & Fixes

### Issue 1: Packets Too Small

**Symptom**: Only seeing 10-15 byte packets

**Cause**: Partial reception or wrong frequency

**Fix**:
- Check antenna connection
- Move devices closer (<1 meter)
- Verify frequency: 906.875 MHz for US LONG_FAST

### Issue 2: Wrong Packet Format

**Symptom**: Packets don't start with `FF FF FF FF`

**Cause**: 
- Receiving LoRaWAN instead of Meshtastic
- Receiving partial/routed packets
- Wrong spreading factor

**Fix**:
- Confirm devices are Meshtastic (not TTN/LoRaWAN)
- Check device settings: Region=US, Preset=LONG_FAST
- Try SF11 (902.125 MHz) instead of SF8

### Issue 3: Packets Are Unencrypted

**Symptom**: 
```
[PSK] ⚠️  Packet appears unencrypted (found 0x08 field tag)
```

**This is actually SUCCESS!** Some Meshtastic packets are unencrypted by design:
- Node info broadcasts
- Routing announcements

**But** position and text should be encrypted on primary channel.

**Check**: Are you using a custom channel with encryption disabled?

### Issue 4: No Packets in Targeted Mode

**Symptom**: Recon finds devices, but targeting shows nothing

**Cause**: Devices transmit infrequently (position every 30-900 seconds)

**Fix**:
- **Be patient** - wait 5-10 minutes in targeted mode
- Force transmission: Send message from Meshtastic app
- Check device position interval: Settings → Position → Broadcast Interval

### Issue 5: Decryption Attempts But No Success

**Symptom**:
```
[PSK] Analyzing 35-byte packet
(no success message)
```

**Possible causes**:

1. **Wrong PSK** - Device using custom key, not "AQ=="
   - Check Meshtastic app: Channel → Edit → PSK
   - Should be "AQ==" for default

2. **Wrong nonce construction** - Firmware version mismatch
   - What firmware version are your devices running?
   - 2.3.x? 2.4.x? 2.5.x+?

3. **Packet corruption** - Low signal, interference
   - Check RSSI: should be > -100 dBm
   - Check SNR: should be > 0 dB

---

## 📊 Success Criteria

### You know decryption is working when you see:

```
[RECON] Packet #47: Meshtastic, 35 bytes, -42.0 dBm, 8.5 dB SNR
[PSK] Analyzing 35-byte packet (text message size)
[PSK] ✓ Decrypted with key #1 ("AQ==")
[PSK] Node: 0x401ACD4E, Packet: 0x00123456, Type: 0x01
[PSK] Type: POSITION_APP (portnum 0x03)
[PSK] No text message found (routing/position/admin packet)
```

Or for actual text messages:
```
[PSK] ✓ Decrypted with key #1 ("AQ==")
[PSK] Type: TEXT_MESSAGE_APP (portnum 0x01)

╔════════════════════════════════════════════╗
║  📧 TEXT MESSAGE: "Hello World"
╚════════════════════════════════════════════╝
```

---

## 🔬 Advanced Debugging

### Enable Verbose PSK Logging

Modify `psk_decryption_simple.cpp` line ~205 to add debug output:

```cpp
// Try each PSK
for (uint8_t i = 0; i < NUM_PSKS; i++) {
    Serial.printf("[PSK DEBUG] Testing key #%d: %s\n", i+1, DEFAULT_PSKS[i]);
    
    uint8_t key[32];
    int keyLen = decodeBase64(DEFAULT_PSKS[i], key, sizeof(key));
    Serial.printf("[PSK DEBUG] Decoded to %d bytes\n", keyLen);
    
    // ... rest of decryption
    
    if (result == 0) {
        Serial.printf("[PSK DEBUG] Decryption OK, checking protobuf...\n");
        Serial.printf("[PSK DEBUG] First byte: 0x%02X (field %d, wire %d)\n", 
                      decrypted[0], decrypted[0] >> 3, decrypted[0] & 0x07);
    }
}
```

This shows exactly where decryption fails.

### Check Meshtastic Device Settings

On your Meshtastic devices:

1. **Verify primary channel**:
   ```
   App → Channels → Primary
   PSK: "AQ==" or "Default (AQ==)"
   ```

2. **Verify position broadcasts enabled**:
   ```
   App → Position → Smart Broadcast
   Interval: 30-900 seconds (not 0/disabled)
   ```

3. **Verify firmware version**:
   ```
   App → Settings → About
   Firmware: 2.3.x, 2.4.x, or 2.5.x+
   ```

---

## 📝 Report Template

If still not working, provide this info:

```
Device Information:
- ESP32 board: [Heltec WiFi LoRa 32 V3?]
- Meshtastic device: [T-Deck? RAK? Heltec?]
- Meshtastic firmware: [version?]
- Channel PSK: [AQ== or custom?]

Build Information:
- PlatformIO version: [?]
- Build flags: [paste from platformio.ini]
- Compilation output: [any warnings?]

Observed Behavior:
- Packets captured? [yes/no, how many?]
- Packet sizes: [typical size in bytes?]
- PSK attempts visible? [yes/no]
- Hex data sample: [first 20 bytes of a packet]

Serial Output Sample:
[paste 30-50 lines showing packet capture attempt]
```

---

## ✅ Next Steps

1. Work through diagnostic steps above
2. Add debug output if needed
3. Collect packet samples (hex dump)
4. Report findings with template above

The PSK code is solid - if it's not working, it's a configuration, reception, or protocol mismatch issue that we can debug with the right data.
