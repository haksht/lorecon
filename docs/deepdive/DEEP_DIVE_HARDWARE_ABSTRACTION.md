# Deep Dive: Hardware Abstraction & RadioLib

## Executive Summary

This document explains how your ESP32 LoRa sniffer communicates with the SX1262 radio chip at the lowest level. You'll understand the SPI protocol, RadioLib's hardware abstraction layers, how the Module class manages GPIO pins, interrupt handling, and the complete radio initialization and configuration process.

---

## Table of Contents
1. [Hardware Architecture](#hardware-architecture)
2. [SPI Protocol](#spi-protocol)
3. [RadioLib Architecture](#radiolib-architecture)
4. [Module Class](#module-class)
5. [SX1262 Radio Driver](#sx1262-driver)
6. [Interrupt Handling](#interrupt-handling)
7. [Radio Configuration](#radio-configuration)
8. [Performance Optimization](#performance)

---

## Hardware Architecture

### System Block Diagram

```
┌──────────────────────────────────────────────────────────────┐
│                    ESP32-S3 Microcontroller                  │
│                                                              │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐  │
│  │   CPU Core   │    │ SPI Hardware │    │ GPIO Engine  │  │
│  │  240 MHz     │◄──►│  Peripheral  │◄──►│ Interrupt    │  │
│  │              │    │              │    │ Controller   │  │
│  └──────────────┘    └──────┬───────┘    └──────┬───────┘  │
│                             │                    │          │
│                             │ SPI Bus            │ GPIO     │
└─────────────────────────────┼────────────────────┼──────────┘
                              │                    │
                    ┌─────────┴────────┐  ┌────────┴────────┐
                    │                  │  │                 │
                  MOSI (GPIO 10)   DIO1 (GPIO 14)          │
                  MISO (GPIO 11)   BUSY (GPIO 13)          │
                   SCK (GPIO 9)    RST  (GPIO 12)          │
                   NSS (GPIO 8)                             │
                              │                    │
┌─────────────────────────────┼────────────────────┼──────────┐
│                  SX1262 LoRa Radio Chip                     │
│                                                             │
│  ┌────────────┐    ┌──────────────┐    ┌────────────────┐ │
│  │ SPI        │    │ LoRa Modem   │    │ RF Frontend    │ │
│  │ Interface  │───►│ FSK/LoRa     │───►│ PA/LNA/Switch  │─┼─► Antenna
│  └────────────┘    └──────────────┘    └────────────────┘ │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Pin Assignments

**SPI Bus (4 wires):**

```
Pin Name  | ESP32-S3 GPIO | Direction | Purpose
----------|---------------|-----------|----------------------------------
MOSI      | GPIO 10       | ESP→SX    | Master Out Slave In (data to radio)
MISO      | GPIO 11       | SX→ESP    | Master In Slave Out (data from radio)
SCK       | GPIO 9        | ESP→SX    | Serial Clock (clock signal)
NSS       | GPIO 8        | ESP→SX    | Not Slave Select (chip select, active LOW)
```

**Control Signals (3 wires):**

```
Pin Name  | ESP32-S3 GPIO | Direction | Purpose
----------|---------------|-----------|----------------------------------
DIO1      | GPIO 14       | SX→ESP    | Digital I/O 1 (interrupt for RX/TX done)
BUSY      | GPIO 13       | SX→ESP    | Busy flag (HIGH when radio processing)
RST       | GPIO 12       | ESP→SX    | Reset (active LOW, pulse to reset radio)
```

**Visual:**

```
ESP32-S3                                    SX1262
────────                                    ──────

GPIO 10 (MOSI) ────────────────────────► SPI_MOSI
GPIO 11 (MISO) ◄──────────────────────── SPI_MISO
GPIO 9  (SCK)  ────────────────────────► SPI_SCK
GPIO 8  (NSS)  ────────────────────────► NSS (Chip Select)

GPIO 14 (DIO1) ◄──────────────────────── DIO1 (Interrupt)
GPIO 13 (BUSY) ◄──────────────────────── BUSY (Status)
GPIO 12 (RST)  ────────────────────────► NRST (Reset)
```

### Why These Specific Pins?

**SPI Pins (HSPI on ESP32-S3):**

```
ESP32-S3 has multiple SPI buses:
  ├─ SPI0/SPI1: Reserved for flash memory
  ├─ SPI2 (HSPI): Available for peripherals ← We use this
  └─ SPI3 (VSPI): Also available

Default HSPI pins:
  ├─ GPIO 9  = SCK  ✅
  ├─ GPIO 10 = MOSI ✅
  ├─ GPIO 11 = MISO ✅
  └─ GPIO 8  = NSS  ✅ (can be any GPIO actually)

Benefits of using hardware SPI:
  ✅ DMA support (direct memory access)
  ✅ Hardware acceleration (fast transfers)
  ✅ CPU can do other work during transfer
  ❌ Software SPI: Slow, blocks CPU, no DMA
```

**Control Pins:**

```
DIO1, BUSY, RST can be any available GPIO:
  ├─ DIO1: Needs interrupt capability → GPIO 14 ✅
  ├─ BUSY: Just read status → GPIO 13 ✅
  └─ RST: Just pulse output → GPIO 12 ✅

Chosen based on:
  ✅ Not conflicting with I2C (GPIO 17, 18)
  ✅ Not conflicting with button (GPIO 0)
  ✅ Available on Heltec V3 board
```

---

## SPI Protocol

### What is SPI?

**SPI = Serial Peripheral Interface**

- **Synchronous protocol** (clock line coordinates data)
- **Full-duplex** (simultaneous send and receive)
- **Master-slave architecture** (ESP32 = master, SX1262 = slave)
- **Four-wire interface** (MOSI, MISO, SCK, NSS)

**Visual:**

```
Master (ESP32)                     Slave (SX1262)
──────────────                     ──────────────

  ┌────────┐                         ┌────────┐
  │ Shift  │───── MOSI ─────────────►│ Shift  │
  │Register│                         │Register│
  │        │◄──── MISO ───────────────│        │
  └────────┘                         └────────┘
      │                                   │
      └───────── SCK ────────────────────┘
      │          (Clock)
      └───────── NSS ─────────────► (Chip Enable)
```

### SPI Transaction Anatomy

**Example: Write register 0x1D with value 0x42**

```
Signal Timeline:

NSS:  ─────┐                                        ┌─────
           └────────────────────────────────────────┘
           ↑ Start                           Stop ↑

SCK:  ────┐ ┐ ┐ ┐ ┐ ┐ ┐ ┐ ┐ ┐ ┐ ┐ ┐ ┐ ┐ ┐ ┐────
          └─┘─┘─┘─┘─┘─┘─┘─┘─┘─┘─┘─┘─┘─┘─┘─┘
          ↑ 8 clocks for cmd, 8 for addr, 8 for data

MOSI: ──▄▄▄▄▄─────▄▄▄▄▄─────────▄▄▄─────▄▄▄▄▄────
         0x20      0x1D           0x42
         (Write)   (Addr)         (Data)

MISO: ──??????????????????????????????????????????
         (Don't care for write)
```

**Breakdown:**

```
Step 1: NSS goes LOW
  └─ Tells SX1262: "Listen, a transaction is starting"
  └─ Radio activates SPI interface

Step 2: Send command byte (0x20 = Write Register)
  ├─ MOSI shifts out bits: 0 0 1 0 0 0 0 0
  ├─ 8 clock pulses on SCK
  └─ SX1262 receives command

Step 3: Send address byte (0x1D = target register)
  ├─ MOSI shifts out bits: 0 0 0 1 1 1 0 1
  ├─ 8 more clock pulses
  └─ SX1262 knows which register to write

Step 4: Send data byte (0x42 = value to write)
  ├─ MOSI shifts out bits: 0 1 0 0 0 0 1 0
  ├─ 8 more clock pulses
  └─ SX1262 writes 0x42 to register 0x1D

Step 5: NSS goes HIGH
  └─ Transaction complete
  └─ Radio processes the command
```

### SPI Timing

**Clock Speed:**

```cpp
// ESP32 SPI configuration
SPISettings settings(2000000, MSBFIRST, SPI_MODE0);
//                   │         │         │
//                   │         │         └─ Mode 0 (CPOL=0, CPHA=0)
//                   │         └─────────── Most Significant Bit first
//                   └─────────────────────── 2 MHz clock frequency
```

**SPI Modes:**

```
Mode | CPOL | CPHA | Clock Polarity       | Clock Phase
-----|------|------|----------------------|------------------------
  0  |  0   |  0   | Idle LOW             | Sample on rising edge
  1  |  0   |  1   | Idle LOW             | Sample on falling edge
  2  |  1   |  0   | Idle HIGH            | Sample on falling edge
  3  |  1   |  1   | Idle HIGH            | Sample on rising edge

SX1262 supports Mode 0:
  ├─ Clock idle state: LOW
  ├─ Data sampled on: Rising edge
  └─ Data changed on: Falling edge
```

**Timing Diagram (Mode 0):**

```
SCK:  ────┐   ┐   ┐   ┐   ┐   ┐   ┐   ┐───
          └───┘   └───┘   └───┘   └───┘
          ↑   ↑   ↑   ↑   ↑   ↑   ↑   ↑
Sample:   1   2   3   4   5   6   7   8

MOSI: ────█▀▀▀█▀▀▀█───█───█▀▀▀█───█▀▀▀█───
          D7  D6  D5  D4  D3  D2  D1  D0
          ↑ Bit 7 (MSB first)

Data valid before rising edge, sampled on rising edge
```

### NSS (Chip Select) Behavior

**Why NSS is Critical:**

```
Multiple SPI devices on same bus:
  ├─ SX1262 radio (NSS on GPIO 8)
  ├─ SD card (if present, different NSS)
  └─ Other peripherals (each has own NSS)

NSS selection:
  NSS LOW  = Device selected (listens to SPI bus)
  NSS HIGH = Device ignored (tri-states MISO)

Without NSS:
  ❌ All devices try to talk at once
  ❌ Bus contention
  ❌ Data corruption
```

**RadioLib NSS Management:**

```cpp
// RadioLib handles this automatically:
void Module::SPIreadRegisterBurst(uint8_t reg, uint8_t* data, size_t len) {
    // Step 1: Assert NSS (start transaction)
    digitalWrite(_cs, LOW);
    
    // Step 2: SPI transfer
    SPI.transfer(reg);
    for (size_t i = 0; i < len; i++) {
        data[i] = SPI.transfer(0x00);  // Dummy write, read MISO
    }
    
    // Step 3: De-assert NSS (end transaction)
    digitalWrite(_cs, HIGH);
}
```

### BUSY Signal

**Why BUSY Exists:**

```
Problem: Radio needs time to process commands
  ├─ Write register: Instant
  ├─ Change frequency: ~100 μs (PLL tuning)
  ├─ Start RX: ~1 ms (calibration)
  └─ Transmit: Duration of packet (variable)

Without BUSY:
  ❌ ESP32 sends next command too soon
  ❌ Radio still processing previous command
  ❌ Second command ignored or corrupts state

With BUSY:
  ✅ Radio asserts BUSY HIGH when processing
  ✅ ESP32 waits for BUSY to go LOW
  ✅ Then sends next command
  ✅ Guaranteed safe timing
```

**BUSY Wait Pattern:**

```cpp
void Module::waitOnBusy() {
    if (_busy == RADIOLIB_NC) {
        // No BUSY pin connected, use delay
        delay(10);  // Conservative fixed delay
        return;
    }
    
    // Wait for BUSY to go LOW (with timeout)
    uint32_t start = millis();
    while (digitalRead(_busy) == HIGH) {
        if (millis() - start > 1000) {
            // Timeout! Radio stuck?
            return;
        }
        delayMicroseconds(10);  // Brief yield
    }
}
```

**Typical BUSY Timeline:**

```
Command: Set Frequency to 915.0 MHz

Time:     0μs       100μs      200μs      300μs
          │         │          │          │
BUSY:   ──┐         │          └──────────────
          └─────────┘
          ↑ Start   ↑ PLL lock
          
ESP32 sends command
Radio asserts BUSY HIGH
Radio tunes PLL to 915.0 MHz (takes ~150 μs)
Radio de-asserts BUSY LOW
ESP32 proceeds with next operation
```

---

## RadioLib Architecture

### Library Structure

```
RadioLib Architecture:

┌────────────────────────────────────────────────────┐
│              Your Application Code                 │
│  (lora_recon_tool.cpp)                            │
└─────────────────┬──────────────────────────────────┘
                  │
                  │ radio.setFrequency(915.0)
                  │ radio.startReceive()
                  ▼
┌────────────────────────────────────────────────────┐
│              SX1262 Class                          │
│  High-level radio-specific API                    │
│  ├─ setFrequency()                                │
│  ├─ setBandwidth()                                │
│  ├─ setSpreadingFactor()                          │
│  └─ startReceive() / transmit()                   │
└─────────────────┬──────────────────────────────────┘
                  │
                  │ SPIwriteCommand(CMD_SET_RF_FREQUENCY)
                  │ SPIsetRegValue(REG_MODEM_CONFIG)
                  ▼
┌────────────────────────────────────────────────────┐
│              PhysicalLayer Class                   │
│  Medium-level protocol handling                    │
│  ├─ Packet formatting                             │
│  ├─ CRC handling                                  │
│  ├─ Interrupt management                          │
│  └─ State machine (RX/TX/IDLE)                    │
└─────────────────┬──────────────────────────────────┘
                  │
                  │ SPIwriteRegister(addr, value)
                  │ SPIreadRegister(addr)
                  ▼
┌────────────────────────────────────────────────────┐
│              Module Class                          │
│  Low-level hardware abstraction                    │
│  ├─ SPI transaction management                     │
│  ├─ GPIO control (NSS, BUSY, RST, DIO)           │
│  ├─ Interrupt registration                         │
│  └─ Hardware initialization                        │
└─────────────────┬──────────────────────────────────┘
                  │
                  │ SPI.transfer()
                  │ digitalWrite()
                  ▼
┌────────────────────────────────────────────────────┐
│              Arduino Hardware Layer                │
│  ├─ SPI peripheral driver                         │
│  ├─ GPIO driver                                   │
│  └─ Interrupt controller                          │
└────────────────────────────────────────────────────┘
                  │
                  ▼
         [ESP32-S3 Hardware]
```

### Abstraction Benefits

**Why Multiple Layers?**

```
Single monolithic driver (bad):
  ❌ Duplicate code for each radio chip
  ❌ Hard to add new hardware support
  ❌ Bugs replicated across implementations
  ❌ No code reuse

Layered architecture (good):
  ✅ Module class: Common to all radios
  ✅ PhysicalLayer: Shared LoRa/FSK logic
  ✅ SX1262 class: Chip-specific details
  ✅ Easy to support new chips (SX1276, RFM95, etc.)
```

**Example: Supporting Multiple Radios:**

```cpp
// Your code works with any radio:
PhysicalLayer* radio;

#ifdef USE_SX1262
    radio = new SX1262(new Module(8, 14, 12, 13));
#elif defined(USE_SX1276)
    radio = new SX1276(new Module(8, 14, 12, 13));
#elif defined(USE_RFM95)
    radio = new RFM95(new Module(8, 14, 12, 13));
#endif

// Same API regardless of hardware:
radio->setFrequency(915.0);
radio->startReceive();
```

---

## Module Class

### Initialization

**Creating a Module:**

```cpp
// Hardware configuration
#define LORA_NSS    8
#define LORA_DIO1   14
#define LORA_RST    12
#define LORA_BUSY   13

// Create Module instance
Module radioModule(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

// Create SX1262 with this module
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);
//                         │          │          │          │
//                         │          │          │          └─ BUSY pin
//                         │          │          └──────────── RST pin
//                         │          └─────────────────────── DIO1 pin (interrupt)
//                         └────────────────────────────────── NSS pin (chip select)
```

**Module Constructor:**

```cpp
Module::Module(int cs, int irq, int rst, int gpio) {
    _cs = cs;      // NSS pin
    _irq = irq;    // DIO1 pin (interrupt)
    _rst = rst;    // Reset pin
    _busy = gpio;  // BUSY pin
    
    // Initialize pins
    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);  // NSS idle HIGH (deselected)
    
    if (_rst != RADIOLIB_NC) {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, HIGH);  // RST idle HIGH (not reset)
    }
    
    if (_busy != RADIOLIB_NC) {
        pinMode(_busy, INPUT);  // BUSY is input from radio
    }
    
    if (_irq != RADIOLIB_NC) {
        pinMode(_irq, INPUT);  // DIO1 is input from radio
    }
}
```

**RADIOLIB_NC Constant:**

```cpp
#define RADIOLIB_NC  0xFF  // Not Connected

// Usage:
Module mod(8, 14, RADIOLIB_NC, 13);  // No reset pin
//                └─ RST not connected

// Library checks:
if (_rst != RADIOLIB_NC) {
    // Reset pin available, use it
} else {
    // No reset pin, skip reset pulse
}
```

### SPI Operations

**Low-Level SPI Read:**

```cpp
uint8_t Module::SPIreadRegister(uint16_t reg) {
    uint8_t result;
    
    // Step 1: Wait for radio ready
    waitOnBusy();
    
    // Step 2: Start transaction
    digitalWrite(_cs, LOW);
    
    // Step 3: Send read command + address
    SPI.transfer((uint8_t)(reg >> 8));  // High byte of address
    SPI.transfer((uint8_t)(reg));       // Low byte of address
    
    // Step 4: Read data
    result = SPI.transfer(0x00);  // Dummy write, read MISO
    
    // Step 5: End transaction
    digitalWrite(_cs, HIGH);
    
    return result;
}
```

**Low-Level SPI Write:**

```cpp
void Module::SPIwriteRegister(uint16_t reg, uint8_t data) {
    // Step 1: Wait for radio ready
    waitOnBusy();
    
    // Step 2: Start transaction
    digitalWrite(_cs, LOW);
    
    // Step 3: Send write command + address
    SPI.transfer((uint8_t)(reg >> 8) | 0x80);  // High byte + write bit
    SPI.transfer((uint8_t)(reg));              // Low byte
    
    // Step 4: Write data
    SPI.transfer(data);
    
    // Step 5: End transaction
    digitalWrite(_cs, HIGH);
    
    // Step 6: Wait for write to complete
    waitOnBusy();
}
```

**Burst Read (Multiple Bytes):**

```cpp
void Module::SPIreadRegisterBurst(uint16_t reg, uint8_t* data, size_t len) {
    waitOnBusy();
    digitalWrite(_cs, LOW);
    
    // Send address
    SPI.transfer((uint8_t)(reg >> 8));
    SPI.transfer((uint8_t)(reg));
    
    // Read multiple bytes
    for (size_t i = 0; i < len; i++) {
        data[i] = SPI.transfer(0x00);
    }
    
    digitalWrite(_cs, HIGH);
}
```

**Why Burst Transfers?**

```
Reading packet (255 bytes):

Single-byte reads:
  └─ 255 transactions = 255× NSS assertions
  └─ Total time: ~25 ms (lots of overhead)

Burst read:
  └─ 1 transaction, 255 bytes transferred
  └─ Total time: ~2 ms (minimal overhead)
  └─ 12× faster!

Burst transfers critical for:
  ✅ Reading RX buffers (packet data)
  ✅ Writing TX buffers (packet data)
  ✅ Dumping registers (debugging)
```

### Reset Pulse

**Hardware Reset:**

```cpp
void Module::reset() {
    if (_rst == RADIOLIB_NC) {
        // No reset pin, can't do hardware reset
        return;
    }
    
    // Assert reset (active LOW)
    digitalWrite(_rst, LOW);
    delay(10);  // Hold reset for 10 ms
    
    // De-assert reset
    digitalWrite(_rst, HIGH);
    delay(10);  // Wait for radio to boot
}
```

**Reset Timeline:**

```
RST Pin:  ─────┐           ┌─────────────────
               └───────────┘
               ↑ 10ms      ↑ 10ms wait
               
Radio State:
  Before: Unknown (could be stuck)
  During: Reset (all state cleared)
  After:  Boot sequence (loads defaults)
  Ready:  ~20ms after de-assert
```

**When to Reset:**

```
✅ During initialization (ensure clean state)
✅ After configuration error (recover)
✅ After radio hangs (unstick)
❌ During normal operation (unnecessary)
```

---

## SX1262 Driver

### Initialization Sequence

**Full Init Process:**

```cpp
int SX1262::begin() {
    // Step 1: Hardware reset
    _mod->reset();
    
    // Step 2: Check SPI communication
    uint8_t syncWord = SPIreadRegister(REG_SYNC_WORD);
    if (syncWord != 0x14 && syncWord != 0x34) {
        // SPI communication failed
        return RADIOLIB_ERR_CHIP_NOT_FOUND;
    }
    
    // Step 3: Set standby mode (STDBY_RC)
    int state = standby(RADIOLIB_SX126X_STANDBY_RC);
    if (state != RADIOLIB_ERR_NONE) return state;
    
    // Step 4: Configure packet type (LoRa)
    state = setPacketType(RADIOLIB_SX126X_PACKET_TYPE_LORA);
    if (state != RADIOLIB_ERR_NONE) return state;
    
    // Step 5: Set default frequency (915.0 MHz)
    state = setFrequency(915.0);
    if (state != RADIOLIB_ERR_NONE) return state;
    
    // Step 6: Set default LoRa parameters
    state = setBandwidth(125.0);             // 125 kHz
    state = setSpreadingFactor(7);           // SF7
    state = setCodingRate(5);                // CR 4/5
    state = setSyncWord(RADIOLIB_SX126X_SYNC_WORD_PRIVATE);
    
    // Step 7: Configure DIO1 for RX/TX done
    state = setDio1Action(nullptr);
    
    // Step 8: Calibrate
    state = calibrateImage(902.0, 928.0);    // ISM band
    
    return RADIOLIB_ERR_NONE;
}
```

**Why This Order?**

```
Step 1 (Reset):
  └─ Ensures known state
  └─ Clears any previous configuration
  
Step 2 (SPI Check):
  └─ Verifies hardware connection
  └─ Catches wiring errors early
  
Step 3 (Standby):
  └─ Radio must be in standby for configuration
  └─ Can't configure while in RX/TX
  
Step 4 (Packet Type):
  └─ LoRa vs FSK (fundamentally different)
  └─ Must be set before other LoRa parameters
  
Steps 5-6 (Parameters):
  └─ Set known-good defaults
  └─ Can be changed later
  
Step 7 (Interrupts):
  └─ Enable RX/TX completion notification
  
Step 8 (Calibration):
  └─ Optimize for frequency range
  └─ Improves sensitivity/selectivity
```

### Register Access

**SX1262 Register Map (Partial):**

```
Address   | Name              | Description
----------|-------------------|----------------------------------------
0x0740    | REG_SYNC_WORD     | LoRa sync word (0x1424 = private)
0x08D8    | REG_RF_FREQUENCY  | RF frequency (24-bit value)
0x0889    | REG_MODEM_CONFIG  | SF, BW, CR configuration
0x0925    | REG_TX_PARAMS     | TX power, ramp time
0x08E3    | REG_RX_GAIN       | RX gain control
```

**Reading Register:**

```cpp
// High-level API
uint8_t syncWord = radio.getLoRaSyncWord();

// What happens internally:
uint16_t SX1262::getLoRaSyncWord() {
    // Read 2-byte sync word from register 0x0740
    uint8_t sync[2];
    _mod->SPIreadRegisterBurst(RADIOLIB_SX126X_REG_LORA_SYNC_WORD_MSB, sync, 2);
    return ((uint16_t)sync[0] << 8) | sync[1];
}
```

**Writing Register:**

```cpp
// High-level API
radio.setSyncWord(0x1424);

// What happens internally:
int SX1262::setSyncWord(uint16_t syncWord) {
    // Write 2-byte sync word to register 0x0740
    uint8_t sync[2] = {
        (uint8_t)(syncWord >> 8),
        (uint8_t)(syncWord & 0xFF)
    };
    return _mod->SPIwriteRegisterBurst(
        RADIOLIB_SX126X_REG_LORA_SYNC_WORD_MSB, 
        sync, 
        2
    );
}
```

### Command Interface

**SX1262 Commands (via SPI):**

```
Command         | Opcode | Parameters      | Description
----------------|--------|-----------------|---------------------------
SetStandby      | 0x80   | mode (1 byte)   | Enter standby mode
SetRx           | 0x82   | timeout (3 bytes)| Start receive
SetTx           | 0x83   | timeout (3 bytes)| Start transmit
SetRfFrequency  | 0x86   | freq (4 bytes)  | Set RF frequency
SetPacketType   | 0x8A   | type (1 byte)   | LoRa or FSK
GetPacketStatus | 0x14   | none            | Get RSSI/SNR
ReadBuffer      | 0x1E   | offset (1 byte) | Read RX buffer
WriteBuffer     | 0x0E   | offset (1 byte) | Write TX buffer
```

**Sending Command:**

```cpp
int SX1262::setRx(uint32_t timeout) {
    // Build command packet
    uint8_t cmd[4];
    cmd[0] = RADIOLIB_SX126X_CMD_SET_RX;  // 0x82
    cmd[1] = (uint8_t)(timeout >> 16);
    cmd[2] = (uint8_t)(timeout >> 8);
    cmd[3] = (uint8_t)(timeout);
    
    // Send via SPI
    return _mod->SPIwriteStream(cmd, 4);
}

// SPIwriteStream:
int Module::SPIwriteStream(uint8_t* cmd, size_t len) {
    waitOnBusy();
    digitalWrite(_cs, LOW);
    
    for (size_t i = 0; i < len; i++) {
        SPI.transfer(cmd[i]);
    }
    
    digitalWrite(_cs, HIGH);
    waitOnBusy();  // Wait for command processing
    
    return RADIOLIB_ERR_NONE;
}
```

---

## Interrupt Handling

### DIO1 Pin

**What is DIO1?**

```
DIO1 = Digital I/O 1
  ├─ Configurable GPIO on SX1262
  ├─ Can be mapped to various internal events
  └─ We use it for: RX_DONE and TX_DONE interrupts

Why interrupts?
  ❌ Polling: Waste CPU checking if packet ready
  ✅ Interrupt: Radio notifies ESP32 when packet arrives
```

**DIO1 Mapping:**

```cpp
int SX1262::setDio1Action(void (*func)(void)) {
    // Configure DIO1 to assert on:
    //   - RX_DONE (packet received)
    //   - TX_DONE (transmission complete)
    //   - RX_TX_TIMEOUT (timeout expired)
    uint16_t irqMask = RADIOLIB_SX126X_IRQ_RX_DONE |
                       RADIOLIB_SX126X_IRQ_TX_DONE |
                       RADIOLIB_SX126X_IRQ_TIMEOUT;
    
    // Set DIO1 mask
    int state = setDioIrqParams(irqMask, irqMask);
    if (state != RADIOLIB_ERR_NONE) return state;
    
    // Register ISR
    if (func) {
        attachInterrupt(digitalPinToInterrupt(_mod->getIrq()), 
                       func, RISING);
    }
    
    return RADIOLIB_ERR_NONE;
}
```

### ESP32 Interrupt

**Registering ISR:**

```cpp
// Your ISR function
void IRAM_ATTR onPacketReceived() {
    // Set atomic flag
    if (g_reconTool) {
        g_reconTool->markPacketReceived();
    }
}

// During init:
pinMode(LORA_DIO1, INPUT);
radio.setDio1Action(onPacketReceived);
//                  └─ Function pointer to your ISR
```

**IRAM_ATTR Attribute:**

```cpp
void IRAM_ATTR onPacketReceived() { ... }
     └─ This attribute is CRITICAL!

What it does:
  ├─ Places function code in IRAM (fast RAM)
  ├─ Not in flash (slow, cache issues)
  └─ Required for interrupt handlers on ESP32

Without IRAM_ATTR:
  ❌ ISR stored in flash
  ❌ Flash cache disabled during ISR
  ❌ CPU crash (instruction fetch fails)
  ❌ System hangs

With IRAM_ATTR:
  ✅ ISR stored in RAM
  ✅ Always accessible
  ✅ Fast execution
  ✅ Reliable interrupts
```

### Interrupt Sequence

**RX Complete Interrupt Flow:**

```
Timeline:

T=0ms:     Radio in RX mode, listening
           │
T=10ms:    LoRa preamble detected
           └─► Radio enters sync acquisition
           
T=15ms:    Sync word matched
           └─► Radio begins demodulation
           
T=35ms:    Packet fully received
           └─► Radio writes packet to RX buffer
           └─► Radio asserts DIO1 HIGH
           
T=35.001ms: ESP32 GPIO interrupt fires
           └─► onPacketReceived() ISR executes
           └─► Sets packetReceived flag
           └─► Returns (ISR complete: ~5 μs)
           
T=36ms:    Main loop checks flag
           └─► Flag is true
           └─► Reads packet from radio
           └─► Processes packet
           └─► Clears flag
```

**Visual:**

```
DIO1 Pin:  ────────────────────┐
                               └────────────
                               ↑ RX_DONE
                               
ESP32 ISR:                     ↓ (~1 μs delay)
                               ▼
                            ISR executes
                            (5 μs duration)
                               │
Main Loop:                     ↓ (~1 ms delay)
                               ▼
                         Check flag → Read packet
```

### Why Not Process in ISR?

**Bad: Process packet in ISR**

```cpp
void IRAM_ATTR onPacketReceived() {
    uint8_t data[256];
    int len = radio.readData(data, 256);  // ❌ BAD!
    
    // Process packet
    decrypt(data, len);  // ❌ BAD!
    parseProtobuf(data);  // ❌ BAD!
    Serial.println("Packet received");  // ❌ BAD!
}

Problems:
  ❌ ISR runs for milliseconds (too long!)
  ❌ Blocks other interrupts
  ❌ Serial.print() uses UART (not ISR-safe)
  ❌ SPI transfer takes time
  ❌ System becomes unresponsive
```

**Good: Flag-based approach**

```cpp
void IRAM_ATTR onPacketReceived() {
    // Just set a flag (atomic, fast)
    packetReceived.store(true);  // ✅ GOOD!
    // ISR done in ~5 microseconds
}

void mainLoop() {
    // Check flag in main loop
    if (packetReceived.load()) {
        // Now we have time to process
        readAndProcessPacket();  // ✅ GOOD!
        packetReceived.store(false);
    }
}

Benefits:
  ✅ ISR extremely fast (<10 μs)
  ✅ Doesn't block other interrupts
  ✅ Processing in main context (safe)
  ✅ Can use Serial, SPI, complex logic
```

---

## Radio Configuration

### Frequency Setting

**setFrequency() Deep Dive:**

```cpp
int SX1262::setFrequency(float freq) {
    // Validate range (SX1262: 150-960 MHz)
    if (freq < 150.0 || freq > 960.0) {
        return RADIOLIB_ERR_INVALID_FREQUENCY;
    }
    
    // Convert MHz to radio's internal format
    // Formula: freq_raw = (freq_mhz * 2^25) / 32
    uint32_t freqRaw = (uint32_t)((freq * 33554432.0) / 32.0);
    
    // Build command
    uint8_t cmd[5];
    cmd[0] = RADIOLIB_SX126X_CMD_SET_RF_FREQUENCY;  // 0x86
    cmd[1] = (uint8_t)(freqRaw >> 24);
    cmd[2] = (uint8_t)(freqRaw >> 16);
    cmd[3] = (uint8_t)(freqRaw >> 8);
    cmd[4] = (uint8_t)(freqRaw);
    
    // Send command
    return _mod->SPIwriteStream(cmd, 5);
}
```

**Frequency Calculation:**

```
Example: 915.0 MHz

Step 1: Convert to Hz
  915.0 MHz = 915,000,000 Hz

Step 2: Apply formula
  freq_raw = (915.0 * 2^25) / 32
           = (915.0 * 33,554,432) / 32
           = 30,712,304,640 / 32
           = 959,759,520
           = 0x3936B000 (hex)

Step 3: Send to radio
  cmd = [0x86, 0x39, 0x36, 0xB0, 0x00]

Radio receives and sets PLL to 915.0 MHz
```

**Frequency Resolution:**

```
Formula: resolution = 32 MHz / 2^25
                   = 32,000,000 / 33,554,432
                   = 0.9536743 Hz

Can tune frequency in ~0.95 Hz steps!

Example:
  915.000000 MHz = step N
  915.000001 MHz = step N+1
  
Incredible precision for LoRa application
```

### Bandwidth Setting

**setBandwidth() Implementation:**

```cpp
int SX1262::setBandwidth(float bw) {
    // Map bandwidth to register value
    uint8_t bwReg;
    
    if (fabs(bw - 7.8) <= 0.1) {
        bwReg = RADIOLIB_SX126X_LORA_BW_7_8;       // 0x00
    } else if (fabs(bw - 10.4) <= 0.1) {
        bwReg = RADIOLIB_SX126X_LORA_BW_10_4;      // 0x08
    } else if (fabs(bw - 15.6) <= 0.1) {
        bwReg = RADIOLIB_SX126X_LORA_BW_15_6;      // 0x01
    } else if (fabs(bw - 20.8) <= 0.1) {
        bwReg = RADIOLIB_SX126X_LORA_BW_20_8;      // 0x09
    } else if (fabs(bw - 31.25) <= 0.1) {
        bwReg = RADIOLIB_SX126X_LORA_BW_31_25;     // 0x02
    } else if (fabs(bw - 41.7) <= 0.1) {
        bwReg = RADIOLIB_SX126X_LORA_BW_41_7;      // 0x0A
    } else if (fabs(bw - 62.5) <= 0.1) {
        bwReg = RADIOLIB_SX126X_LORA_BW_62_5;      // 0x03
    } else if (fabs(bw - 125.0) <= 0.1) {
        bwReg = RADIOLIB_SX126X_LORA_BW_125_0;     // 0x04
    } else if (fabs(bw - 250.0) <= 0.1) {
        bwReg = RADIOLIB_SX126X_LORA_BW_250_0;     // 0x05
    } else if (fabs(bw - 500.0) <= 0.1) {
        bwReg = RADIOLIB_SX126X_LORA_BW_500_0;     // 0x06
    } else {
        return RADIOLIB_ERR_INVALID_BANDWIDTH;
    }
    
    // Write to modem config register
    return setModulationParams(bwReg, _sf, _cr);
}
```

**Why Discrete Values?**

```
LoRa bandwidth is not continuous:
  ├─ Hardware uses fixed filters
  ├─ Each filter has specific cutoff
  └─ Only 10 values supported

Available: 7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250, 500 kHz

Try to set 100 kHz:
  └─ Closest match: 125 kHz
  └─ Or error: INVALID_BANDWIDTH

Meshtastic uses: 125 kHz, 250 kHz, 500 kHz
  └─ Your configs all use these standard values
```

### Spreading Factor

**setSpreadingFactor() Implementation:**

```cpp
int SX1262::setSpreadingFactor(uint8_t sf) {
    // Validate range (SX1262: SF5-SF12)
    if (sf < 5 || sf > 12) {
        return RADIOLIB_ERR_INVALID_SPREADING_FACTOR;
    }
    
    // SF5-SF6 require special handling (DetectionOptimize)
    if (sf == 5 || sf == 6) {
        // Set detection optimize for SF5/SF6
        SPIwriteRegister(REG_DETECTION_OPTIMIZE, 0x05);
    } else {
        // Standard detection for SF7-SF12
        SPIwriteRegister(REG_DETECTION_OPTIMIZE, 0x03);
    }
    
    // Write SF to modem config
    _sf = sf;
    return setModulationParams(_bw, sf, _cr);
}
```

**Spreading Factor Trade-offs:**

```
Spreading Factor    | Range    | Speed    | Sensitivity
--------------------|----------|----------|-------------
SF7  (fastest)      | Shortest | Fastest  | -127 dBm
SF8                 | ↓        | ↓        | -130 dBm
SF9                 | ↓        | ↓        | -132 dBm
SF10                | ↓        | ↓        | -135 dBm
SF11                | ↓        | ↓        | -137 dBm
SF12 (slowest)      | Longest  | Slowest  | -139 dBm

Your Meshtastic configs:
  ├─ Short Fast: SF7  (range, speed, sensitivity)
  ├─ Medium: SF9-SF10 (balance)
  └─ Long Slow: SF11-SF12 (maximum range)
```

### Starting Reception

**startReceive() Flow:**

```cpp
int SX1262::startReceive() {
    // Step 1: Clear any pending interrupts
    clearIrqStatus(RADIOLIB_SX126X_IRQ_ALL);
    
    // Step 2: Set DIO1 to trigger on RX_DONE
    setDio1Action(_rxDoneFunc);
    
    // Step 3: Set RX buffer base address
    setBufferBaseAddress(0x00, 0x00);
    
    // Step 4: Enter continuous RX mode (timeout = 0xFFFFFF)
    uint8_t cmd[4] = {
        RADIOLIB_SX126X_CMD_SET_RX,  // 0x82
        0xFF, 0xFF, 0xFF             // Infinite timeout
    };
    int state = _mod->SPIwriteStream(cmd, 4);
    
    // Radio now listening!
    return state;
}
```

**RX Timeout Values:**

```
Timeout Parameter (24-bit value):

0x000000        = No RX (return immediately)
0x000001-0xFFFFFE = Timeout after N * 15.625 μs
0xFFFFFF        = Continuous RX (no timeout)

Examples:
  0x000064   = 100 * 15.625 μs = 1.5625 ms
  0x001000   = 4096 * 15.625 μs = 64 ms
  0xFFFFFF   = Infinite (Meshtastic uses this)

We use continuous RX:
  ✅ Always listening
  ✅ No need to restart RX
  ✅ Catch all packets
```

---

## Performance

### SPI Speed Optimization

**Clock Frequency Selection:**

```cpp
// RadioLib default: 2 MHz
SPISettings settings(2000000, MSBFIRST, SPI_MODE0);

// Could we go faster?
SPISettings fast(10000000, MSBFIRST, SPI_MODE0);  // 10 MHz

SX1262 maximum: 16 MHz
ESP32 maximum: 80 MHz (HSPI)

Why stick with 2 MHz?
  ✅ Reliable across all conditions
  ✅ Long wire tolerance
  ✅ EMI reduction
  ❌ 10 MHz: Marginal benefit, less reliable
```

**Transfer Time Analysis:**

```
Reading 255-byte packet @ 2 MHz:

Overhead per transaction:
  NSS assertion: ~1 μs
  Command byte: 4 μs (8 bits @ 2 MHz)
  Address byte: 4 μs
  NSS de-assertion: ~1 μs
  Total overhead: ~10 μs

Data transfer:
  255 bytes × 8 bits/byte = 2040 bits
  2040 bits @ 2 MHz = 1020 μs (~1 ms)

Total: ~1.01 ms for 255-byte packet

At 10 MHz:
  Total: ~0.22 ms
  
Improvement: 4.6× faster
But: Still waiting 12 seconds between configs!
  └─ SPI speed not the bottleneck
```

### DMA Acceleration

**ESP32 SPI DMA:**

```cpp
// Arduino SPI library can use DMA automatically
SPI.transfer(buffer, length);
  └─ If length > threshold (default 32 bytes)
  └─ ESP32 uses DMA for transfer
  └─ CPU free during transfer!

Without DMA (byte-by-byte):
  └─ CPU busy for entire transfer
  └─ ~1 ms blocked

With DMA:
  └─ CPU sets up transfer (~50 μs)
  └─ DMA engine handles transfer
  └─ CPU continues other work
  └─ Interrupt when complete
  
RadioLib benefits automatically:
  ✅ Large packet reads use DMA
  ✅ Large packet writes use DMA
  ✅ CPU free for other tasks
```

### Interrupt Latency

**Measuring Interrupt Performance:**

```cpp
void IRAM_ATTR onPacketReceived() {
    uint32_t start = micros();
    
    packetReceived.store(true);
    
    uint32_t duration = micros() - start;
    // Typical: 3-5 microseconds
}
```

**Latency Breakdown:**

```
DIO1 assertion to ISR start: ~1-2 μs
  └─ ESP32 interrupt controller latency
  
ISR execution: ~3-5 μs
  └─ Set atomic flag
  └─ Return from interrupt
  
ISR to main loop processing: ~10-1000 μs
  └─ Depends on what main loop is doing
  
Total: DIO1 → packet processed: ~20-1000 μs

Worst case (1 ms) is acceptable:
  ✅ Packet already in radio buffer (safe)
  ✅ No data loss
  ✅ Processing fast enough for LoRa rates
```

### Memory Efficiency

**RadioLib Memory Usage:**

```
SX1262 object: ~200 bytes
  ├─ Module object: ~50 bytes
  ├─ Configuration cache: ~100 bytes
  └─ Buffer pointers: ~50 bytes

Packet buffers (your code):
  ├─ RX buffer: 256 bytes
  ├─ TX buffer: 256 bytes
  └─ Queue (10 packets): 2560 bytes

Total: ~3 KB RAM

ESP32-S3 has 320 KB RAM:
  └─ RadioLib uses <1% of available RAM
  └─ Very efficient!
```

---

## Summary

### Hardware Abstraction Layers

**Layer 1: Physical (SPI + GPIO):**
- ✅ 7 wires connect ESP32 to SX1262
- ✅ SPI: MOSI, MISO, SCK, NSS (4 wires)
- ✅ Control: DIO1, BUSY, RST (3 wires)
- ✅ 2 MHz SPI clock (reliable, adequate)

**Layer 2: Module Class:**
- ✅ SPI transaction management
- ✅ Register read/write abstractions
- ✅ GPIO control (NSS, RST, BUSY, DIO)
- ✅ Hardware-independent interface

**Layer 3: SX1262 Driver:**
- ✅ High-level radio API
- ✅ Frequency, bandwidth, SF configuration
- ✅ RX/TX state machine
- ✅ Interrupt setup

**Layer 4: Your Application:**
- ✅ Uses simple radio.setFrequency()
- ✅ No need to know SPI details
- ✅ Hardware abstraction complete

### Key Takeaways

**SPI Protocol:**
- ✅ Synchronous, full-duplex serial protocol
- ✅ Mode 0 (CPOL=0, CPHA=0)
- ✅ MSB first, 2 MHz clock
- ✅ NSS chip select (active LOW)
- ✅ BUSY wait before commands

**Module Class:**
- ✅ Hardware abstraction layer
- ✅ Single/burst register access
- ✅ Reset pulse generation
- ✅ Interrupt registration

**SX1262 Driver:**
- ✅ Complex init sequence
- ✅ Frequency: 0.95 Hz resolution
- ✅ Bandwidth: 10 discrete values
- ✅ SF: 5-12 (trade range vs speed)

**Interrupts:**
- ✅ DIO1 triggers on RX_DONE
- ✅ ISR sets atomic flag (fast!)
- ✅ Main loop processes packet
- ✅ IRAM_ATTR required on ESP32

**Performance:**
- ✅ 2 MHz SPI adequate
- ✅ DMA for large transfers
- ✅ <5 μs interrupt latency
- ✅ <1% RAM usage

---

## Code Locations Reference

**Primary files:**
- `lora_recon_tool.h` (lines 31-35): Pin definitions
- `lora_recon_tool.cpp` (line 42): Module creation
- `lora_recon_tool.cpp` (line 85): SPI initialization
- `lora_recon_tool.cpp` (line 88): Radio begin
- `lora_recon_tool.cpp` (line 98): DIO1 interrupt setup

**RadioLib (external library):**
- `Module.cpp`: SPI operations, GPIO control
- `SX1262.cpp`: Radio-specific driver
- `PhysicalLayer.cpp`: Common LoRa logic

**Pin definitions:**
```cpp
#define LORA_NSS    8   // SPI chip select
#define LORA_DIO1   14  // Interrupt pin
#define LORA_RST    12  // Reset pin
#define LORA_BUSY   13  // Busy status
```

**Configuration:**
```cpp
SPI.begin(SCK, MISO, MOSI, LORA_NSS);  // line 85
radio.setFrequency(cfg.frequency);      // line 218
radio.setBandwidth(cfg.bandwidth);      // line 224
radio.setSpreadingFactor(cfg.sf);       // line 230
```

---

## Next Steps

You've now completed **8 out of 9** major deep dives!

Completed:
- ✅ Packet Reception (ISR, queues, watchdog)
- ✅ AES-CTR Decryption (encryption, PSK testing)
- ✅ Protocol Analysis (Protobuf, GPS extraction)
- ✅ State Machine (modes, transitions)
- ✅ Error Handling (watchdog, recovery, diagnostics)
- ✅ Display System (OLED, I2C, U8g2, rendering)
- ✅ Session Key Harvesting (two-layer encryption)
- ✅ Hardware Abstraction (SPI, RadioLib, SX1262)

**One more topic remaining:**

1. **Testing Strategy**: Validation approaches, debugging techniques, stress testing

**You now have conference-level understanding of the complete hardware/software stack from SPI transactions to radio packets!** 🎉

---

*Generated for ESP32-Sniffer deep-dive session*  
*Last updated: 2025-10-16*
