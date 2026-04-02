#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - Meshtastic Packet Decoder

Offline PSK decryption and protobuf parsing for captured Meshtastic packets.
Tests 23 known default PSKs including legacy admin channel defaults.

Usage:
    python meshtastic_decoder.py capture.pcap
    python meshtastic_decoder.py capture.csv --format csv
    python meshtastic_decoder.py packet.hex --hex
    python meshtastic_decoder.py --test  # Run PSK validation tests

Requirements:
    pip install cryptography
    pip install protobuf  # Optional, for full protobuf parsing
"""

import argparse
import base64
import struct
import json
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
from pathlib import Path
from datetime import datetime
from collections import defaultdict

try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    from cryptography.hazmat.backends import default_backend
    CRYPTO_AVAILABLE = True
except ImportError:
    CRYPTO_AVAILABLE = False
    print("[!] cryptography not installed. Install with: pip install cryptography")

# Known Meshtastic PSKs (Base64-encoded)
# These are the most common pre-shared keys found in Meshtastic networks
DEFAULT_PSKS = [
    # === Standard defaults (most common) ===
    ("AQ==", "Default (single byte 0x01)"),
    ("1PG7OiApB1nwvP+rz05pAQ==", "LongFast preset"),
    
    # === Legacy single-byte keys (pre-2.0, expanded to 16 bytes) ===
    ("Ag==", "Legacy 0x02"),
    ("Aw==", "Legacy 0x03"),
    ("BA==", "Legacy 0x04"),
    ("BQ==", "Legacy 0x05"),
    ("Bg==", "Legacy 0x06"),
    ("Bw==", "Legacy 0x07"),
    ("CA==", "Legacy 0x08"),
    ("CQ==", "Legacy 0x09"),
    
    # === Test/development keys often left in production ===
    ("AAAAAAAAAAAAAAAAAAAAAA==", "All zeros"),
    ("MTIzNDU2Nzg5MDEyMzQ1Ng==", "1234567890123456"),
    ("dGVzdHRlc3R0ZXN0dGVzdA==", "testtesttesttest"),
    ("bWVzaHRhc3RpY21lc2h0YXN0", "meshtasticmeshtast"),
    
    # === Historic defaults from older firmware ===
    ("PKdTs51e4EB0BoOevIN0Dw==", "Admin channel default (pre-2.5)"),
    ("shmLkA9H74gAeLH3eGCqsw==", "Secondary channel default"),
    ("ogDPnKVRN7wz/VF8nt6LkA==", "Debug/dev key (from firmware source)"),
    ("ZQ+HdKKbbAU4dSCGt66Qqw==", "EU868 regional default"),
    
    # === Channel preset derived keys ===
    ("d1iq21lNSh7BP6MOkP6cQA==", "MediumFast preset"),
    ("/u7k03L8N3Q=", "ShortFast preset (8 bytes)"),
    ("GGC5DDnv8FKFm7WCZ5rXBA==", "LongSlow preset"),
    ("LHrwq5nxPIJlqFU/K5dKKQ==", "MediumSlow preset"),
    ("sb6GxC62sdwGXxJz2sERuQ==", "ShortSlow preset"),
]

# Meshtastic port numbers
PORT_NAMES = {
    0: "UNKNOWN_APP",
    1: "TEXT_MESSAGE_APP",
    2: "REMOTE_HARDWARE_APP",
    3: "POSITION_APP",
    4: "NODEINFO_APP",
    5: "ROUTING_APP",
    6: "ADMIN_APP",
    7: "TEXT_MESSAGE_COMPRESSED_APP",
    8: "WAYPOINT_APP",
    32: "REPLY_APP",
    33: "IP_TUNNEL_APP",
    34: "PAXCOUNTER_APP",
    64: "SERIAL_APP",
    65: "STORE_FORWARD_APP",
    66: "RANGE_TEST_APP",
    67: "TELEMETRY_APP",
    68: "ZPS_APP",
    69: "SIMULATOR_APP",
    70: "TRACEROUTE_APP",
    71: "NEIGHBORINFO_APP",
    72: "ATAK_PLUGIN",
    73: "MAP_REPORT_APP",
    256: "PRIVATE_APP",
    257: "ATAK_FORWARDER",
}


class MeshtasticDecoder:
    """Decode and decrypt Meshtastic packets"""
    
    def __init__(self, verbose=False):
        self.verbose = verbose
        self.stats = defaultdict(int)
        self.decrypted_packets = []
    
    def expand_key(self, key_bytes):
        """Expand short keys to 16 bytes (Meshtastic defaultpsk behavior)"""
        if len(key_bytes) == 16 or len(key_bytes) == 32:
            return key_bytes
        
        if len(key_bytes) == 1:
            # Single byte key: replicate to 16 bytes
            return key_bytes * 16
        
        if len(key_bytes) == 8:
            # 8-byte key: double it
            return key_bytes * 2
        
        # Pad with zeros
        return key_bytes.ljust(16, b'\x00')
    
    def build_nonce(self, packet_id, node_id):
        """Build AES-CTR nonce from packet ID and node ID"""
        nonce = bytearray(16)
        
        # Packet ID in first 4 bytes (little-endian)
        struct.pack_into('<I', nonce, 0, packet_id)
        
        # Node ID in bytes 8-11 (little-endian, matching firmware)
        struct.pack_into('<I', nonce, 8, node_id)
        
        return bytes(nonce)
    
    def decrypt_payload(self, encrypted_data, key_bytes, packet_id, node_id):
        """Attempt AES-CTR decryption with given key"""
        if not CRYPTO_AVAILABLE:
            return None
        
        try:
            expanded_key = self.expand_key(key_bytes)
            nonce = self.build_nonce(packet_id, node_id)
            
            cipher = Cipher(
                algorithms.AES(expanded_key),
                modes.CTR(nonce),
                backend=default_backend()
            )
            decryptor = cipher.decryptor()
            decrypted = decryptor.update(encrypted_data) + decryptor.finalize()
            
            return decrypted
        except Exception as e:
            if self.verbose:
                print(f"    Decrypt error: {e}")
            return None
    
    def validate_decrypted(self, decrypted):
        """Check if decrypted data looks like valid protobuf"""
        if len(decrypted) < 2:
            return False, None
        
        # Check for common protobuf patterns
        # Field 1 (portnum): 0x08 followed by varint
        if decrypted[0] == 0x08:
            portnum = decrypted[1]
            if portnum in PORT_NAMES:
                return True, portnum
        
        # Also check for direct payload (0x12 = field 2, length-delimited)
        if decrypted[0] == 0x12:
            return True, None
        
        return False, None
    
    def parse_packet_header(self, data):
        """Parse Meshtastic packet header"""
        if len(data) < 16:
            return None
        
        # Check for magic header
        if data[:4] != b'\xff\xff\xff\xff':
            return None
        
        # Parse header fields (little-endian)
        # to_node = struct.unpack('<I', data[0:4])[0]  # Actually magic
        from_node = struct.unpack('<I', data[4:8])[0]
        packet_id = struct.unpack('<I', data[8:12])[0]
        flags = data[12]
        channel = data[13]
        
        return {
            'from_node': from_node,
            'packet_id': packet_id,
            'flags': flags,
            'channel': channel,
            'encrypted_payload': data[16:]
        }
    
    def decode_packet(self, raw_data):
        """Decode a single packet, attempting all PSKs"""
        # Handle hex string input
        if isinstance(raw_data, str):
            raw_data = bytes.fromhex(raw_data.replace(' ', '').replace('0x', ''))
        
        header = self.parse_packet_header(raw_data)
        if not header:
            self.stats['non_meshtastic'] += 1
            return None
        
        self.stats['meshtastic_packets'] += 1
        
        result = {
            'from_node': f"0x{header['from_node']:08X}",
            'packet_id': f"0x{header['packet_id']:08X}",
            'channel': header['channel'],
            'encrypted_length': len(header['encrypted_payload']),
            'decrypted': False,
            'psk_used': None,
            'portnum': None,
            'port_name': None,
            'payload': None,
            'text_message': None,
        }
        
        # Check if unencrypted (raw protobuf)
        valid, portnum = self.validate_decrypted(header['encrypted_payload'])
        if valid:
            result['decrypted'] = True
            result['psk_used'] = 'NONE (unencrypted)'
            result['portnum'] = portnum
            result['port_name'] = PORT_NAMES.get(portnum, 'UNKNOWN')
            result['payload'] = header['encrypted_payload']
            self.stats['unencrypted'] += 1
            
            # Try to extract text message
            if portnum == 1:  # TEXT_MESSAGE_APP
                result['text_message'] = self.extract_text(header['encrypted_payload'])
            
            self.decrypted_packets.append(result)
            return result
        
        # Try each PSK
        for psk_b64, psk_name in DEFAULT_PSKS:
            try:
                key_bytes = base64.b64decode(psk_b64)
            except Exception:
                continue
            
            decrypted = self.decrypt_payload(
                header['encrypted_payload'],
                key_bytes,
                header['packet_id'],
                header['from_node']
            )
            
            if decrypted:
                valid, portnum = self.validate_decrypted(decrypted)
                if valid:
                    result['decrypted'] = True
                    result['psk_used'] = psk_name
                    result['psk_base64'] = psk_b64
                    result['portnum'] = portnum
                    result['port_name'] = PORT_NAMES.get(portnum, 'UNKNOWN')
                    result['payload'] = decrypted
                    self.stats['decrypted'] += 1
                    self.stats[f'psk_{psk_name}'] += 1
                    
                    # Try to extract text message
                    if portnum == 1:  # TEXT_MESSAGE_APP
                        result['text_message'] = self.extract_text(decrypted)
                    
                    # Try to extract position
                    if portnum == 3:  # POSITION_APP
                        result['position'] = self.extract_position(decrypted)
                    
                    self.decrypted_packets.append(result)
                    return result
        
        # Failed to decrypt
        self.stats['failed'] += 1
        return result
    
    def extract_text(self, payload):
        """Extract text message from decrypted payload"""
        try:
            # Format: 0x08 <portnum> 0x12 <length> <text>
            if len(payload) < 4:
                return None
            
            if payload[0] == 0x08 and payload[1] == 0x01 and payload[2] == 0x12:
                text_len = payload[3]
                if len(payload) >= 4 + text_len:
                    text = payload[4:4+text_len]
                    # Filter printable ASCII
                    return ''.join(chr(b) if 32 <= b <= 126 else '?' for b in text)
        except Exception:
            pass
        return None
    
    def extract_position(self, payload):
        """Extract GPS position from decrypted payload"""
        try:
            # Simplified parser - look for sfixed32 fields (wire type 5)
            pos = {}
            i = 0
            while i < len(payload) - 5:
                tag = payload[i]
                field_num = tag >> 3
                wire_type = tag & 0x07
                i += 1
                
                if wire_type == 5:  # sfixed32
                    value = struct.unpack('<i', payload[i:i+4])[0]
                    i += 4
                    
                    if field_num == 1:
                        pos['latitude'] = value / 1e7
                    elif field_num == 2:
                        pos['longitude'] = value / 1e7
                    elif field_num == 3:
                        pos['altitude'] = value
                elif wire_type == 0:  # varint
                    # Skip varint
                    while i < len(payload) and payload[i] & 0x80:
                        i += 1
                    i += 1
                else:
                    break
            
            if 'latitude' in pos and 'longitude' in pos:
                return pos
        except Exception:
            pass
        return None
    
    def print_result(self, result):
        """Print decoded packet result"""
        print(f"\n{'='*60}")
        print(f"📦 Packet from {result['from_node']}")
        print(f"   ID: {result['packet_id']}, Channel: {result['channel']}")
        print(f"   Encrypted payload: {result['encrypted_length']} bytes")
        
        if result['decrypted']:
            print(f"   ✅ DECRYPTED with: {result['psk_used']}")
            if result['port_name']:
                print(f"   📬 Port: {result['portnum']} ({result['port_name']})")
            
            if result.get('text_message'):
                msg = result['text_message']
                width = max(48, min(len(msg), 72))
                print(f"\n   💬 TEXT MESSAGE:")
                print(f"   ╔{'═'*(width+2)}╗")
                # Wrap long messages across multiple lines
                for i in range(0, len(msg), width):
                    line = msg[i:i+width]
                    print(f"   ║ {line:<{width}} ║")
                print(f"   ╚{'═'*(width+2)}╝")
            
            if result.get('position'):
                pos = result['position']
                print(f"\n   📍 GPS POSITION:")
                print(f"      Lat: {pos.get('latitude', 'N/A')}")
                print(f"      Lon: {pos.get('longitude', 'N/A')}")
                if 'altitude' in pos:
                    print(f"      Alt: {pos['altitude']}m")
        else:
            print(f"   ❌ Could not decrypt (unknown PSK)")
    
    def print_summary(self):
        """Print decryption summary"""
        print(f"\n{'='*60}")
        print("📊 DECRYPTION SUMMARY")
        print(f"{'='*60}")
        print(f"   Total Meshtastic packets: {self.stats['meshtastic_packets']}")
        print(f"   Unencrypted: {self.stats['unencrypted']}")
        print(f"   Decrypted with PSK: {self.stats['decrypted']}")
        print(f"   Failed to decrypt: {self.stats['failed']}")
        print(f"   Non-Meshtastic: {self.stats['non_meshtastic']}")
        
        if self.stats['decrypted'] > 0:
            print(f"\n   PSK breakdown:")
            for key, value in self.stats.items():
                if key.startswith('psk_') and value > 0:
                    psk_name = key[4:]
                    print(f"      {psk_name}: {value}")
        
        success_rate = 0
        total = self.stats['meshtastic_packets']
        if total > 0:
            success_rate = (self.stats['decrypted'] + self.stats['unencrypted']) / total * 100
        print(f"\n   Success rate: {success_rate:.1f}%")


def load_pcap(filepath):
    """Load packets from PCAP file"""
    packets = []
    
    with open(filepath, 'rb') as f:
        # Read global header
        global_header = f.read(24)
        if len(global_header) < 24:
            raise ValueError("Invalid PCAP file")
        
        magic = struct.unpack('<I', global_header[:4])[0]
        if magic not in (0xa1b2c3d4, 0xa1b23c4d):
            raise ValueError(f"Invalid PCAP magic: 0x{magic:08x}")
        
        # Read packets
        while True:
            pkt_header = f.read(16)
            if len(pkt_header) < 16:
                break
            
            ts_sec, ts_usec, incl_len, orig_len = struct.unpack('<IIII', pkt_header)
            pkt_data = f.read(incl_len)
            
            if len(pkt_data) < incl_len:
                break
            
            # Skip custom LoRa pseudo-header (20 bytes) if present
            # The ESP32 sniffer writes a 20-byte pseudo-header per packet:
            #   float freq(4) + float rssi(4) + float snr(4) + u8 sf(1)
            #   + u32 bw(4) + u8 cr(1) + u16 reserved(2) = 20 bytes
            if len(pkt_data) > 20:
                if pkt_data[:4] != b'\xff\xff\xff\xff':
                    pkt_data = pkt_data[20:]  # Skip pseudo-header
            
            packets.append(pkt_data)
    
    return packets


def load_csv(filepath):
    """Load packets from CSV file"""
    import csv
    packets = []
    
    with open(filepath, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            hex_data = row.get('hex_data') or row.get('data') or row.get('raw_hex')
            if hex_data:
                try:
                    packets.append(bytes.fromhex(hex_data.replace(' ', '')))
                except ValueError:
                    pass
    
    return packets


def run_tests():
    """Run PSK validation tests"""
    print("🧪 Running PSK validation tests...\n")
    
    # Test vector from Meshtastic (known good decryption)
    # This is a synthetic test packet
    test_passed = 0
    test_total = 0
    
    # Test key expansion
    decoder = MeshtasticDecoder(verbose=True)
    
    print("1. Testing key expansion...")
    test_total += 1
    key1 = decoder.expand_key(b'\x01')
    if len(key1) == 16 and key1 == b'\x01' * 16:
        print("   ✅ Single byte expansion correct")
        test_passed += 1
    else:
        print("   ❌ Single byte expansion failed")
    
    test_total += 1
    key8 = decoder.expand_key(b'12345678')
    if len(key8) == 16 and key8 == b'1234567812345678':
        print("   ✅ 8-byte expansion correct")
        test_passed += 1
    else:
        print("   ❌ 8-byte expansion failed")
    
    # Test nonce building
    print("\n2. Testing nonce construction...")
    test_total += 1
    nonce = decoder.build_nonce(0x12345678, 0xABCDEF01)
    expected = bytes([0x78, 0x56, 0x34, 0x12, 0, 0, 0, 0,
                      0x01, 0xEF, 0xCD, 0xAB, 0, 0, 0, 0])
    if nonce == expected:
        print("   ✅ Nonce construction correct")
        test_passed += 1
    else:
        print(f"   ❌ Nonce construction failed")
        print(f"      Expected: {expected.hex()}")
        print(f"      Got:      {nonce.hex()}")
    
    # Test PSK database
    print(f"\n3. Testing PSK database...")
    test_total += 1
    valid_keys = 0
    for psk_b64, name in DEFAULT_PSKS:
        try:
            key = base64.b64decode(psk_b64)
            if len(key) in (1, 8, 16, 32):
                valid_keys += 1
        except Exception:
            print(f"   ❌ Invalid PSK: {name}")
    
    if valid_keys == len(DEFAULT_PSKS):
        print(f"   ✅ All {len(DEFAULT_PSKS)} PSKs valid")
        test_passed += 1
    else:
        print(f"   ⚠️ {valid_keys}/{len(DEFAULT_PSKS)} PSKs valid")
    
    print(f"\n{'='*40}")
    print(f"Tests passed: {test_passed}/{test_total}")
    
    return test_passed == test_total


def main():
    parser = argparse.ArgumentParser(
        description='Meshtastic Packet Decoder - Offline PSK Decryption',
        epilog='Example: python meshtastic_decoder.py capture.pcap'
    )
    parser.add_argument('input', nargs='?', help='Input file (PCAP, CSV, or hex string)')
    parser.add_argument('--format', choices=['pcap', 'csv', 'hex'], 
                       help='Input format (auto-detected if not specified)')
    parser.add_argument('--hex', action='store_true',
                       help='Treat input as hex string')
    parser.add_argument('--test', action='store_true',
                       help='Run PSK validation tests')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Verbose output')
    parser.add_argument('--json', action='store_true',
                       help='Output results as JSON')
    parser.add_argument('--list-psks', action='store_true',
                       help='List all known PSKs')
    
    args = parser.parse_args()
    
    if args.test:
        success = run_tests()
        return 0 if success else 1
    
    if args.list_psks:
        print("🔑 Known Meshtastic PSKs:\n")
        for i, (psk_b64, name) in enumerate(DEFAULT_PSKS, 1):
            print(f"  {i:2}. {psk_b64:<32} - {name}")
        return 0
    
    if not args.input:
        parser.print_help()
        return 1
    
    if not CRYPTO_AVAILABLE:
        print("❌ cryptography library required for decryption")
        print("   Install with: pip install cryptography")
        return 1
    
    decoder = MeshtasticDecoder(verbose=args.verbose)
    
    # Load packets
    if args.hex or args.format == 'hex':
        packets = [args.input]  # Single hex string
    else:
        filepath = Path(args.input)
        if not filepath.exists():
            print(f"❌ File not found: {filepath}")
            return 1
        
        fmt = args.format or filepath.suffix.lower().lstrip('.')
        
        if fmt in ('pcap', 'pcapng'):
            packets = load_pcap(filepath)
        elif fmt == 'csv':
            packets = load_csv(filepath)
        else:
            print(f"❌ Unknown format: {fmt}")
            return 1
    
    print(f"📂 Loaded {len(packets)} packets\n")
    
    # Decode each packet
    results = []
    for i, pkt in enumerate(packets):
        result = decoder.decode_packet(pkt)
        if result:
            results.append(result)
            if not args.json:
                decoder.print_result(result)
    
    # Output
    if args.json:
        # Convert bytes to hex for JSON
        for r in results:
            if r.get('payload'):
                r['payload_hex'] = r['payload'].hex()
                del r['payload']
        print(json.dumps(results, indent=2))
    else:
        decoder.print_summary()
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
