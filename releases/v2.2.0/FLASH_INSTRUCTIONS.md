# ESP32 LoRa Sniffer v2.2.0 - Flash Instructions

Pre-compiled binaries for **Heltec WiFi LoRa 32 V3** (ESP32-S3 + SX1262).

## Files Included

| File | Size | Description |
|------|------|-------------|
| `esp32-lora-sniffer-v2.2.0-full.bin` | **8 MB** | **Complete image (EASIEST)** - flash at 0x0 |
| `bootloader.bin` | 15 KB | ESP32-S3 bootloader (0x0) |
| `partitions.bin` | 3 KB | Partition table (0x8000) |
| `firmware.bin` | 1.1 MB | Main application (0x10000) |
| `littlefs.bin` | 1.5 MB | Web UI files (0x670000) |

---

## Method 1: Single File Flash (EASIEST)

The merged binary includes everything - just one command:

```bash
esptool.py --chip esp32s3 --port COM3 --baud 921600 write_flash 0x0 esp32-lora-sniffer-v2.2.0-full.bin
```

**That's it!** Replace `COM3` with your port.

---

## Method 2: Using esptool.py (Separate Files)

### Install esptool
```bash
pip install esptool
```

### Flash All Files (One Command)
```bash
esptool.py --chip esp32s3 --port COM3 --baud 921600 \
  write_flash \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin \
  0x670000 littlefs.bin
```

**Windows users:** Replace `COM3` with your port (check Device Manager).  
**Linux/Mac users:** Replace `COM3` with `/dev/ttyUSB0` or `/dev/tty.usbserial-*`.

### Finding Your Port
- **Windows:** Device Manager → Ports (COM & LPT) → "USB-SERIAL CH340" or "CP210x"
- **Linux:** `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`
- **Mac:** `ls /dev/tty.usb*`

### Flash Troubleshooting
If flashing fails:
1. Hold the **BOOT** button while plugging in USB
2. Try lower baud rate: `--baud 115200`
3. Erase flash first: `esptool.py --chip esp32s3 --port COM3 erase_flash`

## Method 2: Using ESP Web Flasher (No Install)

1. Go to https://web.esphome.io/ or https://espressif.github.io/esptool-js/
2. Click "Connect" and select your device
3. Flash each file at the correct address (see table above)

**Note:** Web flasher requires Chrome/Edge browser with WebUSB support.

## Method 3: Using PlatformIO (For Developers)

If you have the source code:
```bash
pio run --target upload      # Flash firmware
pio run --target uploadfs    # Upload web UI
```

## After Flashing

1. **Power cycle** the device (unplug and replug USB)
2. **Connect to WiFi AP:** `ESP32-LoRa-Sniffer-XXYYZZ`
3. **Default password:** `recon-XXYYZZ` (where XXYYZZ matches SSID suffix)
4. **Open browser:** http://192.168.4.1
5. **Serial monitor:** 115200 baud, press Enter to activate commands

## Hardware Compatibility

✅ **Supported:** Heltec WiFi LoRa 32 V3  
❌ **Not Supported:** T-Deck, T-Deck Plus, other ESP32 boards

These binaries are built specifically for the Heltec V3 hardware. Other boards require recompilation with appropriate pin configurations.

## Verify Your Hardware

Look for these markings on your board:
- "Heltec" or "WiFi LoRa 32" silkscreen
- SX1262 LoRa chip
- Built-in 0.96" OLED display
- USB-C connector

## Support

- Documentation: See `GETTING_STARTED.md` in main repo
- Issues: https://github.com/tiarno/esp32-sniffer/issues
