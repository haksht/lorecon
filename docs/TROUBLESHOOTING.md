# Troubleshooting Guide

**Version:** 2.4.1

---

## 🔒 Serial Console Activation (Noise Protection)

To prevent USB electrical noise from triggering phantom commands during unattended operation, serial commands are **disabled by default**.

### Activating Serial Console
1. Press **Enter** once → See "Press Enter again within 1.5s to activate..."
2. Press **Enter** again within 1.5 seconds → Console activated ✓
3. Now commands like `m`, `f`, `r` will work

### Auto-Deactivation
- Console automatically deactivates after **5 minutes of inactivity**
- Re-authenticate by pressing Enter twice again
- Protects against overnight/week-long USB noise issues

### Why This Matters
USB cables can pick up electrical interference that appears as random characters. Without this protection, noise bytes like `0x6D` (ASCII 'm') could switch the device to menu mode unexpectedly.

**Heltec V4 note:** V4 has no serial monitor (native USB is disabled to prevent radio interference). Use the web UI for all monitoring and configuration.

---

## ✅ **Pre-Flight Checklist**

### ESP32 Hardware
- [ ] Antenna connected to ESP32 LoRa module
- [ ] Antenna is the right frequency for your region (915 MHz for US, 868 MHz for EU)
- [ ] ESP32 powered on and running firmware
- [ ] OLED shows scanning status, or web UI is accessible

### Test Transmitter (any Meshtastic device)
- [ ] Meshtastic device powered on and running
- [ ] Can see Meshtastic main screen or app connection
- [ ] Configured for the same frequency region as sniffer

### Physical Setup
- [ ] Devices within 5 meters of each other
- [ ] No metal objects between devices
- [ ] Antennas oriented properly (vertical)

---

## 🔍 **Diagnostic Steps**

### **Step 1: Verify Your Test Device is Transmitting**

**Method A: Using Meshtastic App**
```
1. Connect phone to your Meshtastic device via Bluetooth
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

# Connect to your device
meshtastic --info

# Look for:
# - Modem preset
# - Frequency
# - Region

# Check if it's transmitting
meshtastic --sendtext "Test"
```

### **Step 2: Verify ESP32 Radio is Working**

**Test Basic Reception (Heltec V3 / T3-S3 / T-Beam):**
```
1. Start ESP32: pio device monitor
2. Wait for "[RECON] Started" message
3. Hold test device VERY CLOSE to ESP32 (< 1 foot)
4. Send message from test device
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

On the **Heltec V4**, check the web UI Packets tab instead of serial output.

### **Step 3: Match Frequencies Exactly**

**Get your Meshtastic device's config:**

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
6. Send message from your Meshtastic device
7. Should see packet immediately

---

## 🐛 **Common Issues & Solutions**

### Issue 1: No Antenna on ESP32
**Symptom**: No packets ever detected, even when devices touching
**Solution**: Connect 902-928 MHz antenna to ESP32 LoRa module

### Issue 2: Wrong Region
**Symptom**: Meshtastic device configured for EU (433/868 MHz), ESP32 scanning US (902-928 MHz)
**Solution**:
```
Meshtastic app → Radio Config → Region → Set to "US"
or
meshtastic --set lora.region US
```

### Issue 3: Test Device Not Actually Transmitting
**Symptom**: Meshtastic device seems on but no TX activity
**Check**:
- Battery charged?
- Stuck in boot loop?
- Error messages on screen?

### Issue 4: Replayed Packet Not Recaptured
**Symptom**: You replay a packet but don't see it appear in the captured packets list

**This is expected behavior**, not a bug. The SX1262 radio is **half-duplex** — it can either transmit OR receive, never both simultaneously:

- When you replay, the radio switches to TX mode
- After transmission completes, it returns to RX mode
- By then, your own transmission is already over the air and gone

**You cannot capture your own transmission.**

**To verify replay is working:**
1. **Second Meshtastic device**: Another node in range will receive and display the message
2. **SDR receiver**: Use RTL-SDR + SDR++ to observe the RF transmission
3. **Serial output**: Watch for "TX complete" confirmation
4. **Relayed capture**: If another mesh node relays your packet, you may capture that *relayed* copy (from the other node, not your own TX)

### Issue 5: Queue Overflow / Packet Drops
**Symptom**:
- Serial shows: `[QUEUE] Full (100 packets) - dropping packet! Total drops: X`
- Web UI toast: `⚠️ Queue overload: X packets dropped (Y%)`
- Missing packets in capture log

**When This Occurs:**
- High-traffic environments (conferences, festivals with 50+ devices)
- Burst transmissions (20+ packets arriving within 1 second)
- Slow SD card writes blocking queue processing
- ESP32 busy with web server requests during heavy traffic

**Understanding the Numbers:**
- Queue capacity: **100 packets**
- Drop rate formula: `droppedPackets / (totalPackets + droppedPackets) * 100%`
- Warning threshold: **5% drop rate**
- Example: 1000 packets captured, 50 dropped = 4.8% drop rate (acceptable)
- Example: 800 packets captured, 200 dropped = 20% drop rate (problematic)

**Solutions:**

1. **Disable web UI during critical capture**:
   - WebSocket broadcasts and HTTP requests consume CPU cycles
   - Disconnect phone/browser during high-traffic periods
   - Re-connect after to view captured data

2. **Use faster SD card** (T3-S3 / T-Beam Supreme):
   - Class 10 or UHS-I cards write faster
   - Reduces time spent in blocking write operations
   - Sandisk Extreme recommended over generic cards

3. **Target specific frequency** (temporary):
   - Press `m` → `f` → select one frequency config
   - Eliminates frequency hopping overhead
   - Captures all traffic on that single frequency
   - Resume recon mode afterward with `r`

4. **Monitor drop rate**:
   - Serial: Watch for `[QUEUE]` messages
   - Web UI: Toast warnings appear automatically
   - API: `GET /api/status` returns `droppedPackets` field

**Why Not Backpressure?**
Backpressure (pausing radio when queue fills) would eliminate drops but create coverage gaps. For reconnaissance, continuous monitoring is more valuable than perfect capture. Trade-off: lose some packets randomly vs miss entire transmission windows.

**Acceptable Drop Rates:**
- **<2%**: Excellent - occasional burst handling
- **2-5%**: Good - sustained high traffic
- **5-10%**: Acceptable - very busy environment
- **>10%**: Problematic - consider mitigation strategies above

### Issue 6: Hardware Sensitivity Variance Between Units
**Symptom**: Two identical Heltec V3 boards running same firmware show significantly different packet/device counts

**Example from testing:**
| Metric | Unit A | Unit B | Difference |
|--------|--------|--------|------------|
| Packets (15h) | 214 | 143 | +50% |
| Devices detected | 27 | 12 | +125% |
| Memory (stable) | 189KB | 188KB | Same |

**Why This Happens:**
- SX1262 radio chips have **manufacturing tolerances** affecting receive sensitivity
- Sensitivity can vary ±2-3 dB between individual chips
- A 2-3 dB difference means one radio can hear weaker signals the other misses
- This is **normal semiconductor variation**, not a firmware bug

**What to Do:**
1. **Test your units**: Run both overnight with same firmware/location
2. **Label your boards**: Mark the more sensitive one as your "primary"
3. **Use the better unit for field work**: Higher sensitivity = more devices detected
4. **The "worse" unit is still fine**: Works correctly, just has slightly less range

**Not a Fix For:**
- This is hardware, not fixable in firmware
- Both units work within SX1262 specifications
- Antenna quality also affects sensitivity (check connections)

### Issue 7: Frequency Hop Mismatch
**Symptom**: ESP32 scanning frequency A, test device transmitting on frequency B
**Why**: Meshtastic can use different frequency slots

**Solution - Lock Both to Known Frequency:**

**Meshtastic device:**
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

### Issue 8: Interference or Bad Antenna
**Symptom**: Intermittent detection, weak RSSI
**Solution**:
- Move away from computers/USB hubs
- Try different antenna orientation
- Remove/add antenna extension
- Test in different room

---

## 📊 **Signal Health Check**

### Minimum (device working):
- [ ] ESP32 detects **any** packet from a nearby Meshtastic device
- [ ] Even 14-byte routing packets count
- [ ] RSSI shown (e.g., -45 dBm)

### Good (ready for field use):
- [ ] Packets detected consistently
- [ ] Devices appear in the Devices tab with Node IDs
- [ ] Can target a specific device or frequency

### Full capability:
- [ ] Captures encrypted user messages (>20 bytes)
- [ ] PSK decryption succeeds on devices using default keys
- [ ] Message content visible in Packets tab

---

## 🔧 **Serial Commands Reference**

Activate the console first (double-Enter within 1.5 seconds):

```
'm' → Enter device menu
'f' → Frequency targeting menu
'r' → Resume reconnaissance (keeps discovered devices)
'e' → Exit menu / resume current mode (without clearing state)
'b' → Reboot device (clears all RAM data)
's' → Show summary
'i' → Reset reason & health info
'q' → Toggle quiet/verbose mode
't' → Show API token
```

### Meshtastic CLI reference
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

## **Interpreting Results**

### If you see routing packets (14 bytes):
**Hardware working.** Next:
1. Generate user messages (not just routing) from a test device
2. Target the correct frequency for user data
3. Wait for longer packets (encrypted content)

### If you see nothing at all:
**Configuration mismatch.** Check:
1. Antenna connected?
2. Right frequency range (US vs EU)?
3. Test device actually transmitting?
4. Both devices in same room?

### If you see packets but no PSK success:
**Partial success.** Investigate:
1. Are packets long enough? (Need >20 bytes for decryption)
2. Is the test device using a default PSK?
3. Are you capturing routing packets vs user messages?

---

## 🖥️ **OLED Display Troubleshooting**

### Problem: OLED blank or "No OLED detected"

Check serial output (or web UI on V4):
```
[DISPLAY] Scanning I2C bus...
[DISPLAY]   Address 0x3C: FOUND!
```
or
```
[DISPLAY]   Address 0x3C: not found (error 2)
```

**I2C Error Codes**
- `error 0`: Success (device found)
- `error 2`: Address not acknowledged (no device at that address)
- `error 3`: Data not acknowledged
- `error 4`: Other error
- `error 5`: Timeout

**Common Fixes**
- **Physical connection**: Reseat the board in its socket; check solder joints if assembled yourself
- **Vext rail** (Heltec only): Heltec boards power the OLED via a GPIO-controlled rail. If the rail doesn't come up, the display stays blank. Serial output will show `[DISPLAY] Vext ON` if it toggled correctly.
- **No OLED populated**: Some boards ship without the OLED pre-soldered. The firmware continues without display — check the web UI instead.
- **Board not supported**: Heltec V1/V2 (non-S3) boards are not supported and will not work with this firmware.

### Button Not Responding

1. **Check GPIO 0**: PRG button should be on GPIO 0
2. **Monitor serial**: Look for `[BUTTON] Button pressed` messages
3. **Test timing**: Short press < 3s, long press ≥ 3s

---

## 💾 **SD Card Logging**

SD card logging works automatically on the **T3-S3** and **T-Beam Supreme**. Insert a FAT32-formatted MicroSD card before booting — the firmware creates `/logs/` and starts writing on the first received packet.

**Heltec V3 and V4** do not have SD card slots. All data is RAM-only; export before rebooting.

### SD card not detected

Check serial output:
```
[SD] ✅ SD card detected: SDHC
[SD] Card size: 3756 MB
```
or
```
[SD] ⚠️  No SD card detected
[SD]   Insert SD card for data logging capability
[SD]   Device will continue without logging
```

If the card is not detected:
- Verify it is FAT32 formatted (not exFAT or NTFS)
- Use ≤4 GB cards on T3-S3; T-Beam supports up to 32 GB
- Push the card in firmly until it clicks
- Try a different card — some generic cards fail to initialize

---

## 🔐 **API Authentication Issues**

### Problem: "401 Unauthorized" Response

**Symptoms:**
- API returns `{"status":"error","message":"Authentication required"}`
- Can view devices but can't clear them or use replay

**Solution:**

The web UI handles authentication automatically — if you're accessing the device through a browser, the token is managed for you.

For direct API access (curl, scripts), you need the token:

1. Check serial output at boot (Heltec V3 / T3-S3 / T-Beam):
   ```
   🔐 API Security Enabled
     Token: a1b2c3d4e5f6789012345678abcdef01
     Header: X-API-Token
   ```
2. **Heltec V4** (no serial): open the web UI → Settings tab → the token is shown in the system info panel.

3. Add the token to your requests:
   ```bash
   curl -X POST http://192.168.4.1/api/devices/clear \
        -H "X-API-Token: YOUR_TOKEN_HERE"
   ```

### Problem: Lost API Token

**Solution:** Reboot the device. The token persists across reboots and is displayed at each startup (serial or Settings tab).

### Problem: Can't Connect to WiFi AP

**Symptoms:**
- Wrong password error

**Solution:** The password is always the same suffix as the SSID:
- SSID: `LoRa-A1B2C3` → Password: `recon-A1B2C3`

### Protected vs Public Endpoints

| Endpoint Type | Auth Required | Example |
|--------------|---------------|----------|
| Read-only | No | `/api/devices`, `/api/status` |
| Destructive | Yes | `/api/devices/clear`, `/api/replay/transmit` |
| Configuration | Yes | `/api/wifi/configure`, `/api/firmware/upload` |

---

## 🎯 **Quick Reception Test**

Use this to verify the radio is working before field deployment:

1. **Lock to a known-good frequency**: Press `f` on serial → Select `Meshtastic_LF_906`
2. **Hold a Meshtastic device within 1 foot of the ESP32**
3. **Send a message**: via the Meshtastic app or CLI
4. **Watch for any `[PACKET]` message** in serial output (or in the web UI Packets tab)

**If you see a packet** → Radio working, configure and deploy
**If you see nothing** → Check antenna, region setting, and that the test device is actually transmitting
