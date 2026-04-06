#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - MeshCore Packet Decoder

Offline AES-128-ECB decryption for captured MeshCore packets.
Tests public channel key plus 11 common hashtag room keys.

Algorithm: AES-128-ECB, HMAC-SHA256 (2-byte MAC), channel hash fast-reject.

References:
  - https://docs.meshcore.io/packet_format/
  - https://jacksbrain.com/2026/01/a-hitchhiker-s-guide-to-meshcore-cryptography/
  - meshcore-decoder TypeScript library (michaelhart/meshcore-decoder)

Usage:
    python meshcore_decoder.py capture.pcap
    python meshcore_decoder.py capture.csv --format csv
    python meshcore_decoder.py AABBCC... --hex
    python meshcore_decoder.py --test

Requirements:
    pip install cryptography
"""

import argparse
import base64
import hashlib
import hmac
import json
import struct
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path

try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    from cryptography.hazmat.backends import default_backend
    CRYPTO_AVAILABLE = True
except ImportError:
    CRYPTO_AVAILABLE = False
    print("[!] cryptography not installed. Install with: pip install cryptography")

# MeshCore sync word (RADIOLIB_SX126X_SYNC_WORD_PRIVATE)
MESHCORE_SYNC_WORD = 0x12

# Public channel key (shipped on every MeshCore device)
PUBLIC_CHANNEL_KEY_B64 = "izOH6cXN6mrJ5e26oRXNcg=="

# Hashtag room names — key = SHA256("#roomname")[0:16]
HASHTAG_ROOMS = [
    "#general", "#emergency", "#local", "#mesh", "#public",
    "#chat", "#help", "#info", "#sos", "#weather", "#news",
]

# Payload type names (header bits 2-5)
PAYLOAD_TYPE_NAMES = {
    0: "MSG",
    1: "ACK",
    2: "SIGNED_MSG",
    3: "TRACE_REQ",
    4: "ADVERT",
    5: "DIRECT",
    6: "PING",
    7: "PONG",
    8: "POSITION",
}

# Route type names (header bits 0-1)
ROUTE_TYPE_NAMES = {
    0: "ZERO_HOP",
    1: "FLOOD",
    2: "TRANSPORT_FLOOD",
    3: "TRANSPORT_DIRECT",
}


def derive_keys():
    """Derive all channel keys: public + hashtag rooms."""
    keys = []

    # Key 0: public channel (base64 decode)
    pub_key = base64.b64decode(PUBLIC_CHANNEL_KEY_B64)
    keys.append((pub_key, "public channel"))

    # Keys 1-N: hashtag rooms (SHA256(name)[0:16])
    for room in HASHTAG_ROOMS:
        digest = hashlib.sha256(room.encode()).digest()
        keys.append((digest[:16], room))

    return keys


ALL_KEYS = derive_keys()


class MeshCoreDecoder:
    """Decode and decrypt MeshCore packets."""

    def __init__(self, verbose=False):
        self.verbose = verbose
        self.stats = defaultdict(int)
        self.decrypted_packets = []

    def get_payload_offset(self, data):
        """
        Return byte offset of the encrypted payload, or -1 if malformed.

        Packet layout:
          [header 1B]
          [transport_codes 4B]  -- only for route types 2 and 3
          [path_length 1B]      -- bits 0-5 = hop_count, bits 6-7 = hash_size - 1
          [path N bytes]        -- hop_count * hash_size bytes
          [payload ...]
        """
        if len(data) < 2:
            return -1

        route_type = data[0] & 0x03
        path_len_offset = 5 if route_type in (2, 3) else 1

        if path_len_offset + 1 > len(data):
            return -1

        path_len_byte = data[path_len_offset]
        hop_count = path_len_byte & 0x3F
        hash_size = ((path_len_byte >> 6) & 0x03) + 1

        payload_offset = path_len_offset + 1 + hop_count * hash_size
        if payload_offset >= len(data):
            return -1

        return payload_offset

    def get_originator_hash(self, data):
        """
        Extract originator node hash from path field.
        For route types 0/1: path starts at byte 2.
        For route types 2/3: path starts at byte 6.
        Returns bytes (1-4) or None.
        """
        if len(data) < 3:
            return None

        route_type = data[0] & 0x03
        path_len_offset = 5 if route_type in (2, 3) else 1

        if path_len_offset + 1 > len(data):
            return None

        path_len_byte = data[path_len_offset]
        hop_count = path_len_byte & 0x3F
        hash_size = ((path_len_byte >> 6) & 0x03) + 1

        path_start = path_len_offset + 1
        if hop_count == 0 or path_start + hash_size > len(data):
            return None

        return data[path_start:path_start + hash_size]

    def try_key(self, key, payload):
        """
        Try one 16-byte key against a group message payload.

        Payload format: [channel_hash 1B][mac 2B][ciphertext N*16B]

        Steps:
          1. Fast check: SHA256(key)[0] == payload[0]
          2. HMAC-SHA256(ciphertext, zero_padded_key_32B)[0:2] == payload[1:3]
          3. AES-128-ECB decrypt each 16-byte block
          4. Parse plaintext: [4B LE timestamp][1B flags][text...]
        """
        if not CRYPTO_AVAILABLE:
            return None

        # Need at least: channel_hash(1) + mac(2) + one AES block(16)
        if len(payload) < 19:
            return None

        ciphertext = payload[3:]
        if len(ciphertext) == 0 or len(ciphertext) % 16 != 0:
            return None

        # Step 1: channel hash fast-reject
        key_hash = hashlib.sha256(key).digest()
        if key_hash[0] != payload[0]:
            return None

        # Step 2: HMAC-SHA256 verification
        hmac_key = key + b'\x00' * 16  # zero-pad to 32 bytes
        mac = hmac.new(hmac_key, ciphertext, hashlib.sha256).digest()
        if mac[0] != payload[1] or mac[1] != payload[2]:
            return None

        # Step 3: AES-128-ECB decrypt
        try:
            cipher = Cipher(algorithms.AES(key), modes.ECB(), backend=default_backend())
            decryptor = cipher.decryptor()
            plaintext = decryptor.update(ciphertext) + decryptor.finalize()
        except Exception as e:
            if self.verbose:
                print(f"    AES error: {e}")
            return None

        # Step 4: parse plaintext [4B LE timestamp][1B flags][text...]
        if len(plaintext) < 5:
            return None

        timestamp_epoch = struct.unpack_from('<I', plaintext, 0)[0]
        # flags = plaintext[4]
        text_bytes = plaintext[5:]

        # Extract null-terminated text; reject non-printable (except tab/LF/CR)
        text_chars = []
        for b in text_bytes:
            if b == 0:
                break
            if b < 9 or (13 < b < 32):
                return None  # control character — bad decrypt
            text_chars.append(chr(b) if b < 128 else b.to_bytes(1, 'big').decode('utf-8', errors='replace'))

        text = ''.join(text_chars).strip()
        if not text:
            return None

        ts = None
        try:
            ts = datetime.fromtimestamp(timestamp_epoch, tz=timezone.utc).strftime('%Y-%m-%d %H:%M:%S UTC')
        except Exception:
            ts = f"epoch={timestamp_epoch}"

        return {'text': text, 'timestamp': ts, 'timestamp_epoch': timestamp_epoch}

    def decode_packet(self, raw_data):
        """Decode a single MeshCore packet. Returns result dict or None."""
        if isinstance(raw_data, str):
            raw_data = bytes.fromhex(raw_data.replace(' ', '').replace('0x', ''))

        if len(raw_data) < 4:
            self.stats['too_short'] += 1
            return None

        hdr = raw_data[0]
        version = (hdr >> 6) & 0x03
        payload_type = (hdr >> 2) & 0x0F
        route_type = hdr & 0x03

        # Basic sanity check
        if version > 1 or payload_type > 11:
            self.stats['not_meshcore'] += 1
            return None

        self.stats['meshcore_packets'] += 1

        payload_offset = self.get_payload_offset(raw_data)
        originator = self.get_originator_hash(raw_data)

        result = {
            'version': version,
            'payload_type': payload_type,
            'payload_type_name': PAYLOAD_TYPE_NAMES.get(payload_type, f'TYPE_{payload_type}'),
            'route_type': route_type,
            'route_type_name': ROUTE_TYPE_NAMES.get(route_type, f'RT_{route_type}'),
            'total_bytes': len(raw_data),
            'originator_hash': originator.hex() if originator else None,
            'decrypted': False,
            'channel': None,
            'text': None,
            'timestamp': None,
        }

        if payload_offset < 0:
            self.stats['malformed'] += 1
            return result

        payload = raw_data[payload_offset:]

        for key, key_name in ALL_KEYS:
            decrypted = self.try_key(key, payload)
            if decrypted:
                result['decrypted'] = True
                result['channel'] = key_name
                result['text'] = decrypted['text']
                result['timestamp'] = decrypted['timestamp']
                result['timestamp_epoch'] = decrypted['timestamp_epoch']
                self.stats['decrypted'] += 1
                self.stats[f'key_{key_name}'] += 1
                self.decrypted_packets.append(result)
                return result

        self.stats['encrypted'] += 1
        return result

    def print_result(self, result):
        """Print a decoded packet."""
        if result is None:
            return

        print(f"\n{'='*60}")
        rt = result['route_type_name']
        pt = result['payload_type_name']
        orig = result.get('originator_hash') or 'unknown'
        print(f"  Type: {pt}  Route: {rt}  Size: {result['total_bytes']}B  Node: 0x{orig}")

        if result['decrypted']:
            print(f"  Channel: {result['channel']}")
            if result.get('timestamp'):
                print(f"  Sent:    {result['timestamp']}")
            text = result['text']
            width = max(48, min(len(text), 72))
            print(f"\n  Message:")
            print(f"  +{'-'*(width+2)}+")
            for i in range(0, len(text), width):
                line = text[i:i+width]
                print(f"  | {line:<{width}} |")
            print(f"  +{'-'*(width+2)}+")
        else:
            print(f"  [encrypted — no matching key]")

    def print_summary(self):
        """Print session summary."""
        total = self.stats['meshcore_packets']
        decrypted = self.stats['decrypted']
        encrypted = self.stats['encrypted']
        malformed = self.stats['malformed']

        print(f"\n{'='*60}")
        print("MESHCORE DECRYPTION SUMMARY")
        print(f"{'='*60}")
        print(f"  Total MeshCore packets : {total}")
        print(f"  Decrypted              : {decrypted}")
        print(f"  Encrypted (no key hit) : {encrypted}")
        print(f"  Malformed              : {malformed}")
        print(f"  Non-MeshCore skipped   : {self.stats['not_meshcore']}")

        if decrypted > 0:
            print(f"\n  Key breakdown:")
            for k, v in self.stats.items():
                if k.startswith('key_') and v > 0:
                    print(f"    {k[4:]}: {v}")

        if total > 0:
            rate = (decrypted / total) * 100
            print(f"\n  Decryption rate: {rate:.1f}%")

        if self.decrypted_packets:
            print(f"\n  Decrypted messages ({len(self.decrypted_packets)}):")
            for pkt in self.decrypted_packets:
                ts = pkt.get('timestamp', '?')
                ch = pkt.get('channel', '?')
                text = pkt.get('text', '')
                print(f"    [{ts}] [{ch}] {text[:80]}")


def load_pcap(filepath):
    """Load raw packet bytes from a PCAP file."""
    packets = []
    with open(filepath, 'rb') as f:
        global_header = f.read(24)
        if len(global_header) < 24:
            raise ValueError("Invalid PCAP file")
        magic = struct.unpack('<I', global_header[:4])[0]
        if magic not in (0xa1b2c3d4, 0xa1b23c4d):
            raise ValueError(f"Invalid PCAP magic: 0x{magic:08x}")

        while True:
            pkt_header = f.read(16)
            if len(pkt_header) < 16:
                break
            _, _, incl_len, _ = struct.unpack('<IIII', pkt_header)
            pkt_data = f.read(incl_len)
            if len(pkt_data) < incl_len:
                break
            # Skip 20-byte LoRa pseudo-header if present
            if len(pkt_data) > 20 and pkt_data[0] not in (0x00, 0x02, 0x04, 0x06, 0x08, 0x0C):
                pkt_data = pkt_data[20:]
            packets.append(pkt_data)
    return packets


def load_csv(filepath):
    """Load raw packet bytes from a CSV file."""
    import csv
    packets = []
    with open(filepath, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            # Skip non-MeshCore rows if protocol column present
            protocol = row.get('protocol', '')
            if protocol and 'meshcore' not in protocol.lower():
                continue
            hex_data = row.get('hex_data') or row.get('data') or row.get('raw_hex')
            if hex_data:
                try:
                    packets.append(bytes.fromhex(hex_data.replace(' ', '')))
                except ValueError:
                    pass
    return packets


def run_tests():
    """Run decoder self-tests."""
    print("Running MeshCore decoder tests...\n")
    passed = 0
    total = 0

    decoder = MeshCoreDecoder(verbose=True)

    # Test 1: key derivation
    total += 1
    keys = derive_keys()
    pub_key = base64.b64decode(PUBLIC_CHANNEL_KEY_B64)
    if keys[0][0] == pub_key and len(keys) == 1 + len(HASHTAG_ROOMS):
        print(f"[OK] Key derivation: {len(keys)} keys loaded")
        passed += 1
    else:
        print(f"[FAIL] Key derivation: got {len(keys)}, expected {1 + len(HASHTAG_ROOMS)}")

    # Test 2: channel hash fast-reject
    total += 1
    key = keys[0][0]
    key_hash = hashlib.sha256(key).digest()
    dummy_payload = bytes([key_hash[0] ^ 0xFF]) + b'\x00' * 18  # wrong hash byte
    result = decoder.try_key(key, dummy_payload)
    if result is None:
        print("[OK] Channel hash fast-reject works")
        passed += 1
    else:
        print("[FAIL] Channel hash fast-reject did not reject bad hash")

    # Test 3: payload offset calculation
    total += 1
    # Route type 1 (FLOOD), 0 hops, hash_size=1: path_len_byte = 0x00
    # Layout: [hdr=0x05][path_len=0x00][payload...]
    test_pkt = bytes([0x05, 0x00, 0xAB, 0xCD])
    offset = decoder.get_payload_offset(test_pkt)
    if offset == 2:
        print(f"[OK] Payload offset RT=1, 0 hops: offset={offset}")
        passed += 1
    else:
        print(f"[FAIL] Payload offset RT=1, 0 hops: got {offset}, expected 2")

    # Test 4: originator hash extraction
    total += 1
    # Route type 1, 2 hops, hash_size=1: path_len_byte = 0x02
    # path = [0xC9, 0x54], originator = 0xC9
    test_pkt2 = bytes([0x05, 0x02, 0xC9, 0x54, 0xAB])
    orig = decoder.get_originator_hash(test_pkt2)
    if orig and orig[0] == 0xC9:
        print(f"[OK] Originator hash: 0x{orig.hex()}")
        passed += 1
    else:
        print(f"[FAIL] Originator hash: got {orig}")

    print(f"\n{'='*40}")
    print(f"Tests passed: {passed}/{total}")
    return passed == total


def main():
    parser = argparse.ArgumentParser(
        description='MeshCore Packet Decoder - Offline AES-128-ECB Decryption',
        epilog='Example: python meshcore_decoder.py capture.pcap'
    )
    parser.add_argument('input', nargs='?', help='Input file (PCAP or CSV) or hex string')
    parser.add_argument('--format', choices=['pcap', 'csv', 'hex'],
                        help='Input format (auto-detected if omitted)')
    parser.add_argument('--hex', action='store_true', help='Treat input as hex string')
    parser.add_argument('--test', action='store_true', help='Run self-tests')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose output')
    parser.add_argument('--json', action='store_true', help='Output results as JSON')
    parser.add_argument('--list-keys', action='store_true', help='List all known channel keys')
    args = parser.parse_args()

    if args.test:
        return 0 if run_tests() else 1

    if args.list_keys:
        print("Known MeshCore channel keys:\n")
        for i, (key, name) in enumerate(ALL_KEYS):
            print(f"  {i:2}. {name:<20}  {key.hex()}")
        return 0

    if not args.input:
        parser.print_help()
        return 1

    if not CRYPTO_AVAILABLE:
        print("cryptography library required. Install with: pip install cryptography")
        return 1

    decoder = MeshCoreDecoder(verbose=args.verbose)

    # Load packets
    if args.hex or args.format == 'hex':
        packets = [bytes.fromhex(args.input.replace(' ', ''))]
    else:
        filepath = Path(args.input)
        if not filepath.exists():
            print(f"File not found: {filepath}")
            return 1
        fmt = args.format or filepath.suffix.lower().lstrip('.')
        if fmt in ('pcap', 'pcapng'):
            packets = load_pcap(filepath)
        elif fmt == 'csv':
            packets = load_csv(filepath)
        else:
            print(f"Unknown format: {fmt}. Use --format pcap|csv|hex")
            return 1

    print(f"Loaded {len(packets)} packet(s)\n")

    results = []
    for pkt in packets:
        result = decoder.decode_packet(pkt)
        if result:
            results.append(result)
            if not args.json:
                decoder.print_result(result)

    if args.json:
        print(json.dumps(results, indent=2, default=str))
    else:
        decoder.print_summary()

    return 0


if __name__ == '__main__':
    sys.exit(main())
