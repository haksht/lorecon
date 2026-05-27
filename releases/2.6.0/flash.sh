#!/usr/bin/env bash
# LoRecon — Flash Script (Linux / macOS / Windows git-bash)
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
        LABEL="Heltec WiFi LoRa 32 V3"
        FLASH_SIZE="8MB"
        PORT_HINT="COM3 or /dev/ttyUSB0 (CP210x USB-Serial)"
        ;;
    heltec_v4)
        LABEL="Heltec WiFi LoRa 32 V4 (GPS)"
        FLASH_SIZE="8MB"
        PORT_HINT="COM12 or /dev/ttyACM0 (native USB — hold BOOT if needed)"
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
        echo "  heltec_v3      — Heltec WiFi LoRa 32 V3"
        echo "  heltec_v4      — Heltec WiFi LoRa 32 V4 (GPS)"
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
echo "  LoRecon Flasher"
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
