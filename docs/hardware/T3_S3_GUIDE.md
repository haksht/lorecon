# LilyGO T3-S3 V1.2 Hardware Guide

**Board:** LilyGO T3-S3 V1.2
**Status:** ✅ Supported (v2.3.0+)
**MCU:** ESP32-S3FN16R8 (16MB Flash, 8MB PSRAM)
**LoRa:** SX1262 (433/868/915 MHz variants)
**SD Card:** Native MicroSD slot

---

## Overview

The LilyGO T3-S3 is an excellent alternative to the Heltec WiFi LoRa 32 V3, offering **native SD card support** without requiring hardware modifications. It uses the same ESP32-S3 + SX1262 architecture, making porting minimal (pin definitions only).

### Key Advantages vs Heltec V3

✅ **Native SD Card**: Built-in MicroSD slot for PCAP/CSV logging
✅ **No SPI Conflicts**: Display on I2C, LoRa and SD on separate SPI pins
✅ **Same Performance**: Identical ESP32-S3 + SX1262 radio capabilities
✅ **Cost Competitive**: ~$24 USD (similar to Heltec V3)

---

## Hardware Specifications

### Pinout Summary

| Component | Pins | Notes |
|-----------|------|-------|
| **LoRa Radio (SX1262)** | CS:7, DIO1:33, RST:8, BUSY:34 | SPI: SCK:5, MISO:3, MOSI:6 |
| **SD Card** | CS:13, SCK:14, MISO:2, MOSI:11 | Separate SPI bus from LoRa |
| **OLED Display** | SDA:18, SCL:17 | I2C (no SPI conflict) |
| **User Button** | GPIO 0 (BOOT) | Active low |
| **User LED** | GPIO 37 | Built-in indicator |
| **Battery ADC** | GPIO 1 | Voltage divider (scale: 2.0x) |

### Display Configuration

- **Type**: 0.96" SSD1306 OLED (128x64)
- **Interface**: I2C (not SPI)
- **I2C Address**: 0x3C
- **Power**: Always powered (no Vext control like Heltec)
- **Reset**: Software reset only (no hardware RST pin)

This I2C configuration **eliminates the T-Deck's fatal SPI conflict issue**.

---

## SD Card Support

### Slot Configuration

- **Type**: MicroSD/TF card slot (push-to-insert)
- **Interface**: SPI (separate from LoRa radio)
- **File Systems**: FAT16, FAT32
- **Tested Sizes**: 1GB (FAT16), 4GB (FAT32)
- **Known Issues**: 16GB cards may have reliability issues (use ≤4GB recommended)

### Usage

Insert MicroSD card **before** powering on. The firmware will:
1. Auto-detect SD card on boot
2. Create `/logs/` directory
3. Start logging packets to `snf_<timestamp>.csv` and `.pcap` files
4. Flush every 10 packets to prevent data loss

### File Locations

```
/logs/
  ├── snf_12345.csv          # Packet data (CSV format)
  ├── snf_12345.pcap         # PCAP export (Wireshark compatible)
  ├── devices_summary.csv    # Device tracking log
  └── gps_tracks.csv         # GPS positions
```

---

## Comparison to Other Boards

| Feature | T3-S3 V1.2 | Heltec V3 | T-Deck Plus |
|---------|------------|-----------|-------------|
| **ESP32 Variant** | S3 (16MB/8MB) | S3 (8MB/8MB) | S3 (16MB/8MB) |
| **LoRa Chip** | SX1262 | SX1262 | SX1262 |
| **Display** | I2C OLED | I2C OLED | SPI LCD (conflict!) |
| **SD Card** | ✅ Native | ❌ Requires mod | ✅ Native (conflicts!) |
| **Display Power** | Always on | Vext control (GPIO36) | Shared power rail |
| **SPI Conflicts** | ✅ None | ✅ None | ❌ Display blanks |
| **Price** | ~$24 | ~$25 | ~$70 |
| **Recommended** | ✅ **Best for SD logging** | ✅ Proven stable | ❌ Not compatible |

---

## Building Firmware

### PlatformIO

```bash
# Build for T3-S3
pio run -e t3_s3

# Upload to T3-S3
pio run -e t3_s3 -t upload

# Monitor serial output
pio device monitor
```

### Build Flags

The T3-S3 build automatically sets:
- `BOARD_T3_S3` - Enables T3-S3 pin definitions
- `HAS_OLED_DISPLAY` - Enables OLED display
- `HAS_SD_CARD` - Enables SD card logging

---

## First-Time Setup

1. **Insert SD Card** (≤4GB, FAT32 formatted)
2. **Connect USB-C** cable to computer
3. **Build and Upload** firmware (see above)
4. **Press RESET** button on board
5. **Monitor Serial** for initialization messages

### Expected Serial Output

```
[DISPLAY] Initializing OLED...
[DISPLAY] T3-S3: Using software reset only
[DISPLAY] Initializing I2C: SDA=18, SCL=17
[DISPLAY] ✓ OLED found at 0x3C (attempt 1)
[DISPLAY] OLED ready (128x64, I2C: SDA=18, SCL=17, Vext=-1)

[SD] Initializing SD card logger...
[SD] ✅ SD card detected: SDHC/SDXC
[SD] Card size: 3815 MB
[SD] ✅ Session started: /logs/snf_12345.csv

[RADIO] Initializing LoRa radio (SX1262)...
[RADIO] ✅ Radio ready: 915.0 MHz, SF9, BW125
```

---

## Troubleshooting

### SD Card Not Detected

**Symptom**: `[SD] ⚠️ No SD card detected`

**Solutions**:
1. Ensure card is ≤4GB (larger cards unreliable)
2. Format as FAT32 (not exFAT)
3. Reseat card (push fully until click)
4. Power cycle board after inserting card
5. Try a different SD card brand

### Display Not Working

**Symptom**: OLED stays blank

**Solutions**:
1. Check serial output for I2C errors
2. Verify I2C address is 0x3C: `[DISPLAY] ✓ OLED found at 0x3C`
3. Power cycle the board
4. Display will auto-off after inactivity (press BOOT button to wake)

### Radio Initialization Fails

**Symptom**: `[RADIO] ❌ Failed to initialize`

**Solutions**:
1. Verify LoRa variant matches region (915 MHz for US, 868 MHz for EU)
2. Check antenna is connected (loose antenna = high SWR = init fail)
3. Power cycle board
4. Check serial for specific error codes

---

## Known Limitations

1. **SD Card Size**: Best compatibility with 1-4GB cards (FAT32)
2. **No Vext Control**: Display always powered (unlike Heltec V3)
3. **No Hardware Reset**: OLED uses software reset only
4. **GPIO Availability**: Fewer free GPIOs than Heltec V3 (SD card uses 4 pins)

---

## Purchasing

### Recommended Vendors

- **Official**: [LilyGO Store](https://lilygo.cc/products/t3s3-v1-0) - $23.65
- **Amazon US**: [T3-S3 SX1262 915MHz](https://www.amazon.com/LILYGO-ESP32-S3-Development-Wireless-Display/dp/B0BW5W9QXZ)
- **AliExpress**: [LilyGO Official Store](https://lilygo.aliexpress.com/store/2090076)

### What to Order

- **Frequency**: Match your region (433/868/915 MHz)
- **LoRa Chip**: SX1262 variant (recommended for best performance)
- **Includes**: Board, antenna (U.FL connector), USB-C cable
- **Not Included**: SD card, battery

---

## Additional Resources

- [T3-S3 Official Wiki](https://wiki.lilygo.cc/get_started/en/LoRa_GPS/T3S3/T3S3.html)
- [T3-S3 GitHub Hardware Docs](https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series)
- [T3-S3 Schematic PDF](https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series/blob/master/schematic/T3_S3_V1.1.pdf)
- [Meshtastic T3-S3 Support](https://meshtastic.org/docs/hardware/devices/lilygo/lora/)

---

## Revision History

- **2026-01-10**: Initial T3-S3 support added (v2.3.0)
- **Hardware Tested**: LilyGO T3-S3 V1.2 with SX1262 (915 MHz)
