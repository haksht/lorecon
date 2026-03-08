# Session Handoff — 2026-03-08

## What Was Done
- Committed two pending files from previous session (`command_handler.cpp`, `user_interface.cpp`)
- Fixed `monitor_rts = 0` / `monitor_dtr = 0` missing from `t3_s3` and `tbeam_supreme` envs in `platformio.ini` — closes #13
- Pushed all changes to main (commit 408b839)
- Closed issue #13 (serial monitor) with explanation; V4 serial unavailable by design (USB PHY kills radio)
- Closed issue #14 (128GB SD) with explanation — root cause is exFAT; fix is FAT32 reformat via Rufus, no firmware change needed
- Assigned issues #7, #12, #13, #14 to The-Foe for verification
- Reviewed issue #8 (packets seen not captured) — already documented in issue comments; waiting on The-Foe's LoraTap testing before any code change

## Current State
- Branch: `main`, all changes committed and pushed
- No uncommitted firmware changes
- All four boards (V3, V4, T3-S3, T-Beam Supreme) last confirmed working

## What's Blocked
- **T3-S3 PCAP**: needs ≤32GB FAT32 microSD card (user ordering one)
- **#8 decision**: waiting on The-Foe's short-packet testing before deciding on 20-byte threshold policy
- **#12 V4 GPS**: waiting on The-Foe field test with GPS fix acquired

## Next Steps
1. When SD card arrives — verify T3-S3 PCAP end-to-end
2. Wait for The-Foe feedback on #7, #8, #12
3. **#6** (recon scan settings) — backlog, too large to start now

## Key Parameters
- V3: COM3 (CP210x); V4: COM12 (native USB, no serial monitor); T3-S3: COM9 (running) / COM11 (download); T-Beam: COM10 (running) / COM11 (download)
- SD card requirement: FAT32, ≤32GB SDHC (SDXC/exFAT fails with Arduino SD library)
- PlatformIO: `c:/Users/tim/.platformio/penv/Scripts/pio.exe`

## Files Changed This Session
- `platformio.ini` — added `monitor_rts = 0` / `monitor_dtr = 0` to `t3_s3` and `tbeam_supreme` envs
- `firmware/src/command_handler.cpp` — (committed from previous session)
- `firmware/src/user_interface.cpp` — (committed from previous session)

---

# Previous Session — 2026-03-05

## What Was Done
- Diagnosed V4 PCAP download failure: `heltec_v4` env missing `-DHAS_SD_CARD`, so SD never initializes and PCAP is never written
- Confirmed T3-S3 already has `HAS_SD_CARD` correctly defined
- Diagnosed T3-S3 PCAP failure: 128GB SDXC card fails `f_mount` (FR_NOT_READY) — SDXC incompatibility with Arduino SPI SD library; fix is a ≤32GB SDHC card
- Fixed `cmdExitMenu` mode restore: was relying solely on NVS which gets wiped by webapp "Start Scan"; now uses in-memory snapshot captured at `cmdShowMenu` time, with NVS as fallback
- Fixed phantom keystrokes after `e`: added `while (Serial.available()) Serial.read()` flush in `cmdExitMenu` before radio resume
- Added `e` command to `showReconResults()` display (was in dispatch table and `showCommands()` but missing from the inline menu print)
- Confirmed V4 serial monitor is unusable (USB_CDC_ON_BOOT=0 + USB PHY kills radio)
- Confirmed T3-S3 serial monitor is safe to leave open — double-Enter gate prevents accidental menu activation

## Key Parameters
- T3-S3 SD pins (HSPI): SCK=14, MISO=2, MOSI=11, CS=13
- V4 SD pins (shared LoRa SPI): SCK=9, MISO=11, MOSI=10, CS=5 (placeholder — verify before enabling HAS_SD_CARD on V4)
- V4 upload port: COM12; T3-S3: COM9 (running), COM11 (download mode)
