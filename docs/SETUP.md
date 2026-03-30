# Setup Guide

Flash the firmware, boot the device, connect to WiFi. This guide covers the full path from download to a working device.

---

## What you need

- One of the [supported boards](HARDWARE.md)
- USB-C data cable (not charge-only)
- 902-928 MHz antenna — usually ships with the board
- Python 3 and esptool: `pip install esptool`

---

## Download firmware

Go to [Releases](https://github.com/tiarno/esp32-sniffer/releases/latest) and download `esp32-lora-sniffer-vX.X.X-binaries.zip`. Extract it. The zip contains `flash.sh`, `flash.bat`, and `flash.ps1` at the top level, plus one folder per board — each folder contains a `full.bin` merged image that flashes at offset `0x0`.

---

## Pick your board

| Board | Folder | Typical port | Flash size |
|-------|--------|-------------|------------|
| Heltec WiFi LoRa 32 V3 | `heltec_v3/` | COM3 / /dev/ttyUSB0 | 8 MB |
| Heltec WiFi LoRa 32 V4 (with GPS) | `heltec_v4/` | COM12 / /dev/ttyACM0 | 8 MB |
| LilyGO T3-S3 V1.2/V1.3 | `t3_s3/` | COM9 / /dev/ttyACM0 | 4 MB |
| LilyGO T-Beam Supreme | `tbeam_supreme/` | COM11 / /dev/ttyACM0 | 8 MB |

The **Heltec V3** and **V4 without GPS** use the same `heltec_v3` binary. Only use `heltec_v4` if your V4 board has the L76K GPS module attached.

---

## Flash the firmware

Open a terminal in the extracted folder and run the script for your OS. Port is auto-detected if you omit it.

**Windows PowerShell:**
```powershell
.\flash.ps1 heltec_v3
.\flash.ps1 heltec_v3 COM3        # specify port explicitly
.\flash.ps1 heltec_v4 COM12
.\flash.ps1 t3_s3 COM9
.\flash.ps1 tbeam_supreme COM11
```

**Windows CMD:**
```bat
flash.bat heltec_v3
flash.bat heltec_v3 COM3
```

**Linux / macOS / Git Bash:**
```bash
chmod +x flash.sh
./flash.sh heltec_v3
./flash.sh heltec_v3 /dev/ttyUSB0  # specify port explicitly
```

Flashing takes 30-90 seconds. The script prints `SUCCESS` when done.

### Manual flash (esptool directly)

If you prefer to run esptool yourself:
```bash
python -m esptool --chip esp32s3 --port COM3 --baud 921600 \
  write_flash --flash_size 8MB 0x0 heltec_v3/full.bin
```
Adjust `--port`, `--flash_size`, and the binary path for your board.

---

## Bootloader mode

If the flash script can't connect, put the board into bootloader mode manually:

1. Hold the **BOOT** button
2. Press and release **RST** (or plug in USB while holding BOOT)
3. Release **BOOT**
4. A new COM port appears — flash to that port

T3-S3 and T-Beam Supreme use native USB; they re-enumerate as a different port in bootloader mode.

---

## Finding your COM port

- **Windows:** Device Manager → Ports (COM & LPT)
  - Heltec V3: CP210x (Silicon Labs)
  - Heltec V4 / T3-S3 / T-Beam: USB Serial Device (native USB)
- **Linux:** `ls /dev/ttyUSB* /dev/ttyACM*`
- **macOS:** `ls /dev/tty.usb*`

---

## First boot

After flashing, unplug and replug USB to power-cycle the device.

The OLED shows a welcome screen, then the device starts scanning. Within a few seconds it creates a WiFi access point:

```
SSID:     LoRa-XXYYZZ
Password: recon-XXYYZZ
```

The password always matches the suffix in the SSID — if you see `LoRa-A1B2C3`, the password is `recon-A1B2C3`. 

---

## WiFi provisioning

On first boot with no stored credentials, a setup wizard runs.

### Recommended: connect to your phone hotspot

1. Connect to `LoRa-XXYYZZ` from your phone
2. A captive portal opens automatically (like hotel WiFi)
3. Enter your phone's hotspot SSID and password
4. The device reboots and connects to your hotspot
5. Your phone can now reach the web UI at the IP shown on the OLED

This keeps your phone on cellular while you use the sniffer.

### Alternative: use the device as AP (no setup required)

Skip provisioning entirely. Connect to `LoRa-XXYYZZ` and browse to `http://192.168.4.1`. You lose internet access on that device while connected, but there's no setup step.

---

## Verify it's working

1. Connect to the device (AP or hotspot)
2. Open `http://192.168.4.1` (AP mode) or `http://lora-xxyyzz.local` (hotspot mode)
3. The web UI loads with 7 tabs — Info, Devices, Packets, Frequency, Network, Stats, Settings
4. The Info tab shows scanning status and packet counts

The device starts scanning 26 LoRa configurations immediately on boot. You should see the frequency cycling on the OLED and in the Info tab within seconds.

---

## Troubleshooting

**Flash fails:**
- Try bootloader mode (BOOT + RST, see above)
- Lower baud: replace `921600` with `115200` in the flash script
- Erase first: `python -m esptool --chip esp32s3 --port COM3 erase_flash`

**Device not found after flashing:**
- Power-cycle (unplug + replug)
- Wait 5 seconds for WiFi to start

**Web UI doesn't load:**
- Use `http://` not `https://`
- Disable cellular data on phone while on the device's AP
- Try the mDNS address: `http://lora-xxyyzz.local`

See [TROUBLESHOOTING.md](TROUBLESHOOTING.md) for more.
