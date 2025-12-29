# Threat Model

**Document Version:** 1.0  
**Last Updated:** December 2025  
**Applies to:** ESP32 LoRa Sniffer v2.2.x

---

## 1. System Overview

The ESP32 LoRa Sniffer is a **passive reconnaissance tool** for LoRa network security research. It captures, analyzes, and optionally replays LoRa packets across Meshtastic, LoRaWAN, and Helium networks.

### Components

```
┌─────────────────────────────────────────────────────────────┐
│                      ESP32-S3 Device                        │
├─────────────────────────────────────────────────────────────┤
│  ┌───────────┐  ┌───────────┐  ┌───────────┐               │
│  │ SX1262    │  │ Web UI    │  │ Serial    │               │
│  │ Radio     │  │ (LittleFS)│  │ Console   │               │
│  └─────┬─────┘  └─────┬─────┘  └─────┬─────┘               │
│        │              │              │                      │
│  ┌─────▼──────────────▼──────────────▼─────┐               │
│  │           Firmware (13K lines)          │               │
│  │  - RadioController (ISR-driven capture) │               │
│  │  - PacketProcessor (queue + analysis)   │               │
│  │  - PSK Decryption (23 known keys)       │               │
│  │  - API Controller (REST + WebSocket)    │               │
│  └─────────────────────────────────────────┘               │
│                         │                                   │
│  ┌──────────────────────▼──────────────────┐               │
│  │  Storage: NVS (credentials) + SD (logs) │               │
│  └─────────────────────────────────────────┘               │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Assets

| Asset | Sensitivity | Location |
|-------|-------------|----------|
| **Captured packets** | Medium | RAM (volatile), SD card (persistent) |
| **WiFi credentials** | High | NVS (encrypted flash partition) |
| **API token** | High | RAM (regenerated each boot) |
| **PSK key database** | Low | Firmware (publicly known keys) |
| **Device configuration** | Low | NVS |
| **GPS coordinates** | Medium | RAM, SD card |

---

## 3. Trust Boundaries

### Boundary 1: Physical Access
- **Inside**: Anyone with physical access to the device
- **Outside**: Remote attackers
- **Assumption**: Physical access = full compromise (by design)

### Boundary 2: WiFi Network
- **Inside**: Devices connected to the ESP32's AP or same LAN
- **Outside**: Devices on other networks
- **Control**: Token authentication for destructive operations

### Boundary 3: RF Environment  
- **Inside**: LoRa devices within radio range
- **Outside**: Devices outside radio range
- **Assumption**: All received RF data is untrusted

---

## 4. Threat Actors

| Actor | Capability | Motivation | Likelihood |
|-------|------------|------------|------------|
| **Curious user** | Physical access | Explore functionality | High |
| **Malicious insider** | Network access + token | Data exfiltration | Low |
| **Remote attacker** | Network access only | Device compromise | Very Low |
| **RF attacker** | Craft malicious packets | Buffer overflow, DoS | Low |

---

## 5. Threats and Mitigations

### T1: Unauthorized API Access
- **Threat**: Attacker on same network calls protected endpoints
- **Mitigation**: Token-based authentication with constant-time comparison
- **Residual Risk**: Low — Token required for destructive operations

### T2: Malformed Packet Buffer Overflow
- **Threat**: Crafted LoRa packet causes memory corruption
- **Mitigation**: 
  - Bounds checking on all packet operations
  - `strncpy` with explicit null termination
  - Static assertions on buffer sizes
  - Stack protector enabled (`-fstack-protector`)
- **Residual Risk**: Low — Defense in depth

### T3: Serial Command Injection
- **Threat**: USB noise or malicious input triggers mode changes
- **Mitigation**: 
  - Double-Enter activation requirement
  - 100ms debounce
  - 5-minute auto-deactivation
  - Non-printable character filtering
- **Residual Risk**: Very Low — Multiple layers

### T4: WiFi Credential Theft
- **Threat**: Attacker extracts stored WiFi passwords
- **Mitigation**: 
  - NVS storage (encrypted flash partition)
  - No API endpoint exposes credentials
  - Credentials only settable, not readable
- **Residual Risk**: Medium — Physical access + flash dump possible

### T5: Denial of Service
- **Threat**: Flood device with requests or packets
- **Mitigation**:
  - Rate limiting on API endpoints (10 req/sec)
  - 100-packet queue with overflow protection
  - 30-second watchdog timer
- **Residual Risk**: Low — Device recovers automatically

### T6: Cross-Site Scripting (XSS)
- **Threat**: Malicious data in captured packets rendered in web UI
- **Mitigation**: 
  - `escapeHtml()` on all user-controlled data
  - Protocol fields, node IDs, messages all escaped
- **Residual Risk**: Very Low — Systematic escaping

### T7: Replay Attack Abuse
- **Threat**: User replays packets to disrupt target networks
- **Mitigation**:
  - Authentication required for replay endpoint
  - Bounded repeat count (max 10)
  - Bounded delay (100ms minimum)
  - Explicit user action required
- **Residual Risk**: Accepted — Intended functionality for authorized testing

---

## 6. Security Controls Summary

| Control | Implementation | Status |
|---------|----------------|--------|
| **Authentication** | Token-based, constant-time compare | ✅ Implemented |
| **Input Validation** | Bounds checking, type validation | ✅ Implemented |
| **Memory Safety** | strncpy, static_assert, -fstack-protector | ✅ Implemented |
| **Rate Limiting** | 10 req/sec per endpoint | ✅ Implemented |
| **XSS Prevention** | escapeHtml() on all output | ✅ Implemented |
| **Secure Storage** | NVS for credentials | ✅ Implemented |
| **Watchdog** | 30-second hardware timer | ✅ Implemented |
| **Serial Protection** | Double-Enter + auto-deactivate | ✅ Implemented |

---

## 7. Accepted Risks

These risks are accepted as inherent to the tool's design:

1. **Physical access = compromise** — Intentional, device requires physical presence
2. **Local network traffic unencrypted** — Use HTTPS proxy if needed
3. **PSK keys are public** — Tool tests against known-weak keys only
4. **Replay capability exists** — Core feature for authorized testing
5. **GPS data captured** — User responsibility to handle ethically

---

## 8. Out of Scope

These are explicitly not protected against:

- **Hardware attacks** (JTAG, flash extraction, side-channel)
- **Supply chain** (compromised firmware downloads)
- **RF jamming** (physics, not software)
- **Social engineering** (user training issue)
- **Zero-day in ESP-IDF/Arduino** (upstream responsibility)

---

## 9. Incident Response

If a security incident occurs:

1. **Isolate** — Disconnect device from network
2. **Preserve** — Save SD card logs if applicable
3. **Report** — File GitHub security advisory
4. **Reset** — Factory reset clears NVS credentials

---

## 10. Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | December 2025 | Initial threat model |
