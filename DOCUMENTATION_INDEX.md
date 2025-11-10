# ESP32 LoRa Sniffer - Documentation Index

**Last Updated:** November 10, 2025  
**Project Status:** Production Ready v1.9

---

## 📚 Quick Navigation

### 🚀 Getting Started
- **[README.md](README.md)** - Project overview and quick links
- **[QUICKSTART.md](QUICKSTART.md)** - Get running in 5 minutes
- **[docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md)** - Compilation and build instructions

### 📖 Core Documentation
- **[docs/FEATURES.md](docs/FEATURES.md)** - Complete feature list with explanations
- **[docs/TROUBLESHOOTING_MESHTASTIC.md](docs/TROUBLESHOOTING_MESHTASTIC.md)** - Common issues and solutions
- **[docs/ENCRYPTION_REALITY.md](docs/ENCRYPTION_REALITY.md)** - Understanding Meshtastic encryption

### 🔬 Advanced Topics
- **[docs/deepdive/UNDERSTANDING.md](docs/deepdive/UNDERSTANDING.md)** - Complete technical deep dive
- **[docs/deepdive/DEEP_DIVE_MASTER_INDEX.md](docs/deepdive/DEEP_DIVE_MASTER_INDEX.md)** - Index to all technical topics

### 🏗️ Hardware & Integration
- **[docs/DUAL_BOARD_ARCHITECTURE.md](docs/DUAL_BOARD_ARCHITECTURE.md)** - Dual ESP32 board design for triangulation
- **[docs/PC_INTEGRATION_ARCHITECTURE.md](docs/PC_INTEGRATION_ARCHITECTURE.md)** - PC software integration and analysis

### 🔐 Security Research
- **[conference/OFFENSIVE_TESTING_COMPLETE_GUIDE.md](conference/OFFENSIVE_TESTING_COMPLETE_GUIDE.md)** - Offensive security framework
- **[conference/ETHICAL_TESTING_GUIDELINES.md](conference/ETHICAL_TESTING_GUIDELINES.md)** - Ethics and responsible disclosure
- **[conference/README.md](conference/README.md)** - Conference presentation guide

### 📝 Development Notes
- **[notes.txt](notes.txt)** - Current session notes and TODO list

---

## 📂 Documentation Structure

```
esp32-sniffer/
├── README.md                        # Main project overview
├── QUICKSTART.md                    # Quick start guide
├── notes.txt                        # Working notes (current)
├── DOCUMENTATION_INDEX.md           # This file
│
├── docs/                            # Core documentation
│   ├── BUILD_GUIDE.md              # Build instructions
│   ├── FEATURES.md                 # Feature documentation
│   ├── TROUBLESHOOTING_MESHTASTIC.md  # Troubleshooting
│   ├── ENCRYPTION_REALITY.md       # Encryption explained
│   ├── DUAL_BOARD_ARCHITECTURE.md  # Hardware expansion
│   ├── PC_INTEGRATION_ARCHITECTURE.md  # PC software design
│   │
│   └── deepdive/                   # Technical deep dives
│       ├── UNDERSTANDING.md        # Master technical guide
│       └── DEEP_DIVE_MASTER_INDEX.md  # Topic index
│
├── conference/                      # Security research
│   ├── README.md                   # Conference guide
│   ├── OFFENSIVE_TESTING_COMPLETE_GUIDE.md  # Attack framework
│   └── ETHICAL_TESTING_GUIDELINES.md  # Ethics framework
│
└── tools/                          # PC tools
    ├── README.md                   # Tools documentation
    ├── live_visualizer.py          # Real-time visualization
    ├── analyze_logs.py             # Log analysis
    └── requirements.txt            # Python dependencies
```

---

## 🎯 Documentation by User Type

### New Users
1. Start with [README.md](README.md)
2. Follow [QUICKSTART.md](QUICKSTART.md)
3. Check [docs/FEATURES.md](docs/FEATURES.md) to understand capabilities
4. Refer to [docs/TROUBLESHOOTING_MESHTASTIC.md](docs/TROUBLESHOOTING_MESHTASTIC.md) if issues arise

### Developers
1. Read [docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md) for build system
2. Study [docs/deepdive/UNDERSTANDING.md](docs/deepdive/UNDERSTANDING.md) for architecture
3. Review [notes.txt](notes.txt) for current development status
4. Check [docs/PC_INTEGRATION_ARCHITECTURE.md](docs/PC_INTEGRATION_ARCHITECTURE.md) for planned features

### Security Researchers
1. Review [conference/ETHICAL_TESTING_GUIDELINES.md](conference/ETHICAL_TESTING_GUIDELINES.md) **FIRST**
2. Read [conference/OFFENSIVE_TESTING_COMPLETE_GUIDE.md](conference/OFFENSIVE_TESTING_COMPLETE_GUIDE.md)
3. Understand [docs/ENCRYPTION_REALITY.md](docs/ENCRYPTION_REALITY.md)
4. Follow [conference/README.md](conference/README.md) for presentations

### Hardware Builders
1. Start with [docs/DUAL_BOARD_ARCHITECTURE.md](docs/DUAL_BOARD_ARCHITECTURE.md)
2. Review [docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md) for supported boards
3. Check [docs/FEATURES.md](docs/FEATURES.md) for hardware requirements

---

## 🔍 Finding Specific Information

### I want to know about...

**Building and compilation:**
→ [docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md)

**Features and capabilities:**
→ [docs/FEATURES.md](docs/FEATURES.md)

**Troubleshooting packet capture:**
→ [docs/TROUBLESHOOTING_MESHTASTIC.md](docs/TROUBLESHOOTING_MESHTASTIC.md)

**How encryption works:**
→ [docs/ENCRYPTION_REALITY.md](docs/ENCRYPTION_REALITY.md)

**Technical architecture:**
→ [docs/deepdive/UNDERSTANDING.md](docs/deepdive/UNDERSTANDING.md)

**RSSI and signal strength:**
→ [docs/FEATURES.md](docs/FEATURES.md#understanding-rssi-and-signal-strength)

**PSK decryption troubleshooting:**
→ [docs/TROUBLESHOOTING_MESHTASTIC.md](docs/TROUBLESHOOTING_MESHTASTIC.md#psk-decryption-troubleshooting)

**Attack framework:**
→ [conference/OFFENSIVE_TESTING_COMPLETE_GUIDE.md](conference/OFFENSIVE_TESTING_COMPLETE_GUIDE.md)

**Ethics and legal:**
→ [conference/ETHICAL_TESTING_GUIDELINES.md](conference/ETHICAL_TESTING_GUIDELINES.md)

**PC integration and analysis:**
→ [docs/PC_INTEGRATION_ARCHITECTURE.md](docs/PC_INTEGRATION_ARCHITECTURE.md)

**Dual board design:**
→ [docs/DUAL_BOARD_ARCHITECTURE.md](docs/DUAL_BOARD_ARCHITECTURE.md)

**Real-time visualization:**
→ [tools/README.md](tools/README.md)

**Current development status:**
→ [notes.txt](notes.txt)

---

## 📊 Documentation Status

| Document | Status | Last Updated |
|----------|--------|--------------|
| README.md | ✅ Current | Nov 8, 2025 |
| QUICKSTART.md | ✅ Current | Oct 10, 2025 |
| BUILD_GUIDE.md | ✅ Current | 2025 |
| FEATURES.md | ✅ Current | 2025 |
| TROUBLESHOOTING_MESHTASTIC.md | ✅ Current | Jan 2025 |
| ENCRYPTION_REALITY.md | ✅ Current | 2025 |
| deepdive/UNDERSTANDING.md | ✅ Current | 2025 |
| DUAL_BOARD_ARCHITECTURE.md | ✅ NEW | Nov 10, 2025 |
| PC_INTEGRATION_ARCHITECTURE.md | ✅ NEW | Nov 10, 2025 |
| OFFENSIVE_TESTING_COMPLETE_GUIDE.md | ✅ Current | Nov 9, 2025 |
| ETHICAL_TESTING_GUIDELINES.md | ✅ Current | Nov 9, 2025 |
| notes.txt | 🔄 Working | Nov 9, 2025 |

---

## 🗑️ Recently Consolidated

The following documentation has been consolidated into the files above:

**Removed (obsolete session notes):**
- SESSION_HANDOFF.md
- SESSION_HANDOFF_NOV9.md
- PARSING_FIXES_SUMMARY.md

**Removed (merged into FEATURES.md):**
- RSSI_EXPLAINED.md (→ FEATURES.md)

**Removed (merged into TROUBLESHOOTING_MESHTASTIC.md):**
- PSK_DECRYPTION_TROUBLESHOOTING.md

**Removed (redundant with UNDERSTANDING.md):**
- architecture.md
- DECRYPTION_SUCCESS.md
- 13x DEEP_DIVE_*.md files
- ESP32_LoRa_Sniffer_Complete_Deep_Dive.md/pdf

**Removed (superseded by newer guides):**
- HARDWARE_STRESS_TESTING_GUIDE.md (→ OFFENSIVE_TESTING_COMPLETE_GUIDE.md)
- SESSION_KEY_HARVESTING_GUIDE.md (info in code)
- TEXT_MESSAGE_COMPLETE_GUIDE.md (→ FEATURES.md)
- README_SECURITY.md (→ ETHICAL_TESTING_GUIDELINES.md)
- tdeck_port_guide.md (not relevant)

---

## 💡 Contributing to Documentation

When updating documentation:
1. Keep files focused and single-purpose
2. Update this index when adding new docs
3. Update `Last Updated` dates
4. Add to appropriate user type section above
5. Cross-reference related documents

---

**Total Documents:** 14 (down from 37 - 23 removed)  
**Core Docs:** 6 essential files  
**Advanced:** 4 technical guides  
**Security:** 3 research guides  
**Development:** 1 working notes file
