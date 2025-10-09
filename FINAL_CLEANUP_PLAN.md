# Final Cleanup Plan - October 9, 2025

## 📚 Documentation Audit

### ✅ KEEP - Essential Documentation (Root Level)

1. **README.md** - GitHub landing page
2. **START_HERE.md** - Quick start guide
3. **MESHTASTIC_SECURITY_ANALYSIS.md** - Main deliverable (security findings)
4. **LICENSE** - Legal requirements

### ❌ DELETE - Temporary/Redundant Files (Root Level)

1. **NEXT_SESSION_PROMPT.md** - No longer needed (project complete)
2. **GIT_COMMIT_SUMMARY.md** - Already committed (temporary file)
3. **PROJECT_COMPLETE.md** - Redundant with README/START_HERE
4. **CODE_CLEANUP_OCT9_2025.md** - Historical artifact (archive or delete)
5. **CLEANUP_COMPLETE.md** - Temporary status file
6. **captured_devices.kml** - Empty test file (can regenerate)

### ✅ KEEP - Technical Documentation (docs/)

1. **BUILD_GUIDE.md** - How to compile and upload
2. **FEATURES.md** - Feature list
3. **SESSION_KEY_HARVESTING_GUIDE.md** - Technical implementation
4. **TEXT_MESSAGE_COMPLETE_GUIDE.md** - Text message decryption
5. **TROUBLESHOOTING_MESHTASTIC.md** - Common issues

### ⚠️ REVIEW - May Delete (docs/)

1. **CURRENT_STATUS_JAN2025.md** - Old status (DELETE - use START_HERE.md)
2. **PROJECT_STATUS_FINAL.md** - Old status (DELETE - redundant)
3. **PROTOBUF_APPROACH_UPDATE.md** - Historical (ARCHIVE or DELETE)
4. **USING_OFFICIAL_PROTOBUFS.md** - Historical (ARCHIVE or DELETE)
5. **architecture.md** - May be useful (REVIEW contents)
6. **UNDERSTANDING.md** - Background info (KEEP if useful)
7. **README_SECURITY.md** - Security notes (MERGE into main security analysis?)
8. **BUTTON_DISPLAY_GUIDE.md** - Hardware guide (KEEP)
9. **OLED_TROUBLESHOOTING.md** - Troubleshooting (KEEP)
10. **QUIET_MODE_GUIDE.md** - Feature guide (KEEP or merge into FEATURES.md)
11. **tdeck_port_guide.md** - Hardware guide (KEEP)

### 📦 Already Archived

- `archive_old_sessions/` - Old session notes (already archived ✅)
- `docs/archive/` - Old docs (already archived ✅)
- `docs/archive_oct2025/` - Recent archive (already archived ✅)

---

## 🔍 Code Files Audit

### Files to Review:

1. **firmware/src/*.cpp** - All implementation files
2. **firmware/src/*.h** - All header files
3. **platformio.ini** - Build configuration
4. **firmware/HARDWARE_STRESS_TESTING_GUIDE.md** - Move to docs/?

### Static Analysis Checklist:

- [ ] Remove unused includes
- [ ] Remove commented-out code
- [ ] Check for unused variables
- [ ] Verify all functions are used
- [ ] Check for memory leaks
- [ ] Remove debug Serial.print statements (or guard with #ifdef)
- [ ] Consistent formatting
- [ ] Update copyright/headers

---

## 📋 Cleanup Actions

### Phase 1: Documentation Cleanup

```powershell
# Delete temporary/redundant root files
Remove-Item NEXT_SESSION_PROMPT.md
Remove-Item GIT_COMMIT_SUMMARY.md
Remove-Item PROJECT_COMPLETE.md
Remove-Item CODE_CLEANUP_OCT9_2025.md
Remove-Item CLEANUP_COMPLETE.md
Remove-Item captured_devices.kml

# Delete old status docs
Remove-Item docs/CURRENT_STATUS_JAN2025.md
Remove-Item docs/PROJECT_STATUS_FINAL.md

# Archive historical technical docs (optional)
New-Item -ItemType Directory -Force -Path docs/archive_2025
Move-Item docs/PROTOBUF_APPROACH_UPDATE.md docs/archive_2025/
Move-Item docs/USING_OFFICIAL_PROTOBUFS.md docs/archive_2025/

# Move hardware guide to docs
Move-Item firmware/HARDWARE_STRESS_TESTING_GUIDE.md docs/
```

### Phase 2: Code Static Analysis

Run PlatformIO static analysis:
```powershell
pio check --skip-packages
```

### Phase 3: Final Review

- Update README.md with project completion status
- Ensure START_HERE.md is current
- Verify all doc links work
- Final compile test

---

## 🎯 Expected Final Structure

```
esp32-sniffer/
├── README.md ⭐ (GitHub landing)
├── START_HERE.md ⭐ (Quick start)
├── MESHTASTIC_SECURITY_ANALYSIS.md ⭐ (Main deliverable)
├── LICENSE
├── platformio.ini
├── .gitignore
│
├── firmware/
│   ├── src/
│   │   ├── main.cpp
│   │   ├── lora_recon_tool.cpp/h
│   │   ├── protocol_analyzer.cpp/h
│   │   ├── psk_decryption_simple.cpp/h
│   │   ├── session_key_manager.cpp/h
│   │   ├── command_handler.cpp/h
│   │   ├── geo_intelligence.cpp/h
│   │   ├── oled_display.cpp/h
│   │   └── ... (other source files)
│   └── modules/ (RadioLib, etc.)
│
├── docs/
│   ├── BUILD_GUIDE.md
│   ├── FEATURES.md
│   ├── SESSION_KEY_HARVESTING_GUIDE.md
│   ├── TEXT_MESSAGE_COMPLETE_GUIDE.md
│   ├── TROUBLESHOOTING_MESHTASTIC.md
│   ├── BUTTON_DISPLAY_GUIDE.md
│   ├── OLED_TROUBLESHOOTING.md
│   ├── QUIET_MODE_GUIDE.md
│   ├── tdeck_port_guide.md
│   ├── UNDERSTANDING.md
│   ├── architecture.md
│   ├── README_SECURITY.md
│   └── archive_2025/ (historical docs)
│
├── tools/
│   ├── analyze_logs.py
│   ├── live_visualizer.py
│   ├── requirements.txt
│   └── README.md
│
└── archive_old_sessions/ (old session notes)
```

---

## ✅ Success Criteria

After cleanup:
- [ ] Only essential documentation remains
- [ ] All doc links work
- [ ] Code compiles with no warnings
- [ ] Static analysis clean
- [ ] README is accurate and complete
- [ ] Git status clean
- [ ] Ready to share publicly

---

**Status:** Ready to execute cleanup
