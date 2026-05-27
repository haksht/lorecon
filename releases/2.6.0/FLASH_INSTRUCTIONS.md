# LoRecon 2.6.0 — Flash Instructions

Pre-compiled binaries for **four boards**: Heltec WiFi LoRa 32 V3, Heltec WiFi LoRa 32 V4 (GPS), LilyGO T3-S3, and LilyGO T-Beam Supreme.

---

## Pick Your Board

| Board | Folder | Port (typical) | Flash |
|-------|--------|----------------|-------|
| Heltec WiFi LoRa 32 V3 | `heltec_v3/` | COM3 / /dev/ttyUSB0 | 8 MB |
| Heltec WiFi LoRa 32 V4 (GPS) | `heltec_v4/` | COM12 / /dev/ttyACM0 | 8 MB |
| LilyGO T3-S3 V1.2/V1.3 | `t3_s3/` | COM9 / /dev/ttyACM0 | 4 MB |
| LilyGO T-Beam Supreme | `tbeam_supreme/` | COM11 / /dev/ttyACM0 | 8 MB |

---

## Method 1: Included Flash Script (EASIEST)

### Linux / macOS / Windows git-bash

```bash
# Install esptool first (one-time)
pip install esptool

# Flash your board (port auto-detected)
./flash.sh heltec_v3
./flash.sh heltec_v4
./flash.sh t3_s3
./flash.sh tbeam_supreme

# Or specify port explicitly
./flash.sh heltec_v3 /dev/ttyUSB0
./flash.sh heltec_v4 COM12
./flash.sh tbeam_supreme COM11
```

### Windows CMD

```bat
pip install esptool

flash.bat heltec_v3
flash.bat heltec_v4
flash.bat t3_s3
flash.bat tbeam_supreme COM11
```

### Windows PowerShell

```powershell
pip install esptool

.\flash.ps1 heltec_v3
.\flash.ps1 heltec_v4
.\flash.ps1 t3_s3
.\flash.ps1 tbeam_supreme COM11
```

---

## Method 2: esptool Manually (Single Command)

Each board folder contains a `full.bin` — a merged image that flashes at offset `0x0`:

### Heltec V3
```bash
esptool.py --chip esp32s3 --port COM3 --baud 921600 \
  write_flash --flash_size 8MB 0x0 heltec_v3/full.bin
```

### Heltec V4
```bash
esptool.py --chip esp32s3 --port COM12 --baud 921600 \
  write_flash --flash_size 8MB 0x0 heltec_v4/full.bin
```

### T3-S3
```bash
esptool.py --chip esp32s3 --port COM9 --baud 921600 \
  write_flash --flash_size 4MB 0x0 t3_s3/full.bin
```

### T-Beam Supreme
```bash
esptool.py --chip esp32s3 --port COM11 --baud 921600 \
  write_flash --flash_size 8MB 0x0 tbeam_supreme/full.bin
```

---

## Method 3: esptool Manually (Separate Files)

Addresses for manual multi-file flash:

| File | Heltec V3 | Heltec V4 | T3-S3 | T-Beam Supreme |
|------|-----------|-----------|-------|----------------|
| `bootloader.bin` | `0x0` | `0x0` | `0x0` | `0x0` |
| `partitions.bin` | `0x8000` | `0x8000` | `0x8000` | `0x8000` |
| `firmware.bin` | `0x10000` | `0x10000` | `0x10000` | `0x10000` |
| `littlefs.bin` | `0x670000` | `0x670000` | `0x290000` | `0x300000` |

---

## Troubleshooting

**Flash fails / can't connect:**
1. Hold **BOOT** button while plugging in USB, then run the flash command
2. Try a lower baud rate: replace `921600` with `115200`
3. Erase flash first: `esptool.py --chip esp32s3 --port <PORT> erase_flash`

**T3-S3 / T-Beam Supreme not detected (native USB):**
- These boards use native USB-CDC — they appear as a different COM port in bootloader mode
- Hold BOOT + press RST, release RST first, then release BOOT
- A new COM port should appear — use that port

**Finding your port:**
- Windows: Device Manager → Ports (COM & LPT)
- Linux: `ls /dev/ttyUSB* /dev/ttyACM*`
- macOS: `ls /dev/tty.usb*`

---

## After Flashing

1. **Power-cycle** the device (unplug and replug USB)
2. **Connect to WiFi AP:** `LoRa-XXYYZZ` (XXYYZZ = last 3 bytes of MAC)
3. **Password:** `recon-XXYYZZ` (matches SSID suffix)
4. **Open browser:** http://192.168.4.1

---

## Hardware Compatibility

| Board | Supported | Notes |
|-------|-----------|-------|
| Heltec WiFi LoRa 32 V3 | ✅ | No SD card, no GPS |
| Heltec WiFi LoRa 32 V4 | ✅ | No SD card, GPS (L76K) — **must use heltec_v4 binary** |
| LilyGO T3-S3 V1.2/V1.3 | ✅ | SD card logging |
| LilyGO T-Beam Supreme | ✅ | SD card + GPS position logging |
| Other ESP32 boards | ❌ | Requires recompilation |

---

## Support

- Documentation: `GETTING_STARTED.md` in main repo
- Issues: https://github.com/haksht/lorecon/issues
