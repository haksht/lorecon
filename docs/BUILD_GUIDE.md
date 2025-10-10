# 🏗️ ESP32 LoRa Sniffer - Dual-Track Build System

## 🎯 **Overview**

This project implements a **dual-track build system** using PlatformIO's multi-environment capabilities. You get **two completely different tools** from the same codebase:

### **🔬 Research Platform** (Default)
- **Purpose**: Professional security research and RF analysis  
- **Complexity**: C++ with clean architecture
- **Features**: PSK decryption, hardware stress testing, intelligence storage
- **Target Audience**: Security researchers, penetration testers, RF engineers

### **📚 Educational Simple** 
- **Purpose**: Learning LoRa protocols and embedded development
- **Complexity**: ~300 lines of straightforward, readable code
- **Features**: Basic packet capture and display
- **Target Audience**: Students, developers learning LoRa, demo purposes

---

## 🚀 **Quick Build Commands**

```bash
# Research Platform (Full-Featured) - DEFAULT
pio run --target upload
pio device monitor

# Educational Simple (Learning Version)  
pio run -e simple --target upload
pio device monitor
```

---

## 📊 **Feature Comparison Matrix**

| Capability | Research Platform | Educational Simple | Notes |
|------------|------------------|-------------------|-------|
| **📡 LoRa Packet Capture** | ✅ Advanced | ✅ Basic | Both capture packets |
| **🔓 PSK Decryption** | ✅ 5 default keys | ❌ | Research only |
| **⚡ Hardware Stress Testing** | ✅ Full framework | ❌ | Research only |
| **🎯 Interactive Device Targeting** | ✅ Menu system | ❌ | Research only |
| **🧠 Intelligence Storage** | ✅ Session mgmt | ❌ | Research only |
| **🏗️ State Management** | ✅ ReconState class | ❌ Simple globals | Research only |
| **📱 Protocol Analysis** | ✅ Advanced parsing | ✅ Basic ID | Both versions |
| **💾 JSON Logging** | ✅ Structured | ✅ Simple | Both versions |
| **🔧 Code Complexity** | Professional | Beginner-friendly | Key difference |
| **📖 Learning Curve** | Steep | Gentle | Choose based on needs |

---

## 🔧 **Technical Implementation**

### **PlatformIO Environment Configuration**

The dual-track system uses **build source filtering** to compile different sets of files:

```ini
# Default Environment (Research Platform)
[env:default]
build_src_filter = +<*> -<main_realistic.cpp> -<test_*.cpp> -<archive/>
# ↳ Includes: main.cpp + all advanced modules

# Educational Simple Environment  
[env:simple]
build_src_filter = +<main_realistic.cpp> +<data_structures.h> -<main.cpp> -<recon_state.cpp> -<user_interface.cpp> -<psk_*.cpp> -<hardware_*.cpp> -<test_*.cpp>
# ↳ Includes: main_realistic.cpp only (minimal dependencies)
```

### **Source File Architecture**

```
firmware/src/
├── 🎯 RESEARCH PLATFORM FILES
│   ├── main.cpp                     # Main application entry point
│   ├── lora_recon_tool.cpp/.h       # Main reconnaissance engine
│   ├── recon_state.cpp/.h           # State management and device tracking
│   ├── user_interface.cpp/.h        # Interactive menu system  
│   ├── command_handler.cpp/.h       # Command pattern dispatcher
│   ├── psk_decryption_simple.cpp/.h # PSK testing and AES decryption
│   ├── session_key_manager.cpp/.h   # Session key harvesting
│   ├── hardware_stress_tester.cpp/.h # System validation
│   ├── protocol_analyzer.cpp/.h     # Packet protocol analysis
│   ├── geo_intelligence.cpp/.h      # GPS extraction and mapping
│   ├── oled_display.cpp/.h          # OLED display management
│   ├── error_handler.cpp/.h         # Production error recovery
│   └── ui_components.cpp/.h         # Reusable UI elements
│
├── 📚 EDUCATIONAL SIMPLE FILES  
│   └── main_realistic.cpp           # Complete simple tool (~300 lines)
│
└── 🔄 SHARED COMPONENTS
    ├── data_structures.h            # Common data definitions
    ├── psk_tests.h                  # PSK test suite
    └── unit_tests.h                 # Unit test framework
```

### **Compilation Process**

When you run `pio run -e [environment]`, PlatformIO:

1. **Filters source files** based on `build_src_filter` patterns
2. **Compiles only selected files** for that environment  
3. **Links appropriate libraries** and dependencies
4. **Generates environment-specific binary**

**Key Insight**: Each environment produces a **completely different program** despite sharing the same source directory.

---

## 🎓 **Educational Benefits**

### **For Learning LoRa Protocols**
```bash
# Start with simple version to understand basics
pio run -e simple --target upload

# Graduate to default (full-featured) for advanced techniques  
pio run --target upload
```

### **Code Study Progression**

1. **Begin**: Study `main_realistic.cpp` (300 lines) - Clean, readable LoRa basics
2. **Progress**: Examine `main.cpp` (900+ lines) - Professional architecture patterns  
3. **Advanced**: Analyze individual modules - Specialized subsystems

### **Teaching/Demonstration Scenarios**

- **Basic Workshop**: Use `simple` environment for clear, focused demonstrations
- **Advanced Course**: Use `default` environment for comprehensive feature showcase
- **Code Review**: Compare both versions to illustrate architecture evolution

---

## 🛠️ **Development Workflow**

### **Adding New Features**

```bash
# Test in simple version first (if applicable)
pio run -e simple --target upload

# Implement in default (research platform)
pio run --target upload  

# Validate both versions work independently
pio run -e simple --target upload && pio run --target upload
```

### **Debugging Strategy**

1. **Start Simple**: Debug basic functionality in `simple` environment
2. **Escalate**: Move to `default` environment for advanced features
3. **Isolate**: Use environment switching to isolate issues

### **Maintenance Approach**

- **Keep Simple Clean**: Resist adding complexity to educational version
- **Evolve Default Platform**: Add advanced features to default environment  
- **Maintain Compatibility**: Ensure both versions work with same hardware

---

## 🔍 **Advanced Usage Examples**

### **Security Research Workflow**
```bash
# Deploy research platform for field work
pio run -e research-platform --target upload
# → Full PSK testing, device targeting, intelligence storage

# Use simple version for quick verification  
pio run -e simple --target upload  
# → Confirm basic packet capture is working
```

### **Educational Demonstration**
```bash  
# Classroom demo - start with simple concepts
pio run -e simple --target upload
# → Show basic LoRa packet structure and capture

# Advanced session - demonstrate professional techniques
pio run -e research-platform --target upload
# → Interactive menus, PSK analysis, targeting modes
```

### **Development Testing**
```bash
# Quick functional test
pio run -e simple --target upload
# → Verify radio communication and basic packet handling

# Comprehensive feature test  
pio run -e research-platform --target upload
# → Test all advanced modules and interactions
```

---

## 📈 **Benefits of Dual-Track System**

### **✅ User Benefits**
- **Choose Complexity Level**: Match tool to your skill level and needs
- **Learning Progression**: Natural path from simple to advanced
- **Multiple Use Cases**: Education, research, and development covered
- **Reduced Cognitive Load**: Simple version eliminates overwhelming complexity

### **✅ Development Benefits**  
- **Modular Architecture**: Clean separation of concerns
- **Easier Maintenance**: Changes isolated to appropriate environments
- **Better Testing**: Can validate basic functionality independently
- **Code Reusability**: Shared components benefit both versions

### **✅ Project Benefits**
- **Broader Audience**: Appeals to both beginners and experts
- **Professional Quality**: Research platform shows serious engineering
- **Educational Value**: Simple version perfect for teaching
- **Flexibility**: Easy to add new environments for special purposes

---

## 🚨 **Important Notes**

### **⚠️ Build System Requirements**
- **PlatformIO Core**: This system requires PlatformIO (not Arduino IDE)
- **Environment Selection**: Always specify environment with `-e` flag for clarity
- **Source File Conflicts**: Never edit both `main.cpp` and `main_realistic.cpp` simultaneously

### **💡 Best Practices**
- **Default Environment**: `research-platform` is default for `pio run`
- **Explicit Selection**: Use `-e simple` or `-e research-platform` for clarity
- **Documentation**: Always specify which environment in bug reports
- **Version Control**: Consider separate branches for major changes to each track

---

This dual-track build system represents a **sophisticated approach** to managing complexity while serving multiple user communities from a single codebase. It demonstrates professional software architecture principles applied to embedded development.