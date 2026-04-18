#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - REST API Client

Command-line client for interacting with ESP32 LoRa Sniffer's REST API.
Provides easy access to all device endpoints without needing a browser.

Usage:
    python api_client.py status
    python api_client.py devices
    python api_client.py devices --json
    python api_client.py capture 0x401ACD4E
    python api_client.py stop
    python api_client.py download-pcap
    python api_client.py download-geojson
    python api_client.py command r  # Resume recon

Requirements:
    pip install requests
"""

import argparse
import json
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
from datetime import datetime
from pathlib import Path

try:
    import requests
except ImportError:
    print("Error: requests library required. Install with: pip install requests")
    sys.exit(1)


class ReconClient:
    """REST API client for ESP32 LoRa Sniffer"""
    
    def __init__(self, host="192.168.4.1", port=80, timeout=10):
        self.base_url = f"http://{host}:{port}" if port != 80 else f"http://{host}"
        self.timeout = timeout
    
    def _get(self, endpoint):
        """Make GET request"""
        try:
            response = requests.get(f"{self.base_url}{endpoint}", timeout=self.timeout)
            response.raise_for_status()
            return response.json()
        except requests.exceptions.ConnectionError:
            print(f"❌ Cannot connect to {self.base_url}")
            print("   Make sure you're connected to the ESP32's WiFi network")
            sys.exit(1)
        except requests.exceptions.Timeout:
            print(f"❌ Request timed out after {self.timeout}s")
            sys.exit(1)
        except requests.exceptions.HTTPError as e:
            print(f"❌ HTTP Error: {e}")
            sys.exit(1)
    
    def _post(self, endpoint, data=None):
        """Make POST request"""
        try:
            response = requests.post(
                f"{self.base_url}{endpoint}", 
                json=data,
                timeout=self.timeout
            )
            response.raise_for_status()
            return response.json()
        except requests.exceptions.ConnectionError:
            print(f"❌ Cannot connect to {self.base_url}")
            sys.exit(1)
        except requests.exceptions.HTTPError as e:
            print(f"❌ HTTP Error: {e}")
            sys.exit(1)
    
    def _download(self, endpoint, output_path):
        """Download binary file"""
        try:
            response = requests.get(f"{self.base_url}{endpoint}", timeout=30)
            response.raise_for_status()
            with open(output_path, 'wb') as f:
                f.write(response.content)
            return len(response.content)
        except requests.exceptions.HTTPError as e:
            print(f"❌ Download failed: {e}")
            return 0
    
    # === Status & Info ===
    
    def get_status(self):
        """Get system status"""
        return self._get("/api/status")
    
    def get_dashboard(self):
        """Get dashboard data"""
        return self._get("/api/dashboard")
    
    def get_statistics(self):
        """Get detailed statistics"""
        return self._get("/api/statistics")
    
    def get_config(self):
        """Get scan configuration"""
        return self._get("/api/config")
    
    # === Device Operations ===
    
    def get_devices(self):
        """Get all discovered devices"""
        return self._get("/api/devices")
    
    def get_device(self, node_id):
        """Get specific device details"""
        return self._get(f"/api/device?nodeId={node_id}")
    
    def start_capture(self, node_id):
        """Start targeting a specific device"""
        return self._post("/api/capture/start", {"nodeId": node_id})
    
    def stop_capture(self):
        """Stop targeted capture, resume recon"""
        return self._post("/api/capture/stop")
    
    # === Scan Control ===
    
    def start_scan(self):
        """Start/resume scanning"""
        return self._post("/api/scan/start")
    
    def stop_scan(self):
        """Stop scanning"""
        return self._post("/api/scan/stop")
    
    def target_frequency(self, config_index):
        """Target specific frequency config"""
        return self._post("/api/frequency/target", {"configIndex": config_index})
    
    # === Replay Operations ===
    
    def get_replay_slots(self):
        """Get replay slot contents"""
        return self._get("/api/replay/slots")
    
    def replay_packet(self, slot_index):
        """Transmit packet from replay slot"""
        return self._post("/api/replay/transmit", {"slotIndex": slot_index})
    
    def clear_replay_slots(self):
        """Clear all replay slots"""
        return self._post("/api/replay/clear")
    
    # === Export Operations ===
    
    def download_pcap(self, output="capture.pcap"):
        """Download PCAP file"""
        size = self._download("/api/export/pcap", output)
        return size
    
    def download_geojson(self, output="positions.geojson"):
        """Download GeoJSON export"""
        data = self._get("/api/export/geojson")
        with open(output, 'w') as f:
            json.dump(data, f, indent=2)
        return output
    
    def download_kml(self, output="positions.kml"):
        """Download KML export"""
        try:
            response = requests.get(f"{self.base_url}/api/export/kml", timeout=30)
            response.raise_for_status()
            with open(output, 'w') as f:
                f.write(response.text)
            return output
        except Exception as e:
            print(f"❌ KML download failed: {e}")
            return None
    
    def download_report(self, output="lorecon-report.json"):
        """Download consolidated JSON report (security, devices, stats, GPS)"""
        data = self._get("/api/export/report")
        with open(output, 'w') as f:
            json.dump(data, f, indent=2)
        return output
    
    # === Geographic Data ===
    
    def get_positions(self):
        """Get GPS positions"""
        return self._get("/api/positions")
    
    # === Diagnostics ===
    
    def get_diagnostics(self):
        """Get diagnostic information"""
        return self._get("/api/diagnostics")
    
    def get_anomalies(self):
        """Get detected anomalies"""
        return self._get("/api/anomalies")
    
    def get_security_assessment(self):
        """Get security assessment"""
        return self._get("/api/recon/security")
    
    # === Commands ===
    
    def send_command(self, cmd):
        """Send serial-style command"""
        return self._post("/api/command", {"command": cmd})


def format_uptime(seconds):
    """Format seconds as human-readable duration"""
    if seconds < 60:
        return f"{seconds}s"
    elif seconds < 3600:
        return f"{seconds // 60}m {seconds % 60}s"
    else:
        hours = seconds // 3600
        minutes = (seconds % 3600) // 60
        return f"{hours}h {minutes}m"


def print_status(data):
    """Pretty-print status data"""
    print("\n📡 LoRecon Status")
    print("=" * 40)
    print(f"  Mode:        {data.get('mode', 'Unknown')}")
    print(f"  Uptime:      {format_uptime(data.get('uptime', 0))}")
    print(f"  Devices:     {data.get('devices', 0)}")
    print(f"  Packets:     {data.get('totalPackets', 0)}")
    print(f"  Captured:    {data.get('capturedPackets', 0)}")
    print(f"  Dropped:     {data.get('droppedPackets', 0)}")
    print(f"  Free Heap:   {data.get('freeHeap', 0):,} bytes")
    
    if data.get('target'):
        target = data['target']
        print(f"\n🎯 Active Target:")
        if target.get('nodeId'):
            print(f"  Node ID:     {target['nodeId']}")
        if target.get('frequency'):
            print(f"  Frequency:   {target['frequency']:.3f} MHz")


def print_devices(data, json_output=False):
    """Pretty-print device list"""
    if json_output:
        print(json.dumps(data, indent=2))
        return
    
    devices = data.get('devices', [])
    print(f"\n📱 Discovered Devices ({len(devices)} total)")
    print("=" * 70)
    
    if not devices:
        print("  No devices discovered yet")
        return
    
    print(f"{'#':<3} {'Node ID':<12} {'Protocol':<12} {'RSSI':>8} {'Packets':>8} {'Type':<15}")
    print("-" * 70)
    
    for dev in devices:
        idx = dev.get('index', '?')
        node_id = dev.get('nodeId', 'Unknown')[:10]
        protocol = dev.get('protocol', 'Unknown')[:10]
        rssi = dev.get('rssi', 0)
        packets = dev.get('packetCount', 0)
        dev_type = (dev.get('deviceType', 'Unknown') or 'Unknown')[:15]
        
        print(f"{idx:<3} {node_id:<12} {protocol:<12} {rssi:>7.1f} {packets:>8} {dev_type:<15}")


def print_replay_slots(data):
    """Pretty-print replay slots"""
    slots = data.get('slots', [])
    print(f"\n📦 Replay Slots ({len([s for s in slots if s.get('valid')])} used)")
    print("=" * 50)
    
    for i, slot in enumerate(slots):
        if slot.get('valid'):
            print(f"  Slot {i}: {slot.get('length', 0)} bytes, "
                  f"RSSI {slot.get('rssi', 0):.1f} dBm, "
                  f"{slot.get('protocol', 'Unknown')}")
        else:
            print(f"  Slot {i}: [empty]")


def main():
    parser = argparse.ArgumentParser(
        description='LoRecon REST API Client',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s status                    Show system status
  %(prog)s devices                   List discovered devices
  %(prog)s devices --json            List devices as JSON
  %(prog)s capture 0x401ACD4E        Start targeting device
  %(prog)s stop                      Stop capture, resume recon
  %(prog)s target-freq 5             Target frequency config #5
  %(prog)s download-pcap             Download PCAP to capture.pcap
  %(prog)s download-geojson          Download GeoJSON positions
  %(prog)s download-report           Download consolidated JSON report
  %(prog)s command r                 Send command (r=resume)
  %(prog)s security                  Show security assessment
"""
    )
    
    parser.add_argument('--host', default='192.168.4.1',
                       help='ESP32 IP address (default: 192.168.4.1)')
    parser.add_argument('--port', type=int, default=80,
                       help='HTTP port (default: 80)')
    parser.add_argument('--json', action='store_true',
                       help='Output raw JSON')
    parser.add_argument('--timeout', type=int, default=10,
                       help='Request timeout in seconds (default: 10)')
    
    subparsers = parser.add_subparsers(dest='command', help='Command to execute')
    
    # Status commands
    subparsers.add_parser('status', help='Show system status')
    subparsers.add_parser('dashboard', help='Show dashboard data')
    subparsers.add_parser('stats', help='Show detailed statistics')
    subparsers.add_parser('config', help='Show scan configuration')
    
    # Device commands
    subparsers.add_parser('devices', help='List discovered devices')
    dev_parser = subparsers.add_parser('device', help='Get specific device')
    dev_parser.add_argument('node_id', help='Node ID (hex, e.g., 0x401ACD4E)')
    
    # Capture commands
    cap_parser = subparsers.add_parser('capture', help='Start capturing device')
    cap_parser.add_argument('node_id', help='Node ID to capture')
    subparsers.add_parser('stop', help='Stop capture, resume recon')
    
    # Frequency targeting
    freq_parser = subparsers.add_parser('target-freq', help='Target frequency config')
    freq_parser.add_argument('config_index', type=int, help='Config index (0-25)')
    
    # Replay commands
    subparsers.add_parser('replay-slots', help='Show replay slot contents')
    replay_parser = subparsers.add_parser('replay', help='Transmit from replay slot')
    replay_parser.add_argument('slot', type=int, help='Slot index (0-9)')
    subparsers.add_parser('clear-replay', help='Clear all replay slots')
    
    # Export commands
    pcap_parser = subparsers.add_parser('download-pcap', help='Download PCAP file')
    pcap_parser.add_argument('-o', '--output', default='capture.pcap', help='Output filename')
    
    geojson_parser = subparsers.add_parser('download-geojson', help='Download GeoJSON')
    geojson_parser.add_argument('-o', '--output', default='positions.geojson', help='Output filename')
    
    kml_parser = subparsers.add_parser('download-kml', help='Download KML')
    kml_parser.add_argument('-o', '--output', default='positions.kml', help='Output filename')
    
    report_parser = subparsers.add_parser('download-report', help='Download consolidated report')
    report_parser.add_argument('-o', '--output', default='lorecon-report.json', help='Output filename')
    
    # Geographic data
    subparsers.add_parser('positions', help='Show GPS positions')
    
    # Diagnostics
    subparsers.add_parser('diagnostics', help='Show diagnostics')
    subparsers.add_parser('anomalies', help='Show detected anomalies')
    subparsers.add_parser('security', help='Show security assessment')
    
    # Raw command
    cmd_parser = subparsers.add_parser('command', help='Send serial-style command')
    cmd_parser.add_argument('cmd', help='Command character (m, r, f, etc.)')
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        sys.exit(1)
    
    client = ReconClient(host=args.host, port=args.port, timeout=args.timeout)
    
    # Execute command
    if args.command == 'status':
        data = client.get_status()
        if args.json:
            print(json.dumps(data, indent=2))
        else:
            print_status(data)
    
    elif args.command == 'dashboard':
        data = client.get_dashboard()
        print(json.dumps(data, indent=2))
    
    elif args.command == 'stats':
        data = client.get_statistics()
        print(json.dumps(data, indent=2))
    
    elif args.command == 'config':
        data = client.get_config()
        print(json.dumps(data, indent=2))
    
    elif args.command == 'devices':
        data = client.get_devices()
        print_devices(data, args.json)
    
    elif args.command == 'device':
        data = client.get_device(args.node_id)
        print(json.dumps(data, indent=2))
    
    elif args.command == 'capture':
        data = client.start_capture(args.node_id)
        print(f"✅ Started targeting {args.node_id}")
        print(json.dumps(data, indent=2))
    
    elif args.command == 'stop':
        data = client.stop_capture()
        print("✅ Stopped capture, resuming reconnaissance")
    
    elif args.command == 'target-freq':
        data = client.target_frequency(args.config_index)
        print(f"✅ Targeting frequency config #{args.config_index}")
    
    elif args.command == 'replay-slots':
        data = client.get_replay_slots()
        if args.json:
            print(json.dumps(data, indent=2))
        else:
            print_replay_slots(data)
    
    elif args.command == 'replay':
        data = client.replay_packet(args.slot)
        print(f"✅ Transmitted packet from slot {args.slot}")
    
    elif args.command == 'clear-replay':
        data = client.clear_replay_slots()
        print("✅ Cleared all replay slots")
    
    elif args.command == 'download-pcap':
        size = client.download_pcap(args.output)
        if size:
            print(f"✅ Downloaded {size:,} bytes to {args.output}")
    
    elif args.command == 'download-geojson':
        output = client.download_geojson(args.output)
        print(f"✅ Downloaded GeoJSON to {output}")
    
    elif args.command == 'download-kml':
        output = client.download_kml(args.output)
        if output:
            print(f"✅ Downloaded KML to {output}")
    
    elif args.command == 'download-report':
        output = client.download_report(args.output)
        print(f"✅ Downloaded consolidated report to {output}")
    
    elif args.command == 'positions':
        data = client.get_positions()
        print(json.dumps(data, indent=2))
    
    elif args.command == 'diagnostics':
        data = client.get_diagnostics()
        print(json.dumps(data, indent=2))
    
    elif args.command == 'anomalies':
        data = client.get_anomalies()
        print(json.dumps(data, indent=2))
    
    elif args.command == 'security':
        data = client.get_security_assessment()
        print(json.dumps(data, indent=2))
    
    elif args.command == 'command':
        data = client.send_command(args.cmd)
        print(f"✅ Sent command '{args.cmd}'")
        if data:
            print(json.dumps(data, indent=2))


if __name__ == '__main__':
    main()
