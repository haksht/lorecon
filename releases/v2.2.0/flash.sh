#!/bin/bash
# ESP32 LoRa Sniffer v2.2.0 Flash Script for Linux/Mac
# Requires: pip install esptool

set -e

# Auto-detect port or use argument
if [ -z "$1" ]; then
    if [ -e /dev/ttyUSB0 ]; then
        PORT=/dev/ttyUSB0
    elif [ -e /dev/ttyACM0 ]; then
        PORT=/dev/ttyACM0
    elif ls /dev/tty.usb* 1>/dev/null 2>&1; then
        PORT=$(ls /dev/tty.usb* | head -1)
    else
        echo "ERROR: No serial port detected."
        echo "Usage: ./flash.sh /dev/ttyUSB0"
        exit 1
    fi
else
    PORT=$1
fi

echo ""
echo "============================================"
echo "ESP32 LoRa Sniffer v2.2.0 Flasher"
echo "============================================"
echo "Target port: $PORT"
echo ""
echo "Flashing complete firmware image..."
echo "(This takes about 90 seconds)"
echo ""

esptool.py --chip esp32s3 --port "$PORT" --baud 921600 write_flash 0x0 esp32-lora-sniffer-v2.2.0-full.bin

echo ""
echo "============================================"
echo "SUCCESS! Device flashed."
echo "============================================"
echo ""
echo "Next steps:"
echo "  1. Unplug and replug the device"
echo "  2. Connect to WiFi: ESP32-LoRa-Sniffer-XXYYZZ"
echo "  3. Password: recon-XXYYZZ (matches SSID suffix)"
echo "  4. Browse to: http://192.168.4.1"
echo ""
