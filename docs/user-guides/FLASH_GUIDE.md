# Flashing Pre-built Firmware

No development environment needed! Flash the ESP32 LoRa Sniffer using just your browser or a simple command-line tool.

## Option 1: Web Flasher (Easiest) 🌐

Use [ESP Web Tools](https://esphome.github.io/esp-web-tools/) or [Adafruit ESPTool](https://adafruit.github.io/Adafruit_WebSerial_ESPTool/) directly in Chrome/Edge:

1. Connect your Heltec WiFi LoRa 32 V3 via USB
2. Go to the web flasher site
3. Click "Connect" and select your device's COM port
4. Flash each file at the correct address:

| File | Address |
|------|---------|
| `bootloader.bin` | `0x0` |
| `partitions.bin` | `0x8000` |
| `firmware.bin` | `0x10000` |
| `littlefs.bin` | `0x670000` |

## Option 2: esptool.py (Command Line)

### Install esptool
```bash
pip install esptool
```

### Flash Everything (Single Command)
```bash
esptool.py --chip esp32s3 --port COM3 --baud 921600 \
  write_flash \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin \
  0x670000 littlefs.bin
```

**Windows:** Replace `COM3` with your actual port (check Device Manager)  
**Mac/Linux:** Use `/dev/ttyUSB0` or `/dev/ttyACM0`

### Finding Your Port
- **Windows:** Device Manager → Ports (COM & LPT)
- **Mac:** `ls /dev/tty.usb*`
- **Linux:** `ls /dev/ttyUSB* /dev/ttyACM*`

## Option 3: ESP Flash Download Tool (Windows GUI)

1. Download from [Espressif](https://www.espressif.com/en/support/download/other-tools)
2. Select "ESP32-S3" chip
3. Add files with addresses as shown above
4. Set baud rate to 921600
5. Click "Start"

## Troubleshooting

### Device not detected?
- Install [CP210x drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) or [CH340 drivers](http://www.wch-ic.com/downloads/CH341SER_ZIP.html)
- Try a different USB cable (some are charge-only)
- Hold BOOT button while connecting USB

### Flash failed?
- Hold BOOT button, press RST, release BOOT (enters bootloader mode)
- Try lower baud rate: `--baud 115200`

### Wrong addresses?
The addresses above are for the default partition scheme. If flash fails, the bootloader will show the correct addresses on serial output.

## After Flashing

1. Press RST button or unplug/replug USB
2. Device creates WiFi network: `LoRa-XXXXXX` (unique per device)
3. Connect your phone/laptop to that network
4. Browser auto-opens setup page (or go to `http://192.168.4.1`)
5. Configure your phone hotspot credentials
6. Device restarts and connects to your hotspot

## Building From Source (Developers)

If you want to modify the firmware:

1. Install [VS Code](https://code.visualstudio.com/)
2. Install [PlatformIO extension](https://platformio.org/install/ide?install=vscode)
3. Clone the repository
4. Open folder in VS Code
5. Click PlatformIO: Upload (or run `pio run -t upload`)

Arduino IDE is **not recommended** - the project uses PlatformIO-specific features and library management.
