# ESP32 LoRa Sniffer - Project Instructions

## Stack

C++ Arduino firmware (PlatformIO) for passive LoRa packet sniffing.

- Radio: RadioLib (SX1262)
- Boards: Heltec V3, Heltec V4, LilyGO T3-S3, T-Beam Supreme; optional GPS
- Web: ESPAsyncWebServer + LittleFS webapp
- API: token-authenticated REST + WebSocket live feed
- Pin maps, past bug fixes, and board quirks live in this project's Claude memory

## Git Conventions

NEVER add `Co-Authored-By` trailers or any AI attribution to commits in this repo.

## Embedded/Hardware Debugging

When debugging hardware/embedded issues (ESP32, radio, SPI), always check for pin conflicts and peripheral initialization order conflicts before attempting software-level fixes. Common culprits: SPI bus sharing, WiFi interference with other peripherals, CS pin conflicts.

Before suggesting any fixes, analyze all peripherals for potential conflicts: list every SPI device, shared pins, and initialization order. Then identify which combinations could interfere when WiFi is active.

## File Operations

For generated filenames, avoid colons (:) and unicode characters like em-dashes (--). Use underscores or hyphens instead.

## Build

- Default board: Heltec V3 (`pio run -e heltec_v3`)
- T3-S3 board: `pio run -e t3_s3`
- Upload T3-S3: `pio run -e t3_s3 -t upload --upload-port /dev/ttyACM0`
- T-Beam Supreme: `pio run -e tbeam_supreme`
- Heltec V4 (with GPS): `pio run -e heltec_v4`
- Monitor: `pio device monitor --port /dev/ttyACM0 --baud 115200`
- Serial ports (Linux): native-USB boards enumerate as `/dev/ttyACM*`, USB-UART bridge boards as `/dev/ttyUSB*` — check `ls /dev/ttyACM* /dev/ttyUSB*`. User must be in the `dialout` group.
- Source is C++ (Arduino framework) in `firmware/src/`
- Board configs in `firmware/src/config.h` under `Config::Hardware` namespace
