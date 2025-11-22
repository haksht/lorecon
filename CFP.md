# Conference Talk Proposal: CackalackyCon

## Title
**"LoRa in the Wild: Building a $40 ESP32 Radio Sniffer"**

## Abstract

Ever wonder what's flying through the airwaves around you? Meshtastic networks, LoRaWAN sensors, Helium hotspots - there's an entire invisible radio ecosystem operating on sub-GHz frequencies that most people never see. In this talk, I'll show you how to build a portable LoRa reconnaissance tool using a $40 Heltec ESP32 board and open-source software.

We'll cover:
- **The LoRa landscape**: What protocols are out there and why you should care
- **Building your sniffer**: Hardware selection, firmware flash, getting your first captures
- **Live demo**: Real-time network topology visualization showing discovered devices
- **Packet analysis**: What you can learn from captured traffic (with responsible disclosure principles)
- **SD card logging**: Field data collection for later analysis with Python visualization tools
- **Going deeper**: PSK decryption attempts, protocol fingerprinting, and understanding mesh network behavior

This is a practical, hands-on talk for anyone curious about radio hacking on a budget. You'll leave knowing how to build your own tool, what's legal/ethical to monitor, and how to contribute to the open-source LoRa research community. Perfect for hardware hackers, radio enthusiasts, or anyone who wants to peek behind the curtain of long-range wireless networks.

## Talk Details

**Skill Level**: Beginner to Intermediate (no RF background required)  
**Format**: Demo-heavy presentation with live captures  
**Duration**: 45 minutes (30 min presentation + 15 min demo/Q&A)  
**Recording**: OK to record and publish

## Why This Talk Fits CackalackyCon

✅ **Practical & Actionable** - attendees can build this themselves for ~$40  
✅ **Entertaining** - live demos showing devices appearing on network map in real-time  
✅ **Counter-culture vibes** - exploring invisible infrastructure, DIY tools over commercial solutions  
✅ **Village-friendly** - could complement hardware hacking village activities  
✅ **Great visuals** - interactive network topology map looks impressive on projector  
✅ **Budget hacking** - aligns with maker/hacker ethos (cheap hardware, powerful results)

## Talk Outline

### Part 1: Introduction (5 min)
- What is LoRa? Why should you care?
- Real-world use cases: Meshtastic mesh networks, LoRaWAN IoT sensors, Helium network
- Legal/ethical considerations (monitoring vs. interception)

### Part 2: Hardware & Setup (10 min)
- Bill of materials (~$40 Heltec WiFi LoRa 32 V3)
- Why ESP32-S3 + SX1262 is perfect for this
- Quick firmware flash demo (PlatformIO)
- First boot and OLED display walkthrough

### Part 3: Live Capture Demo (10 min)
- Start reconnaissance mode
- Watch devices appear on OLED display
- Switch to web interface
- **Show network topology map** - devices positioned by signal strength
- Click through device details
- Demonstrate frequency targeting and device targeting modes

### Part 4: Data Analysis (10 min)
- SD card logging structure (CSV format with GPS, frequency, RSSI, encryption status)
- Python analyzer tool demo (matplotlib visualization)
- What you can learn: device types, firmware versions, routing behavior, power classes
- PSK decryption attempts (educational perspective)

### Part 5: Responsible Use & Q&A (10 min)
- Legal considerations (monitoring public airwaves vs. content interception)
- Responsible disclosure if you find vulnerabilities
- Contributing to open-source LoRa research
- Open Q&A with live demos

## Demo Equipment

1. **Heltec WiFi LoRa 32 V3** - running live captures
2. **Laptop** - showing web interface with network map
3. **SD card** - example dataset for visualization demo
4. **Backup device** - in case of demo gremlins
5. **Meshtastic device** (optional) - to generate test traffic if needed

## Interactive Elements

- **Live captures during talk** - if attendees have Meshtastic devices, we'll see them appear!
- **Network map visualization** - real-time graph showing signal strength relationships
- **Audience questions welcome** - throughout the demo portions
- **GitHub repo available** - attendees can clone and build immediately after talk

## Materials to Provide Attendees

- GitHub repo link: https://github.com/tiarno/esp32-sniffer
- Bill of materials with purchase links
- Quick-start guide
- Python tools for data analysis
- Documentation on legal/ethical considerations

## Bio

[Your bio here - experience with embedded systems, radio, security research, etc.]

## Additional Notes

- All code is open-source (GitHub)
- Hardware is readily available (~$40 on Amazon/AliExpress)
- No prior RF experience required for attendees
- Talk emphasizes responsible research and ethical monitoring
- Can provide workshop version for hardware village if interested

## Contact

**Presenter**: [Your name]  
**Email**: [Your email]  
**GitHub**: https://github.com/tiarno  
**Twitter/X**: [If applicable]  

---

## Alternate Titles (if main title doesn't fit the vibe)

- "Sniffing LoRa: A $40 ESP32 Adventure in Sub-GHz Radio"
- "Invisible Networks: Building a Portable LoRa Recon Tool"
- "DIY Radio Hacking: ESP32 LoRa Sniffer from Scratch"
- "What's Flying Through the Air? A LoRa Reconnaissance Tool"

## Alternate Talk Lengths

**30 minutes**: Skip detailed SD logging section, focus on live demo  
**20 minutes (Lightning)**: Hardware overview + network map demo only  
**90 minutes (Workshop)**: Hands-on - attendees bring hardware and build together
