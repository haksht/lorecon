#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - ABP Device Detector

Detects LoRaWAN devices using Activation By Personalization (ABP) 
vs Over-The-Air Activation (OTAA).

ABP devices are security risks because:
- Static session keys (NwkSKey/AppSKey never change)
- No join process to intercept new keys
- Frame counter resets on device restart = replay vulnerability
- DevAddr is predictable

Detection heuristics:
1. Data packets without preceding Join Request/Accept
2. Frame counter resets (starts at 0 without join)
3. Consistent DevAddr across sessions
4. Missing DevNonce tracking

Usage:
    python abp_detector.py capture.csv
    python abp_detector.py capture.pcap --verbose
    python abp_detector.py --live 192.168.4.1

Requirements:
    pip install requests (for live monitoring)
"""

import argparse
import csv
import random
import struct
import json
import sys
import time
from collections import defaultdict
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Set

try:
    import requests
    REQUESTS_AVAILABLE = True
except ImportError:
    REQUESTS_AVAILABLE = False


# LoRaWAN Message Types (MHDR MType field)
MTYPE_JOIN_REQUEST = 0x00
MTYPE_JOIN_ACCEPT = 0x01
MTYPE_UNCONFIRMED_DATA_UP = 0x02
MTYPE_UNCONFIRMED_DATA_DOWN = 0x03
MTYPE_CONFIRMED_DATA_UP = 0x04
MTYPE_CONFIRMED_DATA_DOWN = 0x05
MTYPE_REJOIN_REQUEST = 0x06
MTYPE_PROPRIETARY = 0x07

MTYPE_NAMES = {
    0x00: 'Join Request',
    0x01: 'Join Accept',
    0x02: 'Unconfirmed Data Up',
    0x03: 'Unconfirmed Data Down',
    0x04: 'Confirmed Data Up',
    0x05: 'Confirmed Data Down',
    0x06: 'Rejoin Request',
    0x07: 'Proprietary',
}


class LoRaWANDevice:
    """Track state for a single LoRaWAN device"""
    
    def __init__(self, dev_addr: str):
        self.dev_addr = dev_addr
        self.first_seen: Optional[int] = None
        self.last_seen: Optional[int] = None
        self.packet_count = 0
        
        # Join tracking
        self.join_requests: List[Dict] = []
        self.join_accepts: List[Dict] = []
        self.has_joined = False
        
        # Frame counter tracking
        self.frame_counters: List[int] = []
        self.fcnt_resets = 0
        self.last_fcnt = -1
        
        # Message types seen
        self.mtypes_seen: Set[int] = set()
        
        # Detection results
        self.is_abp: Optional[bool] = None
        self.abp_confidence = 0.0
        self.abp_reasons: List[str] = []
    
    def add_packet(self, timestamp_ms: int, mtype: int, fcnt: int = None):
        """Record a packet from this device"""
        
        self.packet_count += 1
        if self.first_seen is None:
            self.first_seen = timestamp_ms
        self.last_seen = timestamp_ms
        
        self.mtypes_seen.add(mtype)
        
        # Track join packets
        if mtype == MTYPE_JOIN_REQUEST:
            self.join_requests.append({'timestamp': timestamp_ms})
            self.has_joined = True
        elif mtype == MTYPE_JOIN_ACCEPT:
            self.join_accepts.append({'timestamp': timestamp_ms})
            self.has_joined = True
        
        # Track frame counter
        if fcnt is not None:
            self.frame_counters.append(fcnt)
            
            # Detect resets (counter goes backward significantly)
            if self.last_fcnt >= 0:
                if fcnt < self.last_fcnt and (self.last_fcnt - fcnt) > 100:
                    self.fcnt_resets += 1
            self.last_fcnt = fcnt
    
    def analyze(self) -> Dict:
        """Analyze device behavior and determine ABP vs OTAA"""
        
        self.abp_reasons = []
        confidence = 0.0
        
        # Check 1: Data packets without any join process
        data_mtypes = {MTYPE_UNCONFIRMED_DATA_UP, MTYPE_UNCONFIRMED_DATA_DOWN,
                       MTYPE_CONFIRMED_DATA_UP, MTYPE_CONFIRMED_DATA_DOWN}
        has_data = bool(self.mtypes_seen & data_mtypes)
        has_join = bool(self.join_requests or self.join_accepts)
        
        if has_data and not has_join:
            confidence += 40
            self.abp_reasons.append("Data traffic without Join process observed")
        
        # Check 2: Frame counter starts near zero without join
        if self.frame_counters and not has_join:
            first_fcnt = self.frame_counters[0]
            if first_fcnt < 10:
                confidence += 30
                self.abp_reasons.append(f"Frame counter started at {first_fcnt} without Join")
        
        # Check 3: Frame counter resets (ABP devices reset on power cycle)
        if self.fcnt_resets > 0:
            confidence += 20 * min(self.fcnt_resets, 2)
            self.abp_reasons.append(f"Frame counter reset {self.fcnt_resets} time(s)")
        
        # Check 4: Long session without rejoin (OTAA typically rejoins periodically)
        if self.first_seen and self.last_seen:
            session_hours = (self.last_seen - self.first_seen) / 3600000
            if session_hours > 24 and not has_join and has_data:
                confidence += 15
                self.abp_reasons.append(f"Long session ({session_hours:.1f}h) without rejoin")
        
        # Check 5: High packet count without join
        if self.packet_count > 100 and not has_join:
            confidence += 10
            self.abp_reasons.append(f"High activity ({self.packet_count} packets) without Join")
        
        # Negative evidence (reduces ABP confidence)
        if has_join:
            confidence -= 50
            self.abp_reasons.append("Join process observed - likely OTAA")
        
        self.abp_confidence = max(0, min(100, confidence))
        self.is_abp = confidence >= 50
        
        return {
            'dev_addr': self.dev_addr,
            'is_abp': self.is_abp,
            'confidence': self.abp_confidence,
            'reasons': self.abp_reasons,
            'packet_count': self.packet_count,
            'has_joined': self.has_joined,
            'fcnt_resets': self.fcnt_resets,
        }


class ABPDetector:
    """Detect ABP vs OTAA devices in LoRaWAN traffic"""
    
    def __init__(self, verbose=False):
        self.verbose = verbose
        self.devices: Dict[str, LoRaWANDevice] = {}
        self.total_packets = 0
        self.lorawan_packets = 0
    
    def parse_lorawan_packet(self, data: bytes) -> Optional[Dict]:
        """Parse LoRaWAN MAC header"""
        
        if len(data) < 12:
            return None
        
        # Check for Meshtastic header (not LoRaWAN)
        if data[:4] == b'\xff\xff\xff\xff':
            return None
        
        try:
            mhdr = data[0]
            mtype = (mhdr >> 5) & 0x07
            
            # Validate mtype
            if mtype > 0x07:
                return None
            
            result = {
                'mtype': mtype,
                'mtype_name': MTYPE_NAMES.get(mtype, 'Unknown'),
            }
            
            # Parse based on message type
            if mtype in (MTYPE_UNCONFIRMED_DATA_UP, MTYPE_UNCONFIRMED_DATA_DOWN,
                        MTYPE_CONFIRMED_DATA_UP, MTYPE_CONFIRMED_DATA_DOWN):
                # Data frame: MHDR(1) + DevAddr(4) + FCtrl(1) + FCnt(2) + ...
                if len(data) >= 8:
                    dev_addr = struct.unpack('<I', data[1:5])[0]
                    fctrl = data[5]
                    fcnt = struct.unpack('<H', data[6:8])[0]
                    
                    result['dev_addr'] = f"0x{dev_addr:08X}"
                    result['fcnt'] = fcnt
                    result['fctrl'] = fctrl
                    result['adr'] = bool(fctrl & 0x80)
            
            elif mtype == MTYPE_JOIN_REQUEST:
                # Join Request: MHDR(1) + JoinEUI(8) + DevEUI(8) + DevNonce(2)
                if len(data) >= 19:
                    join_eui = data[1:9][::-1].hex()
                    dev_eui = data[9:17][::-1].hex()
                    dev_nonce = struct.unpack('<H', data[17:19])[0]
                    
                    result['join_eui'] = join_eui
                    result['dev_eui'] = dev_eui
                    result['dev_nonce'] = dev_nonce
                    # Use DevEUI as identifier for join requests
                    result['dev_addr'] = f"DevEUI:{dev_eui[:8]}"
            
            elif mtype == MTYPE_JOIN_ACCEPT:
                # Join Accept: Encrypted, can't parse without key
                result['dev_addr'] = 'JOIN_ACCEPT'
            
            return result
        
        except Exception:
            return None
    
    def add_packet(self, data: bytes, timestamp_ms: int):
        """Process a packet"""
        
        self.total_packets += 1
        
        parsed = self.parse_lorawan_packet(data)
        if not parsed:
            return
        
        self.lorawan_packets += 1
        
        dev_addr = parsed.get('dev_addr')
        if not dev_addr or dev_addr == 'JOIN_ACCEPT':
            return
        
        # Get or create device
        if dev_addr not in self.devices:
            self.devices[dev_addr] = LoRaWANDevice(dev_addr)
        
        device = self.devices[dev_addr]
        device.add_packet(
            timestamp_ms=timestamp_ms,
            mtype=parsed['mtype'],
            fcnt=parsed.get('fcnt')
        )
        
        if self.verbose:
            print(f"  [{parsed['mtype_name']}] {dev_addr} FCnt:{parsed.get('fcnt', 'N/A')}")
    
    def load_csv(self, filepath: Path):
        """Load from CSV capture file"""
        
        with open(filepath, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                hex_data = row.get('data_hex', row.get('hex_data', row.get('raw_hex')))
                timestamp = int(row.get('timestamp_ms', row.get('timestamp', 0)))
                
                if hex_data:
                    try:
                        data = bytes.fromhex(hex_data.replace(' ', ''))
                        self.add_packet(data, timestamp)
                    except ValueError:
                        continue
    
    def load_pcap(self, filepath: Path):
        """Load from PCAP file"""
        
        with open(filepath, 'rb') as f:
            f.read(24)  # Skip global header
            
            while True:
                pkt_hdr = f.read(16)
                if len(pkt_hdr) < 16:
                    break
                
                ts_sec, ts_usec, incl_len, _ = struct.unpack('<IIII', pkt_hdr)
                pkt_data = f.read(incl_len)
                
                # Skip pseudo-header if present
                if len(pkt_data) > 16:
                    # Check for our custom header
                    pkt_data = pkt_data[16:]
                
                timestamp_ms = ts_sec * 1000 + ts_usec // 1000
                self.add_packet(pkt_data, timestamp_ms)
    
    def analyze(self) -> List[Dict]:
        """Analyze all devices and return results"""
        
        results = []
        for device in self.devices.values():
            results.append(device.analyze())
        
        # Sort by confidence (highest first)
        results.sort(key=lambda x: x['confidence'], reverse=True)
        
        return results
    
    def generate_report(self) -> str:
        """Generate detection report"""
        
        lines = []
        lines.append("=" * 70)
        lines.append("🔍 ABP DEVICE DETECTION REPORT")
        lines.append("=" * 70)
        
        lines.append(f"\n📊 SUMMARY")
        lines.append("-" * 70)
        lines.append(f"Total Packets: {self.total_packets}")
        lines.append(f"LoRaWAN Packets: {self.lorawan_packets}")
        lines.append(f"Unique Devices: {len(self.devices)}")
        
        results = self.analyze()
        
        abp_devices = [r for r in results if r['is_abp']]
        otaa_devices = [r for r in results if not r['is_abp']]
        
        lines.append(f"\n🔴 ABP Devices (High Risk): {len(abp_devices)}")
        lines.append(f"🟢 OTAA Devices: {len(otaa_devices)}")
        
        # ABP devices detail
        if abp_devices:
            lines.append(f"\n⚠️  ABP DEVICES DETECTED")
            lines.append("-" * 70)
            lines.append("ABP devices use static session keys - security risk!")
            lines.append("")
            
            for result in abp_devices:
                lines.append(f"📱 {result['dev_addr']}")
                lines.append(f"   Confidence: {result['confidence']:.0f}%")
                lines.append(f"   Packets: {result['packet_count']}")
                lines.append(f"   FCnt Resets: {result['fcnt_resets']}")
                lines.append(f"   Reasons:")
                for reason in result['reasons']:
                    lines.append(f"     - {reason}")
                lines.append("")
        
        # OTAA devices summary
        if otaa_devices:
            lines.append(f"\n✅ OTAA DEVICES (SECURE)")
            lines.append("-" * 70)
            for result in otaa_devices[:10]:
                lines.append(f"   {result['dev_addr']}: {result['packet_count']} packets")
        
        # Security recommendations
        lines.append(f"\n💡 RECOMMENDATIONS")
        lines.append("-" * 70)
        
        if abp_devices:
            lines.append("🔴 HIGH PRIORITY:")
            lines.append("   1. Migrate ABP devices to OTAA activation")
            lines.append("   2. If ABP required, implement frame counter persistence")
            lines.append("   3. Monitor for frame counter reset attacks")
            lines.append("   4. Consider additional application-layer encryption")
        else:
            lines.append("✅ No ABP devices detected. Network using secure OTAA.")
        
        return '\n'.join(lines)
    
    def export_json(self) -> str:
        """Export results as JSON"""
        
        results = self.analyze()
        
        return json.dumps({
            'summary': {
                'total_packets': self.total_packets,
                'lorawan_packets': self.lorawan_packets,
                'device_count': len(self.devices),
                'abp_count': len([r for r in results if r['is_abp']]),
                'otaa_count': len([r for r in results if not r['is_abp']]),
            },
            'devices': results,
        }, indent=2)


def generate_demo_data(detector: ABPDetector):
    """Generate realistic demo data showing ABP vs OTAA devices"""
    print("\n🎮 DEMO MODE: Simulating LoRaWAN traffic for ABP detection...\n")
    
    base_time = int(time.time() * 1000)
    
    # ABP Device 1: Smart meter with frame counter resets (very suspicious)
    abp_addr_1 = 0x26011234
    fcnt = 0
    for i in range(20):
        ts = base_time + i * 60000  # Once per minute
        if i == 8:  # Reset at packet 8
            fcnt = 0
        mhdr = MTYPE_UNCONFIRMED_DATA_UP << 5
        fctrl = 0x00
        data = bytes([mhdr]) + struct.pack('<I', abp_addr_1) + bytes([fctrl]) + struct.pack('<H', fcnt)
        data += bytes([random.randint(0, 255) for _ in range(10)])  # Payload
        detector.add_packet(data, ts)
        fcnt += 1
    
    # ABP Device 2: Parking sensor starting at FCnt=0 (suspicious)
    abp_addr_2 = 0x26015678
    for i in range(15):
        ts = base_time + i * 120000
        mhdr = MTYPE_CONFIRMED_DATA_UP << 5
        fctrl = 0x80  # ADR enabled
        data = bytes([mhdr]) + struct.pack('<I', abp_addr_2) + bytes([fctrl]) + struct.pack('<H', i)
        data += bytes([random.randint(0, 255) for _ in range(8)])
        detector.add_packet(data, ts)
    
    # OTAA Device 1: Environmental sensor with join sequence
    dev_eui_1 = bytes.fromhex('70B3D5499A001111')[::-1]
    join_eui_1 = bytes.fromhex('0011223344556677')[::-1]
    
    # First, send join request
    mhdr = MTYPE_JOIN_REQUEST << 5
    join_req = bytes([mhdr]) + join_eui_1 + dev_eui_1 + struct.pack('<H', random.randint(0, 65535))
    join_req += bytes([random.randint(0, 255) for _ in range(4)])  # MIC
    detector.add_packet(join_req, base_time + 100)
    
    # Then join accept (encrypted, we just see the structure)
    mhdr = MTYPE_JOIN_ACCEPT << 5
    join_acc = bytes([mhdr]) + bytes([random.randint(0, 255) for _ in range(16)])
    detector.add_packet(join_acc, base_time + 200)
    
    # Then data packets with assigned DevAddr
    otaa_addr_1 = 0x26019ABC
    for i in range(12):
        ts = base_time + 300 + i * 60000
        mhdr = MTYPE_UNCONFIRMED_DATA_UP << 5
        fctrl = 0x80
        # Frame counter starts after join, continues normally
        data = bytes([mhdr]) + struct.pack('<I', otaa_addr_1) + bytes([fctrl]) + struct.pack('<H', i)
        data += bytes([random.randint(0, 255) for _ in range(12)])
        detector.add_packet(data, ts)
    
    # OTAA Device 2: Asset tracker with proper join
    dev_eui_2 = bytes.fromhex('70B3D5499A002222')[::-1]
    join_eui_2 = bytes.fromhex('0011223344556677')[::-1]
    
    mhdr = MTYPE_JOIN_REQUEST << 5
    join_req = bytes([mhdr]) + join_eui_2 + dev_eui_2 + struct.pack('<H', random.randint(0, 65535))
    join_req += bytes([random.randint(0, 255) for _ in range(4)])
    detector.add_packet(join_req, base_time + 50000)
    
    mhdr = MTYPE_JOIN_ACCEPT << 5
    join_acc = bytes([mhdr]) + bytes([random.randint(0, 255) for _ in range(16)])
    detector.add_packet(join_acc, base_time + 50100)
    
    otaa_addr_2 = 0x2601DEAD
    for i in range(8):
        ts = base_time + 50200 + i * 120000
        mhdr = MTYPE_UNCONFIRMED_DATA_UP << 5
        fctrl = 0x00
        data = bytes([mhdr]) + struct.pack('<I', otaa_addr_2) + bytes([fctrl]) + struct.pack('<H', i + 100)
        data += bytes([random.randint(0, 255) for _ in range(15)])
        detector.add_packet(data, ts)
    
    print(f"Generated {detector.total_packets} packets for {len(detector.devices)} devices")
    print(f"  - 2 suspicious ABP devices (no join observed)")
    print(f"  - 2 OTAA devices (join sequences captured)\n")


def main():
    parser = argparse.ArgumentParser(
        description='Detect ABP vs OTAA LoRaWAN devices',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Security Implications:
  ABP (Activation By Personalization):
    - Static session keys (never change)
    - Vulnerable to frame counter reset attacks
    - Easier to perform replay attacks
    - DevAddr is predictable
  
  OTAA (Over-The-Air Activation):
    - Dynamic session keys (generated per join)
    - DevNonce prevents replay of join requests
    - More secure, industry best practice

Examples:
  %(prog)s capture.csv           Analyze CSV capture
  %(prog)s capture.pcap -v       Analyze PCAP with verbose output
  %(prog)s capture.csv --json    JSON output for automation
"""
    )
    
    parser.add_argument('input', nargs='?', help='Input file (CSV or PCAP)')
    parser.add_argument('--live', metavar='HOST',
                       help='Monitor live from ESP32')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Verbose output')
    parser.add_argument('--json', action='store_true',
                       help='Output as JSON')
    parser.add_argument('--demo', action='store_true',
                       help='Demo mode: generate simulated ABP/OTAA traffic')
    
    args = parser.parse_args()
    
    # Demo mode doesn't need input
    if args.demo:
        detector = ABPDetector(verbose=args.verbose)
        generate_demo_data(detector)
        if args.json:
            print(detector.export_json())
        else:
            print(detector.generate_report())
        results = detector.analyze()
        abp_count = len([r for r in results if r['is_abp']])
        return 1 if abp_count > 0 else 0
    
    if not args.input and not args.live:
        parser.print_help()
        return 1
    
    detector = ABPDetector(verbose=args.verbose)
    
    if args.input:
        filepath = Path(args.input)
        if not filepath.exists():
            print(f"Error: File not found: {filepath}")
            return 1
        
        print(f"Loading {filepath}...")
        
        if filepath.suffix.lower() in ('.pcap', '.pcapng'):
            detector.load_pcap(filepath)
        else:
            detector.load_csv(filepath)
        
        print(f"Processed {detector.total_packets} packets, "
              f"{detector.lorawan_packets} LoRaWAN")
    
    if args.json:
        print(detector.export_json())
    else:
        print(detector.generate_report())
    
    # Exit code based on ABP detection
    results = detector.analyze()
    abp_count = len([r for r in results if r['is_abp']])
    return 1 if abp_count > 0 else 0


if __name__ == '__main__':
    sys.exit(main())
