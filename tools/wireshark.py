#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - Wireshark PCAP Converter

The ESP32 writes PCAP files with a custom link-layer type (DLT_USER0 = 147)
and a 20-byte pseudo-header carrying LoRa metadata (frequency, RSSI, SNR,
SF, BW, CR).  Wireshark does not know this format natively.

This tool converts those files to LoRaTap format (DLT 270), which Wireshark
does understand and uses for its LoRaWAN dissector.

Optionally exports the PSK keys cracked by the firmware as a Wireshark
"IEEE 802.15.4 Decryption Keys" UAT file so Wireshark can decrypt in-session.

Usage:
    python wireshark.py capture.pcap                  # convert + open Wireshark
    python wireshark.py capture.pcap -o out.pcap      # convert only
    python wireshark.py capture.pcap --no-open        # convert, do not open
    python wireshark.py capture.pcap --keys keys.txt  # export PSK key file
    python wireshark.py capture.pcap --summary        # print packet statistics
    python wireshark.py capture.csv  --to-pcap        # CSV hex rows -> raw PCAP

No extra dependencies required (stdlib only).
"""

import argparse
import csv
import os
import struct
import subprocess
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
from pathlib import Path
from typing import Iterator, List, Optional, Tuple

# ---------------------------------------------------------------------------
# PCAP constants
# ---------------------------------------------------------------------------

PCAP_MAGIC_LE   = 0xa1b2c3d4
PCAP_MAGIC_NANO = 0xa1b23c4d
PCAP_VERSION_MAJOR = 2
PCAP_VERSION_MINOR = 4
PCAP_SNAPLEN    = 65535

DLT_USER0   = 147   # ESP32 custom (our format)
DLT_LORATAP = 270   # LoRaTap — Wireshark native LoRaWAN dissector

# ESP32 custom pseudo-header (20 bytes, little-endian)
# struct LoRaPseudoHeader { float freq_mhz; float rssi; float snr;
#   uint8_t sf; uint32_t bw_hz; uint8_t cr; uint16_t reserved; }
ESP32_FMT  = '<fffBIBH'
ESP32_SIZE = struct.calcsize(ESP32_FMT)  # 20

# LoRaTap header (24 bytes)
# https://github.com/rpp0/gr-lora/wiki/LoRaTap-Header
LORATAP_SIZE = 24

# Known Meshtastic PSKs (name -> base64) for key export
KNOWN_PSKS = {
    "Default (0x01)":          "AQ==",
    "LongFast Preset":         "1PG7OiApB1nwvP+rz05pAQ==",
    "Admin Channel (pre-2.5)": "PKdTs51e4EB0BoOevIN0Dw==",
    "All Zeros":               "AAAAAAAAAAAAAAAAAAAAAA==",
}


# ---------------------------------------------------------------------------
# PCAP I/O
# ---------------------------------------------------------------------------

def _read_pcap_global(fh) -> Tuple[bool, bool, int]:
    """
    Read and consume the 24-byte PCAP global header.
    Returns (is_nanosecond, is_big_endian, link_type).
    Raises ValueError if not a valid PCAP.
    """
    raw = fh.read(24)
    if len(raw) < 24:
        raise ValueError("File too short to be a PCAP")

    # Try little-endian magic first
    magic = struct.unpack('<I', raw[:4])[0]
    if magic in (PCAP_MAGIC_LE, PCAP_MAGIC_NANO):
        nano = (magic == PCAP_MAGIC_NANO)
        # Global header: magic(4) ver_maj(2) ver_min(2) thiszone(4) sigfigs(4) snaplen(4) link(4)
        link = struct.unpack('<I', raw[20:24])[0]
        return nano, False, link

    # Try big-endian
    magic = struct.unpack('>I', raw[:4])[0]
    if magic in (PCAP_MAGIC_LE, PCAP_MAGIC_NANO):
        nano = (magic == PCAP_MAGIC_NANO)
        link = struct.unpack('>I', raw[20:24])[0]
        return nano, True, link

    raise ValueError(f"Not a PCAP file (magic={raw[:4].hex()})")


def _read_packets(filepath: str) -> Iterator[Tuple[int, int, bytes, bytes]]:
    """
    Yield (ts_sec, ts_usec, esp32_header_bytes, payload_bytes) for each packet.
    Validates link type = DLT_USER0.
    """
    with open(filepath, 'rb') as fh:
        try:
            nano, swap, link = _read_pcap_global(fh)
        except ValueError as e:
            sys.exit(f"ERROR: {e}")

        endian = '>' if swap else '<'

        if link != DLT_USER0:
            print(f"WARNING: Expected DLT_USER0 ({DLT_USER0}), got {link}. Conversion may be wrong.")

        while True:
            rec = fh.read(16)
            if not rec or len(rec) < 16:
                break
            ts_s, ts_us, cap_len, orig_len = struct.unpack(endian + 'IIII', rec)
            pkt = fh.read(cap_len)
            if len(pkt) < ESP32_SIZE:
                continue
            yield ts_s, ts_us, pkt[:ESP32_SIZE], pkt[ESP32_SIZE:]


def _write_pcap_global(fh, link_type: int):
    fh.write(struct.pack('<IHHiIII',
        PCAP_MAGIC_LE, PCAP_VERSION_MAJOR, PCAP_VERSION_MINOR,
        0, 0, PCAP_SNAPLEN, link_type))


def _write_pcap_record(fh, ts_s: int, ts_us: int, payload: bytes):
    cap = len(payload)
    fh.write(struct.pack('<IIII', ts_s, ts_us, cap, cap))
    fh.write(payload)


# ---------------------------------------------------------------------------
# LoRaTap header builder
# ---------------------------------------------------------------------------

def _build_loratap(esp_hdr: bytes, ts_us: int) -> bytes:
    """
    Convert an ESP32 20-byte pseudo-header to a 24-byte LoRaTap header.

    LoRaTap layout (all big-endian):
      version  u8   = 0x01
      padding  u8   = 0x00
      length   u16  = 0x0018 (24)
      channel  u32  = frequency in Hz
      rssi     u32  = RSSI as milli-dBm (negative → large uint32)
      snr      s8   = SNR * 0.25 (quarter dB)
      noise    s8   = 0
      sf       u8   = spreading factor
      cr       u8   = coding rate denominator (5..8)
      sync     u16  = 0x3444  (standard Meshtastic sync word)
      gt       u8   = 0       (no GPS timestamp)
      rssi2    u8   = 0
      bw       u8   = BW index
      pad2     u8   = 0
    """
    freq_mhz, rssi, snr, sf, bw_hz, cr, _ = struct.unpack(ESP32_FMT, esp_hdr)
    freq_hz = int(freq_mhz * 1_000_000) if freq_mhz == freq_mhz else 915_000_000  # NaN guard
    # LoRaTap RSSI: uint32 in milli-dBm. Negative RSSI stored as 2's complement uint32.
    rssi_mdb = int(rssi * 1000) & 0xFFFFFFFF
    snr_q    = max(-128, min(127, int(snr * 4)))  # SNR in quarter dB, clamped to int8
    bw_idx   = {7800: 0, 10400: 1, 15600: 2, 20800: 3, 31250: 4,
                41700: 5, 62500: 6, 125000: 7, 250000: 8, 500000: 9}.get(bw_hz, 7)
    cr_den   = max(5, min(8, cr + 5)) if cr else 5

    hdr = struct.pack('>BBHIII',
        0x01, 0x00, LORATAP_SIZE,
        freq_hz,
        rssi_mdb,
        0)  # placeholder for the remaining fields (packed differently)

    # Build properly field by field
    return struct.pack('>B B H I I b b B B H B B B B',
        0x01,       # version
        0x00,       # padding
        LORATAP_SIZE,  # header length
        freq_hz,    # channel (freq Hz)
        rssi_mdb,   # RSSI milli-dBm
        snr_q,      # SNR (signed, quarter dB)
        0,          # noise (unknown)
        sf,         # spreading factor
        cr_den,     # coding rate denominator
        0x3444,     # sync word (Meshtastic)
        0,          # GPS timestamp flag
        0,          # reserved
        bw_idx,     # bandwidth index
        0,          # padding
    )


# ---------------------------------------------------------------------------
# Conversion
# ---------------------------------------------------------------------------

def convert(input_path: str, output_path: str) -> int:
    """Convert ESP32 PCAP to LoRaTap PCAP. Returns packet count."""
    count = 0
    with open(output_path, 'wb') as out:
        _write_pcap_global(out, DLT_LORATAP)
        for ts_s, ts_us, esp_hdr, payload in _read_packets(input_path):
            loratap_hdr = _build_loratap(esp_hdr, ts_us)
            pkt = loratap_hdr + payload
            _write_pcap_record(out, ts_s, ts_us, pkt)
            count += 1
    return count


def print_summary(filepath: str):
    """Print per-packet statistics from an ESP32 PCAP."""
    from collections import defaultdict
    freq_counts: dict = defaultdict(int)
    rssi_vals: List[float] = []
    snr_vals:  List[float] = []
    total = 0

    for _, _, esp_hdr, payload in _read_packets(filepath):
        freq_mhz, rssi, snr, sf, bw_hz, cr, _ = struct.unpack(ESP32_FMT, esp_hdr)
        freq_counts[f"{freq_mhz:.3f}"] += 1
        rssi_vals.append(rssi)
        snr_vals.append(snr)
        total += 1

    if total == 0:
        print("No packets found.")
        return

    print(f"Packets:     {total}")
    print(f"RSSI range:  {min(rssi_vals):.1f} .. {max(rssi_vals):.1f} dBm  "
          f"(avg {sum(rssi_vals)/total:.1f})")
    print(f"SNR range:   {min(snr_vals):.1f} .. {max(snr_vals):.1f} dB  "
          f"(avg {sum(snr_vals)/total:.1f})")
    print(f"\nFrequency distribution:")
    for freq, cnt in sorted(freq_counts.items(), key=lambda x: -x[1])[:15]:
        bar = '█' * (cnt * 40 // total)
        print(f"  {freq} MHz  {cnt:5d}  {bar}")


def export_keys(output_path: str):
    """
    Write a Wireshark UAT key file for Meshtastic PSKs.
    Load in Wireshark: Edit > Preferences > Protocols > IEEE 802.15.4 > Decryption Keys
    """
    import base64
    lines = []
    for name, b64 in KNOWN_PSKS.items():
        key_hex = base64.b64decode(b64).hex()
        # Pad/truncate to 16 bytes
        if len(key_hex) < 32:
            key_hex = key_hex + '00' * (16 - len(key_hex) // 2)
        key_hex = key_hex[:32]
        lines.append(f'"{key_hex}","Normal","0x0000","{name}"')
    with open(output_path, 'w') as fh:
        fh.write('\n'.join(lines) + '\n')
    print(f"Key file saved: {output_path}")
    print(f"Load in Wireshark: Edit > Preferences > Protocols > IEEE 802.15.4 > Decryption Keys")


def from_csv(csv_path: str, output_path: str) -> int:
    """Convert CSV raw_hex column to a minimal raw PCAP (DLT_USER0, no ESP32 header)."""
    import time
    count = 0
    with open(csv_path, 'r', encoding='utf-8') as cf, \
         open(output_path, 'wb') as out:
        _write_pcap_global(out, DLT_USER0)
        ts = int(time.time())
        for row in csv.DictReader(cf):
            raw_hex = row.get('raw_hex', '')
            if not raw_hex:
                continue
            try:
                pkt = bytes.fromhex(raw_hex)
            except ValueError:
                continue
            _write_pcap_record(out, ts, 0, pkt)
            ts += 1
            count += 1
    return count


def open_wireshark(filepath: str):
    """Try to open the file in Wireshark."""
    candidates = [
        'wireshark',
        r'C:\Program Files\Wireshark\Wireshark.exe',
        r'C:\Program Files (x86)\Wireshark\Wireshark.exe',
        '/usr/bin/wireshark',
        '/Applications/Wireshark.app/Contents/MacOS/Wireshark',
    ]
    for ws in candidates:
        if os.path.exists(ws) or ws == 'wireshark':
            try:
                subprocess.Popen([ws, filepath])
                print(f"Opened in Wireshark: {filepath}")
                return
            except FileNotFoundError:
                continue
    print(f"Wireshark not found. Open manually: {Path(filepath).resolve()}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description='Convert ESP32 LoRa Sniffer PCAP to Wireshark-compatible LoRaTap format.')
    ap.add_argument('input', help='Input PCAP or CSV file')
    ap.add_argument('-o', '--output', metavar='FILE',
                    help='Output file (default: <stem>_loratap.pcap or <stem>.pcap)')
    ap.add_argument('--no-open', action='store_true',
                    help='Do not open Wireshark after conversion')
    ap.add_argument('--summary', action='store_true',
                    help='Print packet statistics instead of converting')
    ap.add_argument('--keys', metavar='FILE',
                    help='Export PSK keys to a Wireshark UAT decryption key file')
    ap.add_argument('--to-pcap', action='store_true',
                    help='Convert CSV raw_hex column to PCAP (input must be CSV)')
    args = ap.parse_args()

    p = Path(args.input)
    if not p.exists():
        sys.exit(f"File not found: {args.input}")

    if args.keys:
        export_keys(args.keys)
        if not args.summary and not args.to_pcap:
            return

    if args.summary:
        print_summary(str(p))
        return

    if args.to_pcap or p.suffix.lower() == '.csv':
        out = args.output or (p.stem + '.pcap')
        n = from_csv(str(p), out)
        print(f"Converted {n} CSV rows to PCAP: {out}")
        if not args.no_open:
            open_wireshark(out)
        return

    # PCAP -> LoRaTap conversion
    out = args.output or (p.stem + '_loratap.pcap')
    n = convert(str(p), out)
    print(f"Converted {n} packets: {p.name} -> {out}  (DLT_USER0 -> LoRaTap {DLT_LORATAP})")
    if not args.no_open:
        open_wireshark(out)


if __name__ == '__main__':
    main()
