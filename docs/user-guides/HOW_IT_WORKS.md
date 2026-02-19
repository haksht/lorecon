# How It Works — Device Analysis Reference

This document explains the logic behind the device list classifications and displayed values.
Source references point to the relevant implementation files.

---

## Security Score and Vulnerability Rating

**Source**: `firmware/src/utils/security_scorer.h`

Every device starts at **100 points** and loses points for observed risk factors:

| Finding | Points lost | Trigger condition |
|---|---|---|
| Physical proximity | −15 | Best RSSI > −50 dBm (device is within ~5 m) |
| Router device | −10 | Device has relayed ≥ 2 packets |
| Chatty (high traffic) | −15 | Packet count > 100 |
| Intermittent (low traffic) | −5 | Packet count < 5 |
| Outdated firmware | −20 | Firmware string contains `v1.x` or `v2.0` |

The score maps to three ratings:

| Score | Rating | Display |
|---|---|---|
| ≥ 80 | Secure | ✅ SECURE (green) |
| 60–79 | Moderate | ⚠️ MODERATE (yellow) |
| < 60 | Vulnerable | 🔴 VULNERABLE (red) |

**Worst possible score**: 35 (all five point deductions applied simultaneously).

**Note**: The `possibleUnencrypted` flag (Meshtastic device with > 10 packets and no confirmed PSK
decryption) appears in the findings list but does **not** deduct from the score. It is only
confirmed as a finding once PSK testing succeeds.

---

## "Intermittent" Transmission Flag

**Source**: `firmware/src/utils/security_scorer.h:96`, `firmware/src/repositories/device_repository.cpp:99`

A device is flagged intermittent when its **total decoded packet count is less than 5** since
the device was first seen in the current session.

There is **no time component**. The flag does not consider how much time has passed, how long the
device has been silent, or what the interval between packets was. It simply means: fewer than 5
packets have been successfully decoded from this device.

Practical implications:
- Every new device starts with packetCount = 1, so it will always be briefly intermittent.
- The flag clears permanently once packet 5 is decoded — it never re-activates.
- A device that went silent for hours is treated identically to a new device, as long as it had
  ≥ 5 packets before going quiet.

The comment in the scorer ("may indicate battery issues, unreliable") is a guess. In practice
this flag mostly means "we have not seen much from this device yet."

---

## Router Detection

**Source**: `firmware/src/repositories/device_repository.cpp:167–191`,
`firmware/src/protocol_analyzer.cpp:208–218`

Router detection works by comparing the **hop count** field in the Meshtastic packet header.

Meshtastic sets a `hop_limit` (default: 3) when a packet originates. Each node that relays the
packet decrements this counter by 1 before re-transmitting. The sniffer reads this counter from
**byte 12, lower 3 bits** of every Meshtastic packet.

For each device, the sniffer tracks the highest hop count ever seen from that node ID. This
approximates the original `hop_limit` that device uses.

- If a packet arrives with `hop_count == maxHopCount` → the packet came directly from the originator → `originatedPackets++`
- If a packet arrives with `hop_count < maxHopCount` → the packet was relayed by this device → `relayedPackets++`
- Once `relayedPackets >= 2`, the device is permanently flagged `isRouter = true`

**Why you may see the flag appear and disappear on the same device**:

The flag is set based on the *running* maximum hop count. When the first packet arrives, the
max hop count is set to that packet's value. If a later packet arrives with a *higher* hop count,
the max is updated — and packets that previously counted as "relayed" may now be reclassified.

Additionally, `isRouter` is set by the `deviceIdentifier` callback during first-packet
initialization (from `protocol_analyzer.cpp:isRoutingDevice()`), which uses a simpler rule:
any packet with `0 < hop_count < 3` on the first sighting marks the device as a router. The
device_repository then refines this as more packets arrive.

**LoRaWAN note**: Router detection only works for Meshtastic. LoRaWAN devices are always
`isRouter = false` because LoRaWAN gateways are transparent to the air interface.

---

## LoRaWAN Confirmed Up / Confirmed Down / Unconfirmed Up / Unconfirmed Down

**Source**: `firmware/src/protocol_analyzer.cpp:182–195`

These are the **message type (MType)** values from the LoRaWAN MAC header. The first byte of
every LoRaWAN frame is the MHDR, and bits 7–5 of MHDR encode the MType:

| MType value | Display name | Meaning |
|---|---|---|
| 0x00 | LoRaWAN Join Request | Device requesting network join |
| 0x01 | LoRaWAN Join Accept | Network server accepting a join |
| 0x02 | LoRaWAN Unconfirmed Up | Device → network, no ACK required |
| 0x03 | LoRaWAN Unconfirmed Down | Network → device, no ACK required |
| 0x04 | LoRaWAN Confirmed Up | Device → network, ACK required |
| 0x05 | LoRaWAN Confirmed Down | Network → device, ACK required |
| 0x06 | LoRaWAN RejoinReq | Device re-joining (LoRaWAN 1.1) |
| 0x07 | LoRaWAN Proprietary | Vendor-specific frame |

**Up** = uplink (end device transmitting to gateway/network server).
**Down** = downlink (network server transmitting to end device).
**Confirmed** = the sender requests a link-layer ACK. The sniffer only sees the frame — it cannot
tell whether the ACK was actually sent.

Most LoRaWAN sensors use **Unconfirmed Up** to minimize airtime and battery usage.
**Confirmed Up** is used for important messages where delivery acknowledgement matters.
Downlink frames are rare to capture on a passive sniffer because they are targeted to a specific
device and typically transmitted at the gateway side.

---

## Beacons

**Source**: `firmware/src/protocol_analyzer.cpp:81–84`

A packet is classified as `"Beacon"` when it is **8 bytes or shorter** and does not match the
Meshtastic header (`0xFF 0xFF 0xFF 0xFF`) or a valid LoRaWAN frame structure.

This is a catch-all for very short packets that cannot be decoded as a known protocol. They may be:
- Proprietary keep-alive or presence broadcasts
- Experimental LoRa devices with custom framing
- Chirp-only test transmitters

Within the Beacon protocol class, device type is further split:
- **Beacon High-Power**: RSSI > −60 dBm
- **Beacon Standard**: everything else

There is no attempt to parse the payload. The 8-byte threshold is a heuristic — any valid LoRaWAN
frame requires at least 12 bytes (MHDR + DevAddr + FCtrl + FCnt + MIC), so anything shorter
cannot be LoRaWAN.

---

## Firmware Version Estimation

**Source**: `firmware/src/protocol_analyzer.cpp:221–270`

Firmware versions displayed for Meshtastic and LoRaWAN devices are **heuristic estimates**, not
definitive values. Meshtastic does not broadcast its version over the air. The sniffer infers a
version range from observable packet structure:

### Meshtastic

| Observed pattern | Estimated version |
|---|---|
| Byte 8, bit 7 is set (encryption flag present) | `~v2.2+ (est: encryption flag)` |
| Packet length > 50 bytes (extended routing headers) | `~v2.1+ (est: extended headers)` |
| Packet length ≤ 16 bytes | `~v1.x or beacon (est)` |
| Hop count ≤ 3 and upper nibble of byte 9 is zero | `~v2.0.x (est: flag pattern)` |
| None of the above | `~v2.0-2.2 (est)` |

All Meshtastic firmware strings include `(est)` to indicate they are estimates. They should not
be treated as confirmed version numbers.

**Security scoring implication**: The outdated firmware check in `security_scorer.h` looks for
the literal strings `v1.x` or `v2.0` in the firmware version field. The string `~v1.x or beacon (est)`
contains `v1.x`, so it will trigger the −20 penalty. The string `~v2.0.x (est: flag pattern)`
does **not** contain exactly `v2.0` (it has `.x` appended), so it will **not** trigger the penalty —
this is a bug in the scoring logic.

### LoRaWAN

| Observed pattern | Estimated version |
|---|---|
| MType is Join Request (0x00) or Join Accept (0x01) | `~LoRaWAN 1.0/1.1 (est)` |
| All other data frames | `~LoRaWAN 1.0.x (est)` |

Join procedures differ slightly between LoRaWAN 1.0 and 1.1, but the distinction cannot be made
from the sniffer's perspective without decrypting the join payload.

---

## GPS Latitude / Longitude Showing 0

**Source**: `firmware/src/geo_intelligence.cpp:128–203`

GPS coordinates default to `0.0` and are only updated if:

1. The packet is identified as a Meshtastic **position packet** (byte 8 == `0x03` or `0x04`).
2. The protobuf payload is successfully parsed.
3. Both `latitude_i` (field 1) and `longitude_i` (field 2) are present in the protobuf.
4. At least one of them is non-zero after decoding.

From `geo_intelligence.cpp:197`:
```cpp
if (latitudeRaw != 0 || longitudeRaw != 0) {
    point.latitude = convertCoordinate(latitudeRaw);
    point.longitude = convertCoordinate(longitudeRaw);
}
```

If both raw values decode to zero, the coordinate conversion is skipped and the `GeoPoint`
is stored with `latitude = 0, longitude = 0`.

Common reasons for zero coordinates:
- **GPS not acquired**: The remote device has not yet obtained a GPS fix. Meshtastic transmits
  position packets even before acquiring a fix, with all fields at zero.
- **Encrypted packet**: If the packet was encrypted and PSK decryption failed, the position
  payload cannot be parsed. In this case the device entry shows 0,0 because no valid position
  was ever decoded.
- **Protobuf parse failure**: The position protobuf uses a custom minimal parser. If the field
  tags or wire types don't match the expected pattern, parsing silently fails and coordinates
  remain 0.
- **Device has no GPS module**: The remote Meshtastic node may not have GPS hardware. It can
  still transmit position packets (e.g., a manually-set location) but if no location is set,
  all fields will be zero.

The `GeoPoint.valid` flag is only set to `true` when `parseProtobufPosition()` returns `true`,
which requires `hasLat && hasLon` to both be true. Points with `valid = false` are excluded from
KML/GeoJSON export and the serial summary, but they may still appear in the web UI device list
with 0,0 coordinates if the API serializes the raw struct before the `valid` check.

---

## Why Only 10 Packets Are Kept Per Device

**Source**: `firmware/src/repositories/packet_store.h:20`, `firmware/src/config.h:148`

```cpp
// config.h
constexpr uint8_t MAX_SLOTS = 10;  // Replay::MAX_SLOTS

// packet_store.h
static constexpr size_t MAX_SLOTS = 10;  // From Config::Replay::MAX_PACKETS
```

The 10-packet limit applies to the **replay store** — the packets available for the capture/replay
feature. This is a RAM budget constraint, not a logging limit.

Each slot holds up to 256 bytes of raw packet data plus metadata. At 10 slots:
- 10 × 256 bytes = 2,560 bytes of packet payload
- Plus metadata overhead per slot

The ESP32-S3 has 512 KB of SRAM and 2 MB of PSRAM. The 10-slot limit was chosen conservatively to
leave headroom for the packet processing queue (100 slots × 256 bytes = 25,600 bytes), device
tracking, JSON serialization buffers, and the FreeRTOS stack.

**This limit does not affect logging**. All decoded packets are written to SD card (CSV and PCAP
formats) without a count limit — the SD card is flushed every 10 packets to prevent data loss,
but the total number of logged packets is unlimited.

The 10-slot replay store operates as a **ring buffer**: when full, the oldest slot is overwritten
by the newest captured packet. You can select any of the 10 stored packets for replay from the
web UI or serial menu.
