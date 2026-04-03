# Meshtastic Encryption Reference

What the sniffer can and cannot decrypt, and why.

---

## What you can decrypt

These packet types use **channel PSK (AES-256-CTR)** with a shared key. If the device is using a
known default key, the sniffer decrypts them automatically.

- Position broadcasts (GPS coordinates)
- Telemetry (battery, temperature, voltage)
- Node info (device name, hardware model)
- Traceroutes (network topology)
- Routing and mesh control packets
- Channel/group messages (text sent to a channel, not DMs)

## What you cannot decrypt

- **Direct messages** -- use Curve25519 PKC (post-2.5.0 firmware); requires the recipient private key
- **Admin messages** -- also PKC-protected (post-2.5.0 firmware)
- **Custom PSK channels** -- if a user changed the default key, the 23-key database will not match

---

## Two encryption systems

Meshtastic 2.5.0 (June 2024) introduced asymmetric encryption for DMs. Prior to that, everything
used channel PSK.

### Channel PSK (AES-256-CTR)

Used for all automatic broadcasts and channel messages. The key is shared across all nodes on the
channel. Default key is "AQ==" (base64, expands to a 32-byte key). Most out-of-the-box
deployments still use this default.

**Nonce construction:**
```
Bytes 0-3:   Packet ID (little-endian)
Bytes 4-7:   0x00
Bytes 8-11:  From Node ID (big-endian)
Bytes 12-15: 0x00
```

### Curve25519 (direct messages, post-2.5.0)

Each device has a unique Curve25519 key pair. DMs are encrypted to the recipient public key.
Decryption requires the recipient private key, which never leaves the device. Not feasible to break.

---

## Default PSK database (23 keys)

The sniffer tests 23 known default and historically common PSKs:

| Range | Description |
|-------|-------------|
| Key 1 | `"AQ=="` -- official default, most common |
| Key 2 | `"1PG7OiApB1nwvP+rz05pAQ=="` -- LongFast channel key |
| Keys 3-10 | Legacy single-byte keys from pre-2.0 firmware |
| Keys 11-14 | Common test/development keys found in deployments |
| Key 15 | `"PKdTs51e4EB0BoOevIN0Dw=="` -- admin channel default (pre-2.5) |
| Keys 16-18 | Secondary channel defaults, debug key, EU868 regional default |
| Keys 19-23 | Preset-derived keys (MediumFast, ShortFast, LongSlow, etc.) |

**Key 15 note:** Devices still using the pre-2.5 admin channel default with admin channel enabled
are potentially vulnerable to unauthorized remote configuration.

A successful PSK hit means you can read position, telemetry, node info, and any channel messages
from that device.

**DMs (direct messages):** In pre-2.5.0 Meshtastic firmware, DMs were encrypted with the channel PSK, so a PSK hit exposes them. In post-2.5.0 firmware, DMs use Curve25519 public key cryptography; a PSK hit does not expose them.

---

## What changed in firmware 2.5.0

| Message type | Pre-2.5.0 | Post-2.5.0 |
|---|---|---|
| Position, telemetry, node info | Channel PSK | Channel PSK |
| Traceroutes, routing | Channel PSK | Channel PSK |
| Channel/group messages | Channel PSK | Channel PSK |
| **Direct messages** | Session key (derived from channel PSK) | **Curve25519 PKC** |
| **Admin messages** | Channel PSK | **Curve25519 PKC** |

Before 2.5.0, DMs were just channel messages with a `to` field -- anyone with the channel PSK
could read them. Post-2.5.0, DMs have true end-to-end encryption.

---

## Unicast vs broadcast capture

LoRa is always broadcast at the RF layer -- every radio in range receives every transmission.
Destination addressing is application-layer only; the SX1262 does not filter by it.

On the sniffer primary Meshtastic configs (sync word `0x2B`, `0x48`), both broadcast packets
(`0xFFFFFFFF` destination) and unicast packets (specific node ID destination) are captured and
identified as Meshtastic. All downstream analysis -- PSK decryption, device tracking, GPS
extraction -- applies to both.

On `0x12` sync word configs (`Meshtastic_902_SW12` variants, ISM configs), only broadcast packets
are identified as Meshtastic. Unicast on those configs is ambiguous with RadioHead traffic.

---

## Testing decryption

### Verify broadcast decryption

1. One Meshtastic device with default settings, sniffer running
2. Expected serial output:
```
[PSK] Decryption SUCCESS with key #1 "AQ=="
[PSK] Type: POSITION_APP
[GEO] Position: 37.7749 N, 122.4194 W, alt: 15m
```

### Verify channel message decryption

1. Send a message to the **channel** (not DM) from a device using default PSK
2. Expected serial output:
```
[PSK] Decryption SUCCESS with key #1 "AQ=="
[PSK] DECRYPTED TEXT MESSAGE: "your test message"
```

If this fails: confirm the message went to the channel tab, not the DM tab.

### Verify DM protection

1. Send a DM between two 2.5.0+ devices
2. Expected serial output:
```
[PSK] Testing all 23 default PSKs...
[PSK] No valid decryption found
```

This is correct -- DMs are PKC-protected and cannot be decrypted passively.

