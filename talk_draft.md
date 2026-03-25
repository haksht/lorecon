# Sniffing the Silent Network
## Passive LoRa Reconnaissance on $30 of Hardware

**A 50-Minute Conference Talk**
Draft 1.0

---

## Presenters

**[FIELD]** — Hardware specialist and field operator. Responsible for board selection,
deployment testing, discovering what works and what doesn't in the real world.
Knows what the device looks like from the operator's seat.

**[CODE]** — Software developer. Wrote the firmware, the webapp, and the entire
analysis toolkit. Knows why every architectural decision was made and what it
cost.

*This split matters. When [FIELD] describes a problem in the field, [CODE]
explains how the software solved it — or failed to. The audience sees the full
loop: real-world need → technical implementation → real-world result.*

---

## Timing

| Segment | Who | Opens At | Duration |
|---|---|---|---|
| Hook — The Walk | Both | 0:00 | 4 min |
| Why LoRa, Why Now | FIELD | 4:00 | 6 min |
| The Hardware | FIELD | 10:00 | 5 min |
| What the Sniffer Sees | FIELD | 15:00 | 7 min |
| How It Was Built | CODE | 22:00 | 5 min |
| The Analyst Toolkit | CODE | 27:00 | 12 min |
| The Compliance Angle | CODE | 39:00 | 4 min |
| LoRaWAN | Both | 43:00 | 3 min |
| Demo | Both | 46:00 | 2 min |
| Close + Takeaways | Both | 48:00 | 2 min |

---

---

# SEGMENT 1 — Hook: The Walk
*Both presenters. 4 minutes.*

---

**SLIDE: [Single photo — an ordinary neighborhood street or city block at dusk]**

---

**[FIELD]:**
"A few weeks ago I went for a walk. About 45 minutes, maybe a mile and a half.
Nothing special — just a neighborhood. I had a device in my jacket pocket.
About the size of a deck of cards, ran on a battery, no laptop."

"When I got back I handed the SD card to [CODE]."

**[CODE]:**
"I plugged it in and ran our analysis tools against the log file. About ten
minutes of work."

"Here's what we had."

---

**SLIDE: The Results — [show session_analyzer dashboard screenshot, redacted]**

*Four panels: activity timeline, top devices by traffic, frequency distribution,
signal strength distribution. Real data from the walk.*

---

**[CODE]:**
"Twenty-three distinct devices captured. Meshtastic nodes, LoRaWAN sensors,
a couple of things we haven't fully identified yet."

"Eight of them were encrypted. Of those eight, three were using a default key
that ships with the firmware. We decrypted their traffic."

**[FIELD]:**
"No transmissions. No network connections. No special equipment beyond this."

*[Hold up the hardware.]*

"This talk is about how that works, what we built to do it, and what it means
for anyone running one of these networks."

---

**SLIDE: Three things you'll leave with**

1. How passive LoRa reconnaissance works — and why it's easier than most
   people assume
2. A complete open-source toolkit you can run today, no hardware required
   for the analysis side
3. A clear answer to whether your network looks like ours did

*[NOTE: Call out the audience split explicitly. "Whether you're here to build
something like this, audit a network with it, or figure out how to defend
against it — there's a track here for you." This earns the compliance and
ops people in the room.]*

---

---

# SEGMENT 2 — Why LoRa, Why Now
*[FIELD] leads. 6 minutes.*

---

**SLIDE: A Radio Nobody's Watching**

- **LoRa** = Long Range. A radio modulation designed for distance and low power.
- License-free spectrum: 915 MHz in the US, 868 MHz in Europe
- **Range**: 2 to 15 kilometers line-of-sight. Kilometers through buildings.
- **Who uses it**: Smart meters, asset trackers, agricultural sensors, weather
  stations, emergency mesh networks, logistics, disaster response

**[FIELD]:**
"WiFi has a range of about 50 meters and everyone has tools for it. LoRa
has a range of several kilometers and almost nobody in the security community
is looking at it. That gap is what this project is about."

---

**SLIDE: Meshtastic — The Ecosystem We Care About Most**

- Open-source firmware that turns cheap LoRa boards into encrypted mesh nodes
- Off-grid communication: hiking, events, emergency response, disaster areas
- Encrypted with AES — in theory
- Around 200,000 active users, growing after every major storm or outage
- No central infrastructure. Peer-to-peer. Each node relays for its neighbors.

**[FIELD]:**
"Meshtastic is well-intentioned and gaining real traction. After every hurricane
or wildfire there's a spike in new deployments. People are using it for things
that matter. Which is exactly why the security posture deserves scrutiny."

---

**SLIDE: One Thing About How LoRa Works**

*[Keep this non-technical — one clear analogy.]*

LoRa has a setting called Spreading Factor. Higher spreading factor = longer
range, but the packet spends more time in the air.

At maximum range settings, a single LoRa packet takes **nearly 3 seconds**
to transmit.

Compare that to a WiFi beacon: **less than 1 millisecond.**

**[FIELD]:**
"LoRa's greatest strength — reaching through walls and across kilometers — is
also what makes it uniquely easy to capture. A 3-second transmission window,
audible across 15 kilometers. That's a long time to be broadcasting."

---

**SLIDE: The Default Key Problem**

*[This slide stays on screen through the next talking point — let it sit.]*

```
Current default PSK:    1PG7OiQBrwBsSAFaGJjZVA==
Legacy (old firmware):  AQ==    Ag==    Aw==
Named channels:         "admin"   "priv"   "testing"   "dw"
```

**[FIELD]:**
"Meshtastic ships with a default encryption key. Every installation worldwide
starts with the same key. Users who never change it — and the app doesn't
push you to — are on a shared party line. Anyone else using the default key
can read every message."

"We test 23 known default keys against every packet we capture. Automatically.
That's what produced the results we showed you at the start."

*[NOTE: The crypto itself — AES-128-CTR — is solid. This isn't a
cryptographic break. This is a key management failure. Make that
distinction clearly so the Meshtastic project isn't unfairly maligned.
The vulnerability is deployment, not design.]*

---

---

# SEGMENT 3 — The Hardware
*[FIELD] leads. 5 minutes.*

---

**SLIDE: Why Not Just Use an SDR?**

*[SDR = Software Defined Radio — the common tool for RF research.]*

|  | Software-Defined Radio | This Tool |
|---|---|---|
| Cost | $25–$200 | $22–$55 |
| Laptop required | Yes | No |
| Setup time | Hours | 10 minutes |
| LoRa protocol decoding | DIY scripting | Automatic |
| Battery life in the field | Hours (with laptop) | Days |
| Fits in a pocket | No | Yes |

**[FIELD]:**
"SDRs are the right tool for a lot of RF problems. For LoRa recon — especially
for walking around, or leaving a device somewhere unattended for days — they're
the wrong tool. You need something that runs on its own."

---

**SLIDE: Four Boards, One Firmware**

*[Show physical hardware here — photo of all four boards side by side.]*

All four are built on the same chip family: **ESP32-S3 processor + SX1262
LoRa radio.** The firmware runs on all of them.

| Board | Price | Why You'd Pick It |
|---|---|---|
| Heltec WiFi LoRa 32 V3 | ~$22 | Simplest to start. Under $25. |
| Heltec V4 | ~$28 | Adds GPS module. Has a gotcha — [CODE] will explain. |
| LilyGO T3-S3 | ~$23 | Native SD card slot. |
| LilyGO T-Beam Supreme | ~$50 | GPS + real battery management. Field-ready. |

**[FIELD]:**
"The T-Beam Supreme is my daily driver for anything serious. It runs three or
four days on a single 18650 cell, has GPS so every packet gets a location
stamp, and the battery management chip means proper power-off — not just the
processor sleeping while the radio drains the battery."

"The Heltec V3 is the one I hand to someone who's never done this before. It's
$22, plug it in, flash the firmware, you're running."

---

**SLIDE: What Runs On It**

*[No code on this slide — describe the surfaces.]*

- **Firmware**: Flashed once. Runs indefinitely without a laptop.
- **OLED display**: Shows what it's doing in real time.
- **Web UI**: Connect from any phone or laptop browser via WiFi. No app to
  install. Full dashboard.
- **REST API**: Scriptable from any language. Our toolkit uses it.
- **WebSocket**: Live packet feed to any connected client.
- **SD card** (on T-Beam and T3-S3): Continuous CSV logging for offline
  analysis.

**[FIELD]:**
"The entire setup is: flash the firmware once, connect to the device's WiFi
hotspot from your phone, open a browser. Everything else is in the web UI."

---

---

# SEGMENT 4 — What the Sniffer Sees
*[FIELD] leads. 7 minutes.*

---

**SLIDE: Reconnaissance Mode — The Default**

When it boots, the device goes into **reconnaissance mode**: automatically
cycling through 26 different LoRa configurations.

*Why 26?* LoRa doesn't have one "channel." The combination of frequency,
spreading factor, bandwidth, and coding rate defines what you can hear. If
you're only listening on one config, you're deaf to everything else.

The 26 configs cover:
- Meshtastic US, EU, Australian, and Asian variants
- LoRaWAN / The Things Network frequencies
- Helium Network channels
- Generic ISM band beacons

12 seconds per config. Full sweep takes 5 minutes. Repeats continuously.

---

**SLIDE: The OLED — [photo of device running recon mode]**

*[Show the OLED cycling through configs. Live hardware if possible.]*

**[FIELD]:**
"In the field it looks like this. The display cycles through each config,
shows signal activity, counts packets. You can see it working without
touching a phone."

---

**SLIDE: What We Learn From Every Packet**

Even before attempting any decryption, each captured packet tells us:

- **Node ID**: A unique hardware identifier for that device
- **RSSI and SNR**: Signal strength and quality — a proxy for physical distance
- **Which LoRa config it's on**: Frequency, spreading factor, bandwidth
- **Transmission pattern**: How often does this device send?
- **Is it a router?**: Is it relaying traffic for other nodes?
- **Estimated firmware version**: Inferred from packet structure

**[FIELD]:**
"The router detection is a particularly useful piece of intelligence. A
Meshtastic router is a fixed node — usually plugged into wall power — that
relays messages for the rest of the mesh. Finding routers tells you where
the backbone of the network is. Those are your critical nodes."

---

**SLIDE: The Web UI — Devices Tab [screenshot]**

*[Show the Devices tab from the web UI, real data if possible, redacted if not.]*

**[FIELD]:**
"This is what you see from your phone after an hour of recon. Every device
that transmitted in range, ranked by packet count. You can see their
signal profile, their encryption status, and their vulnerability score.
Tap any one of them to switch into targeted mode."

---

**SLIDE: Targeted Capture Mode**

Once something interesting appears, switch to **targeted capture**:

- Lock onto a specific device or frequency configuration
- Watch one node indefinitely
- Capture payloads into a replay buffer
- Track RSSI trend — is the transmitter moving?

**[FIELD]:**
"The workflow is the same as any other kind of surveillance: wide-area scan
to find targets, then focused collection on what matters. That's what we
were doing during the walk."

---

**SLIDE: The Vulnerability Score**

Every device gets a score from 0 to 100. Higher is better for the device owner.

Deductions:
- **Default PSK in use** → −25 points *(messages are readable)*
- **Outdated firmware** → −20 points *(known unpatched issues)*
- **Very strong signal** → −15 points *(physically close, locatable)*
- **High traffic volume** → −15 points *(metadata exposure)*
- **Router role** → −10 points *(critical infrastructure, higher exposure)*

**Green** (80–100) / **Yellow** (60–79) / **Red** (below 60)

*[NOTE: Clarify this is a triage tool, not a verdict. A red score means
"prioritize this" not "this is owned." That framing will matter to the
compliance people in the room.]*

---

*[FIELD hands to CODE:]*

**[FIELD]:**
"That scoring, the protocol identification, the PSK testing — none of that
lives in my head. [CODE] built all of it. Let me hand off and let them
explain what's actually happening."

---

---

# SEGMENT 5 — How It Was Built
*[CODE] leads. 5 minutes. Keep it accessible — no code on screen.*

---

**SLIDE: Architecture in Plain English**

*[Show the ASCII diagram as a simple visual flow — boxes and arrows, not text.]*

```
Radio hardware → interrupt flag → packet queue → protocol analyzer
                                                        |
                                              PSK decryptor
                                                        |
                                           device tracker / state
                                            /                   \
                                      WebSocket               REST API
                                          |                       |
                                      Web UI               our toolkit
```

**[CODE]:**
"The radio fires an interrupt every time a packet arrives. The interrupt
does one thing: sets a flag. All the real work — reading the packet, analyzing
it, attempting decryption — happens in the main loop. That separation is
important on embedded hardware. If you do too much work inside an interrupt,
you miss the next packet."

---

**SLIDE: Two Modes, One Device**

**Reconnaissance** — the default:
Cycles all 26 configs, builds a picture of everything in range. Passive.
You learn who's out there.

**Targeted capture** — operator-triggered:
Locks onto one device or frequency. Captures payloads. Tracks over time.
You learn everything about one target.

**[CODE]:**
"The transition is intentional. Recon gives you the map. Targeted gives
you the detail. You don't lock onto something you haven't found yet."

---

**SLIDE: Two Things That Would Have Crashed It**

*[Pick two war stories — keep them short here, they're the hardware/field ones.
Full stories belong to [FIELD] later in Q&A or as backup slides.]*

**Problem 1: Heap fragmentation**
The packet queue was using a standard library data structure that allocates
and frees memory constantly. After 24 to 48 hours of operation, the
memory was fragmented enough to crash the device.

Fix: pre-allocate 100 packet slots at boot. Zero memory operations at runtime.
The device can now run for weeks.

**Problem 2: Dual-core data races**
The web server runs on a different processor core than the radio. Without
explicit locking, reading packet data from a browser request while the radio
was writing new packets produced garbage — or a crash.

Fix: atomic operations for counters, locks for complex structures, a pattern
that makes it hard to forget.

**[CODE]:**
"Embedded development is unforgiving about these. A race condition that
corrupts a string in a web app produces a bad response. A race condition
on an ESP32 crashes the device and loses everything in the buffer."

---

---

# SEGMENT 6 — The Analyst Toolkit
*[CODE] leads. 12 minutes.*

---

**SLIDE: The Toolkit**

*[Show as a categorized visual — not a file listing, more like a tool inventory.]*

```
┌─ ANALYSIS ──────────────────────────────────────────────────┐
│  session_analyzer     What happened during a capture?        │
│  ws_monitor           Live terminal feed from the device     │
│  enhanced_live_visualizer  5-panel real-time dashboard       │
├─ MESHTASTIC ────────────────────────────────────────────────┤
│  psk_auditor          Which networks are using weak keys?    │
│  psk_decrypt          Decrypt a specific captured packet     │
│  mesh_topology_analyzer  Map the network structure           │
├─ LORAWAN ───────────────────────────────────────────────────┤
│  lorawan_join_analyzer  DevNonce reuse, join storms          │
│  abp_detector         Find static-key devices                │
├─ GEOGRAPHY ─────────────────────────────────────────────────┤
│  position_tracker     Map device locations and movement      │
├─ COMPLIANCE + REPORTING ────────────────────────────────────┤
│  duty_cycle_monitor   Regulatory airtime analysis            │
│  recon_report         Client-ready security assessment       │
├─ FORENSICS ─────────────────────────────────────────────────┤
│  wireshark_exporter   Load captures into Wireshark           │
│  pcap_analyzer        Signal statistics from PCAP files      │
│  packet_differ        Side-by-side hex comparison            │
└─────────────────────────────────────────────────────────────┘
```

All Python. All open source. Most have a `--demo` mode that generates
synthetic data — no hardware needed to run them.

**[CODE]:**
"The device captures. These tools tell you what you captured. You can hand
a CSV file from the SD card to anyone on your team, and they can run the
full analysis without ever touching the hardware."

---

**SLIDE: Step 1 — What Happened? (`session_analyzer`)**

*[Show the 2×2 matplotlib dashboard screenshot.]*

Load any CSV from the SD card or API export. In about 30 seconds you see:

- **Timeline**: when was traffic heaviest during the session?
- **Top devices**: who was transmitting most, and how strong was their signal?
- **Frequency distribution**: which LoRa configs had the most activity?
- **Geographic scatter**: if GPS data is present, where were things?

**[CODE]:**
"This is your first look at a session. The difference between staring at
4,000 rows in a spreadsheet and actually understanding what happened."

---

**SLIDE: Step 2 — Who's Vulnerable? (`psk_auditor`)**

*[Show the audit report output — risk-colored, not raw terminal text.]*

Tests every captured packet against all 23 known default keys. Reports:

- Which default PSKs were found in use, identified by name
- Risk level: **CRITICAL** for current defaults, **HIGH** for legacy keys,
  **MEDIUM** for named channel derivations
- Which devices are on each vulnerable network
- How many packets were exposed

**[CODE]:**
"In a typical residential sweep we see a non-zero hit rate on the current
default key. These are real people who set up the app, confirmed it worked,
and never revisited the encryption settings."

---

**SLIDE: The Moment — `decrypt_reveal.html`**

*[Run this live or show pre-recorded. This is the centerpiece of the demo.]*

We built a presentation tool specifically for this talk.

It takes a captured packet and plays it out in three stages:

1. **Raw bytes**: looks like noise on screen
2. **Key match**: which of the 23 defaults unlocked it
3. **Reveal**: full decrypted message text, full screen, with an animation
   transition

*[NOTE: Practice the timing. If showing real captured data, strip
identifying node IDs — show the message type and content, not the
person. Showing a POSITION message (lat/lon coordinates) that decrypts
cleanly is more impactful than a text message that might embarrass
someone.]*

---

**SLIDE: Step 3 — How Does the Mesh Connect? (`mesh_topology_analyzer`)**

*[Show Graphviz network diagram output.]*

From passively captured routing and neighbor packets — without decrypting
anything — we can map the structure of the mesh:

- Which nodes relay for which others
- Node classification: **gateway**, **relay**, **edge**
- Mesh density and average connectivity
- Export to Graphviz for professional network diagrams

**[CODE]:**
"This is entirely passive. Meshtastic's routing metadata — hop counts,
relay packet IDs, neighbor reports — is unencrypted by design. The mesh
needs to route messages without knowing what's in them. That design
decision hands us the topology for free."

---

**SLIDE: Step 4 — Where Are They? (`position_tracker`)**

*[Show folium interactive map with device markers and movement trails.]*

- Plots every device on an interactive map (OpenStreetMap base)
- Movement trails for mobile devices
- Color-coded by protocol
- Self-contained HTML file — share it without a server

On the T-Beam Supreme, every captured packet is GPS-stamped with the
sniffer's location at the time of capture. With multiple passes through
an area from different angles, you can triangulate where a fixed
transmitter is physically located.

**[CODE]:**
"The 'walking survey' approach: walk the same area on different paths,
note where RSSI peaks for each device across passes. The peak is
centered on the transmitter. You can locate a fixed LoRa node to
within a city block without decrypting anything, without transmitting
anything, and without specialized equipment."

---

**SLIDE: Step 5 — The Deliverable (`recon_report`)**

*[Show the generated HTML report.]*

From any capture — SD card CSV or live API pull — generate a formatted
security assessment document:

- Device inventory with signal profiles
- Protocol distribution
- Encryption posture: default PSK hits, unencrypted traffic, coverage
- GPS tracking summary
- Prioritized risk findings with recommendations

Output formats: Markdown, HTML, or JSON.

**[CODE]:**
"This is the 'hand it to someone who needs to fix it' output. Whether
you're doing a legitimate audit engagement, presenting findings internally,
or just documenting what your own network looks like — the format matters."

---

**SLIDE: Compressed Replay — `timeline_replay`**

*[Show the Rich terminal UI running at 10× speed — use a recording.]*

Play back any captured session at configurable speed: 1×, 5×, 10×, 50×.

- Live stats update as replay progresses: device count, packet count,
  PSK cracks, GPS fixes
- **Auto-pauses on significant events**: PSK crack detected, GPS fix
  acquired, new device discovered
- Can broadcast to the live visualizer in sync

**[CODE]:**
"We'll use this in the demo. A 90-minute walk from [FIELD] compressed
into about 3 minutes, with device discovery accumulating in real time
and the PSK hit moment highlighted automatically."

---

---

# SEGMENT 7 — The Compliance Angle
*[CODE] leads. 4 minutes.*

---

**[CODE]:**
"I want to spend a few minutes on something that matters to a different
part of this audience — the people who operate these networks rather than
attack them."

---

**SLIDE: Duty Cycle Violations (`duty_cycle_monitor`)**

LoRa operates in license-free spectrum, but that spectrum comes with
regulatory limits on how much any device can transmit:

- **European Union (868 MHz)**: 1% duty cycle — transmit no more than
  36 seconds per hour per sub-band
- **United States (915 MHz)**: FCC dwell time limits per channel
- **Australia, Asia**: Similar regional constraints

This tool analyzes captures and reports per-device:

- Actual airtime, calculated from packet length and spreading factor
- Average and peak-hour duty cycle percentages
- Which transmitters exceed regulatory limits
- Violation timestamps

**[CODE]:**
"A device violating duty cycle limits is either misconfigured or
malfunctioning. Either way it's a regulatory problem and a network
health problem — a chatty rogue node can starve its neighbors of
airtime. This tool makes it a reportable finding."

---

**SLIDE: ABP Devices — The Static Key Problem (`abp_detector`)**

LoRaWAN devices join networks in two ways:

- **OTAA** (Over-The-Air Activation): session keys generated fresh on
  each join. Keys rotate. More secure.
- **ABP** (Activation By Personalization): static keys burned in at
  provisioning. Keys never rotate.

The problem with ABP: when a device power-cycles, its frame counter
resets to zero. That reopens the replay window for every message it
has ever sent.

Our detector identifies ABP devices from capture patterns — data
packets with no join request, frame counters resetting — and ranks
them by confidence.

**[CODE]:**
"ABP is a common shortcut taken during prototyping that makes it into
production. 'We'll fix it before deployment' rarely happens. This
tool gives you a ranked list of which devices need to be migrated to
OTAA, with the evidence to support the finding."

---

**SLIDE: Who Should Run This Against Their Own Network?**

- Organizations using Meshtastic for emergency communications — verify
  PSK hygiene before you actually need the network to work
- LoRaWAN network operators — audit for ABP devices, duty cycle
  violations, default AppKeys
- Smart building and campus IoT teams — asset trackers, environmental
  sensors, and access infrastructure all use LoRa
- Security teams expanding their wireless scope — LoRa is wireless,
  and it's almost never in the assessment

---

---

# SEGMENT 8 — LoRaWAN: A Harder Problem
*Both. 3 minutes.*

---

**SLIDE: LoRaWAN vs. Meshtastic**

| | Meshtastic | LoRaWAN |
|---|---|---|
| Architecture | Peer-to-peer mesh | Star: devices → gateways → cloud |
| Encryption | Shared PSK per channel | Per-device session keys |
| Default key problem | 23 known defaults tested | 16 TTN/Helium defaults tested |
| Key rotation | Manual | OTAA rotates automatically |
| Primary vulnerability | Default PSK deployment | ABP static keys, DevNonce reuse |

**[CODE]:**
"Meshtastic is the stronger story for this talk because the default key
problem is so widespread. LoRaWAN properly configured with OTAA is
significantly harder to attack passively. The attack surface is in the
misconfigurations."

---

**SLIDE: DevNonce Reuse — The Smoking Gun (`lorawan_join_analyzer`)**

**[FIELD]:**
"During our captures we see LoRaWAN join requests — devices authenticating
to a network. [CODE] built an analyzer specifically for these."

**[CODE]:**
LoRaWAN uses a 16-bit random number called DevNonce during device join.
It must never repeat — reuse allows replay attacks and makes session keys
predictable.

The join analyzer detects:

- **DevNonce reuse** → CRITICAL: replay attacks possible, keys predictable
- **Sequential nonces** → HIGH: brute-forceable
- **Join storms** → MEDIUM: device malfunction or active attack in progress

"A single CRITICAL finding here is a clean, documentable vulnerability with
a CVSSv3 score attached. This is the kind of thing that goes in a pentest
report."

---

---

# SEGMENT 9 — Demo
*Both. 2 minutes.*

---

**SLIDE: [Live or pre-recorded — announce which one upfront]**

*Option A: Live hardware present*

**[FIELD]** projects web UI from phone connected to Heltec V3.

Show sequence:
1. Boot — OLED cycling through recon configs
2. Devices tab — pre-seeded nodes or any live captures
3. Switch one node to targeted capture

**[CODE]** runs on laptop simultaneously:
1. `timeline_replay.py` against the pre-captured walk session at 10× speed
2. Device count climbing, PSK hit auto-pausing the replay
3. `decrypt_reveal.html` — the reveal animation

---

*Option B: No live hardware (recommended as backup regardless)*

"We're going to show you the toolkit against data we captured this week."

Run `timeline_replay.py --demo` or against real CSV. Follow with
`decrypt_reveal.html` loaded with the pre-baked real packet.

*[NOTE: Always have a recorded backup. LoRa traffic is unpredictable and
conference WiFi is hostile. The reveal.html runs entirely offline. The
most important thing to show is the decryption moment — if nothing else
works, that slide alone makes the point.]*

---

---

# SEGMENT 10 — Close + Takeaways
*Both. 2 minutes.*

---

**SLIDE: If You Run Meshtastic — Do These Four Things**

**[FIELD]:**

1. **Change your PSK.** Generate a random 32-byte key. Do not use a named
   channel.
2. **Update your firmware.** Devices running v1.x or early v2.x have
   known issues.
3. **Understand your exposure.** A signal above −50 dBm from the street
   means someone walking past knows roughly where you are.
4. **Disable router mode** on nodes that don't need to relay. Routers are
   infrastructure; they're also the most visible target.

---

**SLIDE: If You Operate LoRa Infrastructure — Do These Four Things**

**[CODE]:**

1. **Run a passive sweep of your own network** before someone else does.
   This toolkit is free. The findings are yours first.
2. **Audit for ABP devices.** If you find them, prioritize migration to OTAA.
3. **Test for default AppKeys and PSKs.** Treat every device as compromised
   until you have verified it isn't.
4. **Treat routing metadata as public information.** It is. Design your
   network with that assumption in place.

---

**SLIDE: The Uncomfortable Conclusion**

**[Both — alternate]**

**[FIELD]:** "LoRa mesh networks are increasingly used for emergency
communications, off-grid coordination, and community infrastructure.
They carry real messages from real people in situations where those
messages matter."

**[CODE]:** "A device that costs thirty dollars, that takes ten minutes
to flash, that runs for days without a laptop, can listen to all of it."

**[FIELD]:** "That's not a reason to stop using LoRa."

**[CODE]:** "It's a reason to configure it correctly."

---

**SLIDE: Get the Code + Questions**

```
github.com/[your-handle]/esp32-sniffer
```

- Complete firmware, webapp, and full Python toolkit
- Hardware bill of materials and build instructions in README
- Supported boards: Heltec V3/V4, LilyGO T3-S3, T-Beam Supreme
- All analysis tools have `--help` and most have `--demo` mode

*[NOTE: Anticipated questions to prepare for:]*

**"Can it transmit?"**
Yes. The SX1262 is bidirectional — passive capture is a firmware choice, not
a hardware constraint. Replay is in the toolkit and documented. We aren't
enabling transmission during this talk.

**"Is this legal?"**
Passive capture of RF in license-free ISM bands is the same legal question
as passive WiFi sniffing. Jurisdiction-dependent. Not legal advice. Use it on
networks you own or have authorization to audit.

**"What about detecting this tool being used against you?"**
Good question. Short answer: it's hard, because there's no transmission.
Longer answer involves RF emissions analysis and power fingerprinting — good
topic for a follow-up talk.

**"Can it do LoRaWAN server impersonation?"**
Out of scope for this tool. The packet captures are the prerequisite for
that work, but the active attack component is a separate problem.

**"The Heltec V4 — you said there was a gotcha?"**
[FIELD and CODE trade off this one — see Appendix A.]

---

---

# APPENDIX A — The Heltec V4 War Story
*Use if hardware questions come up in Q&A, or as a backup segment
if you have time to fill.*

---

**[FIELD]:**
"The V4 was supposed to be the better board. Same price as the V3, adds
GPS. I flashed the firmware and got nothing. Zero packets received.
The display was running, the web UI was up, but the radio was completely
silent."

"We spent days on this."

**[CODE]:**
"The V3 uses a dedicated USB bridge chip — a CP2102 — that sits between
the USB port and the ESP32. It's electrically isolated from the main chip."

"The V4 uses the ESP32's built-in USB controller. That controller shares
silicon and power rails with the rest of the chip, including the oscillator
the radio depends on."

"When the USB controller is active, it generates enough electrical noise
to disrupt the radio's crystal oscillator. The radio initializes correctly.
SPI communication works fine. It just never receives anything."

**[FIELD]:**
"We tested everything first. Antenna swap — no change. DIO2 RF switch —
no change. Reinstall — no change."

**[CODE]:**
"The fix: disable USB serial entirely on V4. `ARDUINO_USB_CDC_ON_BOOT=0`
in the build config. No serial monitor, ever, on this board. Everything
goes through the web UI."

"There's no software workaround for hardware-level interference.
'Radio initialized successfully' does not mean the radio is working."

**[FIELD]:**
"The lesson for anyone building on embedded hardware: your diagnostics
lie. The system can report nominal while the RF side is completely
broken. You need actual RF measurement to know."

---

---

# APPENDIX B — Spreading Factor Intuition
*For non-technical audience questions.*

---

Think of it like a conversation at a loud party.

- Speak fast and quiet: only the person next to you hears you.
- Speak slow and loud: everyone in the room hears you, but it takes longer.

SF7 = fast and short. Shorter range. Harder to capture.
SF12 = slow and long. 15 km range. Nearly 3 seconds in the air. Anyone
in that radius has the full packet.

The tradeoff that makes LoRa useful for long-range sensors is the same
tradeoff that makes it uniquely sniffable.

---

---

# APPENDIX C — Recommended Slide Production Notes

**Total slides**: Aim for 30–34. The talk breathes better with fewer slides
and more presenter time on each.

**Visuals to prepare** (in order of importance):

1. The session_analyzer 2×2 dashboard — real data from an actual walk,
   redacted if needed
2. The PSK audit report — risk-colored output, clear hit indication
3. The decrypt_reveal.html loaded and ready — practice the timing
4. Physical hardware photo — all four boards side by side, something
   to hold up during the hardware segment
5. The mesh topology Graphviz diagram — clean export, not terminal output
6. The position_tracker folium map — with visible device trails
7. The recon_report HTML — looks professional, not like a terminal dump
8. Network canvas map from the web UI — threat-colored nodes
9. OLED display photo mid-scan
10. timeline_replay running at speed — recorded video embedded in slides

**Opening photo**: Pick something mundane — a suburban street, a
conference hotel exterior, a parking lot. The contrast between the
ordinary setting and "we captured 23 devices here" is the hook.

**For the decrypt reveal**: If using real captured data, agree in advance
on what to show. Message content from a TEXT_MESSAGE portnum is personal.
POSITION (lat/lon coordinates), TELEMETRY (battery, temperature), or
NODEINFO (device name, hardware) makes the point just as clearly and is
less likely to embarrass a real person.

---

*End of draft. Version 1.0. Adjust speaker labels to real names before
sharing externally.*
