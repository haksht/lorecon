# Conference Talk: "Cracking the Mesh: Real-Time LoRa Network Reconnaissance"

**Duration:** 50 minutes (40 min presentation + 10 min Q&A)
**Presenters:**
- **[Your Name]** - Technical Architecture & Implementation
- **[Friend's Name]** - Security Applications & Field Operations

**Target Audience:** Security professionals, penetration testers, red teamers, wireless security researchers

---

## Talk Structure

### Opening: Why This Matters (3 min) - Friend

**Hook & Engagement**
- "How many of you have Meshtastic nodes?" (show of hands)
- "How many have LoRaWAN sensors?" (show of hands)
- "How many know if there are LoRa networks operating around you right now?" (pause)

**Real-World Scenario**
- Quick story: Conference/corporate environment with invisible mesh networks
- Security implications discovered during assessment
- Gap: Most security professionals don't inventory LoRa networks

**Thesis Statement**
> "For the next 50 minutes, we're going to show you how to find, analyze, and assess LoRa networks that are invisible to traditional security tools. And we're going to do it live, right here, right now."

**Transition**
- "Let me turn this over to [Your Name] to start the hunt..."

---

### Live Demo Setup (2 min) - You

**Starting the Reconnaissance**

```bash
python enhanced_live_visualizer.py --auto-detect --audio --web
```

**Show the Dashboard**
- Project on screen: 5-panel matplotlib visualization
- Empty state: "This is what the spectrum looks like before we find anything"
- Enable audio feedback (Geiger counter effect)
- Explain briefly: "Each click is a packet, different tones for different protocols"

**Let It Run**
- Dashboard stays visible for entire presentation
- Devices will accumulate in background
- Creates suspense: "Let's see what we find during the talk..."

**URL on slide:** `http://192.168.4.1` (for audience to follow along if they connect)

---

### Act 1: The Technical Reality (10 min) - You

**Slide: Three Protocols, One Spectrum**

**What We're Hunting:**
1. **Meshtastic** - Peer-to-peer mesh networks (off-grid communication)
2. **The Things Network (LoRaWAN)** - IoT sensor networks (smart cities, agriculture)
3. **Helium Network** - Blockchain-based IoT (decentralized coverage)

**Frequency Coverage:**
- 26 scanning configurations
- 902-928 MHz (US), 863-870 MHz (EU), 915-928 MHz (ISM)
- 5-minute scan cycle to cover all configs
- Point to dashboard: "We rotate through these automatically"

**Hardware Simplicity**
- Show photo of Heltec WiFi LoRa 32 V3
- "$35 ESP32-S3 development board"
- "SX1262 LoRa transceiver built-in"
- "OLED display, WiFi, SD card support"
- Total build cost: ~$50 including enclosure

**Architecture Highlights (Code Snippets)**

*Interrupt-Driven Reception:*
```cpp
void IRAM_ATTR radioISR() {
    if (g_radioController) {
        g_radioController->markPacketReceived();
    }
}
```
- "Atomic flags prevent race conditions"
- "Never miss packets, even in high-traffic environments"

*Queue-Based Processing:*
- 100-packet queue capacity
- Protocol detection: Meshtastic (0x00), LoRaWAN (MAC headers), Helium (longfi)
- GPS extraction from POSITION_APP packets
- Real-time WebSocket streaming to web UI

**The PSK Database (Tease)**
- "We have a database of 23 known Meshtastic Pre-Shared Keys"
- "Including leaked 2023 admin/debug keys"
- "We'll come back to what we can decrypt in Act 3..."

**Check Dashboard**
- Point to screen: "Anyone see devices showing up yet?"
- If packets visible: "Hear that audio feedback? That's [protocol] traffic"
- If not: "We're scanning... 5-minute full cycle"

---

### Act 2: The Security Implications (12 min) - Friend

**As Devices Accumulate, Explain Each Discovery**

**Protocol Identification (Point to Pie Chart)**
- "Blue = Meshtastic (mesh networks)"
- "Orange = LoRaWAN (IoT sensors)"
- "Green = Helium (blockchain IoT)"
- Discuss what each protocol reveals about the environment

**Signal Strength = Proximity (Point to RSSI Panel)**
- RSSI values and distance estimation
- "Strong signal (>-80 dBm) = within 100 meters"
- "Weak signal (<-120 dBm) = up to 10km away"
- Router detection: "If we see -60 dBm, it's probably in this building"

**Device Tracking Over Time (Point to Timeline)**
- Packet rate analysis
- "Burst patterns indicate activity"
- "Long-term monitoring reveals routines"

**Attack Scenarios - Real-World Examples**

1. **Corporate Mesh Networks**
   - Employees using Meshtastic for off-books communication
   - Building layout inference from signal propagation
   - Movement patterns revealing sensitive areas
   - **Risk:** Bypasses corporate network security controls

2. **Event Security Failures**
   - VIP protection teams using mesh radios
   - GPS leakage revealing protectee locations
   - Channel interception exposing security plans
   - **Risk:** Physical security compromise

3. **IoT Sensor Networks**
   - Smart building sensors (LoRaWAN) advertising presence
   - Agricultural sensors revealing crop data
   - Environmental monitors leaking operational patterns
   - **Risk:** Competitive intelligence, operational security

4. **Supply Chain Tracking**
   - Logistics companies using LoRa asset trackers
   - Shipping container sensors revealing routes
   - Inventory levels leaked through sensor telemetry
   - **Risk:** Supply chain disruption planning

**Security Assessment Framework**
- Show `security_scorer.h` logic:
  - Router detection (high-value targets)
  - High RSSI (proximity threat)
  - Default PSK usage (configuration weakness)
  - Encryption status (plaintext = critical)

**Scoring Example:**
```
Score: 85/100 (High Risk)
- Router: Yes (40 points)
- High RSSI: Yes (20 points)
- Default PSK: Yes (20 points)
- Encrypted: Yes (5 points)
Rating: 🔴 High Risk
```

---

### The GPS Reveal (4 min) - Friend

**"And We Know Where They Are..."**

**Pull Up Interactive Map**
- Pre-generated map: `screenshots/lora_map.html` or live data
- OpenStreetMap with device markers
- Color-coded by protocol (blue/orange/green)

**Zoom Sequence (Dramatic Build)**
1. Start zoomed out: City view
2. Zoom to neighborhood: "Multiple devices in this area"
3. Zoom to building: "This is the conference center"
4. Zoom to room: "Three devices are in this room right now"

**Click Markers for Details**
- Device ID (node ID)
- Protocol type
- Packet count
- Average RSSI
- Exact coordinates (latitude/longitude with 6 decimal precision)

**Privacy Implications**
- Position broadcasts every 30-900 seconds (Meshtastic default)
- Historical movement tracking from captured data
- "We can generate animated trails showing movement over time"
- Reference: `position_tracker.py --animate`

**Audience Reaction Moment**
- Pause for effect
- "This is passive reconnaissance. We sent zero packets."
- "Everything you're seeing is broadcast in the clear."

---

### Act 3: The Encryption Deep Dive (10 min) - You

**Slide: The Encryption Reality**

**What Can Be Decrypted (Meshtastic Focus)**

**Two-Tier Encryption Model:**

1. **Channel Messages (PSK-Encrypted)** ✅ Decryptable
   - AES-256-CTR with shared channel key
   - Broadcast to all mesh nodes
   - Includes: TEXT_MESSAGE_APP, POSITION_APP, TELEMETRY_APP
   - "If you know the PSK, you can read everything on that channel"

2. **Direct Messages (PKC-Encrypted)** ❌ Not Decryptable
   - Curve25519 public key cryptography (firmware 2.5.0+)
   - End-to-end encryption
   - Only sender/receiver can decrypt
   - "DMs are actually secure IF users use them"

**The 23-Key Database**

**Show PSK List (Excerpt):**
```
1. "AQ==" (default)               - Risk: CRITICAL
2. "1PG7OiApB1nwvP+rz05pAQ=="    - Risk: HIGH (leaked 2023 admin)
3. "UGxlYXNlQ2hhbmdlVGhpc0tleQ==" - Risk: HIGH (documented)
...
23. Community channel keys
```

**PSK Testing Process:**
```cpp
for (const auto& key : defaultKeys) {
    if (testDecrypt(packet, key)) {
        // Decryption successful
        extractPayload();
        logChannel(keyIndex);
        break;
    }
}
```

**Live Decrypt Demo (If Traffic Captured)**
- Find encrypted packet in queue
- Run through PSK database
- Show plaintext output
- "This was a TEXT_MESSAGE_APP: 'Anyone copy?'"

**Attack Dashboard Demo**
- Open: `tools/visualization/attack_dashboard.html`
- Standalone HTML file with real-time vulnerability assessment
- Show:
  - Device risk scores
  - Default PSK detection
  - Unencrypted packet warnings
  - Router identification

**Controversy: The "Leaked 2023 Keys"**
- Found in public GitHub repos
- Used by developers for testing
- Never rotated in production deployments
- "We're not disclosing new keys - these are already public"
- Ethics discussion: Responsible disclosure vs. security research

**What We DON'T Do**
- Replay attacks (we capture, not transmit)
- Jamming (passive only)
- Private key extraction (not possible with PSK-based channel crypto)

---

### Act 4: Defense & Detection (8 min) - Friend

**What Security Teams Should Do**

**1. Network Inventory**
- "You can't protect what you don't know exists"
- Recommended: Monthly LoRa reconnaissance scans
- Document all discovered devices/networks
- Identify business-critical vs. rogue deployments

**Hunting in Your Environment:**
```bash
# Quick 1-hour scan
python enhanced_live_visualizer.py --auto-detect --duration 3600 --record

# SSH/headless monitoring
python ws_monitor.py --filter meshtastic

# API-based automation
python api_client.py status
```

**2. Configuration Auditing**

**Using the PSK Auditor:**
```bash
python tools/meshtastic/psk_auditor.py --scan-duration 1800
```

**Checks for:**
- Default PSK usage (CRITICAL)
- Weak channel keys (HIGH)
- Unencrypted broadcasts (CRITICAL)
- Router misconfigurations (MEDIUM)

**Sample Output:**
```
[🔴 CRITICAL] Device 0x4FA2E: Default PSK detected
[🟡 MEDIUM]   Device 0x8B3C1: Router with high RSSI
[🟢 LOW]      Device 0x1A7F2: Custom PSK, encrypted DMs
```

**3. Coverage Mapping**

**Defensive Use Cases:**
- "Where are blind spots in our monitoring?"
- "Can adversaries communicate undetected?"
- "What's the effective range of our mesh network?"

**Tools for Mapping:**
- Export to KML: `api_client.py export --format kml`
- Open in Google Earth Pro
- Overlay with facility maps
- Identify coverage gaps

**4. Policy & Governance**

**Questions Security Teams Must Answer:**
- Is LoRa usage authorized in our environment?
- What protocols are allowed (Meshtastic? LoRaWAN? Helium)?
- Who manages device provisioning and key rotation?
- What's our incident response for rogue devices?

**Recommended Controls:**
- Mandatory PSK rotation (90-day cycle)
- Disable plaintext channels
- Enable DM-only mode for sensitive communications
- Log all mesh traffic to SIEM (via this tool's JSON output)

**5. Disclosure Challenges**

**Who Do You Report To?**
- Meshtastic: Open-source project, no security team
- LoRaWAN: Network operator (TTN, private networks)
- Helium: Decentralized - no central authority
- Corporate deployments: IT/Security team

**Recommended Approach:**
- Document findings with screenshots/maps
- Provide mitigation recommendations
- Give 90-day disclosure window
- Consider public disclosure for widespread issues

---

### Demo Payoff: What We Found Today (3 min) - Both

**Return to Live Dashboard**

**Final Statistics (Point to Each Panel):**
- Total devices discovered: X
- Total packets captured: Y
- Protocol breakdown (pie chart)
- Signal strength distribution
- Timeline of discovery

**Auto-Saved Artifacts:**
- Screenshots at milestones (10, 50, 100, 500 packets)
- Final dashboard screenshot
- Interactive map: `screenshots/lora_map.html`
- CSV export: `logs/capture_YYYYMMDD_HHMMSS.csv`

**Pull Up Generated Map (Final Reveal)**
- Show all devices discovered during talk
- Zoom to venue
- "This is what was in the air for the last 45 minutes"

**Post-Processing Demo:**
```bash
python session_analyzer.py logs/capture_20260122_1400.csv --bin-seconds 60
```
- Timeline analysis
- Frequency distribution
- GPS scatter plot
- Export options (PCAP, GeoJSON, KML)

---

### Closing: Open Source & Community (2 min) - Both

**GitHub Repository**
- **You:** "All code is open source: github.com/[your-repo]"
- Documentation: README.md, API_REFERENCE.md, BUILD_GUIDE.md
- Hardware guide: Heltec V3 setup walkthrough
- Python tools: 25+ analysis scripts

**Build Your Own**
- Total cost: ~$50
- Build time: 30 minutes
- Flash guide: `docs/user-guides/FLASH_GUIDE.md`
- Pre-compiled binaries available (release page)

**Real-World Applications**
- **Friend:** "I use this for security assessments, penetration tests, red team ops"
- Network inventory audits
- Wireless security training
- Research into LoRa protocol vulnerabilities

**Ethics & Responsible Use**
- "This is a dual-use tool"
- Intended for: Security research, authorized testing, network defense
- Not intended for: Unauthorized surveillance, privacy violations, illegal interception
- "Know your local laws - RF regulations vary by country"

**Call to Action**
- "Download the code"
- "Test it in your environment"
- "Submit PRs with improvements"
- "Share your findings (responsibly)"

---

### Q&A (10 min) - Both

**Anticipated Questions:**

**Q: Is this legal?**
**A (Friend):** "Passive reception of RF signals is generally legal in most jurisdictions, but check local laws. We don't transmit, jam, or interfere. Similar to listening to police scanners."

**Q: Can you decrypt LoRaWAN traffic?**
**A (You):** "Not easily. LoRaWAN uses device-specific AppSKeys. We can identify devices and see metadata (DevAddr, frame counters), but payload decryption requires the application key."

**Q: What about Helium?**
**A (You):** "Similar to LoRaWAN - we can see network activity, device counts, but encrypted payloads are protected. We focus on network enumeration."

**Q: How far can this detect devices?**
**A (Friend):** "Depends on transmit power and terrain. We've seen up to 15km line-of-sight. Urban environments: 1-3km typical. Indoors: 100-500 meters."

**Q: Can you locate devices without GPS?**
**A (You):** "We can do RSSI-based proximity estimation, but true geolocation requires GPS in the packet payload. Some devices don't broadcast position."

**Q: What's the battery life for field work?**
**A (Friend):** "With USB power bank (20,000 mAh): 12-16 hours. Enough for all-day conferences or overnight captures."

**Q: Can this be used for defense/blue team?**
**A (Friend):** "Absolutely. That's a primary use case. Inventory your own networks, find rogue devices, audit configurations. It's like Wireshark for LoRa."

**Q: Will you add [feature X]?**
**A (You):** "Check the GitHub issues. If it's not there, open a feature request. Better yet, submit a PR - we review all contributions."

---

## Presentation Logistics

### Equipment Needed
- **Laptop** with Python 3.x, all dependencies installed
- **Heltec WiFi LoRa 32 V3** board (primary)
- **Backup Meshtastic node** (your own device for guaranteed traffic)
- **HDMI cable** for projector
- **USB power bank** (backup power)
- **Audio cable** (if venue audio system available for Geiger counter effect)

### Pre-Talk Setup (30 min before)
1. Test projector connection
2. Launch visualizer in demo mode: `--demo` flag (no hardware, simulated traffic)
3. Verify audio output (test on laptop speakers)
4. Load backup map/screenshots (in case live demo fails)
5. Start actual capture 10 minutes before talk begins

### Backup Plans
- **No RF traffic:** Use `--demo` mode or replay previous capture with `session_analyzer.py`
- **Visualizer crashes:** Switch to web UI at `192.168.4.1`
- **Projector fails:** Have screenshot deck ready in slides
- **No questions:** Show hardware teardown, OLED interface demo

### Timing Checkpoints
- 5 min: Should be in Act 1
- 15 min: Should be in Act 2
- 25 min: Should be in GPS reveal
- 35 min: Should be in Act 4
- 40 min: Closing
- 50 min: End Q&A

### Slide Deck (Minimal)
- Title slide with GitHub URL
- Hardware photo (Heltec V3)
- Architecture diagram (optional)
- PSK database table
- Security scoring rubric
- Closing slide with contact info

**Most content is live demo - slides are backup/reference only**

---

## Post-Talk Follow-Up

### Share with Attendees
- GitHub repository link
- Slides (if used)
- Generated map from live capture
- CSV export of session data
- Conference WiFi SSID for continued captures

### Blog Post / Write-Up
- "Cracking the Mesh: Live Recon at [Conference Name]"
- Include screenshots from talk
- Statistics from capture (devices, protocols, GPS points)
- Link to recording (if available)

### Social Media
- Twitter: Demo video clip with Geiger counter audio
- YouTube: Full talk recording
- GitHub: Tag release as "CackalackyCon 2026 Edition"

---

## Speaker Notes

### For You (Technical Presenter)
- **Pace:** Slow down during code snippets - let audience read
- **Jargon:** Define terms first time (PSK, RSSI, ISR, etc.)
- **Live coding:** Avoid it - show snippets in slides instead
- **Dashboard:** Point explicitly ("top-left panel shows...") - don't assume audience can navigate visually

### For Friend (Security Presenter)
- **Stories:** Real examples > hypothetical scenarios
- **Simplify:** Not everyone is a pentester - explain attack chains
- **Enthusiasm:** This is cool! Show excitement when devices appear
- **Pause:** After GPS reveal, give audience time to react

### Transition Phrases
- **You → Friend:** "Now that you see HOW it works, [Friend] will explain WHY that should concern you..."
- **Friend → You:** "Let me bring [Your Name] back to show you the actual decryption process..."
- **Both together:** "While [Your Name] pulls up the code, let me explain the security implications..."

### Energy Management
- **High energy:** Opening, GPS reveal, demo payoff
- **Medium energy:** Technical sections, defense strategies
- **Low energy:** Q&A (conversational tone)

### Dealing with Technical Failures
- If visualizer crashes: "And this is why we have backups..." (stay calm, switch to web UI)
- If no traffic found: "Quiet spectrum today - let me show you a previous capture..." (use demo mode)
- If audio fails: "Well, the audio worked in rehearsal..." (joke, move on)

---

## Practice Schedule

### Week Before
- Full run-through with timer (50 min exactly)
- Practice handoffs between presenters
- Test all equipment/software
- Prepare backup scenarios

### Day Before
- Venue walkthrough (if possible)
- Test projector resolution (adjust matplotlib DPI if needed)
- Charge all batteries
- Run demo mode overnight (stability test)

### 1 Hour Before
- Arrive early for tech check
- Run full setup sequence
- Start live capture (real or demo)
- Hydrate, warm up voices

---

**Good luck at CackalackyCon 2026! You're going to crush it.** 🎤📡
