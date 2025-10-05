# Complete Understanding Guide - Part 2

**Continued from UNDERSTANDING.md**

This part covers embedded systems mastery, debugging techniques, and learning resources.

---

## **11. Memory Map & Buffer Usage**

### **ESP32-S3 Memory Layout**

Understanding where your data lives is crucial for embedded development:

```
ESP32-S3 Memory Map:
────────────────────────────────────────────
0x3C000000 │ External Flash (4-16 MB)
           │ • Your program code (.text)
           │ • Const strings ("Hello")
           │ • Const arrays (scanConfigs[])
────────────────────────────────────────────
0x3FC80000 │ SRAM (512 KB total)
           │ ├─ Heap (dynamic allocation)
           │ │  ~235 KB free at startup
           │ ├─ Stack (local variables)
           │ │  ~8 KB per task
           │ └─ Global/Static variables
           │     Your reconState, arrays, etc.
────────────────────────────────────────────
0x50000000 │ RTC Memory (8 KB)
           │ • Survives deep sleep
           │ • Used for persistent config
────────────────────────────────────────────
0x600XXXXX │ Peripheral Registers
           │ • GPIO, SPI, UART, etc.
           │ • Memory-mapped hardware
────────────────────────────────────────────
```

### **Your Tool's Memory Usage**

```cpp
// Global data (in SRAM, ~50 KB total)
LoRaReconTool reconTool;              // ~8 bytes (just pointers)
ReconState reconState;                 // ~12 KB
  ├─ RFActivity[16]                   //   ~1 KB
  ├─ TargetableDevice[20]             //   ~3 KB
  ├─ TrackedNode[20]                  //   ~2 KB
  ├─ CapturedPacket[10]               //   ~5 KB
  └─ ScanState                        //   ~1 KB

// Stack (per loop iteration, ~2 KB)
uint8_t packetBuffer[256];            // Packet data
PacketInfo info;                      // Temporary struct
Various local variables                // Function parameters, etc.

// Heap (dynamic, ~235 KB free at start)
RadioLib internals                    // ~20 KB
CommandHandler instance               // ~1 KB
String objects                        // Varies
```

### **Memory Monitoring**

```cpp
void printMemoryStatus() {
    Serial.println("\n=== MEMORY STATUS ===");
    
    // Total heap size
    size_t heapSize = ESP.getHeapSize();
    Serial.printf("Total Heap: %d KB\n", heapSize / 1024);
    
    // Currently free
    size_t freeHeap = ESP.getFreeHeap();
    Serial.printf("Free Heap:  %d KB (%d%%)\n", 
                 freeHeap / 1024,
                 (freeHeap * 100) / heapSize);
    
    // Minimum free (lowest point)
    size_t minFree = ESP.getMinFreeHeap();
    Serial.printf("Min Free:   %d KB\n", minFree / 1024);
    
    // Stack usage (approximate)
    void* sp;
    asm("mov %0, sp" : "=r"(sp));  // Get stack pointer
    Serial.printf("Stack Ptr:  0x%08X\n", (uint32_t)sp);
    
    Serial.println("===================\n");
}
```

### **Buffer Design Philosophy**

**Fixed vs Dynamic:**

```cpp
// ✅ Good: Fixed buffer (stack or static)
uint8_t packet[256];           // Predictable, fast
char message[128];             // No allocation needed
CapturedPacket slots[10];      // Known memory usage

// ⚠️ Careful: Dynamic allocation
String message = "";           // Uses heap
std::vector<int> data;         // Fragments memory
char* buffer = new char[256];  // Must remember to free
```

**Why Fixed is Better for Embedded:**

1. **No allocation overhead** - Instant access
2. **No fragmentation** - Memory stays contiguous
3. **Predictable** - Know exactly how much RAM used
4. **No leaks** - Can't forget to free (no free needed!)

**Trade-off:**
- Waste some RAM (buffers not always full)
- But gain reliability (no mysterious crashes after 48 hours)

---

## **12. Debugging Techniques**

### **Serial Print Debugging (Your Main Tool)**

```cpp
// Basic debug output
Serial.println("[DEBUG] Entering function");

// Variable inspection
Serial.printf("[DEBUG] nodeId=0x%08X, rssi=%.1f\n", nodeId, rssi);

// Conditional debugging
#ifdef VERBOSE_DEBUG
Serial.printf("[VERBOSE] Processing packet %d\n", count);
#endif

// Hex dump for packets
void printHexDump(const uint8_t* data, size_t length) {
    Serial.print("[HEX] ");
    for (size_t i = 0; i < length; i++) {
        Serial.printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) Serial.print("\n      ");
    }
    Serial.println();
}
```

### **Common Issues and Solutions**

#### **Problem 1: Radio Not Responding**

```
Symptoms:
- radio.begin() returns error
- No packets received
- SPI communication fails

Debug Steps:
1. Check wiring:
   pio device monitor
   [RADIO] Initializing SX1262... FAILED (-2)
   
   → Error -2 = SPI communication error
   → Check NSS, SCK, MOSI, MISO connections

2. Verify SPI pins:
   Serial.printf("NSS=%d, SCK=%d, MOSI=%d, MISO=%d\n",
                LORA_NSS, SCK, MOSI, MISO);
   
3. Test with known-good board:
   → Eliminates software vs hardware
```

#### **Problem 2: Packets Expected But None Received**

```
Symptoms:
- Radio initialized OK
- No interrupt firing
- No packets in monitor

Debug Steps:
1. Verify frequency/SF match:
   Serial.printf("Listening: %.3f MHz, SF%d\n", 
                cfg.frequency, cfg.spreadingFactor);
   
   → Must match transmitter exactly

2. Check sync word:
   Serial.printf("Sync Word: 0x%02X\n", cfg.syncWord);
   
   → 0x2b for Meshtastic
   → 0x12 for LoRaWAN

3. Monitor for ANY RF activity:
   // In ISR
   Serial.println("[ISR] Interrupt fired!");
   
   → If ISR fires but readData() fails: CRC error (wrong config)
   → If ISR never fires: Wrong frequency or no transmitter

4. Test with known transmitter:
   meshtastic --port COM3 --sendtext "TEST"
   
   → Creates known good signal
```

#### **Problem 3: System Hangs/Reboots**

```
Symptoms:
- Device reboots randomly
- Hangs in loop
- Watchdog timeout

Debug Steps:
1. Check watchdog petting:
   void loop() {
       Serial.println("[LOOP] Start");
       esp_task_wdt_reset();  // Must be here!
       // ...
   }

2. Look for infinite loops:
   while (condition) {
       esp_task_wdt_reset();  // Add here too!
       // ...
   }

3. Monitor stack usage:
   // Add to loop
   if (millis() % 10000 == 0) {
       printMemoryStatus();
   }
   
   → Low free heap = memory leak
   → Stack pointer moving = deep recursion

4. Enable core dump:
   monitor_filters = esp32_exception_decoder
   
   → Shows stack trace on crash
```

### **Advanced Debugging: Logic Analyzer**

For SPI problems, a logic analyzer (Saleae, etc.) is invaluable:

```
View SPI signals in real-time:

NSS  : ‾‾‾‾\____/‾‾‾‾‾
SCK  : ____/‾\_/‾\_/‾\__
MOSI : ===< 0x86 >=====  (Set freq command)
MISO : ========< 0x00 >  (Response)

Reveals:
- Timing issues (too fast/slow)
- Correct command bytes
- Radio responses
```

### **Embedded Debugging Best Practices**

```cpp
// 1. Use descriptive prefixes
Serial.println("[RADIO] Initializing...");
Serial.println("[STATE] Switching to recon mode");
Serial.println("[ERROR] Memory allocation failed");

// 2. Include context
Serial.printf("[PACKET] Received %d bytes at %.1f dBm on %.3f MHz\n",
             length, rssi, frequency);

// 3. Add timestamps
Serial.printf("[%10u] Event occurred\n", millis());

// 4. Use different log levels
#define LOG_ERROR(msg)   Serial.printf("[ERROR] %s\n", msg)
#define LOG_WARNING(msg) Serial.printf("[WARN]  %s\n", msg)
#define LOG_INFO(msg)    Serial.printf("[INFO]  %s\n", msg)
#define LOG_DEBUG(msg)   Serial.printf("[DEBUG] %s\n", msg)

// 5. Conditional compilation
#ifdef DEBUG_PROTOCOL
void dumpPacket(const uint8_t* data, size_t len) {
    // Detailed packet analysis
}
#endif
```

---

## **13. Common Pitfalls & Solutions**

### **Pitfall 1: Forgetting to Call startReceive()**

```cpp
// ❌ Wrong:
void handlePacketReception() {
    if (packetReceived) {
        radio.readData(buffer, size);
        // Process packet...
    }
    // BUG: Radio won't receive more packets!
}

// ✅ Correct:
void handlePacketReception() {
    if (packetReceived) {
        radio.readData(buffer, size);
        // Process packet...
        radio.startReceive();  // CRITICAL!
    }
}
```

**Why:** SX1262 goes to standby after receiving. Must explicitly restart.

### **Pitfall 2: millis() Overflow**

```cpp
// ❌ Wrong: Breaks after 49 days
if (millis() >= lastTime + 12000) {
    // When millis() wraps to 0, this breaks!
}

// ✅ Correct: Works even after overflow
if (millis() - lastTime >= 12000) {
    // Subtraction handles wraparound correctly
}
```

**Why:** Unsigned subtraction handles overflow naturally.

### **Pitfall 3: Using String in Interrupt Context**

```cpp
// ❌ NEVER do this:
void IRAM_ATTR onPacketReceived() {
    String msg = "Packet received!";  // CRASH!
    Serial.println(msg);                // CRASH!
}

// ✅ Do this:
volatile bool packetReceived = false;

void IRAM_ATTR onPacketReceived() {
    packetReceived = true;  // Just set flag
}

void loop() {
    if (packetReceived) {
        String msg = "Packet received!";  // OK in main context
        Serial.println(msg);
    }
}
```

**Why:** ISR must be fast. String/Serial use heap/mutexes (dangerous in ISR).

### **Pitfall 4: Blocking in Main Loop**

```cpp
// ❌ Wrong: Blocks forever
void loop() {
    Serial.println("Enter command:");
    while (!Serial.available()) {
        // Stuck here forever!
        // Watchdog will timeout!
    }
    char cmd = Serial.read();
}

// ✅ Correct: Non-blocking with timeout
void loop() {
    if (Serial.available()) {
        char cmd = Serial.read();
        handleCommand(cmd);
    }
    
    esp_task_wdt_reset();  // Pet watchdog
    delay(10);
}
```

**Why:** Loop must run regularly to pet watchdog and check other events.

### **Pitfall 5: Assuming Packet Size**

```cpp
// ❌ Wrong: Buffer overflow if packet > 50 bytes
uint8_t buffer[50];
radio.readData(buffer, 256);  // Could overflow!

// ✅ Correct: Match buffer to actual size
uint8_t buffer[256];
int state = radio.readData(buffer, sizeof(buffer));
if (state == RADIOLIB_ERR_NONE) {
    size_t len = radio.getPacketLength();
    if (len <= sizeof(buffer)) {
        // Safe to process
    }
}
```

**Why:** LoRa packets can be 0-255 bytes. Always check length.

### **Pitfall 6: Floating Point in Critical Code**

```cpp
// ⚠️ Slow: Floating point math in ISR
void IRAM_ATTR onPacketReceived() {
    float rssi = getRSSI();  // Slow!
    float snr = getSNR();    // Slow!
}

// ✅ Better: Read in main loop
std::atomic<bool> packetFlag;

void IRAM_ATTR onPacketReceived() {
    packetFlag = true;  // Fast!
}

void loop() {
    if (packetFlag) {
        float rssi = radio.getRSSI();  // OK here
        float snr = radio.getSNR();
    }
}
```

**Why:** Floating point operations are slow on ESP32 (no FPU). Keep ISR fast.

---

## **14. Performance Optimization**

### **Timing Analysis**

```cpp
// Measure function execution time
uint32_t start = micros();  // Microsecond precision
doSomething();
uint32_t duration = micros() - start;
Serial.printf("Took %u µs\n", duration);
```

**Typical Timings:**

```
Operation                   Time
─────────────────────────────────────
ISR execution               1-10 µs
SPI read (256 bytes)        5 ms
Protocol analysis           1-3 ms
State update                0.5 ms
Serial.printf()             10-50 ms  (SLOW!)
LittleFS write              20-100 ms (VERY SLOW!)
─────────────────────────────────────
Total packet processing:    ~50 ms
Loop iteration:             10-100 ms
```

### **Optimization Techniques**

#### **1. Reduce Serial Output**

```cpp
// ❌ Slow: Print every packet
for (int i = 0; i < packetCount; i++) {
    Serial.printf("Packet %d\n", i);  // Each print ~20ms
}

// ✅ Fast: Batch output
if (packetCount % 10 == 0) {
    Serial.printf("Processed %d packets\n", packetCount);
}
```

#### **2. Use Integer Math Where Possible**

```cpp
// ⚠️ Slower: Float division
float average = (float)total / count;

// ✅ Faster: Integer division (if precision OK)
uint32_t average = total / count;
```

#### **3. Avoid String Concatenation**

```cpp
// ❌ Slow: Multiple allocations
String msg = "Node ";
msg += String(nodeId, HEX);
msg += " at ";
msg += String(rssi);
msg += " dBm";

// ✅ Fast: Single printf
char msg[64];
snprintf(msg, sizeof(msg), "Node %08X at %.1f dBm", nodeId, rssi);
```

#### **4. Minimize Flash Writes**

```cpp
// ❌ Slow: Write every packet
logPacketToFlash(packet);  // 50ms each!

// ✅ Fast: Batch writes
if (packetBuffer.size() >= 10) {
    logMultiplePackets(packetBuffer);
    packetBuffer.clear();
}
```

#### **5. Use Lookup Tables**

```cpp
// ⚠️ Slower: Switch statement
const char* getProtocol(uint8_t type) {
    switch (type) {
        case 0: return "Text";
        case 1: return "Ack";
        case 3: return "Position";
        // ... 10 more cases
    }
}

// ✅ Faster: Lookup table
const char* PROTOCOL_NAMES[] = {
    "Text", "Ack", NULL, "Position", /* ... */
};

const char* getProtocol(uint8_t type) {
    if (type < sizeof(PROTOCOL_NAMES)/sizeof(char*)) {
        return PROTOCOL_NAMES[type];
    }
    return "Unknown";
}
```

### **Memory Optimization**

#### **Use Appropriate Data Types**

```cpp
// ❌ Wasteful: uint32_t for small values
uint32_t packetCount;  // Only need 0-65535

// ✅ Efficient: Right-size variables
uint16_t packetCount;  // Saves 2 bytes per instance
```

#### **Pack Structs**

```cpp
// ❌ Unaligned: 12 bytes (padding)
struct DeviceInfo {
    uint32_t nodeId;    // 4 bytes
    uint8_t type;       // 1 byte + 3 padding
    float rssi;         // 4 bytes
};

// ✅ Packed: 9 bytes
struct DeviceInfo {
    uint32_t nodeId;    // 4 bytes
    float rssi;         // 4 bytes
    uint8_t type;       // 1 byte
} __attribute__((packed));
```

#### **Const Data in Flash**

```cpp
// ❌ Uses RAM: Writable array
const ScanConfig configs[] = { /* ... */ };

// ✅ Uses Flash: Read-only in program memory
const ScanConfig configs[] PROGMEM = { /* ... */ };
```

---

## **15. Study Guide / Learning Path**

### **Beginner Level (Weeks 1-2)**

**Goal:** Understand the basics

1. **Read and modify** `main_realistic.cpp` (simple version)
   - Change dwell time
   - Add a new frequency
   - Modify serial output

2. **Study LoRa basics**
   - What is spreading factor?
   - How does bandwidth affect range?
   - Read SX1262 datasheet (overview)

3. **Experiment with configuration**
   - Try different SF values
   - Test different frequencies
   - Observe range changes

**Projects:**
- Add a second LED that blinks on packet
- Count total packets and display every minute
- Add a "beep" via GPIO when strong signal detected

### **Intermediate Level (Weeks 3-4)**

**Goal:** Understand the architecture

1. **Study class responsibilities**
   - Read `recon_state.h/cpp`
   - Read `protocol_analyzer.h/cpp`
   - Understand separation of concerns

2. **Learn interrupt handling**
   - What is an ISR?
   - Why use atomic variables?
   - Add debug output to trace packet flow

3. **Modify protocol analyzer**
   - Add detection for a custom protocol
   - Improve device classification
   - Add new packet type recognition

**Projects:**
- Add support for a new LoRa device type
- Create a "signal strength meter" display
- Implement simple packet filtering

### **Advanced Level (Weeks 5-8)**

**Goal:** Master embedded systems concepts

1. **Deep dive into memory management**
   - Study fixed vs dynamic allocation
   - Monitor heap fragmentation
   - Optimize buffer sizes

2. **Error handling patterns**
   - Read `error_handler.cpp`
   - Add new error categories
   - Implement custom recovery strategies

3. **Performance optimization**
   - Profile code execution times
   - Reduce serial output overhead
   - Optimize hot paths

**Projects:**
- Add statistics tracking (packets/sec, etc.)
- Implement adaptive scanning (focus on active freqs)
- Create a "record mode" with high-speed buffering

### **Expert Level (Weeks 9-12)**

**Goal:** Extend and innovate

1. **Add new major features**
   - Web interface (ESP32 as web server)
   - SD card logging
   - GPS integration for geo-tagging

2. **Security research**
   - Complete PSK decryption validation
   - Add packet injection capabilities
   - Implement custom protocol fuzzing

3. **Hardware integration**
   - Add OLED display (U8g2 library)
   - Add buttons for hardware control
   - Implement battery monitoring

**Projects:**
- Build complete security assessment tool
- Create automated network mapping
- Present at local security conference

---

## **16. Quick Reference Cheat Sheet**

### **Common Tasks**

```cpp
// Read a packet
uint8_t buffer[256];
int state = radio.readData(buffer, sizeof(buffer));
if (state == RADIOLIB_ERR_NONE) {
    size_t len = radio.getPacketLength();
    float rssi = radio.getRSSI();
    float snr = radio.getSNR();
}

// Change frequency
radio.setFrequency(902.125);
radio.setBandwidth(250.0);
radio.setSpreadingFactor(11);
radio.startReceive();

// Log error
LOG_RADIO_ERROR(ErrorCodes::RADIO_CONFIG_FAILED, 
               "Failed to set frequency");

// Get state info
uint8_t mode = reconState.scanState.mode;
uint8_t configIndex = reconState.scanState.currentConfig;
uint8_t deviceCount = reconState.numTargetableDevices;

// Add tracked device
reconState.addTargetableDevice(nodeId, configIndex, rssi,
                               "Meshtastic", data, length);
```

### **Important Constants**

```cpp
SCAN_DWELL_TIME     = 12000 ms (12 seconds)
MAX_TRACKED_NODES   = 20
MAX_REPLAY_SLOTS    = 10
PACKET_BUFFER_SIZE  = 256 bytes
WATCHDOG_TIMEOUT    = 30 seconds
```

### **GPIO Pins (Heltec V3)**

```
LORA_NSS    = 8   (SPI Chip Select)
LORA_DIO1   = 14  (Interrupt)
LORA_RST    = 12  (Reset)
LORA_BUSY   = 13  (Busy)
SCK         = 9   (SPI Clock)
MISO        = 11  (SPI Master In)
MOSI        = 10  (SPI Master Out)
```

### **Error Codes**

```cpp
RADIOLIB_ERR_NONE               = 0   (Success)
RADIOLIB_ERR_CHIP_NOT_FOUND     = -2  (SPI problem)
RADIOLIB_ERR_PACKET_TOO_LONG    = -5  (Buffer overflow)
RADIOLIB_ERR_TX_TIMEOUT         = -6  (TX failed)
RADIOLIB_ERR_RX_TIMEOUT         = -7  (No packet)
RADIOLIB_ERR_CRC_MISMATCH       = -8  (Corrupted packet)
```

### **Useful Serial Commands**

```
During Operation:
m - Show menu
8 - Target SF8 frequency
f - Frequency targeting
d - Device type summary
a - Activity details
p - Packet replay menu
r - Restart reconnaissance
v - Security assessment
g - Geographic intelligence
t - Hardware stress testing
```

---

## **17. Questions to Test Your Understanding**

Try answering these without looking back:

### **Fundamental Questions**

1. **Why use `std::atomic<bool>` for `packetReceived` instead of regular `bool`?**
   <details>
   <summary>Answer</summary>
   
   To prevent race conditions between the ISR (interrupt context) and main loop. Atomic operations are indivisible - they can't be interrupted mid-operation, ensuring the flag is read/written safely.
   </details>

2. **What happens if you don't call `esp_task_wdt_reset()` in the main loop?**
   <details>
   <summary>Answer</summary>
   
   The hardware watchdog timer will expire after 30 seconds and automatically reboot the ESP32. This is a safety feature to recover from hung systems.
   </details>

3. **Why does `ReconState` use fixed-size arrays instead of `std::vector`?**
   <details>
   <summary>Answer</summary>
   
   Fixed arrays prevent heap fragmentation, provide predictable memory usage, and eliminate allocation failures. Trade-off: Some wasted RAM, but guaranteed reliability for long-running embedded systems.
   </details>

### **Architecture Questions**

4. **How does the command dispatcher know which function to call for command 'm'?**
   <details>
   <summary>Answer</summary>
   
   It searches the `commands[]` dispatch table for an entry with `key == 'm'`, then calls that entry's `handler` function pointer. This is O(1) lookup using a data structure instead of O(n) if/else chain.
   </details>

5. **What's the difference between `RFActivity`, `TrackedNode`, and `TargetableDevice`?**
   <details>
   <summary>Answer</summary>
   
   Three-tier filtering:
   - **RFActivity**: Any RF energy detected (might be noise)
   - **TrackedNode**: Valid packet with node ID (confirmed device)
   - **TargetableDevice**: Device with enough intelligence gathered (best candidates for targeting)
   </details>

### **Technical Questions**

6. **Why must you call `radio.startReceive()` after reading a packet?**
   <details>
   <summary>Answer</summary>
   
   The SX1262 goes to standby mode after receiving a packet. It won't listen for more packets until explicitly told to resume receiving.
   </details>

7. **How does protobuf encoding save space compared to JSON?**
   <details>
   <summary>Answer</summary>
   
   Protobuf uses:
   - Binary encoding (not text)
   - Varint encoding (variable-length integers)
   - Field numbers instead of field names
   - No formatting characters (braces, quotes, commas)
   
   Example: JSON `{"lat":37.7749}` = 17 bytes, Protobuf = 6 bytes
   </details>

8. **What does `memory_order_release` and `memory_order_acquire` mean?**
   <details>
   <summary>Answer</summary>
   
   Memory ordering for atomic operations:
   - **release**: "Publish" - ensures all prior writes are visible before this write
   - **acquire**: "Consume" - ensures this read sees all writes published by release
   
   Prevents CPU reordering that could cause race conditions.
   </details>

### **Practical Questions**

9. **Your tool scans 16 configs at 12 seconds each. Why not scan faster?**
   <details>
   <summary>Answer</summary>
   
   Meshtastic devices typically transmit every 30-60 seconds. If you switch too fast, you'll miss transmissions. 12 seconds is a balance between catching transmissions and completing full cycles in reasonable time (~3 minutes).
   </details>

10. **Why does the ISR only set a flag instead of processing the packet?**
    <details>
    <summary>Answer</summary>
    
    ISRs must be FAST (1-10 µs). Packet processing takes milliseconds. If ISR is slow:
    - Other interrupts get blocked
    - Watchdog might timeout
    - System becomes unstable
    
    Set flag immediately, process later in main loop.
    </details>

---

## **Conclusion**

You now have a comprehensive understanding of:

✅ **Hardware fundamentals** - ESP32, SX1262, SPI, interrupts  
✅ **Software architecture** - Modular design, separation of concerns  
✅ **Embedded concepts** - Memory management, ISRs, watchdogs  
✅ **Protocol analysis** - Meshtastic, LoRaWAN, packet parsing  
✅ **Error handling** - Production-grade recovery and logging  
✅ **Debugging techniques** - Serial debugging, logic analyzers  
✅ **Performance optimization** - Profiling, memory efficiency  
✅ **Best practices** - Patterns to use, pitfalls to avoid  

**Next Steps:**

1. **Study the code** with this guide open
2. **Make small modifications** to test understanding
3. **Add new features** to practice concepts
4. **Debug issues** using techniques learned
5. **Optimize performance** where needed
6. **Share knowledge** - teach others!

**Remember:** Understanding takes time. Revisit sections as needed. The more you experiment, the deeper your understanding becomes.

**Happy hacking! 🚀**

---

*This guide was created to help you truly understand every aspect of your ESP32 LoRa reconnaissance tool. Use it as a learning companion, reference manual, and debugging aid.*

*Last Updated: October 5, 2025*
