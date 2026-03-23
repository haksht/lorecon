# OTA Firmware Update Guide

**Over-The-Air updates let you flash new firmware without USB cable!**

---

## Quick Reference

```powershell
# Step 1: Build new firmware
pio run

# Step 2: Upload web UI (only if HTML/CSS/JS changed)
pio run --target uploadfs

# Step 3: Upload firmware via web browser
# Navigate to: http://192.168.4.1
# Settings tab → Upload firmware.bin
```

---

## Detailed Walkthrough

### **First Time Setup (Via USB)**

You need USB cable **once** to get OTA capability on the device:

```bash
# Build firmware
pio run

# Upload web UI to LittleFS
pio run --target uploadfs

# Flash firmware via USB (includes OTA code)
pio run --target upload
```

**Result:** Device now has OTA capability. No USB needed for future updates!

---

### **Subsequent Updates (Via WiFi)**

#### **Step 1: Build New Firmware**

```powershell
# Make your code changes, then build
pio run

# Output location: .pio\build\default\firmware.bin
```

**Result:** Fresh `firmware.bin` ready for upload.

#### **Step 2: Connect to Device WiFi**

- **Network Name:** `LoRa-XXYYZZ` (unique per device, based on MAC)
- **Password:** `recon-XXYYZZ` (same suffix as SSID — check serial output for exact credentials)
- **Device IP:** 192.168.4.1 (fixed)

#### **Step 3: Open Web Interface**

Using phone, tablet, or laptop browser:
- Navigate to: `http://192.168.4.1`
- Or use mDNS: `http://esp32-lora.local`

#### **Step 4: Upload Firmware**

1. Click **Settings** tab (⚙️ icon at top)
2. Scroll to **"Firmware Update (OTA)"** section
3. Click **"Choose File"** button
4. Navigate to the PlatformIO build output folder: `.pio/build/<env>/`
5. Select `firmware.bin` file
6. Click **"Upload Firmware"** button

**What Happens:**
- Progress bar shows 0% → 100%
- Device validates firmware
- If valid: Reboots automatically (10 seconds)
- Page reloads automatically
- If invalid: Device stays on old firmware (safe!)

#### **Step 5: Verify Update**

After reboot:
- Check serial monitor for version info
- Verify new features work
- Device should reconnect to WiFi automatically

---

## Understanding Dual-Partition OTA

### **How It Works**

ESP32 has **2 firmware partitions**:

```
┌────────────────────────────────────┐
│ Partition 0: Current Firmware      │ ← Device boots from here
├────────────────────────────────────┤
│ Partition 1: New Firmware Slot     │ ← OTA writes here
└────────────────────────────────────┘
```

**Upload Process:**
1. Device running on Partition 0
2. OTA writes new firmware to Partition 1
3. On next boot, ESP32 tries Partition 1
4. If boot succeeds → Partition 1 becomes active
5. If boot fails → Auto-rollback to Partition 0 (safe!)

**Why This Matters:**
- ❌ Can't brick the device (old firmware always available)
- ✅ Safe to experiment with code changes
- ✅ Bad firmware automatically reverts
- ✅ No "recovery mode" needed

---

## Troubleshooting

### **"Upload Failed: Network Error"**

**Causes:**
- Lost WiFi connection during upload
- Device rebooted unexpectedly
- Browser tab closed during upload

**Solution:**
1. Reconnect to device WiFi
2. Refresh page
3. Retry upload
4. Device is still on old firmware (safe)

### **"Only .bin files are supported"**

**Cause:** Selected wrong file type

**Solution:**
- Must select `firmware.bin` (not `.elf`, `.hex`, or `.ino`)
- Located in: `.pio\build\default\firmware.bin`

### **"Firmware file is too large (max 2MB)"**

**Cause:** Firmware exceeds partition size

**Solution:**
1. Check current size: Look at build output
   ```
   Flash: [===       ]  32.4% (used 1083801 bytes from 3342336 bytes)
   ```
2. If >2MB: Reduce features or enable optimization
3. Current build: ~1MB (plenty of room)

### **Upload Succeeds But Device Won't Boot**

**Symptoms:**
- Progress bar reaches 100%
- Device reboots
- Doesn't come back online

**What Happened:**
- New firmware has a boot-blocking bug
- ESP32 automatically rolled back to old firmware
- Device is back on previous version (safe!)

**Solution:**
1. Connect via USB serial monitor
2. Check boot messages for errors
3. Fix the bug in your code
4. Rebuild and retry OTA

### **Can't Find firmware.bin File**

**File Location:**
```
<project-root>/.pio/build/<env>/firmware.bin
```
Where `<env>` is your board environment (e.g., `heltec_v3`, `t3_s3`, `tbeam_supreme`).

**If Missing:**
- Run `pio run` first to build firmware
- Check for compilation errors
- Verify PlatformIO environment is correct

---

## Best Practices

### **✅ DO:**
- Build firmware before uploading (`pio run`)
- Test on bench before field deployment
- Keep USB cable handy as backup
- Monitor serial output during first boot
- Ensure stable power supply during upload

### **❌ DON'T:**
- Upload during active packet capture (may corrupt)
- Disconnect power during upload
- Close browser tab during upload
- Upload `.elf` or other file types
- Skip testing after upload

---

## Advanced: OTA From Command Line

### **Using cURL (Linux/Mac/WSL)**

```bash
# Upload firmware via command line
curl -X POST \
  -F "firmware=@.pio/build/default/firmware.bin" \
  http://192.168.4.1/api/firmware/upload
```

### **Using PowerShell (Windows)**

```powershell
# Upload firmware via PowerShell
$uri = "http://192.168.4.1/api/firmware/upload"
$filePath = ".pio/build/default/firmware.bin"

$multipartContent = [System.Net.Http.MultipartFormDataContent]::new()
$fileStream = [System.IO.FileStream]::new($filePath, [System.IO.FileMode]::Open)
$fileContent = [System.Net.Http.StreamContent]::new($fileStream)
$multipartContent.Add($fileContent, "firmware", "firmware.bin")

$client = [System.Net.Http.HttpClient]::new()
$response = $client.PostAsync($uri, $multipartContent).Result

Write-Host $response.Content.ReadAsStringAsync().Result
$fileStream.Close()
```

### **Using Python**

```python
import requests

url = "http://192.168.4.1/api/firmware/upload"
files = {'firmware': open('.pio/build/default/firmware.bin', 'rb')}
response = requests.post(url, files=files)
print(response.json())
```

---

## FAQ

**Q: Do I need to upload web UI files every time?**  
A: No! Only when you change HTML/CSS/JavaScript files. Firmware changes don't require `uploadfs`.

**Q: Can I OTA from my phone?**  
A: Yes! Connect phone to device WiFi, open browser, use Settings tab. Works great!

**Q: What if I'm in the middle of a field test?**  
A: OTA is designed for this! No need to retrieve device. Update remotely via WiFi.

**Q: How long does OTA take?**  
A: ~30 seconds for 1MB firmware. Progress bar shows real-time status.

**Q: Can I revert to previous firmware?**  
A: If new firmware fails to boot, ESP32 auto-reverts. For manual revert, flash old firmware via USB.

**Q: Does OTA work with battery power?**  
A: Yes, but ensure battery is charged (>50%). Low battery during upload can cause issues.

---

## File Locations Quick Reference

```
Firmware Binary:
  .pio/build/<env>/firmware.bin              # Upload this via OTA

Web UI Source:
  data/webapp/                               # Edit these files
    index.html
    css/style.css
    js/app.js

Build Commands:
  pio run                                    # Build firmware
  pio run --target uploadfs                  # Upload web UI
  pio run --target upload                    # Flash via USB
```

---

## Summary

1. **First time:** USB flash to get OTA capability
2. **Every update:** Build firmware → Upload via web browser
3. **Safe:** Dual-partition prevents bricking
4. **Fast:** 30 seconds, no cables needed
5. **Works anywhere:** Phone, laptop, field deployment

**You now have wireless firmware updates.**
