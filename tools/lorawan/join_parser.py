#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - LoRaWAN Join Request Parser

Parses LoRaWAN OTAA Join Request and Join Accept frames from PCAP captures.
Join Requests are transmitted in plaintext — every OTAA join leaks DevEUI,
AppEUI (JoinEUI), and DevNonce over the air with no encryption.

Frame types handled:
    MType 0x00 = Join Request  (plaintext: JoinEUI, DevEUI, DevNonce)
    MType 0x01 = Join Accept   (encrypted with AppKey — header fields visible)

Usage:
    python join_parser.py capture.pcap
    python join_parser.py capture.pcap --json
    python join_parser.py capture.pcap -o devices.csv
    python join_parser.py --demo

Requirements:
    None beyond stdlib (scapy optional for advanced features)

LoRaWAN Join Request frame structure (23 bytes):
    MHDR    1 byte   MType=000 (join request), RFU, Major
    AppEUI  8 bytes  Little-endian (also called JoinEUI in LoRaWAN 1.1)
    DevEUI  8 bytes  Little-endian
    DevNonce 2 bytes Little-endian (random, prevents replay)
    MIC     4 bytes  AES-CMAC over MHDR|AppEUI|DevEUI|DevNonce (requires AppKey)
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

# MType definitions (bits 7-5 of MHDR)
MTYPE_JOIN_REQUEST  = 0x00
MTYPE_JOIN_ACCEPT   = 0x01
MTYPE_UNCONF_UP     = 0x02
MTYPE_UNCONF_DOWN   = 0x03
MTYPE_CONF_UP       = 0x04
MTYPE_CONF_DOWN     = 0x05

MTYPE_NAMES = {
    0x00: 'Join Request',
    0x01: 'Join Accept',
    0x02: 'Unconfirmed Up',
    0x03: 'Unconfirmed Down',
    0x04: 'Confirmed Up',
    0x05: 'Confirmed Down',
}

JOIN_REQUEST_SIZE = 23   # MHDR(1) + AppEUI(8) + DevEUI(8) + DevNonce(2) + MIC(4)
JOIN_ACCEPT_MIN   = 13   # MHDR(1) + encrypted payload(12 or 28) + MIC(4)


def eui_str(b: bytes) -> str:
    """Format 8-byte EUI as XX:XX:XX:XX:XX:XX:XX:XX (big-endian display)."""
    return ':'.join(f'{x:02X}' for x in reversed(b))


def parse_join_request(data: bytes, timestamp=None, rssi=0, snr=0, freq=0) -> dict | None:
    """
    Parse a LoRaWAN Join Request frame.

    Returns a dict with extracted fields, or None if not a valid join request.
    """
    if len(data) < JOIN_REQUEST_SIZE:
        return None

    mhdr = data[0]
    mtype = (mhdr >> 5) & 0x07
    major = mhdr & 0x03

    if mtype != MTYPE_JOIN_REQUEST:
        return None

    # Fields are little-endian on the wire; we reverse for display
    app_eui_le  = data[1:9]    # AppEUI / JoinEUI (LE)
    dev_eui_le  = data[9:17]   # DevEUI (LE)
    dev_nonce   = struct.unpack('<H', data[17:19])[0]
    mic         = data[19:23]

    return {
        'type':       'Join Request',
        'mtype':      mtype,
        'major':      major,
        'join_eui':   eui_str(app_eui_le),    # JoinEUI / AppEUI (big-endian display)
        'dev_eui':    eui_str(dev_eui_le),     # DevEUI (big-endian display)
        'dev_nonce':  dev_nonce,
        'mic_hex':    mic.hex(),
        'timestamp':  timestamp,
        'rssi':       round(rssi, 1),
        'snr':        round(snr, 1),
        'freq_mhz':   round(freq, 3),
        'raw_hex':    data.hex(),
    }


def parse_join_accept(data: bytes, timestamp=None, rssi=0, snr=0, freq=0) -> dict | None:
    """
    Parse a LoRaWAN Join Accept frame (header only — body is encrypted with AppKey).

    The Join Accept payload is encrypted with AES-ECB(AppKey) and cannot be
    decoded without the AppKey.  We record the frame for correlation with the
    preceding Join Request (same DevNonce / timing window).
    """
    if len(data) < JOIN_ACCEPT_MIN:
        return None

    mhdr = data[0]
    mtype = (mhdr >> 5) & 0x07

    if mtype != MTYPE_JOIN_ACCEPT:
        return None

    return {
        'type':      'Join Accept',
        'mtype':     mtype,
        'length':    len(data),
        'note':      'Payload encrypted with AppKey — cannot decode without key',
        'timestamp': timestamp,
        'rssi':      round(rssi, 1),
        'snr':       round(snr, 1),
        'freq_mhz':  round(freq, 3),
        'raw_hex':   data.hex(),
    }


class JoinScanner:
    """Scan a PCAP for LoRaWAN OTAA join activity."""

    def __init__(self):
        self.join_requests  = []
        self.join_accepts   = []
        self.devices        = defaultdict(lambda: {
            'join_requests': 0,
            'nonces': [],
            'join_eui': None,
            'rssi_values': [],
            'freqs': set(),
        })

    def scan_pcap(self, pcap_path: Path):
        packets = parse_pcap_native(pcap_path)
        for pkt in packets:
            if pkt.protocol != 'LoRaWAN' or not pkt.data:
                continue
            ts = pkt.pcap_timestamp.isoformat() if pkt.pcap_timestamp else None
            result = parse_join_request(pkt.data, ts, pkt.rssi, pkt.snr, pkt.frequency_mhz)
            if result:
                self.join_requests.append(result)
                dev = self.devices[result['dev_eui']]
                dev['join_requests'] += 1
                dev['nonces'].append(result['dev_nonce'])
                dev['join_eui'] = result['join_eui']
                dev['rssi_values'].append(pkt.rssi)
                dev['freqs'].add(pkt.frequency_mhz)
                continue

            result = parse_join_accept(pkt.data, ts, pkt.rssi, pkt.snr, pkt.frequency_mhz)
            if result:
                self.join_accepts.append(result)

    def print_report(self):
        print(f"\n{'='*60}")
        print(f"📡 LoRaWAN Join Request Analysis")
        print(f"{'='*60}")
        print(f"   Join Requests found: {len(self.join_requests)}")
        print(f"   Join Accepts found:  {len(self.join_accepts)}")
        print(f"   Unique devices:      {len(self.devices)}")

        if not self.devices:
            print("\n   No OTAA join activity found in capture.")
            return

        print(f"\n{'─'*60}")
        print(f"  {'DevEUI':<25} {'JoinEUI':<25} {'Joins':>5} {'Avg RSSI':>9}")
        print(f"{'─'*60}")

        for dev_eui, info in sorted(self.devices.items(), key=lambda x: -x[1]['join_requests']):
            avg_rssi = sum(info['rssi_values']) / len(info['rssi_values']) if info['rssi_values'] else 0
            print(f"  {dev_eui:<25} {info['join_eui'] or 'Unknown':<25} {info['join_requests']:>5} {avg_rssi:>8.1f} dBm")

        # Nonce reuse check — replay attack indicator
        print()
        reuse_found = False
        for dev_eui, info in self.devices.items():
            nonces = info['nonces']
            if len(nonces) != len(set(nonces)):
                if not reuse_found:
                    print("⚠️  DevNonce reuse detected (potential replay vulnerability):")
                    reuse_found = True
                seen = set()
                dupes = [n for n in nonces if n in seen or seen.add(n)]
                print(f"   {dev_eui}: nonces {sorted(set(dupes))} used more than once")

        if not reuse_found and len(self.join_requests) > 0:
            print("✅ No DevNonce reuse detected")

        print()
        print("ℹ️  Note: Join Requests are transmitted in plaintext by the LoRaWAN spec.")
        print("   DevEUI and JoinEUI are device identifiers exposed on every join attempt.")

    def to_json(self):
        return json.dumps({
            'join_requests': self.join_requests,
            'join_accepts': self.join_accepts,
            'devices': {
                dev_eui: {
                    'join_requests': info['join_requests'],
                    'join_eui': info['join_eui'],
                    'nonces': info['nonces'],
                    'avg_rssi': sum(info['rssi_values']) / len(info['rssi_values']) if info['rssi_values'] else None,
                    'frequencies_mhz': sorted(info['freqs']),
                }
                for dev_eui, info in self.devices.items()
            }
        }, indent=2)

    def to_csv(self, output_path: Path):
        fieldnames = ['type', 'dev_eui', 'join_eui', 'dev_nonce', 'mic_hex',
                      'timestamp', 'rssi', 'snr', 'freq_mhz', 'raw_hex']
        with open(output_path, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction='ignore')
            writer.writeheader()
            for r in self.join_requests:
                writer.writerow(r)
        print(f"✅ Exported {len(self.join_requests)} join requests to {output_path}")


def run_demo():
    """Print a realistic demo output without needing hardware."""
    print("\n[DEMO MODE — simulated data]\n")
    demo_devices = [
        ('A8:40:41:FF:FE:12:34:56', '70:B3:D5:7E:D0:05:67:89', [0x1A2B, 0x3C4D], -72.4),
        ('A8:40:41:FF:FE:AB:CD:EF', '70:B3:D5:7E:D0:00:11:22', [0x5E6F, 0x5E6F], -85.1),  # nonce reuse
        ('A8:40:41:FF:FE:DE:AD:BE', '70:B3:D5:7E:D0:08:99:AA', [0x7890], -61.0),
    ]
    print(f"{'='*60}")
    print(f"📡 LoRaWAN Join Request Analysis")
    print(f"{'='*60}")
    print(f"   Join Requests found: 5")
    print(f"   Join Accepts found:  3")
    print(f"   Unique devices:      3")
    print(f"\n{'─'*60}")
    print(f"  {'DevEUI':<25} {'JoinEUI':<25} {'Joins':>5} {'Avg RSSI':>9}")
    print(f"{'─'*60}")
    for dev_eui, join_eui, nonces, rssi in demo_devices:
        print(f"  {dev_eui:<25} {join_eui:<25} {len(nonces):>5} {rssi:>8.1f} dBm")
    print()
    print("⚠️  DevNonce reuse detected (potential replay vulnerability):")
    print("   A8:40:41:FF:FE:AB:CD:EF: nonce 0x5E6F used more than once")
    print()
    print("ℹ️  Note: Join Requests are transmitted in plaintext by the LoRaWAN spec.")
    print("   DevEUI and JoinEUI are device identifiers exposed on every join attempt.")


def main():
    parser = argparse.ArgumentParser(
        description='Parse LoRaWAN OTAA Join Requests from PCAP captures',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s capture.pcap              Scan and print report
  %(prog)s capture.pcap --json       Output JSON
  %(prog)s capture.pcap -o out.csv   Export to CSV
  %(prog)s --demo                    Demo output (no hardware needed)

Security note:
  LoRaWAN Join Requests are unencrypted by specification.
  DevEUI, JoinEUI, and DevNonce are exposed on every join.
  Nonce reuse indicates a device that does not randomize DevNonce —
  an indicator of replay vulnerability (LoRaWAN 1.0.x devices).
"""
    )
    parser.add_argument('pcap_file', nargs='?', help='PCAP file to analyze')
    parser.add_argument('--json', action='store_true', help='Output as JSON')
    parser.add_argument('-o', '--output', metavar='FILE', help='Export to CSV')
    parser.add_argument('--demo', action='store_true', help='Demo output without hardware')

    args = parser.parse_args()

    if args.demo:
        run_demo()
        return 0

    if not args.pcap_file:
        parser.print_help()
        return 1

    pcap_path = Path(args.pcap_file)
    if not pcap_path.exists():
        print(f"❌ File not found: {pcap_path}")
        return 1

    print(f"📂 Loading {pcap_path} ({pcap_path.stat().st_size:,} bytes)")
    scanner = JoinScanner()
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
