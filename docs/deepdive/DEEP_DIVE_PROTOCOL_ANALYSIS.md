# Deep Dive: Protocol Analysis & Packet Dissection

## Executive Summary

This document explains how your ESP32 LoRa sniffer analyzes raw packet bytes to extract intelligence. You'll learn Protocol Buffers wire format, Meshtastic packet structure, device fingerprinting techniques, and GPS coordinate extraction - essentially becoming fluent in reading hex dumps.

---

## Table of Contents
1. [Protocol Buffers Fundamentals](#protobuf-fundamentals)
2. [Meshtastic Packet Anatomy](#meshtastic-anatomy)
3. [Protocol Identification](#protocol-identification)
4. [Device Fingerprinting](#device-fingerprinting)
5. [GPS Extraction](#gps-extraction)
6. [Real Packet Dissection](#real-packet-dissection)

---

## Protobuf Fundamentals

### What Is Protocol Buffers?

**Protocol Buffers (protobuf):**
- **Serialization format**: Binary encoding (like JSON, but compact)
- **Created by**: Google (2001, open-sourced 2008)
- **Used by**: Meshtastic, gRPC, Kubernetes, many Google services
- **Benefits**: Small size (50-80% smaller than JSON), fast parsing, schema validation

**Comparison:**

```
JSON (52 bytes):
{"portnum":1,"text":"Hello","timestamp":1697500000}

Protobuf (14 bytes):
08 01 12 05 48 65 6C 6C 6F 18 80 B4 E1 D2 04
```

**Why Meshtastic uses it:**
- **LoRa bandwidth is precious**: 200 bytes/packet limit
- **Battery life**: Less data = less transmission time = less power
- **Flexible**: Can add fields without breaking old firmware

### Wire Format Basics

**Every field is encoded as: TAG + DATA**

```
┌──────────────────────────────────────────────────┐
│               PROTOBUF FIELD                     │
├──────────────────────────────────────────────────┤
│                                                  │
│  TAG (1-5 bytes):                                │
│    ┌─────────────────┬──────────────────┐       │
│    │  Field Number   │   Wire Type      │       │
│    │  (bits 3-7)     │   (bits 0-2)     │       │
│    └─────────────────┴──────────────────┘       │
│                                                  │
│  DATA (variable):                                │
│    Content depends on wire type                  │
│                                                  │
└──────────────────────────────────────────────────┘
```

**Tag Byte Structure:**

```
Example: 0x08 (binary: 0000 1000)

┌───┬───┬───┬───┬───┬───┬───┬───┐
│ 0 │ 0 │ 0 │ 0 │ 1 │ 0 │ 0 │ 0 │
└───┴───┴───┴───┴───┴───┴───┴───┘
  │   │   │   │   │   └───┴───┘
  │   │   │   │   │       │
  │   │   │   │   │       └─ Wire Type = 0 (varint)
  │   │   │   │   │
  └───┴───┴───┴───┴─ Field Number = 1

Extraction:
  fieldNumber = 0x08 >> 3 = 0000 0001 = 1
  wireType = 0x08 & 0x07 = 0000 0000 = 0
```

### Wire Types

```
┌─────┬──────────────────┬─────────────────────────────────┐
│ ID  │ Type             │ Used For                        │
├─────┼──────────────────┼─────────────────────────────────┤
│ 0   │ Varint           │ int32, int64, uint32, bool,     │
│     │                  │ enum, sint32, sint64            │
├─────┼──────────────────┼─────────────────────────────────┤
│ 1   │ 64-bit           │ fixed64, sfixed64, double       │
├─────┼──────────────────┼─────────────────────────────────┤
│ 2   │ Length-delimited │ string, bytes, nested messages, │
│     │                  │ repeated fields                 │
├─────┼──────────────────┼─────────────────────────────────┤
│ 3   │ Start group      │ (deprecated, don't use)         │
├─────┼──────────────────┼─────────────────────────────────┤
│ 4   │ End group        │ (deprecated, don't use)         │
├─────┼──────────────────┼─────────────────────────────────┤
│ 5   │ 32-bit           │ fixed32, sfixed32, float        │
└─────┴──────────────────┴─────────────────────────────────┘
```

**Meshtastic uses primarily:**
- **Type 0 (Varint)**: Port numbers, node IDs, timestamps
- **Type 2 (Length-delimited)**: Text messages, nested packets, GPS coordinates

### Varint Encoding

**What is a varint?**
- **Variable-length integer**: Small numbers use fewer bytes
- **Encoding**: 7 bits per byte + continuation bit (MSB)
- **Efficiency**: 1 uses 1 byte, 128 uses 2 bytes, 16384 uses 3 bytes

**Encoding Algorithm:**

```
Number: 300 (0x012C)

Step 1: Binary representation
  300 = 0000 0001 0010 1100

Step 2: Split into 7-bit groups (right to left)
  0000010 0101100
  
Step 3: Reverse order (little-endian)
  0101100 0000010

Step 4: Add continuation bits (1 = more bytes, 0 = last byte)
  1010 1100  0000 0010
     ↑            ↑
  Has more     Last byte

Step 5: Result (2 bytes)
  0xAC 0x02
```

**Decoding Example:**

```cpp
// Decode varint from byte stream
int32_t result = 0;
int shift = 0;

for (size_t i = 0; i < maxBytes; i++) {
    uint8_t byte = data[i];
    
    // Extract 7 data bits
    result |= (int32_t)(byte & 0x7F) << shift;
    
    // Check continuation bit (bit 7)
    if (!(byte & 0x80)) {
        // Last byte, done!
        return result;
    }
    
    shift += 7;  // Next 7 bits
}
```

**Example: Decode 0xAC 0x02**

```
Byte 1: 0xAC = 1010 1100
  Continuation bit: 1 (more bytes)
  Data bits: 010 1100 = 0x2C (44)
  Result so far: 44

Byte 2: 0x02 = 0000 0010
  Continuation bit: 0 (last byte)
  Data bits: 000 0010 = 0x02 (2)
  Result: 44 | (2 << 7) = 44 | 256 = 300 ✓
```

**Why Varint Is Clever:**

```
Fixed 32-bit int: ALWAYS 4 bytes
  1 = 0x00 0x00 0x00 0x01 (4 bytes)
  
Varint: 1-5 bytes depending on value
  1 = 0x01 (1 byte)           ← 75% savings!
  127 = 0x7F (1 byte)         ← 75% savings!
  128 = 0x80 0x01 (2 bytes)   ← 50% savings!
  16383 = 0xFF 0x7F (2 bytes) ← 50% savings!
```

### Length-Delimited Fields

**Format: TAG + LENGTH + DATA**

```
Example: Text message "Hi"

Field 1: portnum = 1 (TEXT_MESSAGE_APP)
  Tag: 0x08 (field 1, wire type 0)
  Value: 0x01 (varint)
  Bytes: [0x08, 0x01]

Field 2: payload = "Hi"
  Tag: 0x12 (field 2, wire type 2 = length-delimited)
  Length: 0x02 (varint = 2 bytes)
  Value: [0x48, 0x69] (ASCII "Hi")
  Bytes: [0x12, 0x02, 0x48, 0x69]

Complete message: [0x08, 0x01, 0x12, 0x02, 0x48, 0x69]
                   └──┬──┘  └──────┬──────┘
                   portnum     payload
```

**Parsing Length-Delimited:**

```cpp
if (wireType == 2) {  // Length-delimited
    // Read length (varint)
    size_t lengthBytes = 0;
    uint32_t length = decodeVarint(data + offset, remaining, lengthBytes);
    offset += lengthBytes;
    
    // Read data
    const uint8_t* fieldData = data + offset;
    offset += length;
    
    // fieldData now points to 'length' bytes of content
}
```

---

## Meshtastic Anatomy

### Complete Packet Structure

```
┌────────────────────────────────────────────────────────────┐
│                 MESHTASTIC PACKET (ENCRYPTED)              │
├────────────────────────────────────────────────────────────┤
│                                                            │
│  UNENCRYPTED HEADER (14 bytes):                            │
│  ┌────────────────────────────────────────────────────┐   │
│  │ [0-3]   Magic:      0xFF 0xFF 0xFF 0xFF            │   │
│  │ [4-7]   From Node:  0x12 0x34 0x56 0x78 (BE)      │   │
│  │ [8-9]   Flags:      0x01 0x00                      │   │
│  │ [10-13] Packet ID:  0xAB 0xCD 0xEF 0x01 (LE)      │   │
│  └────────────────────────────────────────────────────┘   │
│                                                            │
│  ENCRYPTED PAYLOAD (variable length):                      │
│  ┌────────────────────────────────────────────────────┐   │
│  │ After decryption:                                  │   │
│  │                                                    │   │
│  │ Field 1 (portnum):                                 │   │
│  │   Tag: 0x08 (field 1, type varint)                │   │
│  │   Value: 0x01 (TEXT_MESSAGE_APP)                  │   │
│  │                                                    │   │
│  │ Field 2 (payload):                                 │   │
│  │   Tag: 0x12 (field 2, type length-delimited)      │   │
│  │   Length: <varint>                                 │   │
│  │   Nested Data message:                             │   │
│  │     Field 1 (text):                                │   │
│  │       Tag: 0x0A (field 1, length-delimited)       │   │
│  │       Length: <varint>                             │   │
│  │       Text: "Hello world"                          │   │
│  │                                                    │   │
│  │ Field 3 (destination):                             │   │
│  │   Tag: 0x18 (field 3, type varint)                │   │
│  │   Value: 0xFFFFFFFF (broadcast)                   │   │
│  │                                                    │   │
│  └────────────────────────────────────────────────────┘   │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Port Numbers (Application Types)

```cpp
// From Meshtastic protocol documentation
enum PortNum {
    UNKNOWN_APP = 0,
    TEXT_MESSAGE_APP = 1,       // 0x01 ← Most common!
    REMOTE_HARDWARE_APP = 2,    // GPIO control
    POSITION_APP = 3,           // 0x03 ← GPS coordinates
    NODEINFO_APP = 4,           // Device info
    ROUTING_APP = 5,            // Mesh routing
    ADMIN_APP = 7,              // 0x07 ← Session keys!
    TEXT_MESSAGE_COMPRESSED_APP = 9,
    // ... more app types
};
```

**Why Port Numbers Matter:**

```
Same packet header, different port = different meaning:

Port 0x01 (TEXT_MESSAGE_APP):
  Payload contains: Text string
  
Port 0x03 (POSITION_APP):
  Payload contains: Latitude, Longitude, Altitude
  
Port 0x07 (ADMIN_APP):
  Payload contains: Session keys, channel config
```

### Nested Message Structure

**Meshtastic uses 3 layers of nesting:**

```
Layer 1: MeshPacket (the radio packet)
  ├─ from: Node ID
  ├─ to: Destination node ID
  ├─ id: Packet ID (for nonce)
  └─ encrypted: Data message (encrypted)

Layer 2: Data (decrypted payload)
  ├─ portnum: Application type (1=text, 3=position, etc.)
  ├─ payload: Application-specific data (nested!)
  └─ want_response: bool

Layer 3: Application Message (e.g., User text message)
  ├─ text: "Hello world"
  ├─ longname: "Alice's Device"
  └─ macaddr: Device MAC address
```

**Example Hex Dump:**

```
Complete packet (encrypted):
FF FF FF FF 12 34 56 78 01 00 AB CD EF 01 08 01 12 0D 0A 0B 48 65 6C 6C 6F 20 77 6F 72 6C 64

After decryption:
08 01 12 0D 0A 0B 48 65 6C 6C 6F 20 77 6F 72 6C 64

Breakdown:
  08 01        ← Field 1 (portnum) = 1 (TEXT_MESSAGE_APP)
  12 0D        ← Field 2 (payload), length = 13 bytes
    0A 0B      ← Nested: Field 1 (text), length = 11 bytes
    48 65 6C 6C 6F 20 77 6F 72 6C 64  ← "Hello world"
```

---

## Protocol Identification

### Why 0xFFFFFFFF Works

**The magic header:**

```cpp
if (data[0] == 0xFF && data[1] == 0xFF && 
    data[2] == 0xFF && data[3] == 0xFF) {
    return "Meshtastic";
}
```

**Statistical uniqueness:**

```
Random 4-byte sequence probability:
  P(0xFFFFFFFF) = (1/256)^4 = 1 / 4,294,967,296

In a typical capture session (1000 packets):
  Expected false positives: 1000 / 4,294,967,296 = 0.0000002%

Conclusion: Effectively zero false positives!
```

**Why Meshtastic chose this:**
- **Easy to spot**: All-ones is distinctive in hex dumps
- **Not used elsewhere**: Most protocols avoid 0xFF headers
- **Sync word**: Also serves as frame synchronization

**Alternative approaches:**

```
LoRaWAN: Uses structured MHDR byte (not magic number)
Helium: Uses protocol version + frame type
Custom: Could use any unique sequence

Meshtastic: 0xFFFFFFFF (simple, effective)
```

### LoRaWAN Detection

**MHDR (MAC Header) byte structure:**

```
Byte 0 (MHDR):
┌───┬───┬───┬───┬───┬───┬───┬───┐
│   MType   │  RFU  │ Major │ │
│ (5-7)     │ (2-4) │ (0-1) │ │
└───┴───┴───┴───┴───┴───┴───┴───┘

MType (bits 5-7):
  000 = Join Request
  001 = Join Accept
  010 = Unconfirmed Data Up
  011 = Unconfirmed Data Down
  100 = Confirmed Data Up
  101 = Confirmed Data Down
  110 = RejoinRequest
  111 = Proprietary

Extraction:
  uint8_t mtype = (data[0] >> 5) & 0x07;
```

**Detection code:**

```cpp
if (length >= 12 && length <= 51) {  // LoRaWAN packet size range
    uint8_t mtype = (data[0] >> 5) & 0x07;
    if (mtype <= 0x07) {  // Valid MType range
        return "LoRaWAN";
    }
}
```

**Why this works:**

1. **Size constraint**: LoRaWAN packets are 12-51 bytes (spec defined)
2. **MType validation**: Only 8 valid message types (0-7)
3. **Low false positives**: Random data unlikely to match both constraints

**LoRaWAN packet structure:**

```
Unconfirmed Data Up (MType = 010):
┌────────────────────────────────────────────┐
│ [0]     MHDR:     0x40 (010 00 00)        │
│ [1-4]   DevAddr:  0x01 0x23 0x45 0x67     │
│ [5]     FCtrl:    0x80 (ADR=1, ACK=0)     │
│ [6-7]   FCnt:     0x00 0x01 (counter)     │
│ [8+]    Payload:  (encrypted)             │
│ [-4]    MIC:      4-byte integrity check  │
└────────────────────────────────────────────┘
```

### Beacon Detection

**Characteristics:**

```cpp
if (length <= 8) return "Beacon";
```

**Why short packets = beacons:**
- **Minimal data**: Just "I'm alive"
- **Battery efficient**: Less transmission time
- **Frequent**: Sent every few seconds
- **No payload**: No user data

**Example beacon:**

```
Bytes: [0x01, 0x23, 0x45, 0x67, 0x89]
       └──────┬──────┘  └───┬───┘
        Device ID      Beacon type
```

---

## Device Fingerprinting

### Node ID Extraction

**Meshtastic (big-endian):**

```cpp
uint32_t nodeId = (uint32_t(data[4]) << 24) | 
                  (uint32_t(data[5]) << 16) | 
                  (uint32_t(data[6]) << 8) | 
                  uint32_t(data[7]);

Example bytes: [0xAB, 0xCD, 0xEF, 0x01]
Result: 0xABCDEF01
```

**LoRaWAN DevAddr (little-endian):**

```cpp
uint32_t devAddr = uint32_t(data[1]) | 
                   (uint32_t(data[2]) << 8) | 
                   (uint32_t(data[3]) << 16) | 
                   (uint32_t(data[4]) << 24);

Example bytes: [0x01, 0xEF, 0xCD, 0xAB]
Result: 0xABCDEF01
```

**Why different endianness?**
- **Meshtastic**: Network byte order (big-endian) for readability
- **LoRaWAN**: Native CPU order (little-endian) for efficiency

### Power Class Estimation

**RSSI-based classification:**

```cpp
uint8_t estimatePowerClass(float rssi) {
    if (rssi > -70) return 2;  // High power (>100mW)
    if (rssi > -90) return 1;  // Medium power (10-100mW)
    return 0;                  // Low power (<10mW)
}
```

**Signal strength ranges:**

```
RSSI (dBm) | Power Class | Typical Device
-----------|-------------|------------------
> -50      | High (2)    | Base station, solar repeater
-50 to -70 | High (2)    | Mobile vehicle, high-power handheld
-70 to -90 | Medium (1)  | Standard handheld
-90 to -110| Low (0)     | Low-power sensor node
< -110     | Low (0)     | Distant or weak transmitter
```

**Why RSSI correlates with power:**

```
Friis Transmission Equation:
  RSSI = TxPower + TxGain + RxGain - PathLoss

For same distance:
  Higher TxPower → Higher RSSI
  
Example:
  100mW (20dBm) transmitter at 1km: -60 dBm
  10mW (10dBm) transmitter at 1km:  -70 dBm
  1mW (0dBm) transmitter at 1km:    -80 dBm
```

### Router Detection

**Hop count analysis:**

```cpp
bool isRoutingDevice(const uint8_t* data, size_t length, const char* protocol) {
    if (strcmp(protocol, "Meshtastic") == 0 && length >= 12) {
        uint8_t hopCount = data[8] & 0x07;  // Bits 0-2
        uint8_t routingFlags = data[9];
        return (hopCount > 0 || (routingFlags & 0x01));
    }
    return false;
}
```

**Flags byte structure (byte 8):**

```
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ E │ R │ R │ R │ R │   HopCnt  │
└───┴───┴───┴───┴───┴───┴───┴───┘
  7   6   5   4   3   2   1   0

E = Encryption enabled (bit 7)
R = Reserved (bits 3-6)
HopCnt = Hop count (bits 0-2, range 0-7)

Example: 0x82 = 1000 0010
  Encryption: 1 (enabled)
  Hop count: 2 (forwarded twice)
```

**Router identification logic:**

```
Hop count = 0: Original sender (not routing)
Hop count = 1: Forwarded once (sender is router)
Hop count = 2: Forwarded twice (multiple routers)
Hop count > 3: Long mesh path

Routing flag (byte 9, bit 0):
  0 = End device (doesn't route)
  1 = Router (forwards packets)
```

### Firmware Version Estimation

**Heuristic analysis:**

```cpp
const char* estimateFirmwareVersion(const uint8_t* data, size_t length, 
                                    const char* protocol) {
    // Check encryption flag (v2.2+)
    if (length >= 9 && (data[8] & 0x80)) {
        return "v2.2+ (encryption enabled)";
    }
    
    // Extended headers (v2.1+)
    if (length > 50) {
        return "v2.1+ (extended headers)";
    }
    
    // Classic routing patterns (v2.0)
    if (length >= 9) {
        uint8_t hopCount = data[8] & 0x07;
        uint8_t flags = data[9];
        if (hopCount <= 3 && (flags & 0xF0) == 0) {
            return "v2.0.x (classic routing)";
        }
    }
    
    // Very short = old firmware or beacons
    if (length <= 16) {
        return "v1.x or beacon";
    }
    
    return "v2.0-2.2 (uncertain)";
}
```

**Why this works:**

1. **Feature detection**: New versions add fields → longer packets
2. **Flag patterns**: Encryption flag added in v2.2
3. **Structure changes**: Extended headers in v2.1
4. **Conservative**: Returns "uncertain" if ambiguous

**Limitations:**
- **Not 100% accurate**: Heuristics can be fooled
- **Custom firmware**: Won't detect modified versions
- **Useful for**: Network profiling, not security decisions

---

## GPS Extraction

### Meshtastic Position Encoding

**Why not standard floats?**

```
Standard GPS (WGS84 floats):
  Latitude:  47.606209° N (4 bytes float)
  Longitude: 122.332071° W (4 bytes float)
  Total: 8 bytes

Meshtastic (fixed-point integers):
  Latitude:  476062090 (4 bytes int32)
  Longitude: -1223320710 (4 bytes int32)
  Total: 8 bytes
  
Same size, but integers are:
  - Easier to validate (range check)
  - No floating-point rounding errors
  - Faster on embedded CPUs
```

**Encoding formula:**

```
degrees = integer_value * 1e-7

Example:
  Raw value: 476062090
  Degrees: 476062090 * 0.0000001 = 47.606209°
```

**Precision:**

```
1e-7 degrees resolution:
  At equator: ~11mm precision
  At 45° latitude: ~8mm precision
  
More than enough for:
  - Hiking/camping (meter-level accuracy)
  - Emergency location (street-level)
  - Not enough for surveying (need cm precision)
```

### Position Packet Structure

**Protobuf definition (simplified):**

```protobuf
message Position {
  int32 latitude_i = 1;   // Degrees * 1e7
  int32 longitude_i = 2;  // Degrees * 1e7
  int32 altitude = 3;     // Meters above MSL
  int32 precision_bits = 4; // GPS precision indicator
  int32 timestamp = 5;    // Unix timestamp
}
```

**Wire format example:**

```
Position for 47.606209° N, 122.332071° W, 100m altitude:

08 EA F4 BC E7 01  ← Field 1: latitude_i = 476062090
   └────┬────┘
   varint (5 bytes)

10 F6 A9 8F C5 7F  ← Field 2: longitude_i = -1223320710
   └────┬────┘
   varint (5 bytes, zigzag encoding for negative)

18 64              ← Field 3: altitude = 100
   └┘
   varint (1 byte)

20 20              ← Field 4: precision_bits = 32
   └┘
   varint (1 byte)
```

### Zigzag Encoding (Signed Integers)

**Problem with signed varints:**

```
-1 as two's complement: 0xFFFFFFFF (4 bytes)
As varint: 0xFF 0xFF 0xFF 0xFF 0x0F (5 bytes!)
  Worse than fixed encoding!
```

**Solution: Zigzag encoding**

```
Map signed integers to unsigned:
  0  →  0
 -1  →  1
  1  →  2
 -2  →  3
  2  →  4
  ...

Formula:
  zigzag(n) = (n << 1) ^ (n >> 31)
  
Reverse:
  n = (zigzag >> 1) ^ -(zigzag & 1)
```

**Example: -1223320710**

```
Binary: 1011 0111 0001 0001 0100 1110 1111 1010

Step 1: Left shift by 1
  0110 1110 0010 0010 1001 1101 1111 0100

Step 2: Arithmetic right shift by 31 (sign extend)
  1111 1111 1111 1111 1111 1111 1111 1111

Step 3: XOR
  1001 0001 1101 1101 0110 0010 0000 1011

Step 4: Encode as varint (now small positive number)
  Result: Much shorter!
```

### Coordinate Extraction Code

**Complete extraction function:**

```cpp
bool GeoIntelligence::parseProtobufPosition(const uint8_t* payload, 
                                            size_t length, 
                                            GeoPoint& point) {
    size_t offset = 0;
    int32_t latitudeRaw = 0, longitudeRaw = 0;
    bool hasLat = false, hasLon = false;
    
    while (offset < length - 1) {
        // Read tag byte
        uint8_t tag = payload[offset++];
        uint8_t fieldNumber = tag >> 3;
        uint8_t wireType = tag & 0x07;
        
        if (wireType == 0) {  // Varint
            size_t bytesRead = 0;
            int32_t value = decodeVarint(payload + offset, 
                                        length - offset, 
                                        bytesRead);
            offset += bytesRead;
            
            switch (fieldNumber) {
                case 1:  // latitude_i
                    latitudeRaw = value;
                    hasLat = true;
                    break;
                case 2:  // longitude_i
                    longitudeRaw = value;
                    hasLon = true;
                    break;
                case 3:  // altitude
                    point.altitude = (float)value;
                    break;
                case 4:  // precision_bits
                    point.precision = (int8_t)value;
                    break;
            }
        } else if (wireType == 2) {  // Skip length-delimited
            size_t bytesRead = 0;
            int32_t fieldLen = decodeVarint(payload + offset, 
                                           length - offset, 
                                           bytesRead);
            offset += bytesRead + fieldLen;
        } else {
            break;  // Unknown wire type, stop parsing
        }
    }
    
    if (hasLat && hasLon) {
        point.latitude = convertCoordinate(latitudeRaw);
        point.longitude = convertCoordinate(longitudeRaw);
        point.valid = true;
        return true;
    }
    
    return false;
}

float GeoIntelligence::convertCoordinate(int32_t raw) const {
    return (float)raw * 1e-7f;
}
```

**Step-by-step for sample packet:**

```
Input bytes: 08 EA F4 BC E7 01 10 F6 A9 8F C5 7F 18 64

Offset 0: Tag = 0x08
  Field: 1, Wire: 0 (varint)
  
Offset 1-5: Varint = 0xEA 0xF4 0xBC 0xE7 0x01
  Decode: 476062090
  Store: latitudeRaw = 476062090
  
Offset 6: Tag = 0x10
  Field: 2, Wire: 0 (varint)
  
Offset 7-11: Varint = 0xF6 0xA9 0x8F 0xC5 0x7F
  Decode (zigzag): -1223320710
  Store: longitudeRaw = -1223320710
  
Offset 12: Tag = 0x18
  Field: 3, Wire: 0 (varint)
  
Offset 13: Varint = 0x64
  Decode: 100
  Store: altitude = 100
  
Convert:
  latitude = 476062090 * 1e-7 = 47.606209°
  longitude = -1223320710 * 1e-7 = -122.332071°
  
Result: 47.606209° N, 122.332071° W, 100m
```

---

## Real Packet Dissection

### Example 1: Text Message

**Hex dump (after decryption):**

```
08 01 12 0D 0A 0B 48 65 6C 6C 6F 20 77 6F 72 6C 64 18 FF FF FF FF 0F
```

**Byte-by-byte breakdown:**

```
Offset | Hex | Binary    | Meaning
-------|-----|-----------|------------------------------------
0      | 08  | 0000 1000 | Tag: Field 1, Wire 0 (varint)
1      | 01  | 0000 0001 | Value: 1 (TEXT_MESSAGE_APP)
-------|-----|-----------|------------------------------------
2      | 12  | 0001 0010 | Tag: Field 2, Wire 2 (length-delim)
3      | 0D  | 0000 1101 | Length: 13 bytes
-------|-----|-----------|------------------------------------
4      | 0A  | 0000 1010 | Nested tag: Field 1, Wire 2
5      | 0B  | 0000 1011 | Nested length: 11 bytes
6-16   | ... | ...       | Text: "Hello world" (ASCII)
-------|-----|-----------|------------------------------------
17     | 18  | 0001 1000 | Tag: Field 3, Wire 0 (varint)
18-22  | FF..| ...       | Value: 0xFFFFFFFF (broadcast dest)
```

**Reconstructed structure:**

```
MeshPacket {
  portnum: TEXT_MESSAGE_APP (1)
  payload: Data {
    text: "Hello world"
  }
  destination: 0xFFFFFFFF (broadcast)
}
```

### Example 2: GPS Position

**Hex dump:**

```
08 03 12 0E 08 EA F4 BC E7 01 10 F6 A9 8F C5 7F 18 64
```

**Breakdown:**

```
08 03              → Port 3 (POSITION_APP)
12 0E              → Payload length: 14 bytes
  08 EA F4 BC E7 01   → latitude_i = 476062090
  10 F6 A9 8F C5 7F   → longitude_i = -1223320710
  18 64               → altitude = 100m
```

**Decoded:**

```
Position {
  latitude: 47.606209° N
  longitude: 122.332071° W
  altitude: 100 meters
}
```

### Example 3: Admin Packet

**Hex dump (session key announcement):**

```
08 07 12 25 0A 23 0A 20 41 42 43 44 45 46 47 48 ...
```

**Breakdown:**

```
08 07              → Port 7 (ADMIN_APP)
12 25              → Payload length: 37 bytes
  0A 23            → Nested field 1, length 35
    0A 20          → Session key field, length 32
    41 42 ... FF   → 32-byte session key
```

**Structure:**

```
AdminMessage {
  session_key_response: {
    key_bytes: [0x41, 0x42, ..., 0xFF]  // 32 bytes
  }
}
```

---

## Summary

### Key Takeaways

**Protocol Buffers:**
- ✅ Binary format: 50-80% smaller than JSON
- ✅ Wire types: Varint (0), 64-bit (1), Length-delimited (2), 32-bit (5)
- ✅ Varint encoding: Small numbers = fewer bytes
- ✅ Zigzag encoding: Efficient negative numbers

**Meshtastic Structure:**
- ✅ Magic header: 0xFFFFFFFF (statistically unique)
- ✅ Node ID: Bytes 4-7 (big-endian)
- ✅ Packet ID: Bytes 10-13 (little-endian, for nonce)
- ✅ Port numbers: TEXT=1, POSITION=3, ADMIN=7

**Device Fingerprinting:**
- ✅ RSSI → Power class (>-70=high, >-90=medium, else=low)
- ✅ Hop count → Router detection (>0 = forwarded)
- ✅ Packet size → Firmware version hints
- ✅ Patterns → Device type classification

**GPS Extraction:**
- ✅ Fixed-point: Integer * 1e-7 = degrees
- ✅ Zigzag encoding: Efficient for negative coordinates
- ✅ Precision: ~11mm at equator
- ✅ Position packet: Port 3, fields 1-2 are lat/lon

### Tools You've Mastered

**Reading hex dumps:**
```
08 01 12 05 48 65 6C 6C 6F
│  │  │  │  └─────┬─────┘
│  │  │  │     "Hello"
│  │  │  └─ Length: 5
│  │  └─ Field 2, length-delimited
│  └─ Value: 1 (TEXT_MESSAGE_APP)
└─ Field 1, varint
```

**Validating protobuf:**
```cpp
uint8_t tag = data[0];
if ((tag >> 3) >= 1 && (tag & 0x07) <= 5) {
    // Valid protobuf!
}
```

**Extracting coordinates:**
```cpp
int32_t raw = decodeVarint(data, length);
float degrees = (float)raw * 1e-7f;
```

---

## Code Locations Reference

**Primary files:**
- `protocol_analyzer.cpp` (lines 1-140): All protocol analysis functions
- `protocol_analyzer.h` (lines 1-30): PacketInfo structure
- `geo_intelligence.cpp` (lines 1-250): GPS extraction
- `geo_intelligence.h` (lines 1-40): GeoPoint structure

**Key functions:**
- `identifyProtocol()`: Magic header detection (line 28)
- `extractNodeId()`: Big/little-endian extraction (line 54)
- `parseProtobufPosition()`: GPS coordinate parsing (line 88)
- `decodeVarint()`: Varint/zigzag decoding (line 115)

---

## Next Steps

Now that you understand protocol analysis, we can explore:

1. **State Machine**: How reconnaissance modes transition
2. **Error Handling**: Recovery strategies and watchdog details
3. **Display System**: OLED rendering and UI optimization
4. **Replay Attacks**: Capturing and retransmitting packets
5. **Session Key Harvesting**: Active mesh participation

**Pick a topic for the next deep dive!**

---

*Generated for ESP32-Sniffer deep-dive session*  
*Last updated: 2025-10-16*
