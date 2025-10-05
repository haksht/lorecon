# Complete Understanding Guide - ESP32 LoRa Reconnaissance Tool

**Your Personal Teacher for This Codebase**

This document explains everything you need to deeply understand this project - from hardware fundamentals to advanced embedded systems concepts. Study at your own pace, revisit sections as needed, and use this as your reference guide.

**Last Updated:** October 5, 2025  
**Your Teacher:** GitHub Copilot  
**Your Goal:** Master this codebase completely

---

## 📚 Table of Contents

### Part 1: Foundations
1. [The Big Picture: What Happens When You Power It On](#1-the-big-picture)
2. [Hardware & Radio Fundamentals](#2-hardware--radio-fundamentals)
3. [Core Architecture: How the Code is Organized](#3-core-architecture)
4. [The Main Loop: Following the Execution Flow](#4-the-main-loop)
5. [State Management: The Brain of the System](#5-state-management)

### Part 2: Core Systems
6. [Interrupt-Driven Packet Reception](#6-interrupt-driven-packet-reception)
7. [Protocol Analysis: Understanding Packets](#7-protocol-analysis)
8. [Command Pattern: User Input Handling](#8-command-pattern)
9. [Error Handling & Recovery](#9-error-handling--recovery)
10. [Advanced Features Deep Dives](#10-advanced-features)

### Part 3: Embedded Systems Mastery
11. [Memory Map & Buffer Usage](#11-memory-map--buffer-usage)
12. [Debugging Techniques](#12-debugging-techniques)
13. [Common Pitfalls & Solutions](#13-common-pitfalls--solutions)
14. [Performance Optimization](#14-performance-optimization)

### Part 4: Learning Resources
15. [Study Guide / Learning Path](#15-study-guide--learning-path)
16. [Quick Reference Cheat Sheet](#16-quick-reference-cheat-sheet)
17. [Questions to Test Your Understanding](#17-questions-to-test-your-understanding)

---

## **1. The Big Picture: What Happens When You Power It On**

Let's trace what happens from power-on to capturing your first packet. Understanding this execution flow is critical.

### **The Arduino Framework Entry Points**

```cpp
// main.cpp (30 lines - just the entry point)
LoRaReconTool reconTool;  // Global instance created before setup()

void setup() {
    reconTool.initialize();  // Called once at boot
}

void loop() {
    reconTool.update();  // Called forever, as fast as possible
}
```

**What's Actually Happening Behind the Scenes:**

The Arduino framework (which runs on ESP32) has a hidden `main()` function that looks like this:

```cpp
// Hidden in the Arduino core (you don't write this)
int main() {
    initArduino();  // Setup hardware, clocks, etc.
    setup();        // Your setup function
    
    while(1) {      // Infinite loop
        loop();     // Your loop function
    }
    
    return 0;  // Never reached
}
```

So when you write `loop()`, you're actually writing code that runs in an infinite while loop!

### **The Initialize Sequence (Critical Order)**

```cpp
bool LoRaReconTool::initialize() {
    // ===== PHASE 1: Communication Setup =====
    Serial.begin(115200);    // Start USB serial for debugging
    delay(2000);             // Give serial time to stabilize
    
    // ===== PHASE 2: Safety Systems First! =====
    // Initialize error handler BEFORE anything else
    // (so we can log errors that occur during initialization)
    ErrorHandler::initialize();
    
    // Enable hardware watchdog (30s timeout)
    // This is a PHYSICAL timer in the ESP32 that resets
    // the CPU if we don't "pet" it regularly
    esp_task_wdt_init(30, true);
    esp_task_wdt_add(NULL);  // Add current task to watchdog
    
    Serial.println("[WATCHDOG] Hardware watchdog enabled (30s timeout)");
    
    displayWelcomeMessage();  // Hello world!
    
    // ===== PHASE 3: Storage System =====
    if (LittleFS.begin()) {
        Serial.println("[FS] Storage ready");
    } else {
        Serial.println("[FS] Storage init failed");
        // Non-critical, continue anyway
    }
    
    // ===== PHASE 4: Radio Hardware Setup =====
    // Initialize SPI bus (communication protocol to radio chip)
    SPI.begin(SCK, MISO, MOSI, LORA_NSS);
    
    Serial.print("[RADIO] Initializing SX1262... ");
    int state = radio.begin();  // RadioLib magic happens here
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("FAILED (%d)\n", state);
        LOG_RADIO_ERROR(ErrorCodes::RADIO_INIT_FAILED, 
                       "SX1262 initialization failed");
        return false;  // Can't continue without radio
    }
    Serial.println("OK");
    
    // ===== PHASE 5: Interrupt Setup =====
    // Tell the radio chip to signal us when packets arrive
    pinMode(LORA_DIO1, INPUT);  // Make sure GPIO is input
    radio.setDio1Action(onPacketReceived);  // Register our ISR
    
    Serial.printf("[RADIO] DIO1 interrupt configured on GPIO %d\n", 
                  LORA_DIO1);
    
    // ===== PHASE 6: Application State =====
    reconState.initialize();  // Clear all tracking data
    
    displayReconStartMessage();
    
    // ===== PHASE 7: Initial Radio Configuration =====
    if (!applyConfig(reconState.scanState.currentConfig)) {
        Serial.println("[ERROR] Failed to apply initial config");
        LOG_RADIO_ERROR(ErrorCodes::RADIO_CONFIG_FAILED, 
                       "Initial radio configuration failed");
        return false;
    }
    
    // ===== PHASE 8: Start Listening! =====
    radio.startReceive();  // Radio now watching for packets
    Serial.println("[RECON] Started\n");
    
    // ===== PHASE 9: Optional Modules =====
    initializeStressTesting();  // Load if enabled
    
    // ===== PHASE 10: User Interface =====
    commandHandler = new CommandHandler(this);
    Serial.println("[SYSTEM] Command handler initialized");
    
    return true;  // Success!
}
```

**Why This Order Matters:**

1. **Serial first** - So you can see debug output
2. **Error handler second** - So you can log subsequent errors
3. **Watchdog early** - Start protection before anything complex
4. **Radio last** - Most complex, needs other systems ready
5. **State after radio** - Depends on knowing radio works

**Common Mistake:** Initializing things in wrong order causes mysterious failures!

---

## **2. Hardware & Radio Fundamentals**

### **The ESP32-S3 Microcontroller**

Think of the ESP32-S3 as a tiny computer:
- **CPU:** Dual-core 240 MHz (faster than old desktop computers!)
- **RAM:** 512 KB (not much - must be careful)
- **Flash:** 4-16 MB (for your program and data)
- **GPIO Pins:** Digital input/output pins to talk to other chips
- **Special Features:** WiFi, Bluetooth, hardware crypto, timers

### **The SX1262 LoRa Radio Chip**

The SX1262 is a specialized radio chip that speaks LoRa (Long Range radio protocol):
- Made by Semtech (the inventors of LoRa)
- Can transmit/receive at 862-928 MHz
- Up to 10 km range in good conditions
- Very low power (great for battery devices)

### **How ESP32 and SX1262 Connect**

```
ESP32-S3 (Microcontroller)          SX1262 (LoRa Radio Chip)
┌─────────────────────┐             ┌────────────────────┐
│                     │             │                    │
│  GPIO 8  (NSS)      │────────────>│ Chip Select (CS)   │ SPI: Select this chip
│  GPIO 14 (DIO1)     │<────────────│ Interrupt Out      │ Alert: "Packet arrived!"
│  GPIO 12 (RST)      │────────────>│ Reset Pin          │ Control: Reset chip
│  GPIO 13 (BUSY)     │<────────────│ Busy Indicator     │ Status: "I'm busy"
│                     │             │                    │
│  GPIO 9  (SCK)      │────────────>│ SPI Clock          │ SPI: Clock signal
│  GPIO 10 (MOSI)     │────────────>│ SPI Data In        │ SPI: ESP32 → SX1262
│  GPIO 11 (MISO)     │<────────────│ SPI Data Out       │ SPI: SX1262 → ESP32
│                     │             │                    │
│  3.3V               │────────────>│ Power              │
│  GND                │────────────>│ Ground             │
└─────────────────────┘             └────────────────────┘
         ▲                                    ▲
         │                                    │
    Your Code                            RadioLib
    (high level)                      (low level driver)
```

### **SPI Communication Explained**

SPI (Serial Peripheral Interface) is how ESP32 sends commands to SX1262:

```cpp
// You write high-level code like:
radio.setFrequency(902.125);

// RadioLib translates to SPI commands:
// 1. Pull NSS (CS) pin LOW  → "Hey SX1262, I'm talking to you"
// 2. Send command byte: 0x86 → "Set frequency command"
// 3. Send frequency data: 0x39 0x0C 0x00 → "902.125 MHz"
// 4. Pull NSS (CS) pin HIGH → "Done talking"

// SX1262 processes it and responds via MISO
```

**Key Insight:** You don't write SPI code - RadioLib does it for you! You just call `radio.setFrequency()` and magic happens.

### **LoRa Radio Parameters (The Science)**

When you configure a LoRa radio, you're setting several parameters that work together:

```cpp
struct ScanConfig {
    float frequency;           // Which "channel" to use
    float bandwidth;           // How wide the channel is
    uint8_t spreadingFactor;   // How to encode data
    uint8_t syncWord;          // Network identifier
    const char* protocol;      // Human name
};
```

#### **1. Frequency** - "Which Radio Station"

```
902.125 MHz  ←  You tune here, like FM radio
│
├─ Meshtastic channel 0 (US)
├─ ISM band (unlicensed, legal to use)
└─ Similar to: 902.1 FM on your car radio
```

**Why Multiple Frequencies?**
- Different networks use different channels
- Reduces interference (like different WiFi channels)
- Your tool scans 16 different frequencies to find all traffic

#### **2. Bandwidth** - "Channel Width"

```
125 kHz  ←  Narrow channel (slower, longer range)
250 kHz  ←  Medium channel (balanced)
500 kHz  ←  Wide channel (faster, shorter range)

Analogy: Like lane width on a highway
- Narrow lane: Cars go slow but can go far
- Wide lane: Cars go fast but need more fuel
```

**Common Values:**
- LoRaWAN: 125 kHz (long range, slow)
- Meshtastic: 250 kHz (balanced)

#### **3. Spreading Factor (SF)** - "How Clearly You Speak"

```
SF7  ←  Fast (5,470 bps) but short range
SF8  ←  Medium speed
SF9  ←  Balanced
SF10 ←  Slower, longer range
SF11 ←  Slow (440 bps) but VERY long range
SF12 ←  Slowest (293 bps) but maximum range

Analogy: Like speaking in a noisy room
- SF7: Talking normal speed (must be close)
- SF11: Speaking... very... slowly... (hear from far away)
```

**The Math:**
- Each increase in SF doubles the time-on-air
- Higher SF = better sensitivity (can hear weaker signals)
- Meshtastic LongFast preset: SF11 (440 bps, ~10 km range)

#### **4. Sync Word** - "Secret Handshake"

```
0x12  ←  Public LoRaWAN networks
0x2b  ←  Private Meshtastic networks
0x34  ←  Private network (custom)

Purpose: Ignore packets from other networks
- Like WiFi SSID, but at hardware level
- Radio only signals interrupt if sync word matches
- Reduces false triggers from random RF noise
```

### **How Your Tool Scans Multiple Configurations**

```cpp
const ScanConfig scanConfigs[] = {
    // Meshtastic US - Long Range
    {906.875, 250.0, 11, 0x2b, "Meshtastic_LF_906"},
    
    // Meshtastic US - Medium Fast
    {903.875, 250.0, 10, 0x2b, "Meshtastic_MF"},
    
    // Meshtastic US - Short Fast
    {904.375, 250.0,  9, 0x2b, "Meshtastic_SF"},
    
    // LoRaWAN channels
    {903.9,   125.0,  7, 0x12, "TTN_CH0_SF7"},
    {904.1,   125.0,  8, 0x12, "TTN_CH1_SF8"},
    
    // ... 11 more configurations
};
```

**The Strategy:**
1. Try each config for 12 seconds (dwell time)
2. If packets arrive, record which config worked
3. After trying all 16, start over
4. Full cycle: ~3 minutes

**Why This Works:**
- Meshtastic might transmit every 30-60 seconds
- By cycling through all configs, you catch transmissions
- Once you know which config has traffic, you can target it

---

## **3. Core Architecture: How the Code is Organized**

### **The Single Responsibility Principle**

Your code follows a fundamental software engineering principle: **each class should have ONE job and do it well.**

```
┌─────────────────────────────────────────────────────────┐
│                   LoRaReconTool                         │
│         "The Orchestra Conductor"                       │
│                                                         │
│  Responsibilities:                                      │
│  • Own the radio hardware (SX1262 instance)            │
│  • Coordinate timing (when to switch frequencies)      │
│  • Delegate work to specialists                        │
│  • Handle mode switching (recon/target/menu)           │
│                                                         │
│  Does NOT: Parse packets, track devices, handle errors │
└─────────────────────────────────────────────────────────┘
                           │
           ┌───────────────┼───────────────┬──────────────┐
           ▼               ▼               ▼              ▼
    ┌──────────┐    ┌────────────┐  ┌──────────┐  ┌────────────┐
    │ReconState│    │ProtocolAn  │  │CommandHa │  │ErrorHand   │
    │          │    │alyzer      │  │ndler     │  │ler         │
    │"Memory"  │    │"Brain"     │  │"Ears"    │  │"Doctor"    │
    └──────────┘    └────────────┘  └──────────┘  └────────────┘
         │                │               │              │
         │                │               │              │
    Remembers        Understands     Processes      Recovers
    everything       packets         commands       from errors
```

### **ReconState - "The Memory"**

**Job:** Remember everything about the reconnaissance session

```cpp
class ReconState {
    // What frequencies are active?
    RFActivity rfActivity[16];
    
    // What devices have we discovered?
    TargetableDevice targetableDevices[20];
    
    // What's our current mode and configuration?
    ScanState scanState;
    
    // What packets have we captured for replay?
    CapturedPacket replaySlots[10];
};
```

**Why Separate?**
- Can reset state without restarting whole system
- Can save/load state to flash
- Easy to test (just check what's in memory)
- Clear "single source of truth"

### **ProtocolAnalyzer - "The Brain"**

**Job:** Understand what packets mean

```cpp
class ProtocolAnalyzer {
    // Is this Meshtastic, LoRaWAN, or something else?
    const char* identifyProtocol(packet);
    
    // What's the sender's ID?
    uint32_t extractNodeId(packet);
    
    // What kind of device is this?
    const char* identifyDeviceType(packet, rssi);
    
    // Is it routing traffic?
    bool isRoutingDevice(packet);
};
```

**Why Separate?**
- Protocol parsing is complex - keep it isolated
- Easy to add support for new protocols
- Can test protocol detection independently
- Reusable in other projects

### **CommandHandler - "The Ears"**

**Job:** Listen to user and execute commands

```cpp
class CommandHandler {
    // Convert keypresses to actions
    bool handleCommand(char cmd);
    
    // Dispatch table (instead of if/else hell)
    static const CommandEntry commands[];
};
```

**Why Separate?**
- User interface logic separate from radio logic
- Easy to add new commands (just edit table)
- Could add other input methods (buttons, web UI)
- Clear place to handle timeouts and validation

### **ErrorHandler - "The Doctor"**

**Job:** Diagnose and fix problems

```cpp
class ErrorHandler {
    // Log errors with full context
    static void logError(category, severity, code, message);
    
    // Try to fix problems automatically
    static bool attemptRecovery(error);
    
    // Track system health
    static bool systemHealthy();
};
```

**Why Separate?**
- Error handling is cross-cutting (affects everything)
- Centralized logging makes debugging easier
- Recovery strategies in one place
- Can analyze error patterns

### **Information Flow Example**

Let's trace what happens when a packet arrives:

```
1. Radio Hardware (SX1262)
   └─> Packet received, triggers GPIO interrupt
   
2. Interrupt Handler (onPacketReceived)
   └─> Sets atomic flag: packetReceived = true
   
3. Main Loop (LoRaReconTool::update)
   └─> Checks flag, calls handlePacketReception()
   
4. Packet Extraction (LoRaReconTool)
   └─> radio.readData() - gets bytes from radio
   
5. Protocol Analysis (ProtocolAnalyzer)
   └─> analyze(data) - returns PacketInfo struct
   
6. State Update (ReconState)
   └─> addTargetableDevice() - stores device info
   
7. Logging (LittleFS)
   └─> logPacket() - saves to flash
   
8. User Display (UserInterface)
   └─> Serial.printf() - shows on screen
```

**Notice:** Each layer does its job and passes data to the next. This is called **separation of concerns**.

---

## **4. The Main Loop: Following the Execution Flow**

### **The Heartbeat of Your System**

The `loop()` function is called continuously, as fast as possible (thousands of times per second on ESP32). This is your system's "heartbeat."

```cpp
void LoRaReconTool::update() {
    uint32_t now = millis();  // Current time (milliseconds since boot)
    
    // ===== STEP 1: Keep Watchdog Happy =====
    esp_task_wdt_reset();  // "I'm still alive!"
    
    // ===== STEP 2: Check for User Input =====
    if (Serial.available()) {
        char cmd = Serial.read();
        commandHandler->handleCommand(cmd);
        return;  // Exit early - command might change mode
    }
    
    // ===== STEP 3: Mode-Specific Behavior =====
    switch (reconState.scanState.mode) {
        case MODE_RECONNAISSANCE:
            handleReconnaissanceMode(now);
            break;
            
        case MODE_TARGETED_CAPTURE:
            handleTargetedCaptureMode(now);
            break;
            
        case MODE_INTERACTIVE_MENU:
            // Just wait for user input
            delay(100);
            return;
            
        case MODE_PACKET_REPLAY:
            // Replay is handled via command handler
            delay(100);
            return;
    }
    
    // ===== STEP 4: Brief Pause =====
    delay(10);  // 10ms pause (not strictly needed, but polite)
}
```

### **Understanding Timing**

**millis()** - The Clock

```cpp
uint32_t now = millis();  // Returns milliseconds since boot

// Example timeline:
// Boot:        0 ms
// After 1 sec: 1,000 ms
// After 1 min: 60,000 ms
// After 1 hr:  3,600,000 ms
// After 49 days: WRAPS AROUND to 0 (uint32_t overflow!)
```

**Important:** `millis()` overflows after ~49 days. Your code handles this correctly using subtraction:

```cpp
// WRONG: Direct comparison breaks after overflow
if (now >= lastTime + 12000) { }

// RIGHT: Subtraction works even after overflow
if (now - lastTime >= 12000) { }
```

### **Reconnaissance Mode - The Scanner**

```cpp
void handleReconnaissanceMode(uint32_t now) {
    // ===== Frequency Switching Logic =====
    if (now - lastScanSwitch >= SCAN_DWELL_TIME) {  // 12 seconds
        
        // Move to next configuration (with wraparound)
        currentConfig = (currentConfig + 1) % NUM_CONFIGS;
        //                                  ^^^^^^^^^^^^^^
        //                                  Modulo: 0,1,2...15,0,1,2...
        
        // Show progress every full cycle
        if (currentConfig == 0) {
            uint32_t elapsed = (now - reconStartTime) / 1000;
            Serial.printf("[RECON] Cycle complete - %u seconds\n", 
                         elapsed);
        }
        
        // Reconfigure radio to new frequency
        if (applyConfig(currentConfig)) {
            radio.startReceive();  // Start listening again
        }
        
        lastScanSwitch = now;  // Reset timer
    }
    
    // ===== Packet Reception (happens every loop) =====
    handlePacketReception();  // Check if packet arrived
    
    // ===== Activity Monitoring =====
    handleReconActivityMonitoring(now);
}
```

**The Strategy:**

1. **Dwell:** Stay on each frequency for 12 seconds
2. **Switch:** Move to next frequency
3. **Repeat:** After trying all 16, start over
4. **Listen:** Check for packets continuously during dwell

**Why 12 Seconds?**
- Meshtastic transmits every 30-60 seconds typically
- Need enough time to catch at least one transmission
- Too short: Might miss devices
- Too long: Takes forever to scan all frequencies
- 12 seconds × 16 configs = ~3 minutes per full cycle

### **Targeted Capture Mode - The Sniper**

```cpp
void handleTargetedCaptureMode(uint32_t now) {
    // In targeted mode, we DON'T switch frequencies
    // We stay locked on targetConfig
    
    // Just process packets
    handlePacketReception();
}
```

**The Difference:**
- **Recon Mode:** "Let's see what's out there" (scanning)
- **Target Mode:** "Lock onto THIS device" (focused)

When you press '1' to target device #1:
```cpp
void startTargetedCapture(uint8_t deviceIndex) {
    const TargetableDevice& target = getDevice(deviceIndex);
    
    // Switch to target mode
    mode = MODE_TARGETED_CAPTURE;
    
    // Lock frequency
    currentConfig = target.configIndex;  // The freq where we saw it
    targetConfig = target.configIndex;
    
    // Reconfigure radio
    applyConfig(currentConfig);
    radio.startReceive();
    
    // Now we'll capture EVERYTHING on this frequency
}
```

### **The Packet Reception Flow**

This is the **MOST IMPORTANT** function - handles every packet that arrives:

```cpp
void handlePacketReception() {
    // ===== STEP 1: Check Atomic Flag =====
    if (!packetReceived.load(std::memory_order_acquire)) {
        return;  // No packet, exit early
    }
    
    // ===== STEP 2: Clear Flag =====
    packetReceived.store(false, std::memory_order_release);
    
    // ===== STEP 3: Read Packet from Radio =====
    uint8_t packetBuffer[PACKET_BUFFER_SIZE];
    int state = radio.readData(packetBuffer, PACKET_BUFFER_SIZE);
    
    if (state == RADIOLIB_ERR_NONE) {
        size_t len = radio.getPacketLength();
        float rssi = radio.getRSSI();  // Signal strength
        float snr = radio.getSNR();    // Signal-to-noise ratio
        
        // ===== STEP 4: Analyze Packet =====
        PacketInfo info = protocolAnalyzer.analyze(
            packetBuffer, len, rssi
        );
        
        // ===== STEP 5: Update State =====
        if (mode == MODE_RECONNAISSANCE) {
            // Track as potential target
            trackTargetableDevice(info.nodeId, currentConfig, 
                                 rssi, info.protocol, 
                                 packetBuffer, len);
        } else if (mode == MODE_TARGETED_CAPTURE) {
            // User might want to capture this
            Serial.println("Press 'c' to capture for replay");
        }
        
        // ===== STEP 6: Log Everything =====
        logPacket(packetBuffer, len, rssi, snr, info.protocol);
        
        // ===== STEP 7: Display to User =====
        Serial.printf("[PACKET] %s, %d bytes, %.1f dBm\n",
                     info.protocol, len, rssi);
    }
    
    // ===== STEP 8: Restart Receiving =====
    radio.startReceive();  // CRITICAL: Must call this!
}
```

**Critical Detail:** After reading a packet, you MUST call `radio.startReceive()` or the radio won't listen for more packets!

---

## **5. State Management: The Brain of the System**

### **Why State Management Matters**

Imagine your tool as a detective investigating a crime scene:
- **State** = Your notebook where you write everything down
- **Without good state management** = You forget what you've seen, make mistakes, waste time
- **With good state management** = You have organized notes, can review evidence, make connections

### **ReconState Class - The Master Notebook**

```cpp
class ReconState {
private:
    // ===== Configuration Data (Const) =====
    static const ScanConfig scanConfigs[16];  // Immutable
    
public:
    // ===== RF Activity (General Signals) =====
    RFActivity rfActivity[16];  // One per scan config
    
    // ===== Device Tracking (Specific Devices) =====
    TargetableDevice targetableDevices[20];
    uint8_t numTargetableDevices;
    
    TrackedNode trackedNodes[20];
    uint8_t nodeCount;
    
    // ===== System State (Current Status) =====
    ScanState scanState;
    
    // ===== Packet Storage (For Replay) =====
    CapturedPacket replaySlots[10];
    uint8_t numCapturedPackets;
    
    // ===== Optional: PSK Testing Stats =====
    #ifdef ENABLE_PSK_TESTING
    PSKStats pskStats;
    #endif
};
```

### **The Three Levels of Tracking**

Your tool uses a **three-tier tracking system** - each level provides different granularity:

#### **Level 1: RF Activity** - "Something is transmitting"

```cpp
struct RFActivity {
    uint8_t configIndex;      // Which frequency config (0-15)
    float avgRSSI;            // Average signal strength
    float peakRSSI;           // Strongest signal seen
    uint16_t activityCount;   // How many detections
    uint32_t lastActivity;    // When (milliseconds)
    uint32_t firstSeen;       // First detection time
    const char* activityLevel; // "LOW", "MEDIUM", "HIGH"
};
```

**Purpose:** Track ALL RF energy, even if we can't decode it

**Example:**
```
Config 0 (906.875 MHz): 
  - 15 detections
  - Avg RSSI: -75 dBm
  - Peak RSSI: -68 dBm
  - Level: MEDIUM
  
→ Tells you: "There's activity on this frequency"
```

**Use Cases:**
- Frequency heatmap ("which channels are busy?")
- Quick overview of RF environment
- Find frequencies worth targeting

#### **Level 2: Tracked Nodes** - "Valid packet with node ID"

```cpp
struct TrackedNode {
    uint32_t nodeId;      // Device identifier (e.g., 0x401ACD4E)
    String protocol;      // "Meshtastic", "LoRaWAN", etc.
    uint16_t packetCount; // How many packets from this node
    float avgRSSI;        // Average signal strength
    float bestRSSI;       // Best signal strength
    uint32_t lastSeen;    // Last packet time
    uint32_t firstSeen;   // First detection
    bool active;          // Recently seen?
};
```

**Purpose:** Count unique devices

**Example:**
```
Node 0x401ACD4E:
  - Protocol: Meshtastic
  - 42 packets
  - Avg RSSI: -82 dBm
  - Last seen: 3 seconds ago
  
→ Tells you: "This specific device exists and is transmitting"
```

**Use Cases:**
- Device counting ("How many devices?")
- Activity tracking ("Who's most active?")
- Presence detection ("Is device still nearby?")

#### **Level 3: Targetable Devices** - "Device I can target"

```cpp
struct TargetableDevice {
    uint32_t nodeId;          // Required!
    uint8_t configIndex;      // Best frequency for this device
    float bestRSSI;           // Strongest signal
    float avgRSSI;            // Average signal
    uint16_t packetCount;     // Successfully decoded packets
    uint32_t lastSeen;        // Last packet time
    uint32_t firstSeen;       // First detection
    char protocol[16];        // Protocol name
    char deviceType[24];      // Device classification
    char firmwareVersion[32]; // Estimated firmware
    bool isRouter;            // Routes traffic?
    uint8_t powerClass;       // 0=low, 1=med, 2=high
};
```

**Purpose:** Provide targeting information for focused capture

**Example:**
```
Device #1:
  - Node ID: 0x401ACD4E
  - Type: Meshtastic Mobile
  - Config: 906.875 MHz, SF11
  - RSSI: -68 dBm (Good signal)
  - Packets: 42
  - Router: No
  - Firmware: v2.2+
  
→ Tells you: "This is a good target for capture"
```

**Use Cases:**
- User selection menu ("Target device #1")
- Security assessment ("What devices are vulnerable?")
- Network mapping ("Where are the routers?")

### **The Filtering Pipeline**

```
All RF Energy
    │
    ├─ Noise, interference, weak signals
    │
    ▼
RF Activity (16 entries)
"I detected something, but might not be valid"
    │
    ├─ Invalid packets, corrupted data
    │
    ▼
Tracked Nodes (20 entries)
"I decoded a valid packet with node ID"
    │
    ├─ Devices with too few packets for good info
    │
    ▼
Targetable Devices (20 entries)
"I have enough info to let user target this"
```

### **Why Three Levels?**

**Example Scenario:**

1. **Scan starts:** Recon mode, cycling through frequencies

2. **906.875 MHz:**
   - Weak signal detected (-95 dBm)
   - RF Activity updated: activityCount++
   - Too weak to decode → No node tracked yet

3. **Same frequency, 30 seconds later:**
   - Stronger signal (-75 dBm)
   - Packet decoded successfully!
   - Node ID extracted: 0x401ACD4E
   - TrackedNode created
   - Not enough info yet → Not targetable

4. **After 5 packets:**
   - Now we know: Meshtastic, likely mobile device, firmware v2.2+
   - Enough intelligence gathered
   - TargetableDevice created
   - User can now select "Target device #1"

**Without this pipeline:**
- You'd show users RF noise as "devices"
- You'd offer to target things you can't decode
- UI would be cluttered with garbage

**With this pipeline:**
- Users only see devices worth targeting
- System tracks everything internally for debugging
- Clean, professional interface

### **Fixed-Size Arrays vs Dynamic Allocation**

**Why use fixed arrays?**

```cpp
// This is what we do:
TargetableDevice targetableDevices[20];  // Fixed size

// We DON'T do this:
std::vector<TargetableDevice> devices;  // Dynamic (heap)
```

**Embedded Systems Lesson: Heap Fragmentation**

```
Initial heap (nice and contiguous):
┌──────────────────────────────────┐
│          Free Heap               │
│         (240 KB)                 │
└──────────────────────────────────┘

After running for hours:
┌─────┬────┬──┬───┬─────┬─────┬───┐
│Used │Free│U │Fre│Used │Free │Usd│  ← "Swiss cheese" effect!
└─────┴────┴──┴───┴─────┴─────┴───┘
         ▲
         └─ Can't allocate large block even though
            total free space is enough!
```

**The Solution: Fixed Arrays**

```cpp
// Allocated at compile time, never moves
TargetableDevice targetableDevices[20];

// Trade-offs:
// ✅ No fragmentation
// ✅ Predictable memory usage
// ✅ No allocation failures
// ❌ Wasted memory if not full
// ❌ Hard limit (20 devices max)
```

**For Long-Running Embedded Systems:**
- Predictability > Flexibility
- Rather waste some RAM than crash after 48 hours
- Hard limits force you to handle "buffer full" gracefully

---

