# ESP32 LoRa Sniffer - Project Instructions

## Embedded/Hardware Debugging

When debugging hardware/embedded issues (ESP32, radio, SPI), always check for pin conflicts and peripheral initialization order conflicts before attempting software-level fixes. Common culprits: SPI bus sharing, WiFi interference with other peripherals, CS pin conflicts.

Before suggesting any fixes, analyze all peripherals for potential conflicts: list every SPI device, shared pins, and initialization order. Then identify which combinations could interfere when WiFi is active.

## File Operations

For generated filenames on Windows, avoid colons (:) and unicode characters like em-dashes (--). Use underscores or hyphens instead.

## Build

- Default board: Heltec V3 (`pio run -e heltec_v3`)
- T3-S3 board: `pio run -e t3_s3`
- Upload T3-S3: `pio run -e t3_s3 -t upload --upload-port COM9`
- Monitor: `pio device monitor --port COM9 --baud 115200`
- Source is C++ (Arduino framework) in `firmware/src/`
- Board configs in `firmware/src/config.h` under `Config::Hardware` namespace
- always use the full path to pio related commands
- alway use the venv python (not the system level python executables.)