# Session Handoff — 2026-03-09

## What Was Done
- Added `monitor_rts = 0` / `monitor_dtr = 0` to `t3_s3` and `tbeam_supreme` envs (closes #13)
- Committed two pending files from 2026-03-05 session (`command_handler.cpp`, `user_interface.cpp`)
- Closed GitHub issues #13 (serial monitor) and #14 (128GB SD) with explanations
- Assigned issues #7, #12, #13, #14 to The-Foe for verification
- Added `[RECON]` serial print to `handleReconPacket()` so visualizer works in recon mode (not just targeted mode)
- Fixed visualizer packet count: `[SMALL]` packets (<40 bytes) now include node ID and match visualizer regex
- Fixed GPS: `storePoint()` now prints `GPS POSITION EXTRACTED!` block to serial so visualizer plots node positions
- Added `m` keypress in plot window to export live folium map to browser on demand
- Fixed histogram title to show "top N of M devices" when truncated
- Uploaded new firmware to T3-S3; confirmed visualizer working with live traffic overnight

## Current State
- Branch: `main`, all changes committed and pushed (latest: c3e4a21)
- T3-S3 running new firmware on COM9, visualizer working
- Visualizer now parses `[RECON]`, `[CAPTURE]`, and `[SMALL]` packet lines
- GPS map panel populates when position packets are decoded; press `m` in plot window for folium map
- Packet count still lower than device total — remaining gap is packets <20 bytes (no serial print at all) and `[CAPTURE]` packets where PSK line has no node ID

## What's Blocked
- **T3-S3 PCAP**: still waiting on ≤32GB FAT32 microSD card — most tools in tools/ blocked on this
- **#8**: waiting on The-Foe's LoraTap short-packet testing before deciding on 20-byte threshold
- **#12 V4 GPS**: no satellite fix after 24hrs + driving — diagnosed as antenna placement issue (ceramic patch needs sky-facing orientation); commented on issue; firmware init is correct

## Next Steps
1. When SD card arrives — verify T3-S3 PCAP end-to-end; then test session_analyzer, timeline_replay, recon_report, pcap_analyzer, wireshark_exporter
2. Test ws_monitor.py and api_client.py (work now, just need device on WiFi)
3. Wait for The-Foe feedback on #7, #8, #12
4. **#6** (recon scan settings) — backlog

## Tools Status
**Tested:** enhanced_live_visualizer.py (serial, recon+targeted modes, GPS panel, folium map, audio, demo mode)
**Ready to test now:** ws_monitor.py, api_client.py
**Blocked on SD card:** session_analyzer.py, timeline_replay.py, recon_report.py, pcap_analyzer.py, wireshark_exporter.py, duty_cycle_monitor.py, mesh_topology_analyzer.py
**Offline/standalone (can test anytime):** packet_differ.py, psk_decrypt.py, psk_auditor.py, meshtastic_decoder.py, lorawan_join_analyzer.py, abp_detector.py, position_tracker.py

## Key Parameters
- T3-S3: COM9 (running), COM11 (download mode); upload: `pio run -e t3_s3 -t upload --upload-port COM9`
- V3: COM3; V4: COM12 (no serial monitor); T-Beam: COM10 (running) / COM11 (download)
- Visualizer: `python tools/enhanced_live_visualizer.py COM9` — press `m` in plot window for map export
- SD card requirement: FAT32, ≤32GB SDHC

## Files Changed This Session
- `platformio.ini` — monitor_rts/dtr for t3_s3 and tbeam_supreme
- `firmware/src/command_handler.cpp` — (committed from 2026-03-05 session)
- `firmware/src/user_interface.cpp` — (committed from 2026-03-05 session)
- `firmware/src/packet_processor.cpp` — [RECON] serial print in handleReconPacket; [SMALL] format with node ID
- `firmware/src/geo_intelligence.cpp` — GPS POSITION EXTRACTED serial block in storePoint()
- `tools/enhanced_live_visualizer.py` — [SMALL] regex, histogram title fix, 'm' map export keypress

---

# Previous Session — 2026-03-05

## What Was Done
- Diagnosed V4 PCAP failure: `heltec_v4` env missing `-DHAS_SD_CARD`
- Diagnosed T3-S3 PCAP failure: 128GB SDXC fails `f_mount` — needs ≤32GB SDHC
- Fixed `cmdExitMenu` mode restore; added serial flush on exit
- Added `e` to `showReconResults()` display

## Key Parameters
- V4 SD pins (shared LoRa SPI): SCK=9, MISO=11, MOSI=10, CS=5 (placeholder — verify before enabling HAS_SD_CARD on V4)
