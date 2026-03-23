# Long-Duration Test Guide (Week+)

**Last Updated:** March 2026
**Firmware Version:** 2.3.1

---

## ✅ Ready for Week-Long Test

The firmware has been hardened for unattended long-duration operation.

### Stability Features

| Feature | Status | Notes |
|---------|--------|-------|
| **Serial Noise Protection** | ✅ Fixed | Double-Enter activation + 5-min auto-deactivate |
| **Mode Persistence** | ✅ Working | NVS stores mode across reboots |
| **Watchdog** | ✅ Enabled | 30-second timeout, auto-reset on hang |
| **WiFi Reconnect** | ✅ Enabled | Auto-reconnects with 5s throttle |
| **AP Fallback** | ✅ Always On | 192.168.4.1 accessible even if hotspot drops |
| **Memory Management** | ✅ Safe | Fixed-size arrays, no dynamic allocation in loops |
| **Device Archival** | ✅ Enabled | LRU eviction at 50 devices |
| **Brownout Protection** | ✅ Disabled | Prevents reboot on USB power fluctuation |

---

## 🔧 Recommended Setup for Week-Long Test

### Physical Setup
1. **Power**: Use a quality USB power supply, not a laptop USB port
2. **USB Cable**: Short, high-quality cable reduces electrical noise
3. **Serial**: Disconnect serial monitor if not needed (reduces noise source)
4. **Antenna**: Ensure properly connected for your frequency band
5. **Location**: Stable temperature, away from RF interference sources

### Mode Selection
Choose ONE of these modes before leaving device:

**Option A: Reconnaissance Mode** (recommended for discovery)
```
- Scans all 26 frequencies continuously
- 5-minute cycle time
- Discovers new devices automatically
- Mode persists across reboots
```

**Option B: Frequency Targeting Mode** (recommended for monitoring known activity)
```
1. Press Enter twice to activate serial
2. Press 'f' for frequency targeting menu
3. Select frequency with known activity
4. Mode persists across reboots
```

### Serial Console State
For unattended operation, the serial console should be **deactivated**:
- Console auto-deactivates after 5 minutes of inactivity
- If you used serial, just wait 5 minutes before leaving
- Or close the serial monitor entirely

---

## 📊 Monitoring During Test

### Web UI Access
- **Primary**: http://[hotspot-ip] (check router for IP)
- **Fallback**: http://192.168.4.1 (connect to device's AP: `LoRa-XXYYZZ`)
- **mDNS**: http://lora-xxyyzz.local

### Key Metrics to Watch
- **Devices Tab**: Count should grow over time
- **Packets Tab**: Should show captured packets (if any)
- **Info Tab**: 
  - Uptime should match wall-clock time
  - Free heap should stay >100KB
  - Battery voltage if on battery

### API Endpoints for Scripted Monitoring
```bash
# Quick status check
curl http://192.168.4.1/api/status

# Device count
curl http://192.168.4.1/api/devices | jq '.count'

# Check mode hasn't changed
curl http://192.168.4.1/api/status | jq '.mode'
```

---

## ⚠️ Known Limitations

### Things That May Happen
1. **WiFi Hotspot Drops**: Device will reconnect automatically. AP fallback always works.
2. **No SD Card Logging**: Without SD card, packet logs are in-memory only (lost on reboot)
3. **Device Buffer Full**: At 50 devices, oldest are evicted (LRU)
4. **Packet Queue Overflow**: At 100 queued packets, drops occur (logged in UI)

### Things That Should NOT Happen
- ❌ Spontaneous mode changes (fixed in 2.2.1)
- ❌ Serial noise triggering commands (fixed in 2.2.1)
- ❌ Watchdog resets (unless actual hang)
- ❌ Memory leaks (no dynamic allocation in hot paths)

---

## 🔍 Post-Test Analysis

After the test, check:

1. **Reset Reason** (on next serial connect):
   ```
   [INFO] Reset reason: Power-on (code 1)  ← Good
   [WARN] Reset reason: Task watchdog      ← Bad - indicates hang
   ```

2. **Statistics**:
   - Total packets captured
   - Number of unique devices
   - Any dropped packets (queue overflow)

3. **Mode State**:
   - Should still be in the mode you set
   - If changed, check for `[SERIAL-CMD]` in any logs

---

## 📝 Test Log Template

```
Start Date/Time: _______________
Firmware Version: 2.3.1
Mode: □ Reconnaissance  □ Frequency Targeting (config #___)
Power Source: □ USB Wall  □ USB Battery  □ Battery

Day 1 Check:
  - Devices: ___  Packets: ___  Mode: ___________
  
Day 3 Check:  
  - Devices: ___  Packets: ___  Mode: ___________
  
Day 7 Check:
  - Devices: ___  Packets: ___  Mode: ___________
  
End Date/Time: _______________
Total Uptime: _______________
Final Device Count: ___________
Any Issues Noted: _____________
```
