# Building from Source

Compiling, uploading, using the serial monitor, and updating firmware over the air.

If you just want to flash pre-built binaries, see [SETUP.md](../SETUP.md) instead.

---

## Prerequisites

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) — install via `pip install platformio` or the PlatformIO IDE extension for VS Code
- Python 3.8+
- USB-C data cable

Verify installation:
```bash
pio --version
```

---

## Build environments

| Board | Environment | Key flags |
|-------|------------|-----------|
| Heltec WiFi LoRa 32 V3 | `heltec_v3` | Default |
| Heltec WiFi LoRa 32 V4 | `heltec_v4` | `HAS_GPS`, USB_CDC_ON_BOOT=0 |
| LilyGO T3-S3 V1.2/V1.3 | `t3_s3` | `HAS_SD_CARD`, 4 MB flash |
| LilyGO T-Beam Supreme | `tbeam_supreme` | `HAS_SD_CARD`, `HAS_GPS`, `HAS_AXP2101`, 8 MB flash |
| Native unit tests | `native` | Runs on host (Linux/macOS only) |

Board configs and pin maps are in `firmware/src/config.h` under the `Config::Hardware` namespace.

---

## Build commands

```bash
# Build default board (heltec_v3)
pio run

# Build specific board
pio run -e t3_s3
pio run -e tbeam_supreme

# Build all boards
pio run -e heltec_v3 -e heltec_v4 -e t3_s3 -e tbeam_supreme

# Compile check only (no upload)
pio run -e heltec_v3

# Clean build artifacts
pio run --target clean
```

---

## Upload commands

```bash
# Upload firmware (default board, auto-detect port)
pio run --target upload

# Upload specific board + port
pio run -e t3_s3 --target upload --upload-port COM9
pio run -e heltec_v3 --target upload --upload-port /dev/ttyUSB0

# Upload web UI files to LittleFS (required on first flash, and when HTML/CSS/JS changes)
pio run --target uploadfs
pio run -e t3_s3 --target uploadfs --upload-port COM9

# Upload firmware + filesystem in one go
pio run --target upload --target uploadfs
```

The `uploadfs` step uploads `data/webapp/` to the device's LittleFS partition. You only need to run it again when web files change — firmware updates don't touch the filesystem.

**Pre-loading WiFi credentials:** If you want the device to connect to a hotspot on first boot without going through the captive portal, create `data/wifi_creds.json` before running `uploadfs`:
```json
{ "ssid": "YourHotspot", "password": "YourPassword" }
```
The firmware migrates these to secure NVS storage on first boot and deletes the file.

### T3-S3 and T-Beam Supreme: bootloader mode

These boards use native USB. If the device is in a crash loop or the upload can't connect:

1. Hold **BOOT**, press **RST**, release **RST**, release **BOOT**
2. A new COM port appears (the device re-enumerates in download mode)
3. Upload to that port

---

## Serial monitor

```bash
pio device monitor --port COM3 --baud 115200
```

### Serial activation

**Serial commands require a double-Enter.** Press Enter twice within 1.5 seconds to activate the console. This prevents phantom commands from USB electrical noise during field use.

After activation, commands are single-key:

| Key | Action |
|-----|--------|
| `m` | Device menu with targeting |
| `f` | Frequency targeting |
| `a` | RF activity and diagnostics |
| `d` | Device type summary |
| `v` | Security assessment |
| `g` | Geo intelligence positions |
| `k` | Export KML |
| `j` | Export GeoJSON |
| `r` | Resume reconnaissance (clears targeting) |
| `e` | Exit menu, resume current mode |
| `c` | Capture last packet to replay slot |
| `p` | Replay menu |
| `l` | Clear replay slots |
| `n` | Clear device list |
| `t` | Show API token |
| `w` | LoRaWAN key testing stats |
| `q` | Toggle quiet mode |
| `x` | Text-packet diagnostic report |
| `s` | Show summary |
| `i` | Reset reason and health info |
| `b` | Reboot |

The console auto-deactivates after 5 minutes of inactivity.

**Heltec V4 note:** V4 uses native USB with `USB_CDC_ON_BOOT=0` — enabling USB CDC causes power-rail interference that kills LoRa reception. There is no serial monitor on V4. Use the web UI.

---

## OTA updates

Once the device is flashed via USB once, both firmware and the web UI filesystem can be updated wirelessly. The Settings tab has two separate upload sections.

### Firmware update (OTA)

1. Build: `pio run -e heltec_v3`
2. Connect to the device (AP or hotspot)
3. Open the web UI → Settings tab → **Firmware Update**
4. Choose `.pio/build/<env>/firmware.bin`
5. Click Upload Firmware

Progress bar shows upload status. Device validates the firmware and reboots automatically. If the new firmware fails to boot, the ESP32 auto-rolls back to the previous partition — the device cannot be bricked via OTA.

### Filesystem update (OTA)

The web UI files (LittleFS) can also be updated over WiFi — no USB required after the initial flash.

1. Build the filesystem image: `pio run -e heltec_v3 --target buildfs`
2. Open the web UI → Settings tab → **Filesystem Update**
3. Choose `.pio/build/<env>/littlefs.bin`
4. Click Upload Filesystem

Device reboots with the new web UI after upload.

### Via command line

```bash
# Firmware (cURL)
curl -X POST -F "firmware=@.pio/build/heltec_v3/firmware.bin" \
  http://192.168.4.1/api/firmware/upload

# Filesystem (cURL)
curl -X POST -F "filesystem=@.pio/build/heltec_v3/littlefs.bin" \
  http://192.168.4.1/api/filesystem/upload
```

Both endpoints require the API token header (`X-API-Token: <token>`).

### How dual-partition OTA works

The ESP32 has two firmware partitions. OTA writes to the inactive partition, then reboots to it. If the new firmware fails to start, the bootloader reverts to the previous partition automatically. The filesystem partition is separate from both and is updated independently.

---

## Running unit tests

Native tests run on the host (Linux/macOS only — MinGW build issue on Windows):

```bash
pio test -e native
```

Tests cover: ring buffer, protocol parsing, security scorer, SD guard (asserts SPI.begin() is never called on boards without SD).

Test stubs are in `test/native/stubs/`.

---

## Common build issues

**Upload fails — port not found:**
```bash
pio device list    # see available ports
pio run --target upload --upload-port COM3
```

**Compilation error after pulling changes:**
```bash
pio run --target clean
pio pkg update
pio run
```

**Flash near capacity (T3-S3 has 4 MB):**
- T3-S3 flash usage appears in the build output as `Flash: [=== ] X%`
- Current firmware fits comfortably; if it grows past ~85%, disable verbose debug output or check for unused code

**Radio returns -2 (CHIP_NOT_FOUND) on T3-S3:**
- Verify `loraSPI(FSPI)` is used explicitly — default SPI may not map reliably to FSPI on ESP32-S3 after full flash erase
- Hardware reset (RST pin toggle) before SPI init is required

**V4 radio receives nothing:**
- FEM (external front-end module) must be enabled: GPIO 2 HIGH (chip enable), GPIO 5 LOW (RX mode)
- `setDio2AsRfSwitch(true)` must be set for V4
- Verify `USB_CDC_ON_BOOT=0` — USB PHY noise kills radio reception on V4
