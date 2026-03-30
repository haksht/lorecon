#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - LoRaWAN Uplink Frame Parser

Parses LoRaWAN uplink (device-to-network) frames from PCAP captures.
Extracts DevAddr, frame counter (FCnt), port (FPort), and payload.

If an AppSKey is provided, decrypts the FRMPayload using AES-128-CTR
per the LoRaWAN 1.0.x specification.

Frame types handled:
    MType 0x02 = Unconfirmed Data Up
    MType 0x04 = Confirmed Data Up

Usage:
    python uplink_parser.py capture.pcap
    python uplink_parser.py capture.pcap --json
    python uplink_parser.py capture.pcap --appskey <hex32>
    python uplink_parser.py --demo

Requirements:
    cryptography  (optional, for payload decryption)
    pip install cryptography

LoRaWAN Data frame structure:
    MHDR     1 byte   MType, RFU, Major
    FHDR     7+ bytes DevAddr(4) + FCtrl(1) + FCnt(2) + FOpts(0-15)
    FPort    1 byte   (absent if payload empty)
    FRMPld   N bytes  AES-CTR encrypted payload (key = AppSKey if FPort>0)
    MIC      4 bytes  AES-CMAC (requires NwkSKey to verify)
"""

import argparse
import csv
import json
import struct
import sys
from collections import defaultdict
from datetime import datetime
from pathlib import Path

# Re-use PCAP parsing from parent tools directory
sys.path.insert(0, str(Path(__file__).parent.parent))
from pcap_analyzer import parse_pcap_native

# Crypto (optional)
try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    from cryptography.hazmat.backends import default_backend
    CRYPTO_AVAILABLE = True
except ImportError:
    CRYPTO_AVAILABLE = False

MTYPE_UNCONF_UP = 0x02
MTYPE_CONF_UP   = 0x04
UPLINK_MTYPES   = {MTYPE_UNCONF_UP, MTYPE_CONF_UP}

MTYPE_NAMES = {
    0x02: 'Unconfirmed Up',
    0x04: 'Confirmed Up',
}

# Minimum uplink: MHDR(1) + DevAddr(4) + FCtrl(1) + FCnt(2) + MIC(4) = 12
MIN_UPLINK_SIZE = 12


def decrypt_frmpayload(payload: bytes, appskey: bytes, devaddr: bytes,
                       fcnt: int, direction: int = 0) -> bytes:
    """
    Decrypt LoRaWAN FRMPayload using AES-128-CTR.

    AES block cipher in CTR mode with a per-block counter (Ai) as the keystream.

    Ai = 0x01 | 0x00*4 | direction(1) | DevAddr(4,LE) | FCnt(4,LE) | 0x00 | i(1)

    direction: 0 = uplink, 1 = downlink
    """
    if not CRYPTO_AVAILABLE:
        raise RuntimeError("cryptography module required — pip install cryptography")

    k = len(payload)
    num_blocks = (k + 15) // 16

    keystream = b''
    for i in range(1, num_blocks + 1):
        a_i = (
            b'\x01'
            + b'\x00' * 4
            + bytes([direction])
            + devaddr                            # 4 bytes little-endian
            + struct.pack('<I', fcnt)            # 4 bytes little-endian
            + b'\x00'
            + bytes([i])
        )
        cipher = Cipher(algorithms.AES(appskey), modes.ECB(), backend=default_backend())
        enc = cipher.encryptor()
        keystream += enc.update(a_i) + enc.finalize()

    return bytes(a ^ b for a, b in zip(payload, keystream[:k]))


def parse_uplink(data: bytes, timestamp=None, rssi=0, snr=0, freq=0,
                 appskey: bytes | None = None) -> dict | None:
    """
    Parse a LoRaWAN uplink frame.

    Returns a dict with extracted fields, or None if not a valid uplink.
    """
    if len(data) < MIN_UPLINK_SIZE:
        return None

    mhdr   = data[0]
    mtype  = (mhdr >> 5) & 0x07

    if mtype not in UPLINK_MTYPES:
        return None

    # FHDR: DevAddr(4 LE) + FCtrl(1) + FCnt(2 LE) + FOpts(0-15)
    dev_addr_le = data[1:5]
    fctrl       = data[5]
    fcnt        = struct.unpack('<H', data[6:8])[0]
    fopts_len   = fctrl & 0x0F
    adr         = bool(fctrl & 0x80)
    ack         = bool(fctrl & 0x20)

    fhdr_end = 8 + fopts_len
    if len(data) < fhdr_end + 4:   # Need at least MIC
        return None

    fopts = data[8:fhdr_end]

    # FPort and FRMPayload
    remaining = data[fhdr_end:-4]   # strip 4-byte MIC
    mic       = data[-4:]

    fport      = None
    frmpayload = b''
    decrypted  = None
    decrypt_ok = False

    if len(remaining) >= 1:
        fport = remaining[0]
        frmpayload = remaining[1:]

    # Attempt decryption
    if frmpayload and appskey and fport is not None and fport > 0:
        try:
            decrypted = decrypt_frmpayload(frmpayload, appskey, dev_addr_le, fcnt)
            decrypt_ok = True
        except Exception as e:
            decrypted = None

    dev_addr_int = struct.unpack('<I', dev_addr_le)[0]

    return {
        'type':           MTYPE_NAMES.get(mtype, 'Unknown Up'),
        'mtype':          mtype,
        'dev_addr':       f'0x{dev_addr_int:08X}',
        'fcnt':           fcnt,
        'fport':          fport,
        'adr':            adr,
        'ack':            ack,
        'fopts_hex':      fopts.hex() if fopts else '',
        'payload_hex':    frmpayload.hex() if frmpayload else '',
        'payload_len':    len(frmpayload),
        'decrypted_hex':  decrypted.hex() if decrypted else None,
        'decrypted_ascii': ''.join(chr(b) if 32 <= b < 127 else '.' for b in (decrypted or b'')),
        'decrypt_ok':     decrypt_ok,
        'mic_hex':        mic.hex(),
        'timestamp':      timestamp,
        'rssi':           round(rssi, 1),
        'snr':            round(snr, 1),
        'freq_mhz':       round(freq, 3),
        'raw_hex':        data.hex(),
    }


class UplinkScanner:
    """Scan a PCAP for LoRaWAN uplink frames."""

    def __init__(self, appskey: bytes | None = None):
        self.appskey = appskey
        self.frames = []
        self.devices = defaultdict(lambda: {
            'packets': 0,
            'fcnt_values': [],
            'fports': set(),
            'rssi_values': [],
            'freqs': set(),
            'confirmed_count': 0,
        })

    def scan_pcap(self, pcap_path: Path):
        packets = parse_pcap_native(pcap_path)
        for pkt in packets:
            if pkt.protocol != 'LoRaWAN' or not pkt.data:
                continue
            ts = pkt.pcap_timestamp.isoformat() if pkt.pcap_timestamp else None
            result = parse_uplink(pkt.data, ts, pkt.rssi, pkt.snr, pkt.frequency_mhz,
                                  appskey=self.appskey)
            if result:
                self.frames.append(result)
                dev = self.devices[result['dev_addr']]
                dev['packets'] += 1
                dev['fcnt_values'].append(result['fcnt'])
                if result['fport'] is not None:
                    dev['fports'].add(result['fport'])
                dev['rssi_values'].append(pkt.rssi)
                dev['freqs'].add(pkt.frequency_mhz)
                if result['mtype'] == MTYPE_CONF_UP:
                    dev['confirmed_count'] += 1

    def print_report(self):
        print(f"\n{'='*60}")
        print(f"📡 LoRaWAN Uplink Frame Analysis")
        print(f"{'='*60}")
        print(f"   Uplink frames found: {len(self.frames)}")
        print(f"   Unique devices:      {len(self.devices)}")
        if self.appskey:
            decrypted_count = sum(1 for f in self.frames if f['decrypt_ok'])
            print(f"   Decrypted payloads: {decrypted_count}/{len(self.frames)}")

        if not self.devices:
            print("\n   No LoRaWAN uplink frames found in capture.")
            return

        print(f"\n{'─'*60}")
        print(f"  {'DevAddr':<14} {'Frames':>6} {'FCnt Range':<16} {'FPorts':<12} {'Avg RSSI':>9}")
        print(f"{'─'*60}")

        for dev_addr, info in sorted(self.devices.items(), key=lambda x: -x[1]['packets']):
            avg_rssi = sum(info['rssi_values']) / len(info['rssi_values']) if info['rssi_values'] else 0
            fcnt_vals = info['fcnt_values']
            fcnt_range = f"{min(fcnt_vals)}–{max(fcnt_vals)}" if fcnt_vals else 'N/A'
            fports = ','.join(str(p) for p in sorted(info['fports'])) or '—'
            print(f"  {dev_addr:<14} {info['packets']:>6} {fcnt_range:<16} {fports:<12} {avg_rssi:>8.1f} dBm")

        # FCnt gap detection (potential packet loss or replay)
        print()
        for dev_addr, info in self.devices.items():
            fcnts = sorted(info['fcnt_values'])
            gaps = [(fcnts[i], fcnts[i+1]) for i in range(len(fcnts)-1)
                    if fcnts[i+1] - fcnts[i] > 10]
            if gaps:
                print(f"⚠️  {dev_addr}: large FCnt gaps (packet loss or missed traffic):")
                for a, b in gaps[:3]:
                    print(f"     {a} → {b} (gap: {b-a})")

        # Show decrypted payloads if available
        if self.appskey:
            decrypted = [f for f in self.frames if f['decrypt_ok'] and f['decrypted_ascii']]
            if decrypted:
                print(f"\n🔓 Decrypted Payloads (first 10):")
                for f in decrypted[:10]:
                    print(f"   {f['dev_addr']} FCnt={f['fcnt']} FPort={f['fport']}: "
                          f"{f['decrypted_ascii'][:80]}")

        print()
        print("ℹ️  DevAddr is assigned by the network server and exposed in every uplink.")
        print("   FCnt gaps may indicate frames captured on other frequencies or channels.")

    def to_json(self):
        return json.dumps({
            'frames': self.frames,
            'devices': {
                dev_addr: {
                    'packets': info['packets'],
                    'fcnt_range': [min(info['fcnt_values']), max(info['fcnt_values'])]
                        if info['fcnt_values'] else None,
                    'fports': sorted(info['fports']),
                    'avg_rssi': sum(info['rssi_values']) / len(info['rssi_values'])
                        if info['rssi_values'] else None,
                    'confirmed_count': info['confirmed_count'],
                }
                for dev_addr, info in self.devices.items()
            }
        }, indent=2)

    def to_csv(self, output_path: Path):
        if not self.frames:
            print("No frames to export.")
            return
        fieldnames = list(self.frames[0].keys())
        with open(output_path, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction='ignore')
            writer.writeheader()
            for frame in self.frames:
                writer.writerow(frame)
        print(f"✅ Exported {len(self.frames)} frames to {output_path}")


def run_demo():
    print("\n[DEMO MODE — simulated data]\n")
    print(f"{'='*60}")
    print(f"📡 LoRaWAN Uplink Frame Analysis")
    print(f"{'='*60}")
    print(f"   Uplink frames found: 47")
    print(f"   Unique devices:      3")
    print(f"   Decrypted payloads: 0/47  (no AppSKey provided)")
    print(f"\n{'─'*60}")
    print(f"  {'DevAddr':<14} {'Frames':>6} {'FCnt Range':<16} {'FPorts':<12} {'Avg RSSI':>9}")
    print(f"{'─'*60}")
    print(f"  0x260B1234     {23:>6} {'100–122':<16} {'1,2':<12} {-71.3:>8.1f} dBm")
    print(f"  0x260BABCD     {18:>6} {'55–72':<16} {'1':<12}  {-83.7:>8.1f} dBm")
    print(f"  0x260BFF00     { 6:>6} {'1–6':<16}   {'10':<12} {-92.1:>8.1f} dBm")
    print()
    print("⚠️  0x260B1234: large FCnt gaps (packet loss or missed traffic):")
    print("     108 → 119 (gap: 11)")
    print()
    print("ℹ️  DevAddr is assigned by the network server and exposed in every uplink.")
    print("   Provide --appskey <hex> to attempt payload decryption.")


def main():
    parser = argparse.ArgumentParser(
        description='Parse LoRaWAN uplink frames from PCAP captures',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s capture.pcap                    Scan and print report
  %(prog)s capture.pcap --json             Output JSON
  %(prog)s capture.pcap -o frames.csv      Export to CSV
  %(prog)s capture.pcap --appskey <hex32>  Decrypt FRMPayload
  %(prog)s --demo                          Demo output (no hardware needed)

AppSKey note:
  The AppSKey is 16 bytes (32 hex chars).
  It is derived from the AppKey during the OTAA join, or set statically (ABP).
  Without it, DevAddr, FCnt, and FPort are visible but payload is ciphertext.
"""
    )
    parser.add_argument('pcap_file', nargs='?', help='PCAP file to analyze')
    parser.add_argument('--json', action='store_true', help='Output as JSON')
    parser.add_argument('-o', '--output', metavar='FILE', help='Export to CSV')
    parser.add_argument('--appskey', metavar='HEX',
                        help='AppSKey (32 hex chars) for payload decryption')
    parser.add_argument('--demo', action='store_true', help='Demo output without hardware')

    args = parser.parse_args()

    if args.demo:
        run_demo()
        return 0

    if not args.pcap_file:
        parser.print_help()
        return 1

    appskey = None
    if args.appskey:
        if not CRYPTO_AVAILABLE:
            print("[!] Payload decryption unavailable - pip install cryptography")
        else:
            try:
                appskey = bytes.fromhex(args.appskey.replace(' ', ''))
                if len(appskey) != 16:
                    print("❌ AppSKey must be exactly 16 bytes (32 hex chars)")
                    return 1
            except ValueError:
                print("❌ Invalid AppSKey hex string")
                return 1

    pcap_path = Path(args.pcap_file)
    if not pcap_path.exists():
        print(f"❌ File not found: {pcap_path}")
        return 1

    print(f"📂 Loading {pcap_path} ({pcap_path.stat().st_size:,} bytes)")
    scanner = UplinkScanner(appskey=appskey)
    scanner.scan_pcap(pcap_path)

    if args.json:
        print(scanner.to_json())
    elif args.output:
        scanner.to_csv(Path(args.output))
    else:
        scanner.print_report()

    return 0


if __name__ == '__main__':
    sys.exit(main())
