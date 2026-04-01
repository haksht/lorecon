#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - Meshtastic PSK Decryption Module

Python implementation of PSK testing for Meshtastic packets.
Matches the 23 default keys used by the firmware.

Usage as module:
    from psk_decrypt import MeshtasticDecryptor
    
    decryptor = MeshtasticDecryptor()
    result = decryptor.try_decrypt(raw_packet_bytes)
    if result:
        print(f"Decrypted with key #{result['key_index']}: {result['plaintext'].hex()}")

Usage as CLI:
    python psk_decrypt.py packet.bin
    python psk_decrypt.py --hex "ffffffff01234567..."
    echo "ffffffff..." | python psk_decrypt.py --stdin

Algorithm:
    1. Parse 16-byte Meshtastic header (4-byte magic, nodeId, packetId, flags, channel)
    2. Build AES-CTR nonce from packetId and nodeId
    3. Try each of 23 default PSKs (expanded to 128/256-bit keys)
    4. Validate decryption by checking for valid protobuf structure
"""

import argparse
import base64
import struct
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
from typing import Dict, List, Optional, Tuple, NamedTuple
from dataclasses import dataclass

# Cryptographic imports
try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    from cryptography.hazmat.backends import default_backend
    CRYPTO_AVAILABLE = True
except ImportError:
    CRYPTO_AVAILABLE = False
    print("Warning: cryptography module not installed. Install with: pip install cryptography")


# ============================================================================
# Default PSKs - Same 23 keys as firmware
# ============================================================================

DEFAULT_PSKS = [
    # === Standard defaults (most common) ===
    "AQ==",                              # 1: Single byte 0x01 - THE classic default
    "1PG7OiApB1nwvP+rz05pAQ==",          # 2: Default public channel key (LongFast)
    
    # === Legacy single-byte keys (pre-2.0, expanded to 16 bytes) ===
    "Ag==",                              # 3: Single byte 0x02
    "Aw==",                              # 4: Single byte 0x03
    "BA==",                              # 5: Single byte 0x04
    "BQ==",                              # 6: Single byte 0x05
    "Bg==",                              # 7: Single byte 0x06
    "Bw==",                              # 8: Single byte 0x07
    "CA==",                              # 9: Single byte 0x08
    "CQ==",                              # 10: Single byte 0x09
    
    # === Test/development keys often left in production ===
    "AAAAAAAAAAAAAAAAAAAAAA==",          # 11: All zeros (16 bytes)
    "MTIzNDU2Nzg5MDEyMzQ1Ng==",          # 12: "1234567890123456" ASCII
    "dGVzdHRlc3R0ZXN0dGVzdA==",          # 13: "testtesttesttest" ASCII
    "bWVzaHRhc3RpY21lc2h0YXN0",          # 14: "meshtasticmeshtast" ASCII
    
    # === Historic defaults from older firmware ===
    "PKdTs51e4EB0BoOevIN0Dw==",          # 15: Admin channel default (pre-2.5)
    "shmLkA9H74gAeLH3eGCqsw==",          # 16: Secondary channel default
    "ogDPnKVRN7wz/VF8nt6LkA==",          # 17: Debug/dev key from firmware source
    "ZQ+HdKKbbAU4dSCGt66Qqw==",          # 18: EU regional default
    
    # === Channel preset derived keys ===
    "d1iq21lNSh7BP6MOkP6cQA==",          # 19: MediumFast preset
    "/u7k03L8N3Q=",                      # 20: ShortFast preset (8 bytes)
    "GGC5DDnv8FKFm7WCZ5rXBA==",          # 21: LongSlow preset
    "LHrwq5nxPIJlqFU/K5dKKQ==",          # 22: MediumSlow preset
    "sb6GxC62sdwGXxJz2sERuQ==",          # 23: ShortSlow preset
]

# Key descriptions for reporting
KEY_DESCRIPTIONS = [
    "Default 0x01 (classic)",
    "LongFast public channel",
    "Legacy 0x02",
    "Legacy 0x03", 
    "Legacy 0x04",
    "Legacy 0x05",
    "Legacy 0x06",
    "Legacy 0x07",
    "Legacy 0x08",
    "Legacy 0x09",
    "All zeros (dev)",
    "1234567890123456",
    "testtesttesttest",
    "meshtasticmeshtast",
    "Admin channel (legacy)",
    "Secondary channel default",
    "Debug key (source)",
    "EU regional default",
    "MediumFast preset",
    "ShortFast preset",
    "LongSlow preset",
    "MediumSlow preset",
    "ShortSlow preset",
]


# Meshtastic portnum values
PORTNUM_NAMES = {
    0x01: "TEXT_MESSAGE",
    0x03: "POSITION",
    0x04: "NODEINFO",
    0x07: "ADMIN",
    0x08: "TELEMETRY",
    0x09: "TEXT_COMPRESSED",
    0x42: "TRACEROUTE",
    0x43: "NEIGHBORINFO",
    0x44: "MAP_REPORT",
}


@dataclass
class DecryptResult:
    """Result of a successful decryption attempt"""
    key_index: int
    key_name: str
    plaintext: bytes
    node_id: int
    packet_id: int
    flags: int
    channel: int
    portnum: Optional[int] = None
    portnum_name: Optional[str] = None


@dataclass
class PacketHeader:
    """Parsed Meshtastic packet header"""
    dest: int      # Destination node ID (4 bytes)
    source: int    # Source node ID (4 bytes) 
    packet_id: int # Packet ID (4 bytes)
    flags: int     # Flags byte
    channel: int   # Channel index
    next_hop: int  # Next hop for routing
    relay: int     # Relay node
    payload: bytes # Encrypted/plaintext payload


class MeshtasticDecryptor:
    """
    Decrypt Meshtastic packets using default PSKs.
    
    Implements the same algorithm as the ESP32 firmware:
    1. Parse 16-byte header after 4-byte magic
    2. Build AES-CTR nonce from packet_id and source node_id
    3. Try each PSK, validate with protobuf structure check
    """
    
    MAGIC = b'\xff\xff\xff\xff'
    HEADER_SIZE = 16
    MIN_PACKET_SIZE = 20  # Magic (4) + Header (16) minimum
    
    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.stats = {
            'attempts': 0,
            'successes': 0,
            'key_hits': [0] * len(DEFAULT_PSKS),
        }
        
        if not CRYPTO_AVAILABLE:
            raise RuntimeError("cryptography module required. Install with: pip install cryptography")
        
        # Pre-decode and expand all keys
        self._keys = self._prepare_keys()
    
    def _prepare_keys(self) -> List[Tuple[bytes, int]]:
        """Decode base64 keys and expand to proper AES key sizes"""
        keys = []
        for psk_b64 in DEFAULT_PSKS:
            try:
                key_raw = base64.b64decode(psk_b64)
                key_len = len(key_raw)
                
                if key_len == 1:
                    # Single byte: expand to 16 bytes by repeating
                    expanded = bytes([key_raw[0]] * 16)
                    keys.append((expanded, 128))
                elif key_len == 8:
                    # 8-byte key: duplicate to fill 16 bytes
                    expanded = key_raw + key_raw
                    keys.append((expanded, 128))
                elif key_len == 16:
                    keys.append((key_raw, 128))
                elif key_len == 32:
                    keys.append((key_raw, 256))
                else:
                    keys.append((None, 0))  # Invalid key
            except Exception:
                keys.append((None, 0))
        
        return keys
    
    def parse_header(self, data: bytes) -> Optional[PacketHeader]:
        """
        Parse Meshtastic packet header.
        
        Packet structure (after 4-byte magic 0xFFFFFFFF):
        Bytes 0-3:   Destination node ID (little-endian)
        Bytes 4-7:   Source node ID (little-endian)
        Bytes 8-11:  Packet ID (little-endian)
        Byte 12:     Flags
        Byte 13:     Channel index
        Byte 14:     Next hop
        Byte 15:     Relay node
        Bytes 16+:   Payload (encrypted or plaintext)
        """
        # Find magic header
        magic_pos = data.find(self.MAGIC)
        if magic_pos == -1:
            return None
        
        # Adjust to start after magic
        data = data[magic_pos + 4:]
        
        if len(data) < self.HEADER_SIZE:
            return None
        
        # Parse header fields (all little-endian)
        dest = struct.unpack('<I', data[0:4])[0]
        source = struct.unpack('<I', data[4:8])[0]
        packet_id = struct.unpack('<I', data[8:12])[0]
        flags = data[12]
        channel = data[13]
        next_hop = data[14]
        relay = data[15]
        payload = data[16:]
        
        return PacketHeader(
            dest=dest,
            source=source,
            packet_id=packet_id,
            flags=flags,
            channel=channel,
            next_hop=next_hop,
            relay=relay,
            payload=payload
        )
    
    def _build_nonce(self, packet_id: int, source: int) -> bytes:
        """
        Build AES-CTR nonce from packet ID and source node ID.
        
        Nonce structure (16 bytes):
        Bytes 0-3:   Packet ID (little-endian)
        Bytes 4-7:   Zeros
        Bytes 8-11:  Source node ID (little-endian)
        Bytes 12-15: Zeros
        """
        nonce = bytearray(16)
        
        # Packet ID at bytes 0-3 (little-endian)
        nonce[0] = packet_id & 0xFF
        nonce[1] = (packet_id >> 8) & 0xFF
        nonce[2] = (packet_id >> 16) & 0xFF
        nonce[3] = (packet_id >> 24) & 0xFF
        
        # Source node ID at bytes 8-11 (little-endian)
        nonce[8] = source & 0xFF
        nonce[9] = (source >> 8) & 0xFF
        nonce[10] = (source >> 16) & 0xFF
        nonce[11] = (source >> 24) & 0xFF
        
        return bytes(nonce)
    
    def _validate_decryption(self, plaintext: bytes) -> bool:
        """
        Validate decryption by checking for valid protobuf structure.
        
        Valid Meshtastic Data messages start with:
        - 0x08 (field 1, varint) = portnum
        - 0x12 (field 2, length-delimited) = payload
        - 0x18 (field 3, varint) = source
        - 0x20 (field 4, varint) = dest
        """
        if len(plaintext) < 2:
            return False
        
        first_byte = plaintext[0]
        wire_type = first_byte & 0x07
        field_num = first_byte >> 3
        
        # Wire types 6 and 7 are invalid
        if wire_type in (6, 7):
            return False
        
        # Field numbers should be reasonable (1-31 common in Meshtastic)
        if field_num < 1 or field_num > 31:
            return False
        
        # Strong indicators of valid protobuf
        if first_byte in (0x08, 0x10, 0x12, 0x18, 0x20):
            return True
        
        return False
    
    def _is_unencrypted(self, payload: bytes) -> bool:
        """Check if payload appears to be unencrypted (valid protobuf already)"""
        if len(payload) < 2:
            return False
        
        # 0x08 followed by valid portnum indicates unencrypted
        if payload[0] == 0x08:
            portnum = payload[1]
            if portnum in PORTNUM_NAMES:
                return True
        
        return False
    
    def try_decrypt(self, data: bytes) -> Optional[DecryptResult]:
        """
        Attempt to decrypt a Meshtastic packet using all default PSKs.
        
        Args:
            data: Raw packet bytes (with or without magic header)
            
        Returns:
            DecryptResult if successful, None otherwise
        """
        self.stats['attempts'] += 1
        
        # Parse header
        header = self.parse_header(data)
        if header is None:
            if self.verbose:
                print("[PSK] No valid Meshtastic header found")
            return None
        
        payload = header.payload
        if len(payload) < 6 or len(payload) > 256:
            if self.verbose:
                print(f"[PSK] Invalid payload length: {len(payload)}")
            return None
        
        # Check if already unencrypted
        if self._is_unencrypted(payload):
            if self.verbose:
                print("[PSK] Packet appears unencrypted")
            portnum = payload[1] if len(payload) > 1 else None
            return DecryptResult(
                key_index=0,
                key_name="Unencrypted",
                plaintext=payload,
                node_id=header.source,
                packet_id=header.packet_id,
                flags=header.flags,
                channel=header.channel,
                portnum=portnum,
                portnum_name=PORTNUM_NAMES.get(portnum, "UNKNOWN") if portnum else None
            )
        
        # Build nonce
        nonce = self._build_nonce(header.packet_id, header.source)
        
        # Try each PSK
        for i, (key, key_bits) in enumerate(self._keys):
            if key is None:
                continue
            
            try:
                # AES-CTR decryption
                cipher = Cipher(
                    algorithms.AES(key),
                    modes.CTR(nonce),
                    backend=default_backend()
                )
                decryptor = cipher.decryptor()
                plaintext = decryptor.update(payload) + decryptor.finalize()
                
                # Validate
                if self._validate_decryption(plaintext):
                    self.stats['successes'] += 1
                    self.stats['key_hits'][i] += 1
                    
                    portnum = plaintext[1] if len(plaintext) > 1 and plaintext[0] == 0x08 else None
                    
                    if self.verbose:
                        print(f"[PSK] ✓ Decrypted with key #{i+1}: {KEY_DESCRIPTIONS[i]}")
                    
                    return DecryptResult(
                        key_index=i + 1,
                        key_name=KEY_DESCRIPTIONS[i],
                        plaintext=plaintext,
                        node_id=header.source,
                        packet_id=header.packet_id,
                        flags=header.flags,
                        channel=header.channel,
                        portnum=portnum,
                        portnum_name=PORTNUM_NAMES.get(portnum, "UNKNOWN") if portnum else None
                    )
            
            except Exception as e:
                if self.verbose:
                    print(f"[PSK] Key #{i+1} failed: {e}")
                continue
        
        if self.verbose:
            print("[PSK] No key matched")
        return None
    
    def get_stats(self) -> Dict:
        """Get decryption statistics"""
        return {
            'attempts': self.stats['attempts'],
            'successes': self.stats['successes'],
            'success_rate': self.stats['successes'] / max(1, self.stats['attempts']) * 100,
            'key_hits': {
                KEY_DESCRIPTIONS[i]: self.stats['key_hits'][i]
                for i in range(len(DEFAULT_PSKS))
                if self.stats['key_hits'][i] > 0
            }
        }


def main():
    """CLI interface for PSK decryption"""
    parser = argparse.ArgumentParser(
        description='Decrypt Meshtastic packets using default PSKs',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
    %(prog)s packet.bin                    Decrypt from binary file
    %(prog)s --hex "ffffffff01234567..."   Decrypt hex string
    echo "ffffffff..." | %(prog)s --stdin  Read from stdin
    %(prog)s --list-keys                   Show all default PSKs

Known Default Keys (23 total):
    - Single byte defaults (0x01-0x09)
    - Channel preset keys (LongFast, MediumSlow, etc.)
    - Test/dev keys (zeros, "test", "1234...")
    - Legacy admin channel defaults
'''
    )
    
    parser.add_argument('input', nargs='?', help='Input file (binary packet)')
    parser.add_argument('--hex', metavar='HEX', help='Hex string to decrypt')
    parser.add_argument('--stdin', action='store_true', help='Read hex from stdin')
    parser.add_argument('--list-keys', action='store_true', help='List all default PSKs')
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
    
    args = parser.parse_args()
    
    if args.list_keys:
        print("Default PSKs (23 keys):")
        print("=" * 60)
        for i, (psk, desc) in enumerate(zip(DEFAULT_PSKS, KEY_DESCRIPTIONS)):
            key_bytes = base64.b64decode(psk)
            print(f"  #{i+1:2d}: {desc}")
            print(f"       Base64: {psk}")
            print(f"       Hex:    {key_bytes.hex()}")
            print()
        return 0
    
    # Get packet data
    data = None
    
    if args.hex:
        try:
            data = bytes.fromhex(args.hex.replace(' ', '').replace(':', ''))
        except ValueError as e:
            print(f"Error: Invalid hex string: {e}")
            return 1
    elif args.stdin:
        hex_str = sys.stdin.read().strip()
        try:
            data = bytes.fromhex(hex_str.replace(' ', '').replace(':', ''))
        except ValueError as e:
            print(f"Error: Invalid hex from stdin: {e}")
            return 1
    elif args.input:
        try:
            with open(args.input, 'rb') as f:
                data = f.read()
        except Exception as e:
            print(f"Error: Could not read file: {e}")
            return 1
    else:
        parser.print_help()
        return 1
    
    if not CRYPTO_AVAILABLE:
        print("Error: cryptography module required")
        print("Install with: pip install cryptography")
        return 1
    
    # Decrypt
    decryptor = MeshtasticDecryptor(verbose=args.verbose)
    result = decryptor.try_decrypt(data)
    
    if result:
        print(f"\n{'='*60}")
        print(f"✓ DECRYPTION SUCCESSFUL")
        print(f"{'='*60}")
        print(f"  Key:        #{result.key_index} - {result.key_name}")
        print(f"  Node ID:    0x{result.node_id:08X}")
        print(f"  Packet ID:  0x{result.packet_id:08X}")
        print(f"  Flags:      0x{result.flags:02X}")
        print(f"  Channel:    {result.channel}")
        if result.portnum:
            print(f"  Portnum:    {result.portnum} ({result.portnum_name})")
        print(f"\n  Plaintext ({len(result.plaintext)} bytes):")
        
        # Pretty hex dump
        for i in range(0, len(result.plaintext), 16):
            chunk = result.plaintext[i:i+16]
            hex_part = ' '.join(f'{b:02x}' for b in chunk)
            ascii_part = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)
            print(f"    {i:04x}: {hex_part:<48} {ascii_part}")
        
        return 0
    else:
        print("\n✗ Decryption failed - no default PSK matched")
        print("  This packet may use a custom PSK or be corrupted.")
        return 1


if __name__ == '__main__':
    sys.exit(main())
