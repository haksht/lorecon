#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - PSK Vulnerability Auditor

Scans captured traffic for networks using weak/default PSKs.
Generates vulnerability reports with risk assessment.

Usage:
    python psk_auditor.py capture.pcap
    python psk_auditor.py capture.csv --verbose
    python psk_auditor.py --live 192.168.4.1  # Monitor live from ESP32

Requirements:
    pip install cryptography requests
"""

import argparse
import base64
import json
import struct
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
from collections import defaultdict
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    from cryptography.hazmat.backends import default_backend
    CRYPTO_AVAILABLE = True
except ImportError:
    CRYPTO_AVAILABLE = False

try:
    import requests
    REQUESTS_AVAILABLE = True
except ImportError:
    REQUESTS_AVAILABLE = False

# PSK Risk Database
# Format: (base64, name, risk_level, description)
PSK_DATABASE = [
    # CRITICAL - Widely known, actively exploited
    ("AQ==", "Default (0x01)", "CRITICAL", 
     "Factory default key used by most Meshtastic installations"),
    ("PKdTs51e4EB0BoOevIN0Dw==", "Admin Channel (pre-2.5)", "CRITICAL",
     "Legacy admin key - allows remote device configuration"),
    
    # HIGH - Common defaults, easy to find
    ("1PG7OiApB1nwvP+rz05pAQ==", "LongFast Preset", "HIGH",
     "Common channel preset default"),
    ("AAAAAAAAAAAAAAAAAAAAAA==", "All Zeros", "HIGH",
     "Trivial test key sometimes left in production"),
    ("MTIzNDU2Nzg5MDEyMzQ1Ng==", "1234567890123456", "HIGH",
     "Common weak password pattern"),
    
    # MEDIUM - Legacy or regional defaults
    ("Ag==", "Legacy 0x02", "MEDIUM", "Old single-byte key format"),
    ("Aw==", "Legacy 0x03", "MEDIUM", "Old single-byte key format"),
    ("BA==", "Legacy 0x04", "MEDIUM", "Old single-byte key format"),
    ("BQ==", "Legacy 0x05", "MEDIUM", "Old single-byte key format"),
    ("Bg==", "Legacy 0x06", "MEDIUM", "Old single-byte key format"),
    ("Bw==", "Legacy 0x07", "MEDIUM", "Old single-byte key format"),
    ("CA==", "Legacy 0x08", "MEDIUM", "Old single-byte key format"),
    ("CQ==", "Legacy 0x09", "MEDIUM", "Old single-byte key format"),
    ("ZQ+HdKKbbAU4dSCGt66Qqw==", "EU868 Regional", "MEDIUM",
     "European regional default"),
    
    # LOW - Test keys, unlikely in production
    ("dGVzdHRlc3R0ZXN0dGVzdA==", "testtesttesttest", "LOW",
     "Development test key"),
    ("bWVzaHRhc3RpY21lc2h0YXN0", "meshtastic...", "LOW",
     "Development test key"),
    ("shmLkA9H74gAeLH3eGCqsw==", "Secondary Default", "LOW",
     "Secondary channel default"),
    ("ogDPnKVRN7wz/VF8nt6LkA==", "Debug Key", "LOW",
     "Debug/dev key from firmware source"),
    
    # Preset keys
    ("d1iq21lNSh7BP6MOkP6cQA==", "MediumFast", "MEDIUM", "Channel preset"),
    ("/u7k03L8N3Q=", "ShortFast", "MEDIUM", "Channel preset"),
    ("GGC5DDnv8FKFm7WCZ5rXBA==", "LongSlow", "MEDIUM", "Channel preset"),
    ("LHrwq5nxPIJlqFU/K5dKKQ==", "MediumSlow", "MEDIUM", "Channel preset"),
    ("sb6GxC62sdwGXxJz2sERuQ==", "ShortSlow", "MEDIUM", "Channel preset"),
]

# Risk level colors for terminal output
RISK_COLORS = {
    'CRITICAL': '\033[91m',  # Red
    'HIGH': '\033[93m',      # Yellow
    'MEDIUM': '\033[94m',    # Blue
    'LOW': '\033[90m',       # Gray
    'RESET': '\033[0m',
}


class PSKAuditor:
    """Audit captured traffic for weak PSK usage"""
    
    def __init__(self, verbose=False, no_color=False):
        self.verbose = verbose
        self.no_color = no_color
        self.vulnerable_networks = defaultdict(lambda: {
            'psk_name': None,
            'psk_risk': None,
            'psk_description': None,
            'packet_count': 0,
            'devices': set(),
            'first_seen': None,
            'last_seen': None,
            'sample_data': [],
        })
        self.total_tested = 0
        self.total_vulnerable = 0   # sensitive portnums (text, admin)
        self.informational = 0      # decryptable but expected public data
        self.unencrypted = 0
        self.failed = 0
        self.text_messages = []     # (node_id_hex, psk_name, text)
    
    def _color(self, text, color_key):
        """Apply terminal color if enabled"""
        if self.no_color:
            return text
        return f"{RISK_COLORS.get(color_key, '')}{text}{RISK_COLORS['RESET']}"
    
    def expand_key(self, key_bytes):
        """Expand short keys to 16 bytes"""
        if len(key_bytes) == 16 or len(key_bytes) == 32:
            return key_bytes
        if len(key_bytes) == 1:
            return key_bytes * 16
        if len(key_bytes) == 8:
            return key_bytes * 2
        return key_bytes.ljust(16, b'\x00')
    
    def build_nonce(self, packet_id, node_id):
        """Build AES-CTR nonce"""
        nonce = bytearray(16)
        struct.pack_into('<I', nonce, 0, packet_id)
        struct.pack_into('<I', nonce, 8, node_id)
        return bytes(nonce)
    
    # Portnums where default-PSK decryption is a real security finding
    SENSITIVE_PORTNUMS = {
        1,   # TEXT_MESSAGE_APP  - private messages
        2,   # REMOTE_HARDWARE_APP - device control
        67,  # ADMIN_APP - remote configuration
    }

    def try_decrypt(self, encrypted, key_bytes, packet_id, node_id):
        """Attempt decryption and validate result. Returns (portnum, plaintext) or None."""
        if not CRYPTO_AVAILABLE:
            return None

        try:
            expanded = self.expand_key(key_bytes)
            nonce = self.build_nonce(packet_id, node_id)

            cipher = Cipher(
                algorithms.AES(expanded),
                modes.CTR(nonce),
                backend=default_backend()
            )
            decryptor = cipher.decryptor()
            decrypted = decryptor.update(encrypted) + decryptor.finalize()

            # Validate: check for protobuf field 1 (portnum)
            if len(decrypted) >= 2 and decrypted[0] == 0x08:
                portnum = decrypted[1]
                # Known Meshtastic portnums
                if portnum in (0, 1, 2, 3, 4, 5, 6, 7, 8, 32, 33, 34, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73):
                    return portnum, decrypted

            return None
        except Exception:
            return None
    
    def audit_packet(self, raw_data, timestamp=None):
        """Audit a single packet for PSK vulnerabilities"""
        if isinstance(raw_data, str):
            raw_data = bytes.fromhex(raw_data.replace(' ', '').replace('0x', ''))
        
        # Minimum Meshtastic packet length (16-byte header + at least some payload)
        if len(raw_data) < 20:
            return None
        
        self.total_tested += 1
        
        # Parse header
        node_id = struct.unpack('<I', raw_data[4:8])[0]
        packet_id = struct.unpack('<I', raw_data[8:12])[0]
        encrypted = raw_data[16:]
        
        # Check if unencrypted
        if len(encrypted) >= 2 and encrypted[0] == 0x08:
            portnum = encrypted[1]
            if portnum in (0, 1, 2, 3, 4, 5, 6, 7, 8):
                self.unencrypted += 1
                return {
                    'node_id': node_id,
                    'vulnerable': True,
                    'risk': 'CRITICAL',
                    'psk_name': 'UNENCRYPTED',
                    'description': 'Traffic sent without any encryption',
                }
        
        # Try each PSK
        for psk_b64, psk_name, risk, description in PSK_DATABASE:
            try:
                key_bytes = base64.b64decode(psk_b64)
            except Exception:
                continue

            decrypt_result = self.try_decrypt(encrypted, key_bytes, packet_id, node_id)
            if decrypt_result is None:
                continue
            portnum, plaintext = decrypt_result

            is_sensitive = portnum in self.SENSITIVE_PORTNUMS

            if is_sensitive:
                self.total_vulnerable += 1
            else:
                self.informational += 1

            # Record finding
            net_key = psk_name
            net = self.vulnerable_networks[net_key]
            net['psk_name'] = psk_name
            net['psk_risk'] = risk
            net['psk_description'] = description
            net['packet_count'] += 1
            net['sensitive_count'] = net.get('sensitive_count', 0) + (1 if is_sensitive else 0)
            net['devices'].add(f"0x{node_id:08X}")

            if timestamp:
                if net['first_seen'] is None:
                    net['first_seen'] = timestamp
                net['last_seen'] = timestamp

            if len(net['sample_data']) < 3:
                net['sample_data'].append(raw_data[:48].hex())

            # Extract text messages (portnum 1 = TEXT_MESSAGE_APP)
            # Wire format: 0x08 0x01 0x12 <len> <text>
            if portnum == 1 and len(plaintext) >= 4 and plaintext[2] == 0x12:
                tlen = plaintext[3]
                if len(plaintext) >= 4 + tlen:
                    text = plaintext[4:4+tlen].decode('utf-8', errors='replace').strip()
                    if text:
                        ts_str = datetime.fromtimestamp(timestamp).strftime('%H:%M:%S') if timestamp else '??:??:??'
                        self.text_messages.append((f"0x{node_id:08X}", ts_str, psk_name, text))

            return {
                'node_id': node_id,
                'vulnerable': is_sensitive,
                'portnum': portnum,
                'risk': risk if is_sensitive else 'INFO',
                'psk_name': psk_name,
                'description': description,
            }
        
        self.failed += 1
        return {
            'node_id': node_id,
            'vulnerable': False,
            'risk': None,
            'psk_name': None,
        }
    
    def print_results(self):
        """Print audit results"""
        print("\n" + "="*70)
        print("🔐 PSK VULNERABILITY AUDIT RESULTS")
        print("="*70)
        
        print(f"\n📊 Summary:")
        print(f"   Total packets tested:   {self.total_tested}")
        print(f"   Sensitive (text/admin): {self.total_vulnerable}  ← real findings")
        print(f"   Informational (pos/nodeinfo/telemetry): {self.informational}  ← expected public data")
        print(f"   Unencrypted:            {self.unencrypted}")
        print(f"   Unknown PSK:            {self.failed}")

        if self.total_tested > 0:
            sensitive_pct = (self.total_vulnerable + self.unencrypted) / self.total_tested * 100
            print(f"   Sensitive exposure rate: {sensitive_pct:.1f}%")
        
        if not self.vulnerable_networks:
            print("\n✅ No vulnerable networks detected")
            return
        
        print(f"\n⚠️  VULNERABLE NETWORKS FOUND: {len(self.vulnerable_networks)}\n")
        
        # Sort by risk level
        risk_order = ['CRITICAL', 'HIGH', 'MEDIUM', 'LOW']
        sorted_networks = sorted(
            self.vulnerable_networks.items(),
            key=lambda x: (risk_order.index(x[1]['psk_risk']), -x[1]['packet_count'])
        )
        
        for psk_name, data in sorted_networks:
            risk = data['psk_risk']
            risk_colored = self._color(risk, risk)
            
            print(f"┌{'─'*68}┐")
            print(f"│ {risk_colored:>8} │ {psk_name:<54} │")
            print(f"├{'─'*68}┤")
            sensitive = data.get('sensitive_count', 0)
            info = data['packet_count'] - sensitive
            print(f"│ Description: {data['psk_description']:<52} │")
            print(f"│ Packets: {data['packet_count']:<56} │")
            print(f"│   Sensitive (text/admin): {sensitive:<45} │")
            print(f"│   Informational (pos/nodeinfo/etc): {info:<35} │")
            print(f"│ Devices: {len(data['devices']):<56} │")
            
            if data['devices']:
                devices_str = ', '.join(list(data['devices'])[:3])
                if len(data['devices']) > 3:
                    devices_str += f" (+{len(data['devices'])-3} more)"
                print(f"│   {devices_str:<64} │")
            
            print(f"└{'─'*68}┘")
            print()
        
        # Intercepted text messages
        if self.text_messages:
            CYAN  = '' if self.no_color else '\033[96m'
            RESET = '' if self.no_color else '\033[0m'
            BOLD  = '' if self.no_color else '\033[1m'
            print(f"\n{BOLD}💬 INTERCEPTED TEXT MESSAGES ({len(self.text_messages)} total):{RESET}")
            print("─"*70)
            for node, ts, psk, text in self.text_messages[:30]:
                node_col = self._color(node, 'HIGH')
                print(f"  {ts}  {node_col}  [{psk}]  {text}")
            if len(self.text_messages) > 30:
                print(f"  ... and {len(self.text_messages) - 30} more")
            print()

        # Recommendations
        print("📋 RECOMMENDATIONS:")
        print("─"*70)
        
        has_critical = any(d['psk_risk'] == 'CRITICAL' for d in self.vulnerable_networks.values())
        has_admin = 'Admin Channel' in str(self.vulnerable_networks.keys())
        
        if has_admin:
            print(self._color("⚠️  URGENT: Devices using legacy admin key detected!", 'CRITICAL'))
            print("   → Update firmware to 2.5+ immediately")
            print("   → Regenerate admin channel PSK")
            print()
        
        if has_critical:
            print(self._color("🔴 CRITICAL: Default PSK in use!", 'CRITICAL'))
            print("   → Change channel PSK from factory default")
            print("   → Use: Meshtastic app → Settings → LoRa → Channel PSK")
            print()
        
        print("💡 General recommendations:")
        print("   1. Generate unique PSK: base64 of 32 random bytes")
        print("   2. Distribute PSK securely (not over cleartext channels)")
        print("   3. Update firmware to 2.5.0+ for PKC-protected DMs")
        print("   4. Disable position broadcasts if OPSEC required")
    
    def export_json(self):
        """Export results as JSON"""
        return json.dumps({
            'audit_time': datetime.now().isoformat(),
            'summary': {
                'total_tested': self.total_tested,
                'sensitive_vulnerable': self.total_vulnerable,
                'informational_decryptable': self.informational,
                'unencrypted': self.unencrypted,
                'unknown_psk': self.failed,
            },
            'vulnerable_networks': {
                name: {
                    'psk_name': data['psk_name'],
                    'risk_level': data['psk_risk'],
                    'description': data['psk_description'],
                    'packet_count': data['packet_count'],
                    'devices': list(data['devices']),
                }
                for name, data in self.vulnerable_networks.items()
            }
        }, indent=2)


def load_pcap(filepath):
    """Load packets from PCAP"""
    packets = []
    
    with open(filepath, 'rb') as f:
        f.read(24)  # Skip global header
        
        while True:
            pkt_header = f.read(16)
            if len(pkt_header) < 16:
                break
            
            ts_sec, ts_usec, incl_len, _ = struct.unpack('<IIII', pkt_header)
            pkt_data = f.read(incl_len)
            
            # Strip custom LoRa pseudo-header (20 bytes) if present
            if len(pkt_data) > 20 and pkt_data[:4] != b'\xff\xff\xff\xff':
                pkt_data = pkt_data[20:]

            # Only audit Meshtastic broadcast packets from PCAP
            # (no protocol label available; unicast requires CSV with protocol column)
            if pkt_data[:4] != b'\xff\xff\xff\xff':
                continue

            packets.append((pkt_data, ts_sec + ts_usec/1e6))
    
    return packets


def load_csv(filepath):
    """Load packets from CSV"""
    import csv
    packets = []
    
    with open(filepath, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            # Only audit packets the firmware identified as Meshtastic
            protocol = row.get('protocol', '')
            if protocol and protocol != 'Meshtastic':
                continue
            hex_data = row.get('hex_data') or row.get('data') or row.get('raw_hex')
            timestamp = float(row.get('timestamp_ms', row.get('timestamp', 0))) / 1000
            if hex_data:
                try:
                    packets.append((bytes.fromhex(hex_data.replace(' ', '')), timestamp))
                except ValueError:
                    pass
    
    return packets


def monitor_live(host, auditor, duration=None):
    """Monitor live from ESP32 WebSocket"""
    if not REQUESTS_AVAILABLE:
        print("Error: requests library required for live monitoring")
        return
    
    try:
        import websocket
    except ImportError:
        print("Error: websocket-client required. Install: pip install websocket-client")
        return
    
    print(f"Connecting to ws://{host}/ws ...")
    
    start = datetime.now()
    
    def on_message(ws, message):
        try:
            data = json.loads(message)
            if data.get('type') == 'packet':
                hex_data = data.get('data_hex') or data.get('hex_data')
                if hex_data:
                    result = auditor.audit_packet(hex_data)
                    if result and result.get('vulnerable'):
                        risk = result['risk']
                        print(f"[{risk}] Vulnerable packet from 0x{result['node_id']:08X} - {result['psk_name']}")
        except Exception:
            pass
        
        if duration and (datetime.now() - start).seconds >= duration:
            ws.close()
    
    def on_error(ws, error):
        print(f"WebSocket error: {error}")
    
    def on_close(ws, close_status_code, close_msg):
        print("WebSocket closed")
        auditor.print_results()
    
    def on_open(ws):
        print(f"Connected! Monitoring for PSK vulnerabilities...")
        if duration:
            print(f"Will run for {duration} seconds")
    
    ws = websocket.WebSocketApp(
        f"ws://{host}/ws",
        on_message=on_message,
        on_error=on_error,
        on_close=on_close,
        on_open=on_open
    )
    
    ws.run_forever()


def dramatic_key_scan(auditor, packets, no_color=False, max_animate=80):
    """Animate PSK attempts for the first max_animate packets, then audit the
    rest silently.  Keeps demo runtime under 30 seconds regardless of file size.
    """
    import time

    RED    = '' if no_color else '\033[91m'
    GRAY   = '' if no_color else '\033[90m'
    GREEN  = '' if no_color else '\033[92m'
    YELLOW = '' if no_color else '\033[93m'
    RESET  = '' if no_color else '\033[0m'
    CLEAR  = '\r\033[K'

    seen_hits = set()
    animate_packets = packets[:max_animate]
    silent_packets  = packets[max_animate:]

    print(f"\n{YELLOW}Scanning {len(packets)} packets against {len(PSK_DATABASE)} known PSKs...{RESET}")
    if silent_packets:
        print(f"{GRAY}  (animating first {max_animate}, auditing remaining {len(silent_packets)} silently){RESET}\n")
    time.sleep(0.3)

    for pkt_idx, (pkt_data, timestamp) in enumerate(animate_packets):
        for psk_b64, psk_name, risk, description in PSK_DATABASE:
            try:
                key_bytes = __import__('base64').b64decode(psk_b64)
            except Exception:
                continue

            label = f"{GRAY}  [{pkt_idx+1:3}/{len(packets)}] Testing {psk_name:<35} ...{RESET}"
            print(label, end='', flush=True)
            time.sleep(0.05)

            node_id   = __import__('struct').unpack('<I', pkt_data[4:8])[0]
            packet_id = __import__('struct').unpack('<I', pkt_data[8:12])[0]
            encrypted = pkt_data[16:]
            decrypt_result = auditor.try_decrypt(encrypted, key_bytes, packet_id, node_id)

            if decrypt_result is not None:
                portnum = decrypt_result[0]
                color = RED if risk in ('CRITICAL', 'HIGH') else YELLOW
                mark  = '\a' if risk == 'CRITICAL' else ''
                print(f"{CLEAR}{color}  [{pkt_idx+1:3}/{len(packets)}] HIT  {psk_name:<35} [{risk}]{mark}{RESET}")
                if psk_name not in seen_hits:
                    seen_hits.add(psk_name)
                    time.sleep(0.4)
                break
            else:
                print(CLEAR, end='', flush=True)

    # Audit remaining packets silently
    if silent_packets:
        print(f"\n{GRAY}  Auditing remaining {len(silent_packets)} packets...{RESET}", end='', flush=True)
        for pkt_data, timestamp in silent_packets:
            auditor.audit_packet(pkt_data, timestamp)
        print(f"{CLEAR}", end='')

    # Also audit the animated packets to populate auditor state
    print(f"\n{GREEN}Scan complete. Running full audit...{RESET}\n")
    time.sleep(0.2)
    for pkt_data, timestamp in animate_packets:
        auditor.audit_packet(pkt_data, timestamp)


def main():
    parser = argparse.ArgumentParser(
        description='PSK Vulnerability Auditor for Meshtastic Networks',
        epilog='Example: python psk_auditor.py capture.pcap'
    )
    parser.add_argument('input', nargs='?', help='Input file (PCAP or CSV)')
    parser.add_argument('--live', metavar='HOST',
                       help='Monitor live from ESP32 (e.g., 192.168.4.1)')
    parser.add_argument('--duration', type=int, metavar='SECONDS',
                       help='Duration for live monitoring')
    parser.add_argument('--json', action='store_true',
                       help='Output results as JSON')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Verbose output')
    parser.add_argument('--no-color', action='store_true',
                       help='Disable colored output')
    parser.add_argument('--list-psks', action='store_true',
                       help='List all known PSKs in database')
    parser.add_argument('--demo', action='store_true',
                       help='Demo mode: simulated audit results (no input needed)')
    parser.add_argument('--dramatic', action='store_true',
                       help='Dramatic pacing: animate each key attempt (great for live demos)')
    
    args = parser.parse_args()
    
    if args.list_psks:
        print("🔑 PSK Vulnerability Database\n")
        print(f"{'Risk':<10} {'Name':<25} {'Description'}")
        print("─"*70)
        for psk_b64, name, risk, desc in PSK_DATABASE:
            print(f"{risk:<10} {name:<25} {desc}")
        return 0
    
    # Demo mode - generate simulated audit results
    if args.demo:
        import random
        print("\n🎮 DEMO MODE: PSK Vulnerability Audit\n")
        print("Simulating audit of captured Meshtastic traffic...\n")
        
        auditor = PSKAuditor(verbose=args.verbose, no_color=args.no_color)
        
        # Simulate findings across different risk levels
        demo_findings = [
            ("Default 0x01 (classic)", "CRITICAL", "Factory default - 60% of devices", 
             ["!a1b2c3d4", "!9a8b7c6d"], 17),
            ("LongFast public channel", "HIGH", "Public preset - unencrypted", 
             ["!f5e6d7c8"], 8),
            ("ShortFast preset", "HIGH", "Common preset", 
             ["!1234abcd"], 3),
        ]
        
        # Populate auditor with simulated data
        total_packets = 43
        vuln_packets = 28
        
        for psk_name, risk, desc, devices, count in demo_findings:
            auditor.vulnerable_networks[psk_name] = {
                'psk_name': psk_name,
                'psk_risk': risk,
                'psk_description': desc,
                'packet_count': count,
                'devices': set(devices),
                'first_seen': f"2024-12-20 10:{random.randint(10,59):02d}:00",
                'last_seen': f"2024-12-20 11:{random.randint(10,59):02d}:00",
                'sample_data': [f"Decrypted content from {devices[0]}..."],
            }
        
        auditor.total_tested = total_packets
        auditor.total_vulnerable = vuln_packets
        auditor.unencrypted = 0
        auditor.failed = total_packets - vuln_packets
        
        if args.dramatic:
            # For demo mode, build a minimal packet list from the simulated findings
            # so the dramatic scanner has something to animate against
            import struct as _struct
            import base64 as _b64
            import random as _rng
            demo_packets = []
            for _psk_b64, _name, _risk, _desc in PSK_DATABASE[:6]:
                # Craft a fake encrypted packet with valid Meshtastic header
                _key = _b64.b64decode(_psk_b64)
                _nid = _rng.randint(0x10000000, 0xFFFFFFFF)
                _pid = _rng.randint(1, 0xFFFFFF)
                _hdr = b'\xff\xff\xff\xff' + _struct.pack('<II', _nid, _pid) + b'\x03\x00\x00\x00'
                _enc = bytes([_rng.randint(0, 255) for _ in range(24)])
                demo_packets.append((_hdr + _enc, None))
            dramatic_key_scan(auditor, demo_packets, no_color=args.no_color)

        print(f"✅ Analyzed {total_packets} encrypted packets\n")

        if args.json:
            print(auditor.export_json())
        else:
            auditor.print_results()
        return 0
    
    if not args.input and not args.live:
        parser.print_help()
        return 1
    
    if not CRYPTO_AVAILABLE:
        print("Error: cryptography library required")
        print("Install: pip install cryptography")
        return 1
    
    auditor = PSKAuditor(verbose=args.verbose, no_color=args.no_color)
    
    if args.live:
        monitor_live(args.live, auditor, args.duration)
    else:
        filepath = Path(args.input)
        if not filepath.exists():
            print(f"Error: File not found: {filepath}")
            return 1
        
        # Load packets
        ext = filepath.suffix.lower()
        if ext in ('.pcap', '.pcapng'):
            packets = load_pcap(filepath)
        elif ext == '.csv':
            packets = load_csv(filepath)
        else:
            print(f"Error: Unknown format: {ext}")
            return 1
        
        print(f"Loaded {len(packets)} packets from {filepath}")

        if args.dramatic:
            dramatic_key_scan(auditor, packets, no_color=args.no_color)
        else:
            print("Auditing for PSK vulnerabilities...\n")
            # Audit each packet
            for i, (pkt_data, timestamp) in enumerate(packets):
                result = auditor.audit_packet(pkt_data, timestamp)

                if args.verbose and result:
                    if result.get('vulnerable'):
                        print(f"  [{i+1}] VULNERABLE: {result['psk_name']} ({result['risk']})")
                    else:
                        print(f"  [{i+1}] Unknown PSK")
        
        # Output results
        if args.json:
            print(auditor.export_json())
        else:
            auditor.print_results()
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
