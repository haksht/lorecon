# Setup Guide

Flash the firmware, boot the device, connect to WiFi. This guide covers the full path from download to a working device.

---

## What you need

- One of the [supported boards](HARDWARE.md)
- USB-C data cable (not charge-only)
- 902-928 MHz antenna — usually ships with the board
- [Python 3.10+](https://www.python.org/downloads/) — needed for `esptool` flashing and the `lorecon` analysis toolkit. Skip if you only want to flash from the browser (see [Flash from your browser](#flash-from-your-browser-no-python-needed) below)

---

## Pick a path

Three options. Easiest first — pick one.

### Path A — Web installer (one click, no downloads, no Python)

Open **<https://haksht.github.io/lorecon/install/>** in Chrome or Edge, plug your board in, and click the button for your board. The installer flashes the same firmware as the binary release, straight from the browser.

This is the fastest start. You do **not** get the Python analysis tools. If you want those later, do Path C as well — it's additive, no re-flashing.

After flashing, skip ahead to [First boot](#first-boot).

### Path B — Binary release (offline, no analysis tools)

Download `lorecon-vX.X.X-binaries.zip` from [Releases](https://github.com/haksht/lorecon/releases/latest) and extract it anywhere. The zip contains `flash.sh`, `flash.bat`, and `flash.ps1` at the top level, plus one folder per board — each folder has a `full.bin` merged image that flashes at offset `0x0`.

Then `cd` into the extracted folder and continue at [Flash the firmware](#flash-the-firmware).

### Path C — Clone or download the repo (firmware + Python tools)

Get the source. Three options:

| How | Command or link | When |
|-----|---|---|
| **git clone** (recommended) | `git clone https://github.com/haksht/lorecon.git` | You have git and want easy updates (`git pull`) |
| **Download ZIP** | [Download main.zip](https://github.com/haksht/lorecon/archive/refs/heads/main.zip) | No git installed — just unzip and go |
| **Fork, then clone your fork** | "Fork" button on the [GitHub page](https://github.com/haksht/lorecon) | You plan to contribute or modify |

Don't have git? Install [Git for Windows](https://git-scm.com/download/win) (Linux/macOS usually have it; check with `git --version`).

Now `cd` into the repo root — the directory containing `README.md`, `pyproject.toml`, and the `firmware/`, `tools/`, and `releases/` folders. Every command in the rest of this guide runs from here.

```bash
cd lorecon            # if you cloned
cd lorecon-main       # if you downloaded the ZIP (GitHub adds "-main")
```

Install the Python toolkit:
```bash
python -m venv venv
.\venv\Scripts\activate.ps1   # Windows PowerShell (or ./venv/bin/activate on Linux/macOS)
pip install -e .              # installs the lorecon CLI + esptool + all deps
```

The pre-built binaries are already in the repo under `releases/vX.X.X/` — the same flash scripts work from there. No separate download needed.

---

## Pick your board

| Board | Folder | Typical port | Flash size |
|-------|--------|-------------|------------|
| Heltec WiFi LoRa 32 V3 | `heltec_v3/` | COM3 / /dev/ttyUSB0 | 8 MB |
| Heltec WiFi LoRa 32 V4 (with GPS) | `heltec_v4/` | COM12 / /dev/ttyACM0 | 8 MB |
| LilyGO T3-S3 V1.2/V1.3 | `t3_s3/` | COM9 / /dev/ttyACM0 | 4 MB |
| LilyGO T-Beam Supreme | `tbeam_supreme/` | COM11 / /dev/ttyACM0 | 8 MB |

All V4 boards use the `heltec_v4` binary — it includes a required USB configuration fix that the V3 binary lacks. Do not flash `heltec_v3` onto a V4.

---

## Flash the firmware

The flash scripts need `esptool`. If you followed Path B, `pip install -e .` already installed it — just activate the venv first. If you're on Path A, run `pip install esptool` once.

Open a terminal where the flash scripts live (the extracted release folder for Path A, or `releases/vX.X.X/` inside the repo for Path B) and run the script for your OS. Port is auto-detected if you omit it.

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

### Flash from your browser (no Python needed)

If you don't want to install Python or esptool, flash directly from Chrome or Edge using a WebSerial tool — drag-and-drop the `full.bin` file for your board at offset `0x0`.

- [Espressif esptool-js](https://espressif.github.io/esptool-js/) — official Espressif web flasher
- [Adafruit WebSerial ESPTool](https://adafruit.github.io/Adafruit_WebSerial_ESPTool/) — simpler UI, same idea

Steps:

1. Plug in the board (hold **BOOT** if it doesn't show up)
2. Open the flasher in Chrome or Edge
3. Click **Connect** and pick your device's port
4. Select `full.bin` from your board's folder, set address to `0x0`
5. Click **Program**

Firefox and Safari do not support WebSerial — use Chrome or Edge.

### Manual flash (esptool directly)

If you prefer to run esptool yourself:
```bash
.\venv\Scripts\activate.ps1  # or ./venv/bin/activate on Linux/macOS
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
3. The web UI loads with 6 tabs — Info, Devices, Packets, Frequencies, Dashboard, Settings
4. The Info tab shows scanning status and packet counts

The device starts scanning 29 LoRa configurations immediately on boot. You should see the frequency cycling on the OLED and in the Info tab within seconds.

**Heltec V4 note:** The V4 has no serial monitor -- the web UI is the only interface. Everything above applies; just skip any step that refers to serial output.

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
