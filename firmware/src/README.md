# ESP32 LoRa Packet Sniffer

Simple LoRa packet capture tool for ESP32-S3 + SX1262 radio combinations.

## Quick Start

1. **Use the simple version**: Copy `main_simple.cpp` to `main.cpp` or change platformio src_filter
2. **Build and upload**: `pio run --target upload`
3. **Monitor**: `pio device monitor`

## File Guide

- **`main_simple.cpp`** - Clean, basic packet sniffer (RECOMMENDED)
- **`main.cpp`** - Full-featured version with stress testing, PSK analysis, etc.
- **`psk_decryption.h`** - PSK testing functions (used by full version)
- **`hardware_stress_tester.*`** - Hardware validation (optional)
- **`intelligence_storage.*`** - Storage module (not fully implemented)

## Hardware

- ESP32-S3 + SX1262 LoRa radio
- Pin mapping configured for Heltec WiFi LoRa 32 V3

## What It Does

1. Scans common LoRa frequencies (Meshtastic, LoRaWAN, etc.)
2. Detects active devices and extracts node IDs
3. Allows targeting specific devices for packet capture
4. Shows basic packet analysis (protocol, RSSI, data)

## Usage

After upload, watch serial output for device detections:
```
[SCAN] Meshtastic_LF_902 (902.125 MHz, SF11)
[DEVICE] New: 0x401ACD4E (Meshtastic) -15.0 dBm
[SCAN] Cycle complete - 1 devices found

Press 'm' for menu
```

Then use interactive menu to select and target devices.

## Simplification Notes

This project was simplified from a complex "reconnaissance tool" to focus on core packet capture functionality. The simple version (`main_simple.cpp`) removes:

- Complex stress testing
- Intelligence storage systems  
- Advanced PSK analysis
- Marketing terminology
- Over-engineered features

Use the simple version for learning LoRa protocols and basic packet capture.