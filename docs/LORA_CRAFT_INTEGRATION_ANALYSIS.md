# LoRa_Craft Integration Analysis

**Analysis Date:** November 29, 2025  
**Source Repository:** [PentHertz/LoRa_Craft](https://github.com/PentHertz/LoRa_Craft)  
**Status:** Evaluated for potential integration

---

## Executive Summary

LoRa_Craft is a Python-based SDR toolkit for LoRaWAN protocol analysis, primarily focused on offline packet analysis, cryptographic operations, and PCAP forensics. After evaluation, **most features are not practical for ESP32 integration** due to architectural differences and limited real-world applicability.

### Key Finding
**Real-world LoRaWAN traffic uses session keys (NwkSKey/AppSKey) that are negotiated during the join procedure and are NOT brute-forceable on ESP32 hardware.** The project's current focus on Meshtastic is more practical given the prevalence of default PSKs in that ecosystem.

---

## About LoRa_Craft

### Architecture
- **Type:** Python-based desktop/PC toolkit
- **Dependencies:** Python, Scapy, GNU Radio, SDR equipment (USRP, bladeRF, RTL-SDR)
- **Primary Use Case:** Offline analysis of captured LoRa/LoRaWAN traffic

### Core Features
1. **Scapy Protocol Layers** - Python packet parsing for LoRaPHY and LoRaWAN 1.0/1.1
2. **MIC Bruteforce** - Dictionary attacks on Message Integrity Codes
3. **Payload Decryption** - If keys are known (not realistic in the field)
4. **Join Procedure Analysis** - Parse Join-Request/Join-Accept messages
5. **Packet Generation** - Craft LoRaWAN packets (Scapy-based)
6. **PCAP Export/Import** - Standard capture format

---

## Integration Opportunities Evaluated

### 🔴 **NOT RECOMMENDED - Low Value**

#### **1. LoRaWAN Deep Protocol Parsing**
**What:** Comprehensive parsing of LoRaWAN frame structures (FHDR, FCtrl, FOpts, MAC commands)

**Why Not Useful:**
- **Session Key Barrier:** Even with perfect parsing, payloads are encrypted with session keys
- **Low Device Count:** User reports rarely seeing LoRaWAN devices in the field
- **Complexity vs. Benefit:** ~2-3 weeks of work for cosmetic improvements to packet display
- **Current Detection Works:** Basic protocol identification is sufficient for reconnaissance

**Verdict:** ❌ Skip - Not worth the effort given low LoRaWAN traffic

---

#### **2. MIC Verification & Bruteforce**
**What:** Verify/bruteforce Message Integrity Codes using AES-CMAC

**Why Not Useful:**
- **Session Key Problem:** MIC is calculated with NwkSKey (established during join)
- **No Default Keys:** Unlike Meshtastic, LoRaWAN networks don't use default session keys
- **ESP32 Performance:** Dictionary attacks are too slow on embedded hardware
- **False Positive Rate:** Current PSK decryption already filters non-ASCII results

**Verdict:** ❌ Skip - Session keys make this impractical

---

#### **3. Join Procedure Analysis**
**What:** Parse Join-Request (AppEUI, DevEUI, DevNonce) and Join-Accept messages

**Why Marginally Useful:**
- ✅ Join-Requests use AppKey (potentially default/weak)
- ✅ Can enumerate devices attempting to join
- ❌ Join-Accept is encrypted with AppKey
- ❌ Rarely see join procedures (devices stay connected)
- ❌ Can't decrypt data packets even if we crack join

**Verdict:** ⚠️ Low priority - Only useful for network enumeration, not decryption

---

#### **4. Payload Decryption with Direction Awareness**
**What:** Enhanced decryption distinguishing uplink/downlink

**Why Not Useful:**
- **Keys Required:** Need both NwkSKey and AppSKey
- **No Key Source:** Session keys are derived during join procedure
- **Current Approach Works:** Meshtastic PSK testing is more practical
- **Wrong Problem:** Direction matters only if you have the keys

**Verdict:** ❌ Skip - Solving a problem we don't have

---

### 🟢 **RECOMMENDED - High Value**

#### **5. PCAP Export Format** ⭐⭐⭐
**What:** Export captured packets in standard PCAP format

**Why Useful:**
- ✅ **Post-Analysis:** Capture in the field, analyze later with LoRa_Craft/Wireshark
- ✅ **Collaboration:** Share captures with other researchers
- ✅ **Archive:** Standard format won't become obsolete
- ✅ **Low Complexity:** PCAP header is simple, ~1-2 days work
- ✅ **Complements Current Features:** Doesn't require decryption to be valuable
- ✅ **ESP32 Friendly:** Just write binary format to SD card

**Implementation:**
```cpp
// firmware/src/pcap_logger.h
class PCAPLogger {
    static const uint32_t PCAP_MAGIC = 0xa1b2c3d4;
    static const uint16_t PCAP_VERSION_MAJOR = 2;
    static const uint16_t PCAP_VERSION_MINOR = 4;
    static const uint32_t PCAP_LINKTYPE_USER = 147; // User-defined
    
public:
    bool begin(const char* filename);
    void writePacket(const uint8_t* data, size_t len, 
                    uint32_t timestamp, int16_t rssi, float snr);
    void close();
};
```

**Integration Points:**
- Add PCAP writer to `packet_logger.cpp`
- HTTP endpoint: `GET /api/export/pcap` (download last session)
- SD card: Save as `capture_YYYYMMDD_HHMMSS.pcap`
- Metadata: Store RSSI/SNR in PCAP comment fields

**Estimated Effort:** 2-3 days

**Value:** 🌟🌟🌟🌟🌟 (High - enables offline analysis without ESP32 limitations)

---

### 🟡 **CONDITIONAL - Depends on Use Case**

#### **6. Basic Join-Request Enumeration**
**What:** Parse Join-Request messages to enumerate devices

**Pros:**
- Track provisioning activity
- Identify device manufacturers (OUI from DevEUI)
- Detect rogue device join attempts
- Minimal complexity (~1 day)

**Cons:**
- Rarely see join procedures
- Limited tactical value
- Doesn't lead to decryption

**Verdict:** ⚠️ Implement only if you see join traffic regularly

---

## Real-World LoRaWAN Reality Check

### Why LoRaWAN Decryption is Impractical

```
Join Procedure (happens once):
┌────────────┐                           ┌────────────┐
│   Device   │  Join-Request (AppKey)    │  Network   │
│            │───────────────────────────>│   Server   │
│            │                            │            │
│            │  Join-Accept (AppKey)      │            │
│            │<───────────────────────────│            │
└────────────┘                           └────────────┘
      │                                         │
      └─────── Derives NwkSKey & AppSKey ──────┘
              (Both have session keys now)

All Subsequent Traffic:
┌────────────┐                           ┌────────────┐
│   Device   │  Data (NwkSKey+AppSKey)   │  Network   │
│            │<──────────────────────────>│   Server   │
└────────────┘                           └────────────┘
```

**Problem:** Session keys are derived using:
- AppKey (network secret)
- DevNonce (random)
- AppNonce (from server)
- NetID (network identifier)

**Why ESP32 Can't Help:**
1. AppKey is never transmitted in cleartext
2. Session keys are unique per device per join
3. Keys are 128-bit AES (2^128 keyspace = impossible)
4. No "default keys" like Meshtastic uses

### Why Meshtastic is Different

Meshtastic often uses **static channel PSKs** that are:
- Shared among all channel members
- Often defaults like `AQ==` or `1PG7OiApB1nwvP+rz05pAQ==`
- Reused across deployments
- Brute-forceable with small dictionary

**This is why your PSK testing works for Meshtastic but won't for LoRaWAN.**

---

## Recommendations

### ✅ **DO THIS: PCAP Export**

**Why:** Maximum value for minimum effort. Enables:
1. Offline analysis with desktop tools (LoRa_Craft, Wireshark, etc.)
2. Long-term archival in standard format
3. Collaboration with other researchers
4. Post-processing without ESP32 power limitations

**Implementation Priority:** HIGH (2-3 days work)

```cpp
// Example usage in packet_processor.cpp
if (sdCardAvailable && pcapLogging) {
    pcapLogger.writePacket(data, length, millis(), rssi, snr);
}
```

### ⚠️ **CONSIDER: Minimal Join-Request Parser**

**Only if you observe LoRaWAN join traffic regularly.**

Simple struct to display:
```cpp
struct JoinRequest {
    uint64_t appEUI;   // Application EUI
    uint64_t devEUI;   // Device EUI (contains manufacturer OUI)
    uint16_t devNonce; // Random nonce
};
```

Display in web UI as "Network Provisioning Activity" - shows what devices are trying to join.

**Implementation Priority:** LOW (1 day if needed)

### ❌ **DON'T DO THIS:**

1. **Deep LoRaWAN parsing** - Complex for no decryption benefit
2. **MIC bruteforce** - Session keys make this pointless
3. **Payload decryption** - No keys available in passive mode
4. **MAC command parsing** - Encrypted in FOpts, can't decrypt

---

## Alternative Strategy: Focus on What Works

### Current Strengths
1. ✅ **Meshtastic:** Default PSK testing works well
2. ✅ **GPS Extraction:** Valuable even without message decryption
3. ✅ **Node Discovery:** Device enumeration is useful
4. ✅ **Signal Analysis:** RSSI/SNR for RF mapping
5. ✅ **Multi-Protocol:** Covers Meshtastic, LoRaWAN, Helium

### Future Focus Areas
1. **Enhance Meshtastic PSK Dictionary** - Add more known defaults
2. **GPS Geofencing** - Alert on new nodes in specific areas
3. **PCAP Export** - Enable post-capture analysis
4. **Traffic Pattern Analysis** - Frequency, timing, burst detection
5. **Node Relationship Mapping** - Who talks to whom (even if encrypted)

---

## Conclusion

**LoRa_Craft is excellent for what it does** (offline LoRaWAN forensics), but most features require:
- Pre-captured traffic (not real-time)
- Known keys (not available in passive mode)
- Desktop computational power (not ESP32)
- Python/Scapy ecosystem (not C++)

**The ONE feature worth porting is PCAP export.** This bridges the gap between ESP32's real-time capture capabilities and desktop analysis tools' processing power.

**Your current focus on Meshtastic is the right call** - it's the only protocol where passive reconnaissance can realistically decrypt traffic due to default PSKs.

---

## Implementation Recommendation

### Phase 1: PCAP Export (Do This)
**Timeline:** 1 week  
**Files to Create:**
- `firmware/src/pcap_logger.h`
- `firmware/src/pcap_logger.cpp`

**Files to Modify:**
- `firmware/src/packet_logger.cpp` - Add PCAP option
- `firmware/src/web_server.cpp` - Add `/api/export/pcap` endpoint
- `data/webapp/index.html` - Add PCAP download button

**Testing:**
1. Capture packets to SD card in PCAP format
2. Download via web interface
3. Open in Wireshark or LoRa_Craft decoder
4. Verify packet integrity and metadata

### Phase 2: Enhanced Meshtastic (Higher ROI)
**Instead of LoRaWAN complexity, enhance what works:**
- Expand PSK dictionary (community-sourced keys)
- Better text extraction from decrypted packets
- Message threading/conversation reconstruction
- Network topology visualization

---

## References

- **LoRa_Craft Repository:** https://github.com/PentHertz/LoRa_Craft
- **LoRaWAN Specification:** https://lora-alliance.org/resource_hub/lorawan-specification-v1-1/
- **PCAP Format:** https://wiki.wireshark.org/Development/LibpcapFileFormat
- **Meshtastic Protocol:** https://meshtastic.org/docs/developers/protobufs/

---

**Last Updated:** November 29, 2025  
**Reviewed By:** GitHub Copilot  
**Status:** ✅ Analysis Complete - PCAP Export Recommended
