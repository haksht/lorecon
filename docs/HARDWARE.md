# Hardware Guide

Supported boards, specifications, purchase links, and antenna selection.

---

## Board comparison

| | Heltec V3 | Heltec V4 | T3-S3 | T-Beam Supreme |
|---|---|---|---|---|
| **MCU** | ESP32-S3 (8 MB flash) | ESP32-S3 (8 MB flash) | ESP32-S3FH4R2 (4 MB flash, 2 MB PSRAM) | ESP32-S3FN8 (8 MB flash, 8 MB PSRAM) |
| **LoRa** | SX1262 | SX1262 | SX1262 | SX1262 |
| **Display** | 0.96" SSD1306 OLED | 0.96" SSD1306 OLED | 0.96" SSD1306 OLED | 1.3" SH1106 OLED |
| **SD card** | No | No | Native MicroSD slot | Native MicroSD slot |
| **GPS** | No | Optional L76K module | No | Quectel L76K or MAX-M10S (built in) |
| **PMIC** | No | No | No | AXP2101 |
| **Price** | ~$22-32 | ~$25-35 | ~$23-30 | ~$45-55 |

**Which to buy:**
- Cheapest path to start: **Heltec V3**
- Need SD card for logging: **T3-S3** (simpler) or **T-Beam Supreme** (adds GPS + PMIC)
- Want GPS + SD + battery management: **T-Beam Supreme**

---

## Heltec WiFi LoRa 32 V3

The most proven board for this firmware.

**Purchase:** [Amazon](https://www.amazon.com/dp/B0B697NLJ5) · [Heltec Store](https://heltec.org/project/wifi-lora-32-v3/) · [AliExpress](https://www.aliexpress.com/item/1005005967763162.html)

**Antenna connector:** RP-SMA

---

## Heltec WiFi LoRa 32 V4

Hardware is nearly identical to the V3. The V4 has a header for an optional L76K GPS module.

Flash the `heltec_v3` binary unless your V4 has the GPS module — in that case use `heltec_v4` to enable GPS support.

**Purchase:** [Heltec Store](https://heltec.org/project/wifi-lora-32-v3/)

**No serial monitor.** The V4 uses native USB (no CP210x bridge). Enabling USB CDC kills radio reception due to shared silicon noise, so serial output is permanently disabled. Use the web UI for all monitoring and commands.

**Hotspot setup is required on V4.** On other boards you can read the API token from the serial output at boot and authenticate via the device AP. On V4 there is no serial output, so there is no way to retrieve the token while connected to the device AP. You must configure a hotspot first:

1. Connect to `LoRa-XXXXXX` AP (password `recon-XXXXXX`)
2. Open `http://192.168.4.1` → Settings → Configure Hotspot
3. Enter your phone hotspot credentials and save
4. Device reboots and connects to your hotspot
5. Connect your phone/laptop to the same hotspot — the token is now auto-fetched and saved to your browser

After this one-time setup, the token is stored in your browser's localStorage and you will not be prompted again.

---

## LilyGO T3-S3 V1.2/V1.3

Best choice if you want native SD card logging without GPS.

**Purchase:** [Official Store](https://lilygo.cc/products/t3s3-v1-0) ($23.65) · [Amazon](https://www.amazon.com/dp/B0BW5W9QXZ)

**SD card:** MicroSD slot, push-to-insert. Use ≤4 GB cards formatted FAT32.

**What to order:** SX1262 variant (not SX1276 or SX1280). For US, order 915 MHz (SKU H598).

---

## LilyGO T-Beam Supreme

The most capable board: GPS, SD card, and battery management.

**Purchase:** [Official Store](https://lilygo.cc/products/t-beam-supreme) · [AliExpress](https://lilygo.aliexpress.com)

**SD card:** MicroSD slot. Supports up to 32 GB (FAT32).

**GPS:** Built-in Quectel L76K or MAX-M10S module. Coordinates are stamped on every captured packet when a fix is acquired.

**Battery:** 18650 Li-Ion cell with charging and safe hardware power-off. The power-off button (or Settings tab) cuts all rails — no residual drain.

---

## Antenna

All boards ship with an antenna that works fine for most use cases.

For better range:
- A 6 dBi 915 MHz directional antenna improves reception in open areas
- Match your region: **915 MHz** (US), **868 MHz** (EU), **433 MHz** (Asia)

**Connector types:**
- Heltec V3/V4: RP-SMA
- T3-S3, T-Beam: U.FL (pigtail often included)

Never power on the radio without an antenna connected.

---

## Accessories

| Item | Purpose | Notes |
|------|---------|-------|
| 18650 Li-Ion battery | Portable operation, 4-8 hours | Protected cells recommended |
| MicroSD card (≤4 GB) | PCAP/CSV logging | FAT32, Class 10 |
| 3D printed enclosure | Protection | Search "Heltec V3 case" on Printables |
| USB power bank | Extended field operation | Avoids battery swaps |

---

## Unsupported boards

The T-Deck Plus is not supported due to hardware design issues that cannot be worked around in software. If you're interested in porting to another ESP32-S3 board, see [developers/ARCHITECTURE.md](developers/ARCHITECTURE.md).

