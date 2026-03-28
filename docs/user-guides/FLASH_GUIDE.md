# Flashing Pre-built Firmware

No development environment needed. Download the release zip, run the flash script, done.

## Quickest: Use the included flash scripts

Download and extract `esp32-lora-sniffer-vX.X.X-binaries.zip` from [Releases](https://github.com/tiarno/esp32-sniffer/releases/latest).

**Requires:** `pip install esptool`

**Windows (CMD or PowerShell):**
```
flash.bat heltec_v3 COM3
flash.bat t3_s3 COM9
flash.bat tbeam_supreme COM11
```

**Linux / Mac:**
```bash
chmod +x flash.sh
./flash.sh heltec_v3 /dev/ttyUSB0
./flash.sh t3_s3 /dev/ttyACM0
./flash.sh tbeam_supreme /dev/ttyACM0
```

Omit the port to auto-detect (works if only one device is plugged in).

### Finding your COM port

- **Windows:** Device Manager → Ports (COM & LPT)
  - Heltec V3: appears as CP210x (Silicon Labs)
  - T3-S3 / T-Beam: appears as USB Serial Device
- **Linux:** `ls /dev/ttyUSB* /dev/ttyACM*`
- **Mac:** `ls /dev/tty.usb*`

---

## Option 2: esptool command line (manual)

### Install esptool
```bash
pip install esptool
```

### Flash addresses by board

| Board | bootloader | partitions | firmware | littlefs |
|-------|-----------|------------|----------|----------|
| heltec_v3 / heltec_v4 | 0x0 | 0x8000 | 0x10000 | 0x670000 |
| t3_s3 | 0x0 | 0x8000 | 0x10000 | 0x290000 |
| tbeam_supreme | 0x0 | 0x8000 | 0x10000 | 0x300000 |

### Flash command (use your board's addresses from the table above)

**Windows:**
```
python -m esptool --chip esp32s3 --port COM3 --baud 921600 write-flash --flash-size 8MB 0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin 0x670000 littlefs.bin
```

**Linux / Mac:**
```bash
python3 -m esptool --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 write-flash --flash-size 8MB 0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin 0x670000 littlefs.bin
```

Or use the merged `full.bin` (single file, includes all of the above):
```bash
python3 -m esptool --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 write-flash 0x0 full.bin
```

---

## Option 3: Web flasher (Chrome/Edge only)

Use [Adafruit WebSerial ESPTool](https://adafruit.github.io/Adafruit_WebSerial_ESPTool/) in Chrome or Edge:

1. Connect your board via USB
2. Click Connect and select your device's COM port
3. Flash each file at the correct address (see table above)

---

## Troubleshooting

**Device not detected?**
- Install [CP210x drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) (Heltec V3) or [CH340 drivers](http://www.wch-ic.com/downloads/CH341SER_ZIP.html)
- Try a different USB cable (some are charge-only)
- Hold BOOT button while plugging in USB

**Flash failed?**
- Hold BOOT, press RST, release BOOT (enters bootloader mode manually)
- Try lower baud: change `921600` to `115200`

---

## After flashing

1. Unplug and replug USB (power cycle)
2. Device creates WiFi: `LoRa-XXXXXX` (unique per device)
3. Connect your phone to that network — setup page opens automatically
4. Enter your hotspot credentials
5. Device restarts and connects to your hotspot
6. Open browser to the IP shown on the OLED or serial output
