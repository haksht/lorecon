# Hardware Guide

Supported boards, specifications, purchase links, and antenna selection.

---

## Board comparison

| | Heltec V3 | Heltec V4 | T3-S3 | T-Beam Supreme |
|---|---|---|---|---|
| **MCU** | ESP32-S3 (8 MB flash) | ESP32-S3 (8 MB flash) | ESP32-S3FH4R2 (4 MB flash, 2 MB PSRAM) | ESP32-S3FN8 (8 MB flash, 8 MB PSRAM) |
| **LoRa** | SX1262 | SX1262 | SX1262 | SX1262 |
| **Display** | 0.96" SSD1306 OLED | 0.96" SSD1306 OLED | 0.96" SSD1306 OLED | 1.3" SH1106 OLED |
| **SD card** | No (external mod needed) | No (external mod needed) | Native MicroSD slot | Native MicroSD slot |
| **GPS** | No | Optional L76K module | No | u-blox (built in) |
| **PMIC** | No | No | No | AXP2101 |
| **USB** | CP210x (COM port) | Native USB (no COM) | Native USB | Native USB |
| **Build env** | `heltec_v3` | `heltec_v4` | `t3_s3` | `tbeam_supreme` |
| **Price** | ~$22-32 | ~$25-35 | ~$23-30 | ~$45-55 |

**Which to buy:**
- Cheapest path to start: **Heltec V3**
- Need SD card for logging: **T3-S3** (simpler) or **T-Beam Supreme** (adds GPS + PMIC)
- Want GPS + SD + battery management: **T-Beam Supreme**
- Have a Heltec V4 with L76K GPS attached: use the `heltec_v4` binary

---

## Heltec WiFi LoRa 32 V3

The most proven board for this firmware. CP210x USB bridge means it always shows up as a stable COM port and supports a serial monitor.

**Purchase:** [Amazon](https://www.amazon.com/dp/B0B697NLJ5) · [Heltec Store](https://heltec.org/project/wifi-lora-32-v3/) · [AliExpress](https://www.aliexpress.com/item/1005005967763162.html)

**SD card:** No native slot. An external SPI SD module can be wired in with hardware modification.

**Notes:**
- GPIO 36 (Vext) controls display power (active LOW)
- Button on GPIO 0
- Antenna connector: RP-SMA

---

## Heltec WiFi LoRa 32 V4

Hardware is nearly identical to V3 with the addition of an L76K GNSS module header.

**Important:** The V4 uses native USB (no CP210x). The `ARDUINO_USB_CDC_ON_BOOT` flag must remain OFF — enabling USB CDC while the radio is running causes power-rail noise that kills LoRa reception. This means **no serial monitor** on V4. Use the web UI for all monitoring.

Use the `heltec_v4` build environment only if your board has the L76K GPS module connected. Otherwise use `heltec_v3` — the firmware is identical for V4 boards without GPS.

**Purchase:** [Heltec Store](https://heltec.org/project/wifi-lora-32-v3/)

---

## LilyGO T3-S3 V1.2/V1.3

Best choice if you want native SD card logging without GPS or a PMIC. The SD card uses a completely separate SPI bus from the LoRa radio, so there are no bus-sharing conflicts.

**Purchase:** [Official Store](https://lilygo.cc/products/t3s3-v1-0) ($23.65) · [Amazon](https://www.amazon.com/dp/B0BW5W9QXZ)

**SD card:** MicroSD slot, push-to-insert. Use ≤4 GB cards formatted FAT32. Cards ≥16 GB may be unreliable.

**Pin map:**

| Peripheral | Pins |
|-----------|------|
| LoRa (FSPI) | SCK=5, MISO=3, MOSI=6, NSS=7, DIO1=33, RST=8, BUSY=34 |
| SD (HSPI) | SCK=14, MISO=2, MOSI=11, CS=13 |
| OLED (I2C) | SDA=18, SCL=17 |

**What to order:** SX1262 variant (not SX1276 or SX1280). For US, order 915 MHz (SKU H598).

**Flash memory:** 4 MB — fits firmware comfortably. SD card handles unlimited logging.

---

## LilyGO T-Beam Supreme

The most capable board: GPS, SD, and an AXP2101 PMIC for battery management and hardware power-off. The PMIC must be initialized before any other peripheral — it powers all rails.

**Purchase:** [Official Store](https://lilygo.cc/products/t-beam-supreme) · [AliExpress](https://lilygo.aliexpress.com)

**SD card:** MicroSD slot. Supports up to 32 GB (FAT32).

**GPS:** Built-in u-blox module on UART (RX=9, TX=8). Coordinates are stamped on every captured packet when a fix is acquired.

**Battery:** 18650 Li-Ion cell. PMIC handles charging, voltage regulation, and safe shutdown. Power-off button (or `/api/command` with `'s'`) cuts all rails via PMIC — no residual drain.

**Pin map:**

| Peripheral | Pins |
|-----------|------|
| LoRa (FSPI) | SCK=12, MISO=13, MOSI=11, NSS=10, DIO1=1, RST=5, BUSY=4 |
| SD (HSPI) | SCK=36, MISO=37, MOSI=35, CS=47 |
| OLED I2C | SDA=17, SCL=18 |
| PMU I2C | SDA=42, SCL=41 |
| GPS UART | RX=9, TX=8, EN=7 |

**Init order:** PMU → radio → OLED → SD → GPS. Deviation causes peripheral failures.

---

## Antenna

All boards ship with an antenna. The included antenna works fine for most use cases.

For better range or signal clarity:
- A 6 dBi 915 MHz directional antenna improves reception noticeably in open areas
- Match the antenna to your region: **915 MHz** (US), **868 MHz** (EU), **433 MHz** (Asia)

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
| 3D printed enclosure | Protection, professional look | Search "Heltec V3 case" on Printables |
| USB power bank | Extended field operation | Avoids battery swaps |

---

## Unsupported boards

**T-Deck Plus** — Not supported. Two hardware design flaws make it incompatible:

1. GPIO 10 (`PERI_POWERON`) powers both the LoRa radio and the TFT display through the same LDO rail. You cannot reset or power-cycle one without affecting the other, which makes radio recovery impossible.

2. The HSPI bus (SCK=40, MISO=38, MOSI=41) is shared between the LoRa radio, TFT display, and SD card with no hardware arbitration. Radio SPI transactions corrupt the display state within seconds — the display goes black and cannot be recovered without a full power cycle.

Both are hardware-level constraints with no software workaround.

Other ESP32 boards would require a port: new `config.h` pin definitions, display driver selection, and testing. The firmware architecture is designed to make this straightforward, but no other boards are tested or officially supported.
