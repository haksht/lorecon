#!/usr/bin/env bash
# tools/make_release.sh — Build and package release binaries for all boards
#
# Usage:   ./tools/make_release.sh <version>
# Example: ./tools/make_release.sh v2.3.0
#
# Outputs: releases/<version>/   (per-board subdirs + flash scripts)
#          releases/esp32-lora-sniffer-<version>-binaries.zip

set -euo pipefail

# ---- Version arg ------------------------------------------------------------
VERSION=${1:-}
if [ -z "$VERSION" ]; then
    echo "Usage: $0 <version>  (e.g. $0 v2.3.0)"
    exit 1
fi

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RELEASE_DIR="$REPO_ROOT/releases/$VERSION"

# ---- Auto-detect pio --------------------------------------------------------
if [ -z "${PIO:-}" ]; then
    # Check common PlatformIO install locations
    for _pio_candidate in \
        "$HOME/.platformio/penv/Scripts/pio.exe" \
        "$HOME/.platformio/penv/bin/pio" \
        "$USERPROFILE/.platformio/penv/Scripts/pio.exe"; do
        if [ -f "$_pio_candidate" ]; then
            PIO="$_pio_candidate"
            break
        fi
    done
    if [ -z "${PIO:-}" ]; then
        if command -v pio &>/dev/null; then
            PIO="pio"
        else
            echo "ERROR: pio not found. Set PIO env var or install PlatformIO."
            exit 1
        fi
    fi
fi

# ---- Auto-detect esptool ----------------------------------------------------
PIO_BASE="${PLATFORMIO_CORE_DIR:-${HOME}/.platformio}"
PIO_ESPTOOL="$PIO_BASE/packages/tool-esptoolpy/esptool.py"
PIO_PYTHON="$PIO_BASE/penv/Scripts/python.exe"
# Also check Unix path
[ ! -f "$PIO_PYTHON" ] && PIO_PYTHON="$PIO_BASE/penv/bin/python"
if [ -z "${ESPTOOL:-}" ]; then
    if [ -f "$PIO_ESPTOOL" ] && [ -f "$PIO_PYTHON" ]; then
        ESPTOOL="$PIO_PYTHON $PIO_ESPTOOL"
    elif command -v esptool.py &>/dev/null; then
        ESPTOOL="esptool.py"
    elif command -v esptool &>/dev/null; then
        ESPTOOL="esptool"
    else
        echo "ERROR: esptool not found. Install with: pip install esptool"
        exit 1
    fi
fi

# ---- Board config -----------------------------------------------------------
# Format: "flash_size:littlefs_offset:label:usb_hint"
declare -A BOARD_CFG=(
    [heltec_v3]="8MB:0x670000:Heltec WiFi LoRa 32 V3/V4:COM3 or /dev/ttyUSB0 (CP210x)"
    [t3_s3]="4MB:0x290000:LilyGO T3-S3 V1.2/V1.3:COM9 or /dev/ttyACM0 (native USB)"
    [tbeam_supreme]="8MB:0x300000:LilyGO T-Beam Supreme:COM11 or /dev/ttyACM0 (native USB)"
)
BOARDS=(heltec_v3 t3_s3 tbeam_supreme)

echo "========================================"
echo "  ESP32 LoRa Sniffer Release Builder"
echo "  Version: $VERSION"
echo "========================================"
echo ""

# ---- Build firmware ---------------------------------------------------------
echo "[1/3] Building firmware..."
"$PIO" run -e heltec_v3 -e t3_s3 -e tbeam_supreme -d "$REPO_ROOT"

echo ""
echo "[2/3] Building filesystems..."
"$PIO" run -t buildfs -e heltec_v3 -e t3_s3 -e tbeam_supreme -d "$REPO_ROOT"

# ---- Package ----------------------------------------------------------------
echo ""
echo "[3/3] Packaging..."

if [ -d "$RELEASE_DIR" ]; then
    echo "  WARNING: $RELEASE_DIR already exists — overwriting."
    rm -rf "$RELEASE_DIR"
fi
mkdir -p "$RELEASE_DIR"

for BOARD in "${BOARDS[@]}"; do
    IFS=: read -r FLASH_SIZE FS_OFFSET LABEL USB_HINT <<< "${BOARD_CFG[$BOARD]}"

    echo ""
    echo "  [$BOARD] $LABEL"

    BUILD_DIR="$REPO_ROOT/.pio/build/$BOARD"
    BOARD_DIR="$RELEASE_DIR/$BOARD"
    mkdir -p "$BOARD_DIR"

    cp "$BUILD_DIR/bootloader.bin" "$BOARD_DIR/"
    cp "$BUILD_DIR/partitions.bin" "$BOARD_DIR/"
    cp "$BUILD_DIR/firmware.bin"   "$BOARD_DIR/"
    cp "$BUILD_DIR/littlefs.bin"   "$BOARD_DIR/"

    # Merged image — flash at 0x0 for simplest user experience
    $ESPTOOL --chip esp32s3 merge_bin \
        --flash_size "$FLASH_SIZE" \
        -o "$BOARD_DIR/full.bin" \
        0x0      "$BOARD_DIR/bootloader.bin" \
        0x8000   "$BOARD_DIR/partitions.bin" \
        0x10000  "$BOARD_DIR/firmware.bin" \
        "$FS_OFFSET" "$BOARD_DIR/littlefs.bin"

    FULL_SIZE=$(wc -c < "$BOARD_DIR/full.bin")
    echo "    full.bin: $(( FULL_SIZE / 1024 / 1024 ))MB (${FULL_SIZE} bytes)"
done

# ---- flash.sh ---------------------------------------------------------------
cat > "$RELEASE_DIR/flash.sh" << 'FLASH_SH'
#!/usr/bin/env bash
# ESP32 LoRa Sniffer — Flash Script (Linux / macOS / Windows git-bash)
# Usage: ./flash.sh <board> [port]
#
# Boards: heltec_v3 | t3_s3 | tbeam_supreme
# Port:   auto-detected if omitted
#
# Example:
#   ./flash.sh heltec_v3
#   ./flash.sh t3_s3 /dev/ttyACM0
#   ./flash.sh tbeam_supreme COM11

set -e

BOARD=${1:-}
PORT=${2:-}

# Board validation
case "$BOARD" in
    heltec_v3)
        LABEL="Heltec WiFi LoRa 32 V3/V4"
        FLASH_SIZE="8MB"
        PORT_HINT="COM3 or /dev/ttyUSB0 (CP210x USB-Serial)"
        ;;
    t3_s3)
        LABEL="LilyGO T3-S3 V1.2/V1.3"
        FLASH_SIZE="4MB"
        PORT_HINT="COM9 or /dev/ttyACM0 (native USB — hold BOOT if needed)"
        ;;
    tbeam_supreme)
        LABEL="LilyGO T-Beam Supreme"
        FLASH_SIZE="8MB"
        PORT_HINT="COM11 or /dev/ttyACM0 (native USB — hold BOOT if needed)"
        ;;
    *)
        echo "ERROR: Unknown board '$BOARD'"
        echo ""
        echo "Usage: $0 <board> [port]"
        echo "Boards:"
        echo "  heltec_v3      — Heltec WiFi LoRa 32 V3/V4"
        echo "  t3_s3          — LilyGO T3-S3 V1.2/V1.3"
        echo "  tbeam_supreme  — LilyGO T-Beam Supreme"
        exit 1
        ;;
esac

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$SCRIPT_DIR/$BOARD/full.bin"

if [ ! -f "$BIN" ]; then
    echo "ERROR: $BIN not found."
    exit 1
fi

# Auto-detect port
if [ -z "$PORT" ]; then
    if [ -e /dev/ttyUSB0 ]; then
        PORT=/dev/ttyUSB0
    elif [ -e /dev/ttyACM0 ]; then
        PORT=/dev/ttyACM0
    elif ls /dev/tty.usbserial-* 2>/dev/null | head -1 | grep -q .; then
        PORT=$(ls /dev/tty.usbserial-* | head -1)
    elif ls /dev/tty.usbmodem* 2>/dev/null | head -1 | grep -q .; then
        PORT=$(ls /dev/tty.usbmodem* | head -1)
    else
        echo "ERROR: No serial port detected."
        echo "Hint: $PORT_HINT"
        echo "Usage: $0 $BOARD <port>"
        exit 1
    fi
fi

echo ""
echo "============================================"
echo "  ESP32 LoRa Sniffer Flasher"
echo "  Board: $LABEL"
echo "  Port:  $PORT"
echo "============================================"
echo ""
echo "Flashing full image (this takes ~30-90 seconds)..."
echo ""

esptool.py --chip esp32s3 --port "$PORT" --baud 921600 \
    write_flash --flash_size "$FLASH_SIZE" 0x0 "$BIN"

echo ""
echo "============================================"
echo "  SUCCESS! Device flashed."
echo "============================================"
echo ""
echo "Next steps:"
echo "  1. Power-cycle the device (unplug and replug USB)"
echo "  2. Connect to WiFi AP: LoRa-XXYYZZ"
echo "  3. Password: recon-XXYYZZ  (matches SSID suffix)"
echo "  4. Open browser: http://192.168.4.1"
echo ""
FLASH_SH
chmod +x "$RELEASE_DIR/flash.sh"

# ---- flash.bat --------------------------------------------------------------
cat > "$RELEASE_DIR/flash.bat" << 'FLASH_BAT'
@echo off
REM ESP32 LoRa Sniffer — Flash Script (Windows CMD)
REM Usage: flash.bat <board> [port]
REM
REM Boards: heltec_v3 | t3_s3 | tbeam_supreme
REM
REM Example:
REM   flash.bat heltec_v3
REM   flash.bat heltec_v3 COM3
REM   flash.bat t3_s3 COM9
REM   flash.bat tbeam_supreme COM11

setlocal

set BOARD=%1
set PORT=%2

if "%BOARD%"=="heltec_v3" (
    set LABEL=Heltec WiFi LoRa 32 V3/V4
    set FLASH_SIZE=8MB
    set PORT_HINT=Usually COM3 (CP210x in Device Manager)
) else if "%BOARD%"=="t3_s3" (
    set LABEL=LilyGO T3-S3 V1.2/V1.3
    set FLASH_SIZE=4MB
    set PORT_HINT=Usually COM9 (native USB - hold BOOT button if needed)
) else if "%BOARD%"=="tbeam_supreme" (
    set LABEL=LilyGO T-Beam Supreme
    set FLASH_SIZE=8MB
    set PORT_HINT=Usually COM11 (native USB - hold BOOT button if needed)
) else (
    echo ERROR: Unknown board '%BOARD%'
    echo.
    echo Usage: flash.bat ^<board^> [port]
    echo Boards:
    echo   heltec_v3      - Heltec WiFi LoRa 32 V3/V4
    echo   t3_s3          - LilyGO T3-S3 V1.2/V1.3
    echo   tbeam_supreme  - LilyGO T-Beam Supreme
    exit /b 1
)

set BIN=%~dp0%BOARD%\full.bin

if not exist "%BIN%" (
    echo ERROR: %BIN% not found.
    exit /b 1
)

REM Auto-detect port if not specified
if "%PORT%"=="" (
    echo Detecting COM port...
    for /f "tokens=1 delims=:" %%i in ('mode ^| findstr /i "COM"') do (
        set PORT=%%i
        goto :found_port
    )
    echo ERROR: No COM port detected.
    echo Hint: %PORT_HINT%
    echo Usage: flash.bat %BOARD% COM3
    exit /b 1
    :found_port
    set PORT=%PORT: =%
)

echo.
echo ============================================
echo   ESP32 LoRa Sniffer Flasher
echo   Board: %LABEL%
echo   Port:  %PORT%
echo ============================================
echo.
echo Flashing full image (this takes 30-90 seconds)...
echo.

esptool.py --chip esp32s3 --port %PORT% --baud 921600 write_flash --flash_size %FLASH_SIZE% 0x0 "%BIN%"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Flash FAILED. Try:
    echo   1. Hold BOOT button while plugging in USB, then run again
    echo   2. Specify port explicitly: flash.bat %BOARD% COM3
    echo   3. Try lower baud: edit this script, change 921600 to 115200
    echo   4. Make sure esptool is installed: pip install esptool
    exit /b 1
)

echo.
echo ============================================
echo   SUCCESS! Device flashed.
echo ============================================
echo.
echo Next steps:
echo   1. Power-cycle the device (unplug and replug USB)
echo   2. Connect to WiFi AP: LoRa-XXYYZZ
echo   3. Password: recon-XXYYZZ  (matches SSID suffix)
echo   4. Open browser: http://192.168.4.1
echo.
FLASH_BAT

# ---- FLASH_INSTRUCTIONS.md --------------------------------------------------
cat > "$RELEASE_DIR/FLASH_INSTRUCTIONS.md" << INSTRUCTIONS
# ESP32 LoRa Sniffer $VERSION — Flash Instructions

Pre-compiled binaries for **three boards**: Heltec WiFi LoRa 32 V3/V4, LilyGO T3-S3, and LilyGO T-Beam Supreme.

---

## Pick Your Board

| Board | Folder | Port (typical) | Flash |
|-------|--------|----------------|-------|
| Heltec WiFi LoRa 32 V3/V4 | \`heltec_v3/\` | COM3 / /dev/ttyUSB0 | 8 MB |
| LilyGO T3-S3 V1.2/V1.3 | \`t3_s3/\` | COM9 / /dev/ttyACM0 | 4 MB |
| LilyGO T-Beam Supreme | \`tbeam_supreme/\` | COM11 / /dev/ttyACM0 | 8 MB |

---

## Method 1: Included Flash Script (EASIEST)

### Linux / macOS / Windows git-bash

\`\`\`bash
# Install esptool first (one-time)
pip install esptool

# Flash your board (port auto-detected)
./flash.sh heltec_v3
./flash.sh t3_s3
./flash.sh tbeam_supreme

# Or specify port explicitly
./flash.sh heltec_v3 /dev/ttyUSB0
./flash.sh tbeam_supreme COM11
\`\`\`

### Windows CMD

\`\`\`bat
pip install esptool

flash.bat heltec_v3
flash.bat t3_s3
flash.bat tbeam_supreme COM11
\`\`\`

---

## Method 2: esptool Manually (Single Command)

Each board folder contains a \`full.bin\` — a merged image that flashes at offset \`0x0\`:

### Heltec V3
\`\`\`bash
esptool.py --chip esp32s3 --port COM3 --baud 921600 \\
  write_flash --flash_size 8MB 0x0 heltec_v3/full.bin
\`\`\`

### T3-S3
\`\`\`bash
esptool.py --chip esp32s3 --port COM9 --baud 921600 \\
  write_flash --flash_size 4MB 0x0 t3_s3/full.bin
\`\`\`

### T-Beam Supreme
\`\`\`bash
esptool.py --chip esp32s3 --port COM11 --baud 921600 \\
  write_flash --flash_size 8MB 0x0 tbeam_supreme/full.bin
\`\`\`

---

## Method 3: esptool Manually (Separate Files)

Addresses for manual multi-file flash:

| File | Heltec V3 | T3-S3 | T-Beam Supreme |
|------|-----------|-------|----------------|
| \`bootloader.bin\` | \`0x0\` | \`0x0\` | \`0x0\` |
| \`partitions.bin\` | \`0x8000\` | \`0x8000\` | \`0x8000\` |
| \`firmware.bin\` | \`0x10000\` | \`0x10000\` | \`0x10000\` |
| \`littlefs.bin\` | \`0x670000\` | \`0x290000\` | \`0x300000\` |

---

## Troubleshooting

**Flash fails / can't connect:**
1. Hold **BOOT** button while plugging in USB, then run the flash command
2. Try a lower baud rate: replace \`921600\` with \`115200\`
3. Erase flash first: \`esptool.py --chip esp32s3 --port <PORT> erase_flash\`

**T3-S3 / T-Beam Supreme not detected (native USB):**
- These boards use native USB-CDC — they appear as a different COM port in bootloader mode
- Hold BOOT + press RST, release RST first, then release BOOT
- A new COM port should appear — use that port

**Finding your port:**
- Windows: Device Manager → Ports (COM & LPT)
- Linux: \`ls /dev/ttyUSB* /dev/ttyACM*\`
- macOS: \`ls /dev/tty.usb*\`

---

## After Flashing

1. **Power-cycle** the device (unplug and replug USB)
2. **Connect to WiFi AP:** \`LoRa-XXYYZZ\` (XXYYZZ = last 3 bytes of MAC)
3. **Password:** \`recon-XXYYZZ\` (matches SSID suffix)
4. **Open browser:** http://192.168.4.1

---

## Hardware Compatibility

| Board | Supported | Notes |
|-------|-----------|-------|
| Heltec WiFi LoRa 32 V3/V4 | ✅ | No SD card, no GPS |
| LilyGO T3-S3 V1.2/V1.3 | ✅ | SD card logging |
| LilyGO T-Beam Supreme | ✅ | SD card + GPS position logging |
| Other ESP32 boards | ❌ | Requires recompilation |

---

## Support

- Documentation: \`GETTING_STARTED.md\` in main repo
- Issues: https://github.com/tiarno/esp32-sniffer/issues
INSTRUCTIONS

# ---- Zip -------------------------------------------------------------------
echo ""
echo "Creating zip archive..."
ZIP_NAME="esp32-lora-sniffer-${VERSION}-binaries.zip"
ZIP_PATH="$REPO_ROOT/releases/$ZIP_NAME"

cd "$REPO_ROOT/releases"
zip -r "$ZIP_NAME" "$VERSION/"

echo ""
echo "========================================"
echo "  Release $VERSION complete!"
echo "========================================"
echo ""
echo "  Directory: releases/$VERSION/"
echo "  Archive:   releases/$ZIP_NAME"
echo "  Size:      $(du -sh "$ZIP_PATH" | cut -f1)"
echo ""
echo "  Boards packaged:"
for BOARD in "${BOARDS[@]}"; do
    IFS=: read -r FLASH_SIZE FS_OFFSET LABEL USB_HINT <<< "${BOARD_CFG[$BOARD]}"
    FULL_SIZE=$(wc -c < "$RELEASE_DIR/$BOARD/full.bin")
    printf "    %-16s %s  (%s, full.bin %dMB)\n" \
        "$BOARD" "$LABEL" "$FLASH_SIZE" "$(( FULL_SIZE / 1024 / 1024 ))"
done
echo ""

# ---- GitHub Release (optional) ---------------------------------------------
# Auto-detect gh CLI
GH=""
for GH_CANDIDATE in \
    "gh" \
    "/c/Program Files/GitHub CLI/gh.exe" \
    "$HOME/AppData/Local/Programs/GitHub CLI/gh.exe"; do
    if command -v "$GH_CANDIDATE" &>/dev/null 2>&1 || [ -f "$GH_CANDIDATE" ]; then
        GH="$GH_CANDIDATE"
        break
    fi
done

if [ -n "$GH" ]; then
    echo "Publishing GitHub release $VERSION..."
    cd "$REPO_ROOT"
    "$GH" release create "$VERSION" \
        "$ZIP_PATH#$ZIP_NAME" \
        --title "$VERSION — ESP32 LoRa Sniffer Multi-board Release" \
        --notes "See FLASH_INSTRUCTIONS.md inside the zip. Boards: Heltec V3, T3-S3, T-Beam Supreme." \
        2>&1 && echo "  Published: $(\"$GH\" release view \"$VERSION\" --json url -q .url)"
else
    echo "  (gh CLI not found — upload $ZIP_NAME to GitHub Releases manually)"
    echo "  https://github.com/tiarno/esp32-sniffer/releases/new?tag=$VERSION"
fi
