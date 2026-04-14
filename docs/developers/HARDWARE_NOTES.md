# Hardware Engineering Notes

Non-obvious hardware constraints, bug post-mortems, and pin maps for each supported board. If you're adding a board or touching radio/SPI/power code, read the relevant section first.

User-facing hardware info (purchase links, antennas, accessories) lives in [../HARDWARE.md](../HARDWARE.md). This file is for developers.

---

## Heltec WiFi LoRa 32 V3

Pin-compatible reference board. The `BOARD_HELTEC_V3` macro defines the shared V3/V4 pin map in `config.h`.

**SD/SPI guard (critical).** V3 has no SD card, but `SD.begin()` unconditionally calls `SPI.begin()` with board-default pins, which reconfigures the shared FSPI hardware and kills the LoRa bus. `SDUtils::initialize()` has an early `#if !defined(HAS_SD_CARD)` return that prevents this. Never remove it. Regression test: `test/native/test_sd_guard/`.

**DIO2 as RF switch.** Not needed on V3/V4 for reception. `setDio2AsRfSwitch(true)` was added then removed after inconclusive bisect testing; do not re-add without a controlled RX test.

---

## Heltec WiFi LoRa 32 V4

Pin-compatible with V3 (same `BOARD_HELTEC_V3` macro defines the pin map) but with additional hardware: external FEM and L76K GPS. Uses its own `heltec_v4` env for `-DHAS_GPS` and `-DBOARD_HELTEC_V4`.

**External FEM must be enabled in firmware.** V4 adds a GC1109 (V4.2) or KCT8103L (V4.3) front-end module that gates the LNA and PA. Without explicit GPIO setup the radio receives nothing (initializes fine, `isrCount=0`). Enabled in `radio_controller.cpp` under `#if defined(BOARD_HELTEC_V4)`:
- `V4_FEM_EN` (GPIO 2) → OUTPUT HIGH — chip enable
- `V4_FEM_PA_CTX` (GPIO 5) → OUTPUT LOW — KCT8103L RX mode
- GPIO 46 → left as INPUT — GC1109 strapping (defaults LOW = RX)
- `setDio2AsRfSwitch(true)` — V4 uses DIO2 for RF switching

**USB PHY kills radio reception.** V4 has no CP210x bridge; serial is native USB Serial/JTAG, whose PHY shares silicon and power rails with the ESP32-S3 and emits enough noise to disrupt the SX1262's bare crystal oscillator. Symptom: zero packets received whenever USB CDC is active. Confirmed not fixable in software — late `USBSerial.begin()` is equally bad, the interference is continuous.

**Consequence:** `-DARDUINO_USB_CDC_ON_BOOT=0` is mandatory on V4. There is no serial monitor. All monitoring and commands happen through the web UI. (V3 is immune because the CP210x bridge isolates the USB PHY from the SoC; T3-S3 and T-Beam Supreme are immune because they use TCXOs instead of bare crystals.)

**Upload port.** COM12 on the dev machine (USB Serial/JTAG, no CP210x). `uploadfs` required after code upload to install the webapp.

---

## LilyGO T3-S3 V1.2 / V1.3

Chip: ESP32-S3FH4R2 (4 MB flash, 2 MB Quad PSRAM — GPIO 33–37 are safe, not OPI).

**Pin map:**
- LoRa SPI (FSPI/SPI2): SCK=5, MISO=3, MOSI=6, NSS=7
- LoRa control: DIO1=33, RST=8, BUSY=34
- SD (HSPI/SPI3 — separate bus): SCK=14, MISO=2, MOSI=11, CS=13
- OLED I2C: SDA=18, SCL=17
- TCXO: 1.8 V via DIO3; DIO2 as RF switch

**SD uses a dedicated SPI bus.** The SD card pins differ from LoRa pins, so they can coexist on separate SPI peripherals. `SDUtils` selects HSPI on T3-S3 via `#if defined(BOARD_T3_S3) || defined(BOARD_TBEAM_SUPREME)`.

**Historical bug: hardcoded SD_CS_PIN.** `packet_logger.h` once had `#define SD_CS_PIN 5`, which on T3-S3 collided with the LoRa SPI_SCK pin. `SD.begin(5)` reconfigured GPIO 5 from SPI clock to GPIO output, killing the LoRa bus. Fix: use `Config::Hardware::SD_CS` everywhere, never hardcode.

**Explicit FSPI bus required.** After a full flash erase, the default `SPI` object did not reliably map to FSPI on ESP32-S3; radio returned `-2` (CHIP_NOT_FOUND) on every boot. Fix in `radio_controller.cpp`: `static SPIClass loraSPI(FSPI)` with explicit `begin()`, plus a hardware reset of the SX1262 RST pin before SPI init.

**USB / serial.** Native USB (303A:1001), no CP210x bridge. `-DARDUINO_USB_CDC_ON_BOOT=1` required. If the device is in a crash loop, hold BOOT + tap RST to enter bootloader. TCXO immunity means USB CDC does not interfere with the radio.

---

## LilyGO T-Beam Supreme

Chip: ESP32-S3FN8 (8 MB QIO flash, 8 MB Quad PSRAM — GPIO 33–37 safe). AXP2101 PMIC, GPS, SD card, OLED.

**Pin map:**
- LoRa SPI (FSPI): SCK=12, MISO=13, MOSI=11, NSS=10
- LoRa control: DIO1=1, RST=5, BUSY=4
- SD (HSPI): SCK=36, MISO=37, MOSI=35, CS=47
- OLED (SH1106) I2C: SDA=17, SCL=18
- PMU I2C (AXP2101 @ 0x34, separate bus): SDA=42, SCL=41
- GPS UART: RX=9, TX=8, EN=7

**AXP2101 init order is load-bearing.** The PMIC controls all power rails (radio, OLED, SD, GPS). `PMUController::initialize()` must run before any peripheral in `LoRaReconTool::initialize()`. Main sequence: PMU → radio → OLED → SD → GPS.

**XPowersLib quirks.**
- `#define XPOWERS_CHIP_AXP2101` required before `#include <XPowersLib.h>` for the `XPowersPMU` typedef.
- Generic `enablePowerOutput()` / `setPowerChannelVoltage()` methods are **protected** — use per-channel methods (`enableALDO3()`, `setALDO3Voltage(3300)`, etc.).
- `begin(Wire1, AXP2101_SLAVE_ADDRESS, sda, scl)` — note the arg order (addr before pins).

**TCXO init.** Pass `tcxoVoltage=1.8` to `radio->begin()`. The PMIC supplies continuous 3.3 V to the TCXO, but RadioLib still needs the TCXO mode flag to skip the DIO3-as-TCXO-power path.

**Display driver.** Uses SH1106 (not SSD1306). `oled_display.h` typedefs `DisplayDriver` to `U8G2_SH1106_128X64_NONAME_F_HW_I2C` when `BOARD_TBEAM_SUPREME` is defined.

**GPS getter constness.** TinyGPS++ accessors are non-const (they mutate an internal "updated" flag). Do not declare GPS-reading methods as `const`.

**Power-off semantics.** `esp_deep_sleep_start()` only suspends the CPU — the AXP2101 keeps all rails live (LED glows, battery drains). `PMUController::shutdown()` calls `pmu.shutdown()` to cut all rails. Shared by the hardware button and the `/api/command 's'` endpoint via `LoRaReconTool::performShutdown()`.

**Battery reporting.** `json_builders.cpp` has an `#ifdef HAS_AXP2101` branch that reads from `PMUController::getBatteryVoltage()/getBatteryPercent()` instead of the ADC (which reads 0 V because `VBAT_ADC_PIN=PIN_UNUSED` on T-Beam).

**Partition table.** `firmware/partitions_8MB.csv` — ~3 MB app + ~5 MB LittleFS. Required because the default 4 MB-flash layout wastes the extra space.

**Upload ports.** Running mode: COM10 (303A:1001). Download mode re-enumerates to COM11. First flash: hold BOOT + tap RST while upload is running; after that, auto-reset works normally and the device stays on COM11.

---

## Cross-board: SPI bus selection rules

| Board | LoRa bus | SD bus | Shared? |
|---|---|---|---|
| Heltec V3/V4 | FSPI (explicit `SPIClass`) | — (no SD) | `SDUtils` early-returns to avoid touching `SPI.begin()` |
| T3-S3 | FSPI (explicit) | HSPI (explicit) | No — dedicated buses |
| T-Beam Supreme | FSPI (explicit) | HSPI (explicit) | No — dedicated buses |

**Rule:** never call `SD.begin(csPin)` without first ensuring the SPI object that call implicitly uses is either (a) uninitialized and will be configured on the correct pins, or (b) not the same peripheral as the LoRa bus. The HAS_SD_CARD guard exists because option (a) failed on V3.

---

## Cross-board: USB PHY and TCXO

| Board | USB | Radio clock | USB CDC safe? |
|---|---|---|---|
| Heltec V3 | CP210x bridge (isolated) | Bare crystal | ✓ Yes |
| Heltec V4 | Native USB Serial/JTAG | Bare crystal | ✗ No — kills RX |
| T3-S3 | Native USB | TCXO | ✓ Yes |
| T-Beam Supreme | Native USB | TCXO (PMIC-powered) | ✓ Yes |

The V4 case is the only unsafe combination: native USB PHY + bare crystal + shared silicon = continuous interference. TCXO-equipped boards are immune; isolated-bridge boards are immune.
