#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - LoRaWAN Join Analyzer

Analyzes LoRaWAN Join Request/Accept sequences for security assessment.
Tracks DevNonce for reuse vulnerabilities and monitors join success rates.

Security Issues Detected:
- DevNonce reuse (replay attack vulnerability)
- Join storms (potential attack or misconfiguration)
- High join failure rates (network issues)
- Missing join accepts (blocked devices)

Usage:
    python lorawan_join_analyzer.py capture.csv
    python lorawan_join_analyzer.py capture.pcap --verbose
    python lorawan_join_analyzer.py --live 192.168.4.1
    python lorawan_join_analyzer.py capture.csv --json -o report.json

Requirements:
    pip install requests (for live monitoring)
"""

import argparse
import csv
import json
import random
import struct
import sys
import time
from collections import defaultdict
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

try:
    import requests
    REQUESTS_AVAILABLE = True
except ImportError:
    REQUESTS_AVAILABLE = False


# LoRaWAN Message Types (MHDR MType field, bits 5-7)
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

# Rejoin types (for Rejoin Request)
REJOIN_TYPE_0 = 0  # Rejoin to same network
REJOIN_TYPE_1 = 1  # Rejoin to different network
REJOIN_TYPE_2 = 2  # Rejoin to rekey


class JoinRequest:
    """Represents a parsed LoRaWAN Join Request"""
    
    def __init__(self, timestamp_ms: int, raw_data: bytes):
        self.timestamp_ms = timestamp_ms
        self.raw_data = raw_data
        self.valid = False
        
        # Parsed fields
        self.join_eui: Optional[str] = None   # AppEUI/JoinEUI (8 bytes)
        self.dev_eui: Optional[str] = None    # DevEUI (8 bytes)
        self.dev_nonce: Optional[int] = None  # DevNonce (2 bytes)
        self.mic: Optional[bytes] = None      # MIC (4 bytes)
        
        self._parse()
    
    def _parse(self):
        """Parse Join Request packet"""
        # Join Request: MHDR(1) + JoinEUI(8) + DevEUI(8) + DevNonce(2) + MIC(4) = 23 bytes
        if len(self.raw_data) < 23:
            return
        
        mhdr = self.raw_data[0]
        mtype = (mhdr >> 5) & 0x07
        
        if mtype != MTYPE_JOIN_REQUEST:
            return
        
        # JoinEUI/AppEUI (reversed for display - little-endian to big-endian)
        join_eui_bytes = self.raw_data[1:9][::-1]
        self.join_eui = join_eui_bytes.hex().upper()
        
        # DevEUI (reversed for display)
        dev_eui_bytes = self.raw_data[9:17][::-1]
        self.dev_eui = dev_eui_bytes.hex().upper()
        
        # DevNonce (little-endian 16-bit)
        self.dev_nonce = struct.unpack('<H', self.raw_data[17:19])[0]
        
        # MIC
        self.mic = self.raw_data[19:23]
        
        self.valid = True
    
    def __repr__(self):
        return f"JoinRequest(DevEUI={self.dev_eui}, DevNonce={self.dev_nonce})"


class JoinAccept:
    """Represents a parsed LoRaWAN Join Accept (encrypted, limited parsing)"""
    
    def __init__(self, timestamp_ms: int, raw_data: bytes):
        self.timestamp_ms = timestamp_ms
        self.raw_data = raw_data
        self.valid = False
        self.length = len(raw_data)
        
        # Join Accept is encrypted, we can only verify structure
        # Unencrypted: MHDR(1) + JoinNonce(3) + NetID(3) + DevAddr(4) + 
        #              DLSettings(1) + RxDelay(1) + [CFList(16)] + MIC(4)
        # Total: 17 bytes (without CFList) or 33 bytes (with CFList)
        
        mhdr = raw_data[0] if len(raw_data) > 0 else 0
        mtype = (mhdr >> 5) & 0x07
        
        if mtype == MTYPE_JOIN_ACCEPT:
            if len(raw_data) in [17, 33]:  # Valid sizes
                self.valid = True


class DeviceJoinTracker:
    """Track join activity for a single device"""
    
    def __init__(self, dev_eui: str):
        self.dev_eui = dev_eui
        self.join_eui: Optional[str] = None
        
        # Timestamps
        self.first_seen: Optional[int] = None
        self.last_seen: Optional[int] = None
        
        # Join requests
        self.join_requests: List[JoinRequest] = []
        self.dev_nonces: Set[int] = set()
        self.nonce_reuse_count = 0
        self.duplicate_nonces: List[Tuple[int, int, int]] = []  # (nonce, ts1, ts2)
        
        # Join accepts (can't link to specific device without keys)
        self.estimated_accepts = 0
        
        # Timing analysis
        self.join_intervals: List[int] = []  # ms between join requests
        
        # Security assessment
        self.risk_score = 0
        self.risk_reasons: List[str] = []
    
    def add_join_request(self, request: JoinRequest):
        """Add a join request and track nonce"""
        if self.first_seen is None:
            self.first_seen = request.timestamp_ms
        self.last_seen = request.timestamp_ms
        
        if self.join_eui is None:
            self.join_eui = request.join_eui
        
        # Track interval between requests
        if self.join_requests:
            interval = request.timestamp_ms - self.join_requests[-1].timestamp_ms
            self.join_intervals.append(interval)
        
        # Check for nonce reuse
        if request.dev_nonce in self.dev_nonces:
            self.nonce_reuse_count += 1
            # Find original timestamp
            for prev in self.join_requests:
                if prev.dev_nonce == request.dev_nonce:
                    self.duplicate_nonces.append(
                        (request.dev_nonce, prev.timestamp_ms, request.timestamp_ms)
                    )
                    break
        else:
            self.dev_nonces.add(request.dev_nonce)
        
        self.join_requests.append(request)
    
    def calculate_risk(self):
        """Calculate security risk score and reasons"""
        self.risk_score = 0
        self.risk_reasons = []
        
        # Critical: DevNonce reuse
        if self.nonce_reuse_count > 0:
            self.risk_score += 40
            self.risk_reasons.append(
                f"CRITICAL: DevNonce reused {self.nonce_reuse_count}x - replay attack possible"
            )
        
        # High: Sequential nonces (predictable)
        nonce_list = sorted(self.dev_nonces)
        sequential_count = 0
        for i in range(1, len(nonce_list)):
            if nonce_list[i] == nonce_list[i-1] + 1:
                sequential_count += 1
        
        if len(nonce_list) > 5 and sequential_count > len(nonce_list) * 0.8:
            self.risk_score += 20
            self.risk_reasons.append("HIGH: Sequential DevNonces - predictable values")
        
        # Medium: Low nonce values (counter reset)
        low_nonces = [n for n in self.dev_nonces if n < 10]
        if len(low_nonces) > 1:
            self.risk_score += 15
            self.risk_reasons.append(
                f"MEDIUM: Multiple low nonces ({low_nonces}) - device may reset frequently"
            )
        
        # Medium: High join frequency (join storm)
        if self.join_intervals:
            avg_interval = sum(self.join_intervals) / len(self.join_intervals)
            if avg_interval < 5000:  # Less than 5 seconds average
                self.risk_score += 15
                self.risk_reasons.append(
                    f"MEDIUM: Rapid join attempts ({avg_interval/1000:.1f}s avg) - "
                    "possible attack or misconfiguration"
                )
        
        # Low: Many join attempts without apparent success
        if len(self.join_requests) > 10 and self.estimated_accepts == 0:
            self.risk_score += 10
            self.risk_reasons.append(
                f"LOW: {len(self.join_requests)} join requests with no observed accepts"
            )
        
        return self.risk_score


class JoinAnalyzer:
    """Analyze LoRaWAN join sequences"""
    
    def __init__(self, verbose=False):
        self.verbose = verbose
        self.devices: Dict[str, DeviceJoinTracker] = {}
        self.join_accepts: List[JoinAccept] = []
        
        # Global stats
        self.stats = {
            'total_packets': 0,
            'join_requests': 0,
            'join_accepts': 0,
            'rejoin_requests': 0,
            'unique_devices': 0,
            'unique_join_euis': set(),
            'nonce_reuse_events': 0,
        }
        
        # Timeline for correlation
        self.timeline: List[Dict] = []
    
    def get_or_create_device(self, dev_eui: str) -> DeviceJoinTracker:
        """Get or create device tracker"""
        if dev_eui not in self.devices:
            self.devices[dev_eui] = DeviceJoinTracker(dev_eui)
            self.stats['unique_devices'] += 1
        return self.devices[dev_eui]
    
    def process_packet(self, timestamp_ms: int, raw_data: bytes):
        """Process a raw LoRaWAN packet"""
        self.stats['total_packets'] += 1
        
        if len(raw_data) < 1:
            return
        
        mhdr = raw_data[0]
        mtype = (mhdr >> 5) & 0x07
        
        if mtype == MTYPE_JOIN_REQUEST:
            request = JoinRequest(timestamp_ms, raw_data)
            if request.valid:
                self._handle_join_request(request)
        
        elif mtype == MTYPE_JOIN_ACCEPT:
            accept = JoinAccept(timestamp_ms, raw_data)
            if accept.valid:
                self._handle_join_accept(accept)
        
        elif mtype == MTYPE_REJOIN_REQUEST:
            self.stats['rejoin_requests'] += 1
            if self.verbose:
                print(f"[{timestamp_ms}] Rejoin Request detected")
    
    def _handle_join_request(self, request: JoinRequest):
        """Handle a valid join request"""
        self.stats['join_requests'] += 1
        self.stats['unique_join_euis'].add(request.join_eui)
        
        device = self.get_or_create_device(request.dev_eui)
        
        # Check for nonce reuse before adding
        was_reused = request.dev_nonce in device.dev_nonces
        
        device.add_join_request(request)
        
        if was_reused:
            self.stats['nonce_reuse_events'] += 1
        
        # Timeline entry
        self.timeline.append({
            'timestamp': request.timestamp_ms,
            'type': 'join_request',
            'dev_eui': request.dev_eui,
            'join_eui': request.join_eui,
            'dev_nonce': request.dev_nonce,
            'nonce_reused': was_reused,
        })
        
        if self.verbose:
            reused_str = " ⚠️ REUSED!" if was_reused else ""
            print(f"[{request.timestamp_ms}] Join Request: "
                  f"DevEUI={request.dev_eui} Nonce={request.dev_nonce}{reused_str}")
    
    def _handle_join_accept(self, accept: JoinAccept):
        """Handle a join accept (can't link to device without keys)"""
        self.stats['join_accepts'] += 1
        self.join_accepts.append(accept)
        
        # Timeline entry
        self.timeline.append({
            'timestamp': accept.timestamp_ms,
            'type': 'join_accept',
            'length': accept.length,
        })
        
        if self.verbose:
            print(f"[{accept.timestamp_ms}] Join Accept: {accept.length} bytes")
        
        # Try to correlate with recent join request
        # Join accepts typically follow within a few seconds
        recent_requests = [
            e for e in self.timeline
            if e['type'] == 'join_request' and
            0 < accept.timestamp_ms - e['timestamp'] < 10000  # Within 10 seconds
        ]
        
        if recent_requests:
            # Attribute to most recent request
            latest = recent_requests[-1]
            dev_eui = latest['dev_eui']
            if dev_eui in self.devices:
                self.devices[dev_eui].estimated_accepts += 1
    
    def process_csv_row(self, row: Dict):
        """Process a CSV row from capture file"""
        # Try to extract raw packet data
        raw_hex = row.get('raw', row.get('data', row.get('payload', '')))
        timestamp = int(row.get('timestamp_ms', row.get('timestamp', 0)))
        
        if raw_hex:
            try:
                raw_data = bytes.fromhex(raw_hex.replace(' ', '').replace('0x', ''))
                self.process_packet(timestamp, raw_data)
            except ValueError:
                pass
        
        # Also check if protocol is already identified as LoRaWAN
        protocol = row.get('protocol', '').lower()
        if 'join' in protocol or 'lorawan' in protocol:
            mtype_str = row.get('mtype', row.get('type', ''))
            if 'request' in mtype_str.lower():
                # Try to extract fields directly
                dev_eui = row.get('dev_eui', row.get('DevEUI', ''))
                join_eui = row.get('join_eui', row.get('AppEUI', row.get('JoinEUI', '')))
                dev_nonce = row.get('dev_nonce', row.get('DevNonce', ''))
                
                if dev_eui and dev_nonce:
                    device = self.get_or_create_device(dev_eui)
                    device.join_eui = join_eui
                    
                    # Create pseudo-request
                    class PseudoRequest:
                        pass
                    req = PseudoRequest()
                    req.timestamp_ms = timestamp
                    req.dev_eui = dev_eui
                    req.join_eui = join_eui
                    req.dev_nonce = int(dev_nonce) if isinstance(dev_nonce, str) else dev_nonce
                    req.valid = True
                    
                    self._handle_join_request(req)
    
    def analyze(self) -> Dict:
        """Run analysis and return results"""
        # Calculate risk for all devices
        for device in self.devices.values():
            device.calculate_risk()
        
        # Sort devices by risk
        high_risk = [d for d in self.devices.values() if d.risk_score >= 40]
        medium_risk = [d for d in self.devices.values() if 15 <= d.risk_score < 40]
        low_risk = [d for d in self.devices.values() if d.risk_score < 15]
        
        # Calculate join success rate
        total_requests = self.stats['join_requests']
        total_accepts = self.stats['join_accepts']
        success_rate = total_accepts / total_requests if total_requests > 0 else 0
        
        return {
            'summary': {
                'total_packets': self.stats['total_packets'],
                'join_requests': total_requests,
                'join_accepts': total_accepts,
                'rejoin_requests': self.stats['rejoin_requests'],
                'unique_devices': self.stats['unique_devices'],
                'unique_networks': len(self.stats['unique_join_euis']),
                'join_success_rate': round(success_rate * 100, 1),
                'nonce_reuse_events': self.stats['nonce_reuse_events'],
            },
            'security': {
                'high_risk_devices': len(high_risk),
                'medium_risk_devices': len(medium_risk),
                'low_risk_devices': len(low_risk),
                'total_risk_score': sum(d.risk_score for d in self.devices.values()),
            },
            'devices': {
                dev_eui: {
                    'join_eui': device.join_eui,
                    'join_count': len(device.join_requests),
                    'unique_nonces': len(device.dev_nonces),
                    'nonce_reuse_count': device.nonce_reuse_count,
                    'estimated_accepts': device.estimated_accepts,
                    'risk_score': device.risk_score,
                    'risk_reasons': device.risk_reasons,
                    'first_seen': device.first_seen,
                    'last_seen': device.last_seen,
                }
                for dev_eui, device in self.devices.items()
            },
            'timeline_length': len(self.timeline),
        }
    
    def print_report(self):
        """Print human-readable analysis report"""
        results = self.analyze()
        
        print('=' * 70)
        print('LORAWAN JOIN SEQUENCE ANALYSIS')
        print('=' * 70)
        print()
        
        # Summary
        s = results['summary']
        print('SUMMARY:')
        print('-' * 50)
        print(f"  Total Packets Analyzed:    {s['total_packets']}")
        print(f"  Join Requests:             {s['join_requests']}")
        print(f"  Join Accepts:              {s['join_accepts']}")
        print(f"  Rejoin Requests:           {s['rejoin_requests']}")
        print(f"  Unique Devices (DevEUI):   {s['unique_devices']}")
        print(f"  Unique Networks (JoinEUI): {s['unique_networks']}")
        print(f"  Join Success Rate:         {s['join_success_rate']}%")
        print()
        
        # Security summary
        sec = results['security']
        print('SECURITY ASSESSMENT:')
        print('-' * 50)
        
        if sec['high_risk_devices'] > 0:
            print(f"  🔴 HIGH RISK Devices:      {sec['high_risk_devices']}")
        if sec['medium_risk_devices'] > 0:
            print(f"  🟡 MEDIUM RISK Devices:    {sec['medium_risk_devices']}")
        print(f"  🟢 LOW RISK Devices:       {sec['low_risk_devices']}")
        print()
        
        if s['nonce_reuse_events'] > 0:
            print(f"  ⚠️  CRITICAL: {s['nonce_reuse_events']} DevNonce reuse event(s) detected!")
            print("     This indicates potential replay attack vulnerability.")
            print()
        
        # Device details
        if self.devices:
            print('DEVICE DETAILS:')
            print('-' * 50)
            
            # Sort by risk score
            sorted_devices = sorted(
                self.devices.values(),
                key=lambda d: d.risk_score,
                reverse=True
            )
            
            for device in sorted_devices:
                risk_color = '🔴' if device.risk_score >= 40 else '🟡' if device.risk_score >= 15 else '🟢'
                print(f"\n  {risk_color} DevEUI: {device.dev_eui}")
                print(f"     JoinEUI:       {device.join_eui or 'Unknown'}")
                print(f"     Join Requests: {len(device.join_requests)}")
                print(f"     Unique Nonces: {len(device.dev_nonces)}")
                print(f"     Nonce Reuses:  {device.nonce_reuse_count}")
                print(f"     Risk Score:    {device.risk_score}/100")
                
                if device.risk_reasons:
                    print("     Issues:")
                    for reason in device.risk_reasons:
                        print(f"       - {reason}")
                
                # Show duplicate nonces
                if device.duplicate_nonces:
                    print("     Duplicate Nonces:")
                    for nonce, ts1, ts2 in device.duplicate_nonces[:5]:
                        print(f"       - Nonce {nonce}: first at {ts1}ms, reused at {ts2}ms")
                    if len(device.duplicate_nonces) > 5:
                        print(f"       ... and {len(device.duplicate_nonces) - 5} more")
        
        print()
        print('=' * 70)
    
    def to_json(self) -> str:
        """Export analysis as JSON"""
        results = self.analyze()
        results['generated_at'] = datetime.now().isoformat()
        return json.dumps(results, indent=2, default=str)


def load_csv(filepath: Path, analyzer: JoinAnalyzer):
    """Load and process CSV file"""
    with open(filepath, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            analyzer.process_csv_row(row)


def load_pcap(filepath: Path, analyzer: JoinAnalyzer):
    """Load and process PCAP file"""
    # Use our custom PCAP format
    import sys
    sys.path.insert(0, str(filepath.parent.parent))
    
    try:
        from pcap_analyzer import PcapAnalyzer
        
        pcap = PcapAnalyzer()
        pcap.load_file(filepath)
        
        for pkt in pcap.packets:
            analyzer.process_packet(pkt.timestamp_ms, pkt.data)
    except ImportError:
        print("ERROR: Could not import pcap_analyzer. Ensure it's in the tools directory.")
        sys.exit(1)


def monitor_live(host: str, analyzer: JoinAnalyzer):
    """Monitor live from ESP32"""
    if not REQUESTS_AVAILABLE:
        print("ERROR: requests package required for live monitoring")
        sys.exit(1)
    
    try:
        import websocket
    except ImportError:
        print("ERROR: websocket-client required. Install with: pip install websocket-client")
        sys.exit(1)
    
    ws_url = f"ws://{host}/ws"
    print(f"Connecting to {ws_url}...")
    
    def on_message(ws, message):
        try:
            data = json.loads(message)
            if data.get('type') == 'packet':
                pkt = data.get('data', data)
                raw_hex = pkt.get('raw', pkt.get('data', ''))
                timestamp = pkt.get('timestamp_ms', 0)
                
                if raw_hex:
                    raw_data = bytes.fromhex(raw_hex.replace(' ', ''))
                    analyzer.process_packet(timestamp, raw_data)
        except (json.JSONDecodeError, ValueError):
            pass
    
    def on_error(ws, error):
        print(f"Error: {error}")
    
    def on_close(ws, close_status_code, close_msg):
        print("\nConnection closed. Final analysis:")
        analyzer.print_report()
    
    def on_open(ws):
        print("Connected! Monitoring for LoRaWAN join sequences...")
        print("Press Ctrl+C to stop and see analysis.\n")
    
    ws = websocket.WebSocketApp(
        ws_url,
        on_open=on_open,
        on_message=on_message,
        on_error=on_error,
        on_close=on_close
    )
    
    try:
        ws.run_forever()
    except KeyboardInterrupt:
        print("\nStopped.")
        analyzer.print_report()


def generate_demo_data(analyzer: JoinAnalyzer):
    """Generate realistic demo data with security issues for conference demos"""
    print("\n🎮 DEMO MODE: Generating simulated LoRaWAN join traffic...\n")
    
    # Define some fake devices
    devices = [
        ('70B3D5499A001234', '0011223344556677'),  # Smart meter
        ('70B3D5499A005678', '8899AABBCCDDEEFF'),  # Environmental sensor
        ('70B3D5499A009ABC', '1122334455667788'),  # Asset tracker
        ('0004A30B001C0FFE', 'AABBCCDDEEFF0011'),  # Vulnerable device (nonce reuse)
        ('0004A30B00DEADBE', 'DEADBEEFCAFEBABE'),  # Sequential nonces device
    ]
    
    base_time = int(time.time() * 1000)
    nonce_tracker = {}  # Track nonces per device for realism
    
    print("Simulating 30 minutes of join activity...\n")
    
    # Generate ~50 join requests over 30 simulated minutes
    for i in range(50):
        dev_eui, join_eui = random.choice(devices)
        timestamp_ms = base_time + (i * 36000)  # ~36 seconds apart on average
        
        # Initialize nonce tracking
        if dev_eui not in nonce_tracker:
            nonce_tracker[dev_eui] = {'nonces': [], 'style': 'random'}
            # Some devices have vulnerable patterns
            if 'DEAD' in dev_eui:
                nonce_tracker[dev_eui]['style'] = 'sequential'
            elif '1C0FFE' in dev_eui:
                nonce_tracker[dev_eui]['style'] = 'reuse'
        
        tracker = nonce_tracker[dev_eui]
        
        # Generate nonce based on device behavior
        if tracker['style'] == 'sequential':
            nonce = len(tracker['nonces']) + 1  # Sequential = predictable
        elif tracker['style'] == 'reuse' and tracker['nonces'] and random.random() < 0.4:
            nonce = random.choice(tracker['nonces'])  # Reuse = critical vuln
        else:
            nonce = random.randint(0, 65535)
        
        tracker['nonces'].append(nonce)
        
        # Build Join Request packet
        mhdr = MTYPE_JOIN_REQUEST << 5  # MType in bits 5-7
        join_eui_bytes = bytes.fromhex(join_eui)[::-1]  # Reverse for little-endian
        dev_eui_bytes = bytes.fromhex(dev_eui)[::-1]
        nonce_bytes = struct.pack('<H', nonce)
        mic = bytes([random.randint(0, 255) for _ in range(4)])
        
        raw_data = bytes([mhdr]) + join_eui_bytes + dev_eui_bytes + nonce_bytes + mic
        
        # Use process_packet which handles everything
        analyzer.process_packet(timestamp_ms, raw_data)
        
        # Occasionally add a join storm (5-10 rapid requests)
        if random.random() < 0.1:
            for j in range(random.randint(5, 10)):
                storm_ts = timestamp_ms + (j * 100)  # 100ms apart
                storm_nonce = random.randint(0, 65535)
                raw_data = bytes([mhdr]) + join_eui_bytes + dev_eui_bytes + \
                           struct.pack('<H', storm_nonce) + mic
                analyzer.process_packet(storm_ts, raw_data)
    
    # Simulate some join accepts (can't link to devices)
    for _ in range(30):
        timestamp_ms = base_time + random.randint(0, 1800000)
        mhdr = MTYPE_JOIN_ACCEPT << 5
        # 17 bytes total for accept without CFList
        raw_data = bytes([mhdr]) + bytes([random.randint(0, 255) for _ in range(16)])
        analyzer.process_packet(timestamp_ms, raw_data)
    
    print(f"Generated {analyzer.stats['join_requests']} join requests from {len(analyzer.devices)} devices")
    print(f"Simulated {analyzer.stats['join_accepts']} join accepts\n")


def main():
    parser = argparse.ArgumentParser(
        description='Analyze LoRaWAN join sequences for security issues',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
    %(prog)s capture.csv                # Analyze CSV capture
    %(prog)s capture.pcap --verbose     # Analyze PCAP with details
    %(prog)s capture.csv --json -o report.json
    %(prog)s --live 192.168.4.1         # Monitor live from ESP32

Security Issues Detected:
    - DevNonce reuse (critical - enables replay attacks)
    - Sequential nonces (high - predictable values)
    - Join storms (medium - possible attack)
    - Frequent counter resets (medium - device instability)
'''
    )
    
    parser.add_argument('input', nargs='?', help='Input file (CSV or PCAP)')
    parser.add_argument('--live', metavar='HOST',
                       help='Monitor live from ESP32 at IP address')
    parser.add_argument('--json', action='store_true',
                       help='Output JSON instead of human-readable report')
    parser.add_argument('-o', '--output', help='Output file path')
    parser.add_argument('-v', '--verbose', action='store_true',
                       help='Show each packet as processed')
    parser.add_argument('--demo', action='store_true',
                       help='Demo mode: generate simulated traffic with security issues')
    
    args = parser.parse_args()
    
    analyzer = JoinAnalyzer(verbose=args.verbose)
    
    # Demo mode
    if args.demo:
        generate_demo_data(analyzer)
        if args.json:
            output = analyzer.to_json()
            if args.output:
                with open(args.output, 'w') as f:
                    f.write(output)
                print(f"Saved to {args.output}")
            else:
                print(output)
        else:
            analyzer.print_report()
        return
    
    # Live mode
    if args.live:
        monitor_live(args.live, analyzer)
        return
    
    # File mode
    if not args.input:
        parser.print_help()
        sys.exit(1)
    
    input_path = Path(args.input)
    if not input_path.exists():
        print(f"ERROR: File not found: {input_path}")
        sys.exit(1)
    
    print(f"Loading {input_path}...")
    
    if input_path.suffix.lower() == '.csv':
        load_csv(input_path, analyzer)
    elif input_path.suffix.lower() == '.pcap':
        load_pcap(input_path, analyzer)
    else:
        print(f"ERROR: Unsupported file format: {input_path.suffix}")
        sys.exit(1)
    
    # Output
    if args.json:
        output = analyzer.to_json()
        if args.output:
            with open(args.output, 'w') as f:
                f.write(output)
            print(f"Saved to {args.output}")
        else:
            print(output)
    else:
        analyzer.print_report()
        if args.output:
            with open(args.output, 'w') as f:
                # Redirect print to file
                import io
                old_stdout = sys.stdout
                sys.stdout = io.StringIO()
                analyzer.print_report()
                output = sys.stdout.getvalue()
                sys.stdout = old_stdout
                f.write(output)
            print(f"Report saved to {args.output}")


if __name__ == '__main__':
    main()
