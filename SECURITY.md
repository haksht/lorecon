# Security Policy

## Reporting a Vulnerability

If you discover a security vulnerability in this project, please report it responsibly:

### Contact

- **Email**: [Create a private security advisory on GitHub](https://github.com/haksht/lorecon/security/advisories/new)
- **Response Time**: We aim to respond within 72 hours

### What to Include

1. **Description** of the vulnerability
2. **Steps to reproduce** (proof of concept if possible)
3. **Impact assessment** (what could an attacker do?)
4. **Affected versions**
5. **Suggested fix** (optional)

### What to Expect

1. **Acknowledgment** within 72 hours
2. **Initial assessment** within 1 week
3. **Fix timeline** communicated within 2 weeks
4. **Credit** in CHANGELOG.md and release notes (unless you prefer anonymity)

### Scope

**In scope:**
- API authentication bypass
- Remote code execution
- Buffer overflows in packet parsing
- PSK key exposure beyond intended functionality
- Denial of service against the device

**Out of scope:**
- Physical access attacks (device already requires physical access)
- Social engineering
- RF jamming (hardware limitation)
- Vulnerabilities in third-party libraries (report upstream)

## Security Model

This tool is designed for **authorized security research** on networks you own or have permission to test. See [docs/reference/THREAT_MODEL.md](docs/reference/THREAT_MODEL.md) for detailed security assumptions.

### Design Principles

1. **Receive-only by default** — Active transmission requires explicit user action
2. **Local network only** — No cloud connectivity, no telemetry
3. **Token authentication** — Protected endpoints require `X-API-Token` header
4. **Minimal attack surface** — No shell access, no remote code execution

### Known Limitations

- **WiFi AP password** is device-unique but printed at boot (physical access = full access)
- **API token** is generated at boot and displayed on serial (physical access = full access)
- **No encryption** on local WiFi traffic (use HTTPS proxy if needed)
- **CORS allows all origins** — Acceptable for local-only tool

## Secure Configuration Checklist

- [ ] Change default WiFi credentials via web UI Settings
- [ ] Note your API token from serial output or `t` command
- [ ] Use AP mode (192.168.4.1) for isolated operation
