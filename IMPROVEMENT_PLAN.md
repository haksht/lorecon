# ESP32 LoRa Sniffer - Improvement Plan

Based on full code review (2026-02-17). Prioritized by impact and risk.

---

## P0: Correctness Bugs (fix first)

### ~~1. LoRaWAN detection is a tautology~~ DONE
Fixed in previous session. Proper MHDR/length/FCtrl validation. 9 regression tests in native test suite.

### ~~2. Duplicate `Logging` namespace in config.h~~ DONE
Removed dead comment block. Single `namespace Logging` with `constexpr bool PCAP_EXPORT_ENABLED` already correct.

### ~~3. `uint8_t = -1` for unused pins~~ DONE
Already uses `PIN_UNUSED = 0xFF` sentinel throughout.

---

## P1: Reliability (affects long-running operation)

### ~~4. Blocking serial I/O freezes the entire system~~ SKIPPED
Serial replay menu only runs when user explicitly enters it via USB. Web UI is the primary interface. Not worth the refactor.

### ~~5. String fragmentation in API response building~~ SKIPPED
ArduinoJson already handles most serialization. Only 1 String concat found in json_builders. NVS/heap fixes from previous session resolved actual stability issues (3.5+ hour stable runs confirmed).

### ~~6. Packet header extraction duplicated ~120 lines~~ DONE
Already extracted to `findAndExtractMeshtasticHeader()` with `MeshtasticHeader` struct. Both code paths call it.

---

## P2: Security (low risk given local-network threat model, but worth fixing)

### ~~7. Private-network auto-trust bypasses auth on device AP~~ SKIPPED
Personal research tool on local AP. Anyone with WiFi access already has physical proximity. Not a production service.

### ~~8. Token comparison leaks length via timing~~ DONE
Already uses fixed-length rejection (TOKEN_LENGTH is public) + volatile XOR constant-time comparison.

---

## P3: Architecture (technical debt, not urgent)

### ~~9. Extract God Object: `lora_recon_tool.cpp` (870 lines)~~ SKIPPED
Single-developer project. File is stable and functional. Refactoring risks introducing bugs for no user-facing benefit.

### ~~10. Replace globals with dependency injection~~ SKIPPED
45 tests work around globals fine. Not worth the effort for a personal embedded tool.

### ~~11. Firmware version estimation is unreliable~~ DONE
Already uses `~v2.2+ (est: encryption flag)` labeling pattern throughout. Clearly marked as estimates.

---

## P4: Quality of Life

### ~~12. Add heap pre-checks before large allocations~~ SKIPPED
These allocations happen once at boot. If heap is insufficient at boot, device can't function regardless.

### ~~13. Web app cache busting is manual~~ DONE
`Cache-Control: no-cache` already set globally in web_server.cpp. Removed redundant `?v=4` params.

### ~~14. No automated tests~~ DONE
45 native tests across 4 suites (channel_hash, ring_buffer, protocol_analyzer, security_scorer). Unity framework, PlatformIO native env.

---

## Summary

**8 of 14 items completed:** #1, #2, #3, #6, #8, #11, #13, #14
**6 items skipped** (not worth the effort for project scope): #4, #5, #7, #9, #10, #12

Plan complete as of 2026-02-17.
