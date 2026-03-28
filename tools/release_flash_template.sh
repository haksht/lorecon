#!/usr/bin/env bash
# ESP32 LoRa Sniffer - Flash Script (Linux / macOS / Windows git-bash)
# Usage: ./flash.sh <board> [port]
#
# Boards: heltec_v3 | t3_s3 | tbeam_supreme
# Port:   auto-detected if omitted
#
# Examples:
#   ./flash.sh heltec_v3
#   ./flash.sh heltec_v3 /dev/ttyUSB0
#   ./flash.sh t3_s3 /dev/ttyACM0
#   ./flash.sh tbeam_supreme COM11
#
# Requires: pip install esptool

set -e

BOARD=${1:-}
PORT=${2:-}

case "$BOARD" in
    heltec_v3)
        LABEL="Heltec WiFi LoRa 32 V3/V4"
        FLASH_SIZE="8MB"
        PORT_HINT="Try /dev/ttyUSB0 (Linux) or /dev/tty.usbserial-* (Mac) - CP210x driver"
        ;;
    t3_s3)
        LABEL="LilyGO T3-S3 V1.2/V1.3"
        FLASH_SIZE="4MB"
        PORT_HINT="Try /dev/ttyACM0 (Linux) or /dev/tty.usbmodem* (Mac) - hold BOOT button if needed"
        ;;
    tbeam_supreme)
        LABEL="LilyGO T-Beam Supreme"
        FLASH_SIZE="8MB"
        PORT_HINT="Try /dev/ttyACM0 (Linux) or /dev/tty.usbmodem* (Mac) - hold BOOT button if needed"
        ;;
    *)
        echo "ERROR: Unknown board '$BOARD'"
        echo ""
        echo "Usage: $0 <board> [port]"
        echo "Boards:"
        echo "  heltec_v3      - Heltec WiFi LoRa 32 V3/V4"
        echo "  t3_s3          - LilyGO T3-S3 V1.2/V1.3"
        echo "  tbeam_supreme  - LilyGO T-Beam Supreme"
        exit 1
        ;;
esac

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$SCRIPT_DIR/$BOARD/full.bin"

if [ ! -f "$BIN" ]; then
    echo "ERROR: Binary not found: $BIN"
    echo "Make sure you extracted the full release zip."
    exit 1
fi

# Auto-detect port if not specified
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
        echo "Try: $0 $BOARD /dev/ttyUSB0"
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
echo "Flashing full image - this takes 30-90 seconds..."
echo ""

python3 -m esptool --chip esp32s3 --port "$PORT" --baud 921600 \
    write-flash --flash-size "$FLASH_SIZE" 0x0 "$BIN"

if [ $? -ne 0 ]; then
    echo ""
    echo "Flash FAILED. Try:"
    echo "  1. Hold BOOT button while plugging in USB, then run again"
    echo "  2. Specify port: $0 $BOARD /dev/ttyUSB0"
    echo "  3. Lower baud: edit this script, change 921600 to 115200"
    echo "  4. Install esptool: pip install esptool"
    exit 1
fi

echo ""
echo "============================================"
echo "  SUCCESS! Device flashed."
echo "============================================"
echo ""
echo "Next steps:"
echo "  1. Power-cycle the device - unplug and replug USB"
echo "  2. Connect to WiFi AP:  LoRa-XXYYZZ"
echo "  3. Password:            recon-XXYYZZ  (matches SSID suffix)"
echo "  4. Open browser:        http://192.168.4.1"
echo ""
