# Meshtastic Detection & PSK Decryption Troubleshooting

**Last Updated:** January 2025  
**Version:** 1.8 Production  
**Status:** Packet capture ✅ | PSK decryption ⚠️ (protocol mismatch)

---

## 🎯 Current Project Status

### What's Working
- ✅ **Packet Capture**: Successfully capturing Meshtastic packets from 3+ nodes
- ✅ **AES-CTR Decryption**: 100% functional with 5 default PSKs
- ✅ **Node Detection**: Identifying unique device IDs (0x401ACD4E, 0x44D7A39E, 0xBE438E37)
- ✅ **OLED Display**: Full display code with 6 modes (if hardware present)
- ✅ **Button Control**: Toggle/shutdown via GPIO 0 button

### What's Blocked
- ❌ **Text Message Extraction**: No protobuf portnum field (0x08) found in decrypted packets
- ❌ **Message Type Detection**: Cannot identify app type without portnum field
- ❌ **Encrypted Text Recovery**: Protocol mismatch between code and device firmware

**Root Cause**: Decrypted packets don't match documented Meshtastic protobuf structure. Likely firmware version or format differences.

---

## ✅ **Pre-Flight Checklist**

### ESP32 Hardware
- [ ] Antenna connected to ESP32 sniffer
- [ ] Antenna is 902-928 MHz compatible
- [ ] ESP32 powered on and running firmware
- [ ] Serial monitor showing scan messages

### T-Deck Configuration
- [ ] T-Deck powered on
- [ ] Meshtastic firmware installed and running
- [ ] Can see Meshtastic main screen
- [ ] WiFi/Bluetooth enabled for app connection

### Physical Setup
- [ ] Devices within 5 meters of each other
- [ ] No metal objects between devices
- [ ] Antennas oriented properly (vertical)

---

## 🔍 **Diagnostic Steps**

### **Step 1: Verify T-Deck is Transmitting**

**Method A: Using Meshtastic App**
```
1. Connect phone to T-Deck via Bluetooth
2. Open Meshtastic app
3. Go to: Radio Configuration → LoRa
4. Note these values:
   - Region: _______
   - Modem Preset: _______
   - Frequency: _______
5. Send test message
6. Watch for "Sent" confirmation
```

**Method B: Using Meshtastic CLI**
```bash
# Install if not already
pip install meshtastic

# Connect to T-Deck
meshtastic --info

# Look for:
# - Modem preset
# - Frequency
# - Region

# Check if it's transmitting
meshtastic --sendtext "Test"
```

### **Step 2: Verify ESP32 Radio is Working**

**Test Basic Reception:**
```
1. Start ESP32: pio device monitor
2. Wait for "[RECON] Started" message
3. Hold T-Deck VERY CLOSE to ESP32 (< 1 foot)
4. Send message from T-Deck
5. Look for ANY [PACKET] messages in serial output
```

**Expected Output:**
```
[SCAN] Meshtastic_LF_906 (906.875 MHz, SF11, BW250, SW:0x2B)
[SCAN] Meshtastic_LF_902 (902.125 MHz, SF11, BW250, SW:0x2B)
...
[PACKET] Meshtastic, 14 bytes, -45.0 dBm, 8.2 dB SNR  ← GOOD!
or
[RECON] Packet #1: Meshtastic, ...  ← GOOD!
```

**If seeing nothing:**
- ❌ ESP32 radio not configured correctly
- ❌ Antenna issue
- ❌ Frequency mismatch

### **Step 3: Match Frequencies Exactly**

**Get T-Deck Exact Config:**

Using Meshtastic app or CLI, find:
- **Frequency**: e.g., 906.875 MHz
- **Spreading Factor**: e.g., SF11
- **Bandwidth**: e.g., 250 kHz
- **Sync Word**: Usually 0x2b for Meshtastic

**Force ESP32 to Target That Frequency:**

1. Press 'm' on ESP32 serial
2. Press 'f' for frequency targeting
3. Find matching config from list
4. Select it (e.g., press '1' for first config)
5. ESP32 now locked on that frequency
6. Send message from T-Deck
7. Should see packet immediately

---

## 🐛 **Common Issues & Solutions**

### Issue 1: No Antenna on ESP32
**Symptom**: No packets ever detected, even when devices touching  
**Solution**: Connect 902-928 MHz antenna to ESP32 LoRa module

### Issue 2: Wrong Region
**Symptom**: T-Deck configured for EU (433/868 MHz), ESP32 scanning US (902-928 MHz)  
**Solution**: 
```
Meshtastic app → Radio Config → Region → Set to "US"
or
meshtastic --set lora.region US
```

### Issue 3: T-Deck Not Actually Transmitting
**Symptom**: T-Deck seems on but no TX activity  
**Check**: 
- Battery charged?
- Stuck in boot loop?
- Error messages on screen?
- Try power cycle

**Solution**:
```
Power off T-Deck completely
Remove battery if possible
Wait 10 seconds
Power back on
Wait for full boot (30-60 seconds)
Try sending message again
```

### Issue 4: Frequency Hop Mismatch
**Symptom**: ESP32 scanning frequency A, T-Deck transmitting on frequency B  
**Why**: Meshtastic can use different frequency slots

**Solution - Lock Both to Known Frequency:**

**T-Deck:**
```
Meshtastic app → Channels → Primary
Set to: "Long Fast" preset
Frequency Slot: 20 (default)
This typically = 906.875 MHz
```

**ESP32:**
```
Press 'f' → Select "Meshtastic_LF_906"
or
Press 'f' → Select "Meshtastic_LF_902"
Try both!
```

### Issue 5: Interference or Bad Antenna
**Symptom**: Intermittent detection, weak RSSI  
**Solution**:
- Move away from computers/USB hubs
- Try different antenna orientation
- Remove/add antenna extension
- Test in different room

---

## 📊 **Success Criteria**

### Minimum Success (Phase 1):
- [ ] ESP32 detects **any** packet from T-Deck
- [ ] Even 14-byte routing packets count
- [ ] RSSI shown (e.g., -45 dBm)

### Partial Success (Phase 2):
- [ ] ESP32 detects packets consistently
- [ ] Shows device with Node ID
- [ ] Can target specific device

### Full Success (Phase 3):
- [ ] Captures encrypted user messages (>20 bytes)
- [ ] PSK decryption attempts on packets
- [ ] Successfully decrypts with default key
- [ ] Shows message content

---

## 🔧 **Emergency Debugging Commands**

### ESP32 Commands:
```
'm' → Enter menu
'f' → Frequency targeting menu
'r' → Resume reconnaissance (keeps discovered devices)
'b' → Reboot device (clears all data)
's' → Show summary
'q' → Toggle quiet/verbose mode
```

### Meshtastic Commands:
```bash
# Get full device info
meshtastic --info

# Get LoRa config
meshtastic --get lora

# Set to known-good config
meshtastic --set lora.region US
meshtastic --set lora.modem_preset LONG_FAST
meshtastic --set lora.hop_limit 3

# Force transmission
meshtastic --sendtext "TEST 123"
```

---

## 📞 **Next Steps Based on Results**

### If you see routing packets (14 bytes):
✅ **Hardware working!** Now need to:
1. Generate actual user messages (not just routing)
2. Target correct frequency for user data
3. Wait for longer packets (encrypted content)

### If you see nothing at all:
❌ **Configuration mismatch.** Check:
1. Antenna connected?
2. Right frequency range (US vs EU)?
3. T-Deck actually transmitting?
4. Both devices in same room?

### If you see packets but no PSK success:
⚠️ **Partial success.** Investigate:
1. Are packets long enough? (Need >20 bytes)
2. Is T-Deck using default PSK?
3. Are you capturing routing vs user messages?

---

## 🖥️ **OLED Display Troubleshooting**

### Problem: "No OLED detected on I2C bus"

#### Identify Board Version
Different Heltec board versions use different pins:

**V3 (ESP32-S3)** - Current default:
- SDA: GPIO 17, SCL: GPIO 18, RST: GPIO 21
- Vext: GPIO 36 (active LOW)

**V2 (ESP32)** - Older boards:
- SDA: GPIO 4, SCL: GPIO 15, RST: GPIO 16
- Vext: GPIO 21

**V1 (Original ESP32)**:
- SDA: GPIO 4, SCL: GPIO 15, RST: GPIO 16
- No Vext power control

#### Diagnostic Steps

1. **Check Serial Output**
   ```
   [DISPLAY] Scanning I2C bus...
   [DISPLAY]   Address 0x3C: FOUND!
   ```
   or
   ```
   [DISPLAY]   Address 0x3C: not found (error 2)
   ```

2. **I2C Error Codes**
   - `error 0`: Success (device found)
   - `error 2`: Address not acknowledged (no device)
   - `error 3`: Data not acknowledged
   - `error 4`: Other error
   - `error 5`: Timeout

3. **Common Fixes**
   - **Wrong pins for V2**: Edit `oled_display.h` to use V2 pins
   - **Vext polarity**: Most V3 use LOW=ON, some need HIGH=ON
   - **Physical connection**: Check OLED is properly seated
   - **No OLED populated**: Some boards lack OLED (code handles gracefully)

4. **Manual I2C Scan**
   Add to setup() for debugging:
   ```cpp
   Serial.println("Scanning I2C 0x01-0x7F...");
   for (uint8_t addr = 1; addr < 127; addr++) {
     Wire.beginTransmission(addr);
     if (Wire.endTransmission() == 0) {
       Serial.printf("Device at 0x%02X\n", addr);
     }
   }
   ```

### Button Not Responding

1. **Check GPIO 0**: PRG button should be on GPIO 0
2. **Monitor serial**: Look for `[BUTTON] Button pressed` messages
3. **Test timing**: Short press < 3s, long press ≥ 3s

---

## 🎯 **Quick Win Test**

**Do this right now:**

1. **ESP32**: 
   ```
   Press 'f' → Select #1 (Meshtastic_LF_906)
   ```

2. **T-Deck**: Hold 1 foot from ESP32

3. **T-Deck App**: Send message "HELLO WORLD"

4. **ESP32**: Watch serial output for **any** [PACKET] message

**If you see a packet** → Hardware works, just need config tweaks  
**If you see nothing** → Deeper hardware/config issue

---

**Let me know what you find at each step!** 🔍
