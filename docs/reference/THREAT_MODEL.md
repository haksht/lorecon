# Threat Model

**Document Version:** 1.1
**Last Updated:** March 2026
**Applies to:** LoRecon v2.5.x

---

## 1. System Overview

LoRecon is a **passive reconnaissance tool** for LoRa network security research. It captures, analyzes, and optionally replays LoRa packets across Meshtastic, LoRaWAN, and Helium networks.

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
│  │           Firmware (16K lines)          │               │
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
| **AP password** | High | NVS (encrypted flash partition) |
| **API token** | Low | RAM (regenerated each boot) — see §3 |
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
- **Control**: **AP password is the real gate.** See §5 T1 for full explanation.

The device operates in two network modes:

| Mode | Client subnet | Auth behavior |
|------|--------------|---------------|
| AP (`192.168.4.x`) | Direct connection to device AP | Token required; auto-retrieved on first request |
| STA/AP_STA (LAN) | External hotspot/home network | RFC 1918 clients auto-trusted; no token check |

**Implication**: Any client that can join the network — AP or LAN — effectively has full API access. The AP password is therefore the only meaningful access control. The API token provides a defense-in-depth layer for edge cases (e.g., a client arriving via VPN with a non-RFC-1918 address) but is not the primary barrier.

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
- **Mitigation**: AP password controls network access. Once on the network, clients are either auto-trusted (LAN) or auto-issued a token (AP subnet via `/api/auth/token`). Token validation with constant-time comparison applies in both cases but is not a meaningful barrier against anyone who can join the network.
- **Why this is correct**: This is a single-owner field device, not a multi-tenant system. The relevant adversary is someone who is *not* on your network — and they are blocked at the WiFi layer. Requiring a separate password after WiFi authentication would add friction with no security benefit against realistic attackers.
- **Residual Risk**: Medium — entirely dependent on AP password strength. A weak AP password = full device access. Default password is `recon-XXXXXX` (MAC-derived, unique per device). Change it via Settings → AP Password before field use.
- **What the token actually protects against**: A rogue device that somehow obtains LAN access without knowing the AP password (e.g., via the hotspot network the device has joined). In that scenario the RFC 1918 auto-trust would grant access — the token would block it. This is a low-probability threat for typical field use.

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
| **AP Password** | WPA2, configurable via webapp Settings | Implemented |
| **Authentication** | Token-based, constant-time compare (secondary layer) | Implemented |
| **Input Validation** | Bounds checking, type validation | Implemented |
| **Memory Safety** | strncpy, static_assert, -fstack-protector | Implemented |
| **Rate Limiting** | 10 req/sec per endpoint | Implemented |
| **XSS Prevention** | escapeHtml() on all output | Implemented |
| **Secure Storage** | NVS for credentials | Implemented |
| **Watchdog** | 30-second hardware timer | Implemented |
| **Serial Protection** | Double-Enter + auto-deactivate | Implemented |

---

## 7. Accepted Risks

These risks are accepted as inherent to the tool's design:

1. **Physical access = compromise** — Intentional, device requires physical presence
2. **AP password = full access** — Anyone who joins the WiFi network has effective API access. This is a deliberate design choice: the device has one owner, and network access is the appropriate trust boundary.
3. **LAN clients auto-trusted** — Any RFC 1918 client on the STA network gets full API access without a token. Acceptable because the hotspot owner controls who is on that network.
4. **Local network traffic unencrypted** — Use HTTPS proxy if needed
5. **PSK keys are public** — Tool tests against known-weak keys only
6. **Replay capability exists** — Core feature for authorized testing
7. **GPS data captured** — User responsibility to handle ethically

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
