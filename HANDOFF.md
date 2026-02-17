# Session Handoff

## Session Date: 2026-02-16

## What Was Done
Investigated spontaneous reboot of Heltec V3 after ~23 hours of unattended operation. Implemented 4 stability fixes targeting NVS flash wear and heap fragmentation.

### Context
- Device: Heltec V3 on COM3 (SPI: SCK=9, MISO=11, MOSI=10)
- Mode at crash: TARGETED capture, config 0 (Meshtastic_LF_906 @ 906.875 MHz)
- WiFi: "Sploosh" not available, running in AP-only mode
- Web UI: Unattended, no active clients
- Original reset reason lost (overwritten by subsequent power cycle)

### Fix 1: NVS write interval 10s -> 5 minutes
- `main.cpp:343` — changed `>= 10000` to `>= 300000`
- Old rate: ~41K writes/day. New rate: ~288/day
- NVS flash wear was the primary suspect for the 23-hour reboot

### Fix 2: Reset reason persistence in NVS
- `main.cpp` CrashContext — new `saveResetReason()` stores esp_reset_reason to NVS at boot
- `loadAndReport()` now reads and displays previous session's reset reason
- Boot always calls `loadAndReport()` then `saveResetReason()` regardless of reset type
- Next spontaneous reboot will show the actual cause (watchdog, brownout, panic, etc.)

### Fix 3: Static ring buffer replaces std::queue
- `packet_processor.h` — new `StaticRingBuffer<T, N>` template (push/front/pop/empty/size)
- Replaces `std::queue<QueuedPacket>` (which used `std::deque` with dynamic allocation)
- `std::vector<uint8_t> lastPacketData` replaced with `uint8_t lastPacketData[256]`
- Zero heap allocations for all packet queue operations

### Fix 4: Serial command 'i' for reset/health info
- `command_handler.h/.cpp` — `cmdResetInfo` in dispatch table
- `main.cpp` — `CrashContext::printResetInfo()` + extern wrapper
- Shows: reset reason, uptime, free/min heap, previous boot reason, NVS save count

## Current State
- Both `heltec_v3` and `t3_s3` build clean, zero warnings
- Heltec V3 uploaded (COM3) — verified boot, all subsystems OK
- T3-S3 uploaded (COM9)
- Commit: `147dcd9`

## What's Blocked
- Nothing blocked

## What to Watch For
- If reboots continue, boot log and `i` command will now show the actual reset reason
- NVS save count growth should be slow (~17/day vs ~41K/day previously)
- Monitor `min free heap` via `i` command to verify heap stability over multi-day runs

## Key Parameters
- PlatformIO: `c:/Users/tim/.platformio/penv/Scripts/pio.exe`
- Heltec V3 port: COM3
- T3-S3 upload port: COM9
- Branch: main

## Files Changed This Session
- `firmware/src/main.cpp` — NVS interval, reset reason persistence, printResetInfo, extern wrapper
- `firmware/src/packet_processor.h` — StaticRingBuffer template, static buffers
- `firmware/src/packet_processor.cpp` — memset/memcpy instead of vector ops
- `firmware/src/command_handler.h` — cmdResetInfo declaration + dispatch table entries
- `firmware/src/command_handler.cpp` — cmdResetInfo implementation
