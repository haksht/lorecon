"""Meshtastic decryption + protobuf primitives shared by map/report/topology.

This module does NOT replace tools/meshtastic/ — it only holds the small
subset of helpers that were being copy-pasted across the three analysis
tools (map.py, report.py, topology.py).
"""
from __future__ import annotations

import base64
import struct
from dataclasses import dataclass
from typing import List, Optional, Tuple

try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    from cryptography.hazmat.backends import default_backend
    CRYPTO_AVAILABLE = True
except ImportError:
    CRYPTO_AVAILABLE = False


# --- PSK database ----------------------------------------------------------

@dataclass
class PSKEntry:
    b64: str
    name: str
    risk: str  # CRITICAL | HIGH | MEDIUM | LOW | UNKNOWN


PSK_DB: List[PSKEntry] = [
    PSKEntry("AQ==",                        "Default (0x01)",          "CRITICAL"),
    PSKEntry("PKdTs51e4EB0BoOevIN0Dw==",    "Admin Channel (pre-2.5)", "CRITICAL"),
    PSKEntry("1PG7OiApB1nwvP+rz05pAQ==",    "LongFast Preset",         "HIGH"),
    PSKEntry("AAAAAAAAAAAAAAAAAAAAAA==",    "All Zeros",               "HIGH"),
    PSKEntry("MTIzNDU2Nzg5MDEyMzQ1Ng==",    "1234567890123456",        "HIGH"),
    PSKEntry("Ag==",                        "Legacy 0x02",             "MEDIUM"),
    PSKEntry("Aw==",                        "Legacy 0x03",             "MEDIUM"),
    PSKEntry("BA==",                        "Legacy 0x04",             "MEDIUM"),
    PSKEntry("BQ==",                        "Legacy 0x05",             "MEDIUM"),
    PSKEntry("Bg==",                        "Legacy 0x06",             "MEDIUM"),
    PSKEntry("Bw==",                        "Legacy 0x07",             "MEDIUM"),
    PSKEntry("CA==",                        "Legacy 0x08",             "MEDIUM"),
    PSKEntry("CQ==",                        "Legacy 0x09",             "MEDIUM"),
    PSKEntry("ZQ+HdKKbbAU4dSCGt66Qqw==",    "EU868 Regional",          "MEDIUM"),
    PSKEntry("d1iq21lNSh7BP6MOkP6cQA==",    "MediumFast",              "MEDIUM"),
    PSKEntry("/u7k03L8N3Q=",                "ShortFast",               "MEDIUM"),
    PSKEntry("GGC5DDnv8FKFm7WCZ5rXBA==",    "LongSlow",                "MEDIUM"),
    PSKEntry("LHrwq5nxPIJlqFU/K5dKKQ==",    "MediumSlow",              "MEDIUM"),
    PSKEntry("sb6GxC62sdwGXxJz2sERuQ==",    "ShortSlow",               "MEDIUM"),
    PSKEntry("dGVzdHRlc3R0ZXN0dGVzdA==",    "testtesttesttest",        "LOW"),
    PSKEntry("bWVzaHRhc3RpY21lc2h0YXN0",    "meshtastic...",           "LOW"),
    PSKEntry("shmLkA9H74gAeLH3eGCqsw==",    "Secondary Default",       "LOW"),
    PSKEntry("ogDPnKVRN7wz/VF8nt6LkA==",    "Debug Key",               "LOW"),
    # Carried over from topology.py's key list; origins unknown.
    PSKEntry("ZiETcjFHbk+ygALDp8tB3g==",    "Custom #1",               "UNKNOWN"),
    PSKEntry("LiDWCBX5RWCGy3T9dKiTpw==",    "Custom #2",               "UNKNOWN"),
    PSKEntry("8LQW/RhCEWKyV9HfSF3osA==",    "Custom #3",               "UNKNOWN"),
]


def expand_psk(b64: str) -> bytes:
    k = base64.b64decode(b64)
    n = len(k)
    if n == 1:  return k * 16
    if n == 8:  return k + k
    if n in (16, 32): return k
    return (k + b'\x00' * (16 - n)) if n < 16 else k[:32]


_COMPILED: List[Tuple[bytes, PSKEntry]] = [(expand_psk(e.b64), e) for e in PSK_DB]


# --- Decrypt ---------------------------------------------------------------

@dataclass
class DecryptResult:
    entry: PSKEntry
    plaintext: bytes
    portnum: int


def try_decrypt(raw: bytes) -> Optional[DecryptResult]:
    """AES-CTR decrypt a Meshtastic packet, trying each PSK in PSK_DB.

    `raw` is the full Meshtastic frame starting at the destination field
    (so raw[4:8] = src, raw[8:12] = packet_id, raw[16:] = ciphertext).
    """
    if not CRYPTO_AVAILABLE or len(raw) < 20:
        return None
    src_int = struct.unpack('<I', raw[4:8])[0]
    pid     = struct.unpack('<I', raw[8:12])[0]
    payload = raw[16:]
    if not payload:
        return None
    nonce = struct.pack('<IIII', pid, 0, src_int, 0)
    for key, entry in _COMPILED:
        try:
            pt = Cipher(algorithms.AES(key), modes.CTR(nonce),
                        backend=default_backend()).decryptor().update(payload)
        except Exception:
            continue
        if not pt or pt[0] != 0x08:
            continue
        portnum_raw = pt[1] if len(pt) > 1 else 0
        if portnum_raw == 0 or portnum_raw > 127:
            continue
        portnum = read_portnum(pt)
        if portnum is None:
            continue
        return DecryptResult(entry=entry, plaintext=pt, portnum=portnum)
    return None


# --- Protobuf primitives ---------------------------------------------------

def decode_varint(data: bytes, offset: int) -> Tuple[int, int]:
    result, shift, consumed = 0, 0, 0
    while offset + consumed < len(data):
        b = data[offset + consumed]; consumed += 1
        result |= (b & 0x7F) << shift; shift += 7
        if not (b & 0x80): break
    return result, consumed


def read_portnum(payload: bytes) -> Optional[int]:
    """Read field 1 (portnum varint) from a Meshtastic Data message."""
    offset = 0
    while offset < len(payload):
        tag, c = decode_varint(payload, offset); offset += c
        fn, wt = tag >> 3, tag & 0x07
        if fn == 1 and wt == 0:
            v, _ = decode_varint(payload, offset); return v
        if wt == 0:
            _, c = decode_varint(payload, offset); offset += c
        elif wt == 2:
            l, c = decode_varint(payload, offset); offset += c + l
        elif wt == 5: offset += 4
        elif wt == 1: offset += 8
        else: break
    return None


def extract_inner(payload: bytes) -> Optional[bytes]:
    """Extract field 2 (inner bytes) from a Meshtastic Data message."""
    offset = 0
    while offset < len(payload):
        tag, c = decode_varint(payload, offset); offset += c
        fn, wt = tag >> 3, tag & 0x07
        if fn == 2 and wt == 2:
            l, c = decode_varint(payload, offset); offset += c
            return payload[offset:offset + l]
        if wt == 0:
            _, c = decode_varint(payload, offset); offset += c
        elif wt == 2:
            l, c = decode_varint(payload, offset); offset += c + l
        elif wt == 5: offset += 4
        elif wt == 1: offset += 8
        else: break
    return None


def parse_position(inner: bytes) -> Optional[Tuple[float, float, float]]:
    lat_i = lon_i = alt = None
    offset = 0
    while offset < len(inner):
        tag, c = decode_varint(inner, offset); offset += c
        fn, wt = tag >> 3, tag & 0x07
        if fn == 1 and wt == 5 and offset + 4 <= len(inner):
            lat_i = struct.unpack('<i', inner[offset:offset+4])[0]; offset += 4
        elif fn == 2 and wt == 5 and offset + 4 <= len(inner):
            lon_i = struct.unpack('<i', inner[offset:offset+4])[0]; offset += 4
        elif fn == 3 and wt == 0:
            alt, c = decode_varint(inner, offset); offset += c
        elif wt == 0:
            _, c = decode_varint(inner, offset); offset += c
        elif wt == 2:
            l, c = decode_varint(inner, offset); offset += c + l
        elif wt == 5: offset += 4
        elif wt == 1: offset += 8
        else: break
    if lat_i is not None and lon_i is not None:
        lat, lon = lat_i / 1e7, lon_i / 1e7
        if lat != 0.0 or lon != 0.0:
            return (lat, lon, float(alt or 0))
    return None


# Meshtastic portnums (the ones used by these tools)
PORT_TEXT_MESSAGE = 1
PORT_POSITION     = 3
PORT_NODEINFO     = 4
PORT_TRACEROUTE   = 70
PORT_NEIGHBORINFO = 71


# Approximate Meshtastic HardwareModel enum (subset — the common ones).
HW_MODEL_NAMES = {
    0: "UNSET", 1: "TLORA_V2", 2: "TLORA_V1", 3: "TLORA_V2_1_1P6",
    4: "TBEAM", 5: "HELTEC_V2_0", 6: "TBEAM_V0P7", 7: "T_ECHO",
    8: "TLORA_V1_1P3", 9: "RAK4631", 10: "HELTEC_V2_1", 11: "HELTEC_V1",
    12: "LILYGO_TBEAM_S3_CORE", 13: "RAK11200", 14: "NANO_G1",
    15: "TLORA_V2_1_1P8", 16: "TLORA_T3_S3", 17: "NANO_G1_EXPLORER",
    18: "NANO_G2_ULTRA", 19: "LORA_TYPE", 20: "WIPHONE", 21: "WIO_WM1110",
    25: "STATION_G1", 26: "RAK11310", 27: "SENSELORA_RP2040",
    28: "SENSELORA_S3", 29: "CANARYONE", 30: "RP2040_LORA",
    31: "STATION_G2", 32: "LORA_RELAY_V1", 33: "NRF52840DK",
    34: "PPR", 35: "GENIEBLOCKS", 36: "NRF52_UNKNOWN", 37: "PORTDUINO",
    38: "ANDROID_SIM", 39: "DIY_V1", 40: "NRF52840_PCA10059",
    41: "DR_DEV", 42: "M5STACK", 43: "HELTEC_V3", 44: "HELTEC_WSL_V3",
    45: "BETAFPV_2400_TX", 46: "BETAFPV_900_NANO_TX", 47: "RPI_PICO",
    48: "HELTEC_WIRELESS_TRACKER", 49: "HELTEC_WIRELESS_PAPER",
    50: "T_DECK", 51: "T_WATCH_S3", 52: "PICOMPUTER_S3",
    53: "HELTEC_HT62", 54: "EBYTE_ESP32_S3", 55: "ESP32_S3_PICO",
    56: "CHATTER_2", 57: "HELTEC_WIRELESS_PAPER_V1_0",
    58: "HELTEC_WIRELESS_TRACKER_V1_0", 59: "UNPHONE",
    60: "TD_LORAC", 61: "CDEBYTE_EORA_S3", 62: "TWC_MESH_V4",
    63: "NRF52_PROMICRO_DIY", 64: "RADIOMASTER_900_BANDIT_NANO",
    65: "HELTEC_CAPSULE_SENSOR_V3", 66: "HELTEC_VISION_MASTER_T190",
    67: "HELTEC_VISION_MASTER_E213", 68: "HELTEC_VISION_MASTER_E290",
    69: "HELTEC_MESH_NODE_T114",
}


def parse_user(inner: bytes) -> Optional[dict]:
    """Parse a Meshtastic User protobuf (payload of NODEINFO portnum 4).

    Fields:
      1 = id (string)
      2 = long_name (string)
      3 = short_name (string)
      5 = hw_model (enum varint)
      6 = is_licensed (bool)
    """
    id_str = short = long = None
    hw_model: Optional[int] = None
    is_licensed = False
    offset = 0
    while offset < len(inner):
        tag, c = decode_varint(inner, offset); offset += c
        fn, wt = tag >> 3, tag & 0x07
        if wt == 2:
            l, c = decode_varint(inner, offset); offset += c
            val = inner[offset:offset + l]; offset += l
            try:
                s = val.decode('utf-8', errors='replace').strip('\x00').strip()
            except Exception:
                s = ''
            if   fn == 1: id_str = s
            elif fn == 2: long   = s
            elif fn == 3: short  = s
        elif wt == 0:
            v, c = decode_varint(inner, offset); offset += c
            if   fn == 5: hw_model = v
            elif fn == 6: is_licensed = bool(v)
        elif wt == 5: offset += 4
        elif wt == 1: offset += 8
        else: break
    if not (id_str or short or long or hw_model is not None):
        return None
    return {
        'id':          id_str,
        'short_name':  short,
        'long_name':   long,
        'hw_model':    hw_model,
        'hw_model_name': HW_MODEL_NAMES.get(hw_model) if hw_model is not None else None,
        'is_licensed': is_licensed,
    }
