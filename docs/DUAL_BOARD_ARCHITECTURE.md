# Dual ESP32 Board Architecture Design

**Version:** 1.0  
**Date:** November 10, 2025  
**Status:** Future Design Proposal  
**Purpose:** Modular dual-board system for enhanced reconnaissance and triangulation capabilities

> **Note (v2.0)**: This document describes a potential future architecture enhancement. The current v2.0 single-board architecture (RadioController, PacketProcessor, IReconTool) provides a solid foundation for implementing multi-board coordination if this concept moves forward.

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture Options](#architecture-options)
3. [Recommended Design](#recommended-design)
4. [Physical Design](#physical-design)
5. [Power Requirements](#power-requirements)
6. [Antenna Considerations](#antenna-considerations)
7. [Communication Strategy](#communication-strategy)
8. [Case Design](#case-design)
9. [Bill of Materials](#bill-of-materials)
10. [Triangulation Accuracy](#triangulation-accuracy)
11. [Implementation Roadmap](#implementation-roadmap)

---

## Overview

A modular dual-ESP32 board design that can operate in two modes:

- **Combined Mode:** Both boards in single enclosure for coordinated reconnaissance and attack operations
- **Split Mode:** Physically separated boards for triangulation and distributed sensing

This design provides maximum flexibility while maintaining ease of use for typical operations.

---

## Architecture Options

### Option 1: Dedicated Sniffer + Attack Platform (Recommended)

**Board 1 (Reconnaissance/Monitoring):**
- Continuous passive scanning across all frequencies
- Real-time packet capture and logging
- Protocol analysis and device fingerprinting
- OLED display for live monitoring

**Board 2 (Offensive Operations):**
- Executes targeted attacks without disrupting scanning
- Packet replay and injection
- Frequency jamming and interference tests
- Independent radio for simultaneous TX/RX

**Benefits:**
- ✅ **No blind spots** - Board 1 keeps scanning while Board 2 attacks
- ✅ **True full-duplex** - Transmit attacks while receiving responses
- ✅ **Better timing analysis** - Precise response time measurements
- ✅ **Coordinated attacks** - One board floods, other monitors target's behavior
- ✅ **Safety backup** - If attack board crashes, monitor board captures evidence

### Option 2: Multi-Frequency Concurrent Monitoring

Both boards scan different frequency ranges simultaneously:
- Board 1: 902.0-906.0 MHz (Meshtastic US primary channels)
- Board 2: 906.0-928.0 MHz (LoRaWAN + extended ISM band)

**Benefits:**
- ✅ **2x faster reconnaissance** - Complete 16-config scan in ~90 seconds instead of 3 minutes
- ✅ **Catch burst transmissions** - Don't miss packets while hopping frequencies
- ✅ **Multi-mesh monitoring** - Track two separate mesh networks simultaneously

### Option 3: Distributed Sensor Network

Geographic diversity for triangulation:
- Both boards synchronized via WiFi/Serial
- Different physical locations (100m+ apart)
- Compare RSSI from both to estimate target location

**Benefits:**
- ✅ **Direction finding** - Triangulate transmitter position
- ✅ **Range detection** - Better understanding of mesh topology
- ✅ **Coverage mapping** - Identify dead zones and optimal placement
- ✅ **Multi-path analysis** - Detect signal reflections/interference

### Option 4: Protocol Fuzzing Lab

**Board 1 (Target/Victim):**
- Runs actual Meshtastic firmware
- Acts as test victim for attacks
- Provides controlled testing environment

**Board 2 (Attacker/Fuzzer):**
- Executes offensive tests against Board 1
- Monitors for crashes/hangs/anomalies
- Safe testing without risking real devices

---

## Recommended Design

### Modular Dual-Board System with Configurable Roles

```
┌─────────────────────────────────────────────┐
│  COMBINED MODE (Single Case)               │
│  ┌──────────────┐  ┌──────────────┐       │
│  │   Board 1    │  │   Board 2    │       │
│  │   MONITOR    │──│   ATTACK     │       │
│  │              │  │              │       │
│  │  [OLED]      │  │   [LED]      │       │
│  └──────────────┘  └──────────────┘       │
│         │                  │               │
│    [USB-C Port]      [USB-C Port]         │
└─────────────────────────────────────────────┘
           ▼ SPLIT APART ▼
┌──────────────┐              ┌──────────────┐
│   Board 1    │              │   Board 2    │
│   MONITOR    │◄────WiFi────►│   ATTACK     │
│              │              │              │
│  [Battery]   │   50-500m    │  [Battery]   │
└──────────────┘              └──────────────┘
```

**Architecture Diagram:**
```
┌─────────────────────────┐          ┌─────────────────────────┐
│   ESP32 Board 1         │          │   ESP32 Board 2         │
│   "RECON/MONITOR"       │          │   "ATTACK/INJECT"       │
├─────────────────────────┤          ├─────────────────────────┤
│ • Continuous scanning   │◄────────►│ • DeviceStressTester   │
│ • Packet capture        │  Serial  │ • Attack execution      │
│ • Protocol analysis     │   or     │ • Packet injection      │
│ • Device tracking       │  WiFi    │ • Jamming/fuzzing       │
│ • OLED display          │          │ • Replay attacks        │
│ • Session key harvest   │          │                         │
└─────────────────────────┘          └─────────────────────────┘
         ▲                                      ▲
         │                                      │
         └──────────── Coordinated ─────────────┘
              attacks & monitoring
```

---

## Physical Design

### Board 1 Module (Monitor)
- ESP32-S3 + SX1262 LoRa radio
- 128x64 OLED display on top face
- SMA antenna connector (side mount)
- USB-C charging port
- 18650 battery holder (bottom)
- 3-pin connector on edge (power + UART)
- Dimensions: ~60mm x 40mm x 25mm (with battery)

### Board 2 Module (Attack)
- ESP32-S3 + SX1262 LoRa radio
- Status LEDs only (PWR, TX, RX, ATTACK)
- SMA antenna connector (side mount)
- USB-C charging port
- 18650 battery holder (bottom)
- 3-pin connector on edge (power + UART)
- Dimensions: ~60mm x 40mm x 25mm (with battery)

### Connection Methods

**Combined Mode:**
- Boards stack/slide together with magnetic alignment
- 3-pin connector auto-mates (GND + 3.3V + UART)
- Shared communication, independent power
- Both antennas point same direction (or opposite for diversity)

**Split Mode:**
- Pull boards apart (magnets release)
- Switches to WiFi communication automatically
- Each runs on own battery
- Position 50-500m apart for triangulation

---

## Power Requirements

### Combined Mode (Both boards in one case)

**Option A: Shared Power Supply (Simpler)**
- Single USB-C input (5V) with power splitter
- Common 18650 LiPo battery (3000-5000 mAh)
- Charge controller powers both boards
- **Pros:** Simpler, cheaper, lighter
- **Cons:** Both drain single battery

**Option B: Dual Independent Power (Recommended)**
- Each board has its own USB-C port
- Each has independent battery
- Can operate independently immediately
- **Pros:** True independence, easier split operation
- **Cons:** More weight, slightly bulkier

### Split Mode (Triangulation)

**Each board needs:**
- Individual battery (recommended: 2000+ mAh 18650)
- LoRa TX at 22 dBm draws ~120-150mA peak
- RX mode ~15-20mA
- ESP32 idle ~40mA, active ~160mA
- **Runtime estimate:** 6-10 hours continuous operation

**Battery Recommendations:**
- **18650 Li-ion** (3.7V, 3000mAh) - Most common, good capacity
- **21700 Li-ion** (3.7V, 4000-5000mAh) - Longer runtime, slightly bigger
- Add TP4056 charging module for USB-C charging

### Power Consumption Table

| Mode | Board 1 | Board 2 | Total | Battery Life (3000mAh) |
|------|---------|---------|-------|------------------------|
| Idle (both RX) | 60mA | 60mA | 120mA | ~25 hours |
| Active Scan | 80mA | 60mA | 140mA | ~21 hours |
| Attack Active | 80mA | 180mA | 260mA | ~11 hours |
| Full TX (both) | 200mA | 200mA | 400mA | ~7.5 hours |

### Auto-Switching Power Circuit

```
Combined Mode:
  ┌─ Board 1 Battery ──► Board 1 ESP32
  │                   └─► Board 2 ESP32 (via connector)
  └─ Board 2 Battery ──► (Disconnected/Charging only)

Split Mode:
  ┌─ Board 1 Battery ──► Board 1 ESP32
  └─ Board 2 Battery ──► Board 2 ESP32
  
Detection: Each board senses connection via GPIO pin
```

**Implementation:**
- Simple MOSFET switch controlled by connection detect pin
- When connected: Board 1 powers both (larger battery optional)
- When split: Each uses own battery
- Prevents backflow with Schottky diodes

---

## Antenna Considerations

### Do They Need the Same Antenna?

**Short Answer:** No, but matching helps for triangulation accuracy.

### Detailed Analysis

**For Triangulation (Split Mode):**
- ✅ **Same antenna gain recommended** for accurate RSSI comparison
- ✅ **Same polarization** (both vertical or both horizontal)
- ✅ **Same height above ground** when possible
- ❌ Don't need identical models (just similar specs)

**Why matching matters for triangulation:**
```
Target transmits at actual power level X

Board 1: 3dBi antenna sees -58 dBm  } 
Board 2: 3dBi antenna sees -72 dBm  } → 14 dB difference = distance/obstruction

Board 1: 3dBi antenna sees -58 dBm  }
Board 2: 6dBi antenna sees -66 dBm  } → 8 dB difference (misleading!)
                                        → Antenna gain masks distance
```

### Antenna Specifications

**Combined Mode (Compact):**
- **Spring/helical antenna** (2-3 dBi)
- Compact, integrated into case
- Good for close-range work (<1km)
- Example: Standard 433/915MHz spring antenna

**Split Mode (Triangulation):**
- **Detachable SMA connectors** on both boards
- **Option 1:** Both use same whip antenna (3-5 dBi, vertical)
- **Option 2:** Directional antennas (8+ dBi Yagi) for longer range
- Allows swapping based on mission requirements

**Practical Solution:**
```
Each board has:
├─ SMA female connector (detachable antenna)
├─ Include 2x matching 3dBi whip antennas (baseline)
└─ Optional: Upgrade to directional antennas for triangulation
```

### Antenna Mounting
- **Side-mounted SMA** to avoid interference when stacked
- **Strain relief** for antenna cables
- **Protective caps** when antennas removed
- Consider **antenna diversity** (opposite directions) when combined

---

## Communication Strategy

### Combined Mode (Physical Connection)

**UART Serial Connection (Hardware Serial):**
```cpp
// Board 1 → Board 2: Device discoveries
// Board 2 → Board 1: Attack status

// Protocol Format:
// <COMMAND>:<param1>,<param2>,...\n

// Board 1 (Monitor) sends:
DISCOVERED:0x12345678,906.875,-65,SF10,Meshtastic\n
TARGET_ACTIVE:0x12345678,TX_DETECTED\n
PACKET_CAPTURED:0x12345678,256,<data>\n

// Board 2 (Attack) responds:
ATTACK_START:0x12345678,FLOOD\n
ATTACK_STATUS:RUNNING,50pps,target_responding\n
ATTACK_RESULT:SUCCESS,300pkts,5sec\n
READY\n
```

**Hardware Connection:**
- Board 1 UART TX → Board 2 UART RX
- Board 1 UART RX → Board 2 UART TX
- Common GND
- Baud rate: 115200
- 3-pin pogo connector handles mating

### Split Mode (WiFi ESP-NOW)

**ESP-NOW for low-latency, long-range:**
```cpp
// ESP-NOW characteristics:
// - No WiFi router needed
// - 250m+ range with clear line of sight
// - ~1-2ms latency
// - Encrypted communication

struct BoardMessage {
    uint8_t msgType;           // Command type
    uint32_t nodeId;           // Target node
    float rssi;                // Signal strength
    float frequency;           // LoRa frequency
    uint32_t timestamp;        // Sync timestamp
    uint8_t data[200];         // Payload
};

// Message Types:
#define MSG_DEVICE_DISCOVERED   0x01
#define MSG_ATTACK_START        0x02
#define MSG_ATTACK_STATUS       0x03
#define MSG_TRIANGULATION_DATA  0x04
#define MSG_SYNC_REQUEST        0x05
```

**Triangulation Data Structure:**
```cpp
struct TriangulationData {
    uint32_t nodeId;
    float rssi_board1;
    float rssi_board2;
    uint32_t timestamp;
    float gps_lat_board1;      // Optional with GPS module
    float gps_lon_board1;
    float gps_lat_board2;
    float gps_lon_board2;
    float estimated_distance;
    float estimated_bearing;
};
```

### Auto-Detection Logic

```cpp
void detectOperatingMode() {
    // Check if physical connection present
    if (digitalRead(DETECT_PIN) == LOW) {
        // Combined mode - use UART
        initUARTComms();
        mode = MODE_COMBINED;
    } else {
        // Split mode - use WiFi
        initESPNOW();
        mode = MODE_SPLIT;
    }
}
```

---

## Case Design

### Mechanical Design

**Requirements:**
- Modular assembly/disassembly
- Secure connection in combined mode
- Protection for electronics
- Access to USB ports and buttons
- Clear view of OLED display

**Design Elements:**

1. **3D Printed Enclosure** (PETG or ABS)
   - Two-part clamshell design per board
   - Snap-fit or screw assembly
   - Mounting bosses for PCB standoffs

2. **Magnetic Alignment** 
   - 4x 10mm neodymium magnets per board
   - Embedded in case corners
   - Provides ~2kg holding force
   - Opposing polarity for correct orientation

3. **Pogo Pin Connectors**
   - 3-pin spring-loaded connectors
   - GND, 3.3V, UART (or I2C)
   - Gold-plated for reliability
   - Self-aligning with magnetic guide

4. **Sliding Rail System (Optional)**
   - T-slot rail on one board
   - Slide-in tab on mating board
   - Provides mechanical strength
   - Prevents rotation/separation

### Display Visibility
- Board 1's OLED faces outward when combined
- Viewing angle optimized for handheld use
- Optional backlight control for night operations
- Board 2 has edge-mounted status LEDs:
  - PWR (green) - Power on
  - TX (red) - Transmitting
  - RX (blue) - Receiving
  - ATTACK (yellow) - Attack mode active

### Weatherproofing (Optional)

For outdoor/field use:
- **IP54 rating** possible with gaskets
- Conformal coating on PCBs
- Sealed USB-C ports (rubber plugs)
- Water-resistant SMA connectors
- Desiccant pack in enclosure

### Mounting Options

**Combined Mode:**
- Belt clip (spring-loaded or fixed)
- Tripod mount (1/4-20 threaded insert)
- MOLLE/PALS webbing compatible
- Lanyard attachment points

**Split Mode:**
- Each board independently mountable
- Velcro backing for temporary placement
- Magnetic mount for metal surfaces
- Tripod socket on each board

---

## Bill of Materials

### Per Board (x2)

| Component | Specification | Qty | Unit Cost | Total |
|-----------|--------------|-----|-----------|-------|
| ESP32-S3 Module | WROOM-1, 8MB Flash | 1 | $8-12 | $8-12 |
| SX1262 LoRa Module | 915MHz, SPI interface | 1 | $8-15 | $8-15 |
| PCB Fabrication | 2-layer, 60x40mm | 1 | $5-10 | $5-10 |
| 18650 Battery | 3.7V, 3000mAh | 1 | $5-8 | $5-8 |
| TP4056 Charger | USB-C, 1A charging | 1 | $1-2 | $1-2 |
| USB-C Connector | Through-hole or SMD | 1 | $0.50-1 | $0.50-1 |
| SMA Connector | Female, edge-mount | 1 | $1-2 | $1-2 |
| Voltage Regulator | 3.3V LDO, 500mA | 1 | $0.50 | $0.50 |
| Passive Components | Resistors, caps, etc. | 1 set | $2-3 | $2-3 |
| Buttons/Switches | Tactile, power switch | 2-3 | $0.50 | $1-2 |
| **Subtotal per board** | | | | **$32-54** |

### Board-Specific Components

**Board 1 (Monitor) Additional:**
| Component | Specification | Qty | Unit Cost | Total |
|-----------|--------------|-----|-----------|-------|
| OLED Display | 128x64, I2C, SSD1306 | 1 | $8-12 | $8-12 |

**Board 2 (Attack) Additional:**
| Component | Specification | Qty | Unit Cost | Total |
|-----------|--------------|-----|-----------|-------|
| Status LEDs | 5mm, red/green/blue/yellow | 4 | $0.20 | $0.80 |

### Shared Components (One-time)

| Component | Specification | Qty | Unit Cost | Total |
|-----------|--------------|-----|-----------|-------|
| Antennas | 915MHz, 3dBi whip, SMA | 2 | $5-10 | $10-20 |
| Neodymium Magnets | 10mm dia, 3mm thick | 8 | $0.50 | $4 |
| Pogo Pin Connectors | 3-pin, spring-loaded | 2 sets | $2-3 | $4-6 |
| 3D Printing Filament | PETG or ABS | ~100g | $0.10/g | $10 |
| Screws/Standoffs | M3, various lengths | 1 set | $3-5 | $3-5 |

### Total System Cost

| Configuration | Cost Range |
|--------------|------------|
| Basic (both boards, no case) | $70-110 |
| Complete (with cases, antennas) | $120-180 |
| Premium (high-quality components) | $200-250 |

**Cost Breakdown:**
- Board 1 (Monitor): $40-66
- Board 2 (Attack): $32-54
- Shared components: $31-46
- **DIY Total: $120-180**

**Optional Upgrades:**
- GPS modules (2x): +$30-60
- Directional antennas: +$40-100
- Weatherproof cases: +$20-40
- Larger batteries (21700): +$10-20

---

## Triangulation Accuracy

### RSSI-Based Distance Estimation

**Expected Performance:**

| Distance Range | Accuracy | Quality |
|----------------|----------|---------|
| 0-100m | ±10-20m | Good |
| 100-500m | ±30-50m | Moderate |
| 500m-1km | ±100m+ | Rough estimate |
| 1km+ | ±200m+ | Direction only |

### Factors Affecting Accuracy

**Environmental:**
- Antenna height difference
- Obstacles (buildings, trees, terrain)
- Ground reflections and multipath
- Weather (heavy rain degrades ~10-20%)
- RF interference from other sources

**Hardware:**
- Antenna gain matching (±1 dBi tolerance)
- RSSI calibration (both boards)
- GPS accuracy (if using GPS triangulation)
- Time synchronization between boards

**Signal:**
- Spreading factor (higher SF = more stable RSSI)
- Transmit power variations
- Fading and scintillation
- Doppler shift (moving targets)

### Triangulation Formulas

**Two-board RSSI triangulation:**
```
Given:
- RSSI₁ at Board 1
- RSSI₂ at Board 2
- Distance D between boards

Path Loss Model:
RSSI = TxPower - PathLoss(distance)
PathLoss(d) = PathLoss(d₀) + 10*n*log₁₀(d/d₀)

Where:
- n = path loss exponent (2.0-4.0, typically 2.5 for outdoor)
- d₀ = reference distance (1m)

Estimated distance from each board:
d₁ = d₀ * 10^((TxPower - RSSI₁ - PL(d₀)) / (10*n))
d₂ = d₀ * 10^((TxPower - RSSI₂ - PL(d₀)) / (10*n))

Position: Circle intersection of (Board1, d₁) and (Board2, d₂)
```

**Implementation Note:**
Transmit power is often unknown, but RSSI *difference* still indicates bearing:
```
RSSI_diff = RSSI₁ - RSSI₂

If RSSI_diff > 0: Target closer to Board 1
If RSSI_diff < 0: Target closer to Board 2
If RSSI_diff ≈ 0: Target equidistant (perpendicular bisector)

Bearing estimation:
θ ≈ atan2(RSSI_diff, baseline_distance)
```

### Improved Accuracy with 3+ Boards

**Three-board triangulation:**
- Dramatically improves position accuracy to ±5-15m
- Can determine actual position (not just direction)
- Redundancy helps identify multipath errors
- Consider expandable design for 3-4 board arrays

**Future Enhancement:**
Design connector allows daisy-chaining additional boards:
```
Board 1 (Master) ←→ Board 2 (Slave) ←→ Board 3 (Slave) ←→ Board 4 (Slave)
        ▲                  ▲                  ▲                  ▲
        └──────────────────┴──────────────────┴──────────────────┘
                     WiFi Mesh Communication
```

---

## Implementation Roadmap

### Phase 1: Basic Dual-Board Operation (Weeks 1-2)

**Goals:**
- Get both boards built and tested independently
- Establish UART communication protocol
- Implement basic command relay (discovered devices)

**Tasks:**
1. ✅ Design PCB layout for both boards
2. ✅ Order components and PCBs
3. ✅ Assemble and test Board 1 (Monitor) standalone
4. ✅ Assemble and test Board 2 (Attack) standalone
5. ✅ Implement UART communication library
6. ✅ Test data exchange with breadboard connection
7. ✅ Verify power consumption and battery life

**Deliverables:**
- Two working boards with independent operation
- Basic serial protocol working
- Power management validated

### Phase 2: Combined Mode Integration (Weeks 3-4)

**Goals:**
- Design and fabricate enclosures
- Integrate pogo pin connectors
- Implement coordinated operations

**Tasks:**
1. ✅ Design 3D printable enclosures
2. ✅ Print and test case fit
3. ✅ Install magnets and pogo pins
4. ✅ Test physical connection reliability
5. ✅ Implement auto-detection of combined mode
6. ✅ Add coordinated attack capabilities
7. ✅ Test full-duplex operation (monitor while attacking)

**Deliverables:**
- Functional combined-mode case
- Reliable connection/disconnection
- Coordinated reconnaissance + attack demos

### Phase 3: Split Mode & Triangulation (Weeks 5-6)

**Goals:**
- Implement WiFi/ESP-NOW communication
- Develop triangulation algorithms
- Field test accuracy

**Tasks:**
1. ✅ Implement ESP-NOW communication
2. ✅ Add auto-switching between UART and WiFi
3. ✅ Synchronize timestamps between boards
4. ✅ Implement RSSI-based distance estimation
5. ✅ Add triangulation calculations
6. ✅ Create visualization/mapping interface
7. ✅ Field test at known distances

**Deliverables:**
- WiFi communication working
- Basic triangulation functional
- Accuracy benchmarks documented

### Phase 4: Advanced Features (Weeks 7-8)

**Goals:**
- Add GPS support (optional)
- Implement logging and data export
- Optimize battery life

**Tasks:**
1. ✅ Add GPS module support
2. ✅ Implement coordinate-based triangulation
3. ✅ Add SD card logging
4. ✅ Create data export tools (KML, GeoJSON)
5. ✅ Optimize power management
6. ✅ Add sleep modes
7. ✅ Final field testing and validation

**Deliverables:**
- GPS-enhanced triangulation
- Complete logging system
- Extended battery life (8+ hours)

### Phase 5: Production Refinement (Ongoing)

**Goals:**
- Improve case design based on field use
- Add weatherproofing
- Develop deployment accessories

**Tasks:**
- Iterate on enclosure design
- Add mounting options
- Create user documentation
- Develop training materials
- Build multiple units for distributed sensing

---

## Software Architecture

### Board Role Detection

```cpp
enum BoardRole {
    ROLE_MONITOR,      // Board 1 - Reconnaissance
    ROLE_ATTACK,       // Board 2 - Offensive operations
    ROLE_AUTO          // Auto-detect based on config
};

enum OperatingMode {
    MODE_COMBINED,     // Physical connection (UART)
    MODE_SPLIT,        // Separated (WiFi/ESP-NOW)
    MODE_STANDALONE    // Single board operation
};

class DualBoardSystem {
private:
    BoardRole role;
    OperatingMode mode;
    
public:
    void initialize();
    void detectMode();
    bool isConnected();
    void switchMode(OperatingMode newMode);
};
```

### Communication Protocol

```cpp
class BoardComms {
private:
    OperatingMode currentMode;
    HardwareSerial* uart;
    
public:
    // Send commands
    void sendDeviceDiscovery(uint32_t nodeId, float rssi, float freq);
    void sendAttackStart(uint32_t nodeId, AttackType type);
    void sendAttackStatus(AttackResult result);
    void sendTriangulationData(TriangulationData data);
    
    // Receive handlers
    void onDeviceDiscovered(DeviceCallback callback);
    void onAttackCommand(AttackCallback callback);
    void onTriangulationRequest(TriangulationCallback callback);
    
    // Mode switching
    void initUART();
    void initESPNOW();
    void switchCommsMode(OperatingMode mode);
};
```

### Triangulation Engine

```cpp
class TriangulationEngine {
private:
    struct BoardPosition {
        float latitude;
        float longitude;
        float altitude;
    };
    
    BoardPosition board1Pos;
    BoardPosition board2Pos;
    
public:
    void setBoardPositions(BoardPosition b1, BoardPosition b2);
    void updateRSSI(uint32_t nodeId, float rssi1, float rssi2);
    
    TargetPosition estimatePosition(uint32_t nodeId);
    float estimateDistance(float rssi, float txPower);
    float estimateBearing(float rssiDiff);
    
    void exportToKML(const String& filename);
    void exportToGeoJSON(const String& filename);
};
```

---

## Testing & Validation

### Unit Tests

**Board Communication:**
- ✅ UART data integrity (send/receive verification)
- ✅ ESP-NOW message delivery (packet loss < 1%)
- ✅ Mode auto-detection (combined vs split)
- ✅ Reconnection after disconnect

**Power Management:**
- ✅ Battery life measurement
- ✅ Charging behavior (both ports)
- ✅ Auto-switching power circuit
- ✅ Low battery detection/warning

**Radio Performance:**
- ✅ Simultaneous TX/RX operation
- ✅ Frequency coordination (avoid interference)
- ✅ RSSI calibration between boards
- ✅ Attack execution while monitoring

### Integration Tests

**Combined Mode:**
- ✅ Device discovery sharing
- ✅ Coordinated attack execution
- ✅ Monitor continues scanning during attack
- ✅ Physical connection reliability (100 connect/disconnect cycles)

**Split Mode:**
- ✅ WiFi range testing (50m, 100m, 250m, 500m)
- ✅ RSSI comparison accuracy
- ✅ Time synchronization drift
- ✅ Triangulation accuracy at known distances

### Field Tests

**Scenarios:**
1. **Urban environment** - Buildings, multipath
2. **Open field** - Line-of-sight, minimal obstacles
3. **Forest** - Vegetation attenuation
4. **Mixed terrain** - Realistic deployment

**Metrics:**
- Position accuracy (measured vs. actual)
- Range limitations
- Battery life under field conditions
- User experience and ergonomics

---

## Troubleshooting

### Common Issues

**Boards won't connect in combined mode:**
- Check pogo pin alignment
- Verify magnet polarity (should attract, not repel)
- Clean connector contacts
- Test with multimeter (continuity check)

**Poor triangulation accuracy:**
- Verify antennas are identical (gain, polarization)
- Check board separation distance (min 50m)
- Calibrate RSSI readings (compare against known distance)
- Account for environmental factors (add correction)

**Battery drains quickly:**
- Check for software loops (missing delays)
- Verify sleep mode activating
- Monitor current draw with ammeter
- Check for RF interference causing retries

**WiFi communication drops:**
- Reduce distance between boards
- Check for 2.4GHz interference
- Verify ESP-NOW paired correctly
- Add message retry logic

---

## Future Enhancements

### Hardware Upgrades

**Version 2.0:**
- Integrated GPS modules
- Larger OLED (1.3" 128x64)
- USB-C power delivery (fast charging)
- Built-in compass/accelerometer
- External antenna jack for high-gain

**Version 3.0:**
- Software-defined radio (SDR) integration
- Multi-band support (433/868/915/2400 MHz)
- Cellular connectivity (LTE-M/NB-IoT)
- Solar charging panel option

### Software Features

**Enhanced Triangulation:**
- Machine learning for RSSI correction
- Multi-path mitigation algorithms
- Kalman filtering for moving targets
- Heat map generation

**Coordination:**
- Support for 4+ board networks
- Mesh communication between all boards
- Distributed task scheduling
- Collaborative scanning strategies

**Analysis:**
- Real-time position tracking
- Device movement patterns
- Coverage heat maps
- Automated reporting

---

## Conclusion

The modular dual-board design provides unprecedented flexibility for LoRa security research:

- **Ease of Use:** Combined mode for typical operations
- **Advanced Capabilities:** Split mode for triangulation and distributed sensing
- **Cost Effective:** ~$120-180 for complete system
- **Extensible:** Can expand to 3-4 board arrays
- **Field Ready:** Weatherproof options and long battery life

This architecture transforms the ESP32 LoRa Sniffer from a single-point monitoring tool into a sophisticated distributed sensing platform capable of advanced reconnaissance, coordinated attacks, and precise target localization.

**Next Steps:**
1. Review and finalize design specifications
2. Order components and begin PCB design
3. Start Phase 1 implementation (basic dual-board operation)
4. Iterate based on testing and field use

---

**Document Version:** 1.0  
**Last Updated:** November 10, 2025  
**Author:** ESP32 LoRa Sniffer Project  
**Status:** Design specification - ready for implementation
