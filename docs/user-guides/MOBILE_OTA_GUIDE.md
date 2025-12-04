# Mobile OTA Guide (iPhone/iPad)

## The Problem

iPhone and iPad cannot browse to files on your computer due to iOS security restrictions. When you tap the file picker in Safari, you can only select files from:
- iCloud Drive
- Photos
- Files app (local storage)
- Other iOS-compatible sources

## Solutions for iPhone/iPad Users

### Option 1: Use AirDrop (Easiest - Mac Users)

1. **Build firmware on your Mac:**
   ```bash
   cd ~/lora/esp32-sniffer
   pio run
   ```

2. **Locate the firmware file:**
   - Path: `.pio/build/default/firmware.bin`
   - In Finder, press Cmd+Shift+G and paste the full path

3. **AirDrop to iPhone:**
   - Right-click `firmware.bin` → Share → AirDrop
   - Select your iPhone
   - File will appear in Files app under "Downloads"

4. **Upload via Web UI:**
   - Connect to ESP32 WiFi (192.168.4.1)
   - Open Settings tab
   - Tap "Choose File"
   - Navigate to Files → Downloads
   - Select `firmware.bin`
   - Tap "Upload Firmware"

### Option 2: Use iCloud Drive

1. **Upload firmware to iCloud Drive from computer:**
   - Build firmware: `pio run`
   - Copy `.pio/build/default/firmware.bin` to iCloud Drive folder
   - Wait for sync (check iCloud icon)

2. **Download on iPhone:**
   - Open Files app
   - Go to iCloud Drive
   - Tap `firmware.bin` → Download if needed

3. **Upload via Web UI:**
   - Settings tab → Choose File
   - Files → iCloud Drive → `firmware.bin`

### Option 3: Use Cloud Storage (Dropbox/Google Drive)

1. **Upload from computer:**
   - Build firmware: `pio run`
   - Upload `.pio/build/default/firmware.bin` to Dropbox/Drive

2. **Download to iPhone:**
   - Open Dropbox/Drive app
   - Tap `firmware.bin` → "Save to Files"
   - Choose location (On My iPhone or iCloud Drive)

3. **Upload via Web UI:**
   - Settings → Choose File → Browse to saved location

### Option 4: Email Yourself (Small Files)

1. **Email the firmware:**
   - Build firmware: `pio run`
   - Email `.pio/build/default/firmware.bin` to yourself as attachment

2. **Download on iPhone:**
   - Open Mail app
   - Long-press attachment → Share → Save to Files

3. **Upload via Web UI:**
   - Settings → Choose File → Files → Downloads

### Option 5: Use Computer for OTA (No iPhone)

If file transfer is too complicated, just use your computer:

```powershell
# PowerShell (Windows)
$firmwarePath = ".pio\build\default\firmware.bin"
curl.exe -X POST -F "firmware=@$firmwarePath" http://192.168.4.1/api/ota/upload
```

```bash
# Mac/Linux Terminal
curl -X POST -F "firmware=@.pio/build/default/firmware.bin" http://192.168.4.1/api/ota/upload
```

## Why Can't iPhone See My Computer?

iOS uses a sandboxed security model:
- **No filesystem browsing**: Can't access PC/Mac files directly
- **App-specific storage**: Each app has isolated storage
- **Cloud/Share only**: Must transfer via AirDrop, cloud, or cable

Android devices can browse network shares and USB storage, but iOS cannot.

## Troubleshooting

### "Configuration unavailable" in Settings Tab

**Symptom:** Settings tab shows loading spinner then displays "Configuration unavailable" or error message.

**Possible Causes:**
1. **Not connected to ESP32 WiFi:** Check you're on the "ESP32-LORA-RECON" network
2. **Network timeout:** iPhone may have slow WiFi connection to ESP32
3. **API endpoint error:** Backend may be failing to generate config JSON
4. **CORS issue:** Unlikely but possible on iOS Safari

**Solutions:**
1. **Check WiFi connection:**
   - Settings app → WiFi
   - Ensure connected to ESP32 network (192.168.4.1)
   - Try disabling cellular data to force WiFi

2. **Check browser console (Safari):**
   - Enable Safari Developer Mode: Settings → Safari → Advanced → Web Inspector
   - Connect iPhone to Mac
   - Safari (Mac) → Develop → [Your iPhone] → 192.168.4.1
   - Check console for error messages

3. **Test API endpoint manually:**
   - Open Safari on iPhone
   - Navigate to: `http://192.168.4.1/api/config/system`
   - Should see JSON response
   - If "404 Not Found", firmware may not have endpoint
   - If timeout, WiFi connection issue

4. **Try desktop browser first:**
   - Connect computer to ESP32 WiFi
   - Open Chrome/Firefox → 192.168.4.1
   - Check Settings tab works on desktop
   - If desktop works, it's iOS-specific

5. **Check serial monitor (if accessible):**
   - Look for errors when loading Settings tab
   - May show API handler crashes or JSON errors

### File Upload Fails

**Symptom:** Upload starts but fails partway through.

**Solutions:**
1. **Check file size:** Must be < 2MB (firmware is typically 1.2-1.5MB)
2. **Check filename:** Must end in `.bin`
3. **Stay on screen:** Don't switch apps during upload (iOS may pause)
4. **Disable Auto-Lock:** Settings → Display → Auto-Lock → Never (during upload)

### AirDrop Not Working

**Solutions:**
1. **Enable AirDrop on iPhone:** Control Center → Long-press network card → AirDrop → Everyone
2. **Enable Bluetooth/WiFi:** AirDrop requires both
3. **Check Mac AirDrop:** Finder → Go → AirDrop → "Allow me to be discovered by: Everyone"

## Quick Reference

| Method | Speed | Difficulty | Requirements |
|--------|-------|-----------|--------------|
| **AirDrop** | ⚡ Fast | ⭐ Easy | Mac + iPhone, Bluetooth/WiFi |
| **iCloud Drive** | 🐢 Slow | ⭐⭐ Medium | iCloud account, sync time |
| **Dropbox/Drive** | 🐢 Slow | ⭐⭐ Medium | Cloud account, app installed |
| **Email** | 🐢 Slow | ⭐ Easy | Email account |
| **Computer CLI** | ⚡ Fast | ⭐⭐⭐ Advanced | Terminal/PowerShell |

**Recommended:** AirDrop for Mac users, Email for quick tests, Computer CLI for frequent updates.

## See Also

- [OTA_GUIDE.md](OTA_GUIDE.md) - Comprehensive OTA documentation
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) - General troubleshooting guide
- [GETTING_STARTED.md](../../GETTING_STARTED.md) - Initial setup instructions
