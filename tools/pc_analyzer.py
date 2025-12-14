#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - PC Analysis Tool

Analyzes captured packet logs from SD card for:
- Device mapping and tracking
- Signal strength heatmaps
- Protocol distribution
- Timeline visualization
- Geographic mapping (if GPS data available)

Supports both CSV and JSONL log formats.

Usage:
    python pc_analyzer.py logs/recon_123456.csv
    python pc_analyzer.py logs/packets.jsonl
    python pc_analyzer.py logs/  # Analyze all logs in directory
    python pc_analyzer.py logs/capture.csv --json results.json
"""

import csv
import json
import argparse
from pathlib import Path
from datetime import datetime, timedelta
from collections import defaultdict
import statistics

class PacketAnalyzer:
    def __init__(self, log_file):
        self.log_file = Path(log_file)
        self.packets = []
        self.devices = defaultdict(lambda: {
            'packets': [],
            'rssi_values': [],
            'protocols': set(),
            'first_seen': None,
            'last_seen': None
        })
        self.format = self._detect_format()
    
    def _detect_format(self):
        """Detect log file format (CSV or JSONL)"""
        suffix = self.log_file.suffix.lower()
        if suffix == '.jsonl' or suffix == '.ndjson':
            return 'jsonl'
        elif suffix == '.json':
            # Check if it's actually JSONL (one JSON per line)
            with open(self.log_file, 'r') as f:
                first_line = f.readline().strip()
                second_line = f.readline().strip()
                if first_line.startswith('{') and second_line.startswith('{'):
                    return 'jsonl'
            return 'json'
        else:
            return 'csv'
    
    def load_data(self):
        """Load packets from log file (auto-detects format)"""
        print(f"Loading {self.log_file} (format: {self.format})...")
        
        if self.format == 'csv':
            self._load_csv()
        elif self.format == 'jsonl':
            self._load_jsonl()
        elif self.format == 'json':
            self._load_json()
        else:
            raise ValueError(f"Unknown format: {self.format}")
        
        print(f"Loaded {len(self.packets)} packets from {len(self.devices)} devices")
    
    def _load_csv(self):
        """Load packets from CSV file"""
        with open(self.log_file, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                # Handle various CSV column name formats
                timestamp = int(row.get('timestamp', row.get('timestamp_ms', 0))) / 1000
                node_id = row.get('nodeId', row.get('node_id', row.get('node_id_hex', 'Unknown')))
                protocol = row.get('protocol', 'Unknown')
                rssi = float(row.get('rssi', row.get('rssi_dbm', 0)))
                snr = float(row.get('snr', row.get('snr_db', 0)))
                length = int(row.get('length', row.get('length_bytes', 0)))
                data = row.get('hex_data', row.get('data', ''))
                
                self._add_packet(timestamp, node_id, protocol, rssi, snr, length, data)
    
    def _load_jsonl(self):
        """Load packets from JSONL file (one JSON object per line)"""
        with open(self.log_file, 'r') as f:
            for line_num, line in enumerate(f, 1):
                line = line.strip()
                if not line:
                    continue
                try:
                    row = json.loads(line)
                    timestamp = row.get('timestamp', row.get('timestamp_ms', 0))
                    if timestamp > 1e12:  # Milliseconds
                        timestamp = timestamp / 1000
                    
                    node_id = row.get('nodeId', row.get('node_id', 'Unknown'))
                    protocol = row.get('protocol', 'Unknown')
                    rssi = float(row.get('rssi', 0))
                    snr = float(row.get('snr', 0))
                    length = int(row.get('length', 0))
                    data = row.get('data', row.get('hex_data', ''))
                    
                    self._add_packet(timestamp, node_id, protocol, rssi, snr, length, data)
                except json.JSONDecodeError as e:
                    print(f"Warning: Skipping invalid JSON at line {line_num}: {e}")
    
    def _load_json(self):
        """Load packets from JSON array file"""
        with open(self.log_file, 'r') as f:
            data = json.load(f)
        
        # Handle both array format and {packets: [...]} format
        packets = data if isinstance(data, list) else data.get('packets', [])
        
        for row in packets:
            timestamp = row.get('timestamp', row.get('timestamp_ms', 0))
            if timestamp > 1e12:
                timestamp = timestamp / 1000
            
            node_id = row.get('nodeId', row.get('node_id', 'Unknown'))
            protocol = row.get('protocol', 'Unknown')
            rssi = float(row.get('rssi', 0))
            snr = float(row.get('snr', 0))
            length = int(row.get('length', 0))
            data = row.get('data', row.get('hex_data', ''))
            
            self._add_packet(timestamp, node_id, protocol, rssi, snr, length, data)
    
    def _add_packet(self, timestamp, node_id, protocol, rssi, snr, length, data):
        """Add a packet to the analyzer"""
        packet = {
            'timestamp': timestamp,
            'node_id': node_id,
            'protocol': protocol,
            'rssi': rssi,
            'snr': snr,
            'length': length,
            'data': data
        }
        
        self.packets.append(packet)
        
        # Track per-device statistics
        device = self.devices[node_id]
        device['packets'].append(packet)
        device['rssi_values'].append(rssi)
        device['protocols'].add(protocol)
        
        if device['first_seen'] is None:
            device['first_seen'] = timestamp
        device['last_seen'] = timestamp
        
    def analyze_devices(self):
        """Analyze device characteristics"""
        print("\n" + "="*70)
        print("DEVICE ANALYSIS")
        print("="*70)
        
        for node_id, data in sorted(self.devices.items(), 
                                    key=lambda x: len(x[1]['packets']), 
                                    reverse=True):
            packets = data['packets']
            rssi_vals = data['rssi_values']
            
            duration = data['last_seen'] - data['first_seen']
            packet_rate = len(packets) / max(duration, 1)
            
            print(f"\n{node_id}")
            print(f"  Packets: {len(packets)}")
            print(f"  Protocols: {', '.join(data['protocols'])}")
            print(f"  RSSI: avg={statistics.mean(rssi_vals):.1f} dBm, "
                  f"min={min(rssi_vals):.1f}, max={max(rssi_vals):.1f}")
            print(f"  Active duration: {duration:.1f}s ({packet_rate:.2f} pkt/s)")
            
            # Classify device based on behavior
            if packet_rate > 0.5:
                print(f"  Classification: 🔥 High activity (router/base)")
            elif statistics.mean(rssi_vals) > -70:
                print(f"  Classification: 📡 Close range (mobile/handheld)")
            else:
                print(f"  Classification: 📱 Standard device")
    
    def analyze_protocols(self):
        """Analyze protocol distribution"""
        print("\n" + "="*70)
        print("PROTOCOL DISTRIBUTION")
        print("="*70)
        
        protocol_counts = defaultdict(int)
        for packet in self.packets:
            protocol_counts[packet['protocol']] += 1
        
        for protocol, count in sorted(protocol_counts.items(), 
                                     key=lambda x: x[1], 
                                     reverse=True):
            percentage = (count / len(self.packets)) * 100
            print(f"  {protocol:20s} {count:5d} packets ({percentage:5.1f}%)")
    
    def analyze_timeline(self):
        """Analyze activity over time"""
        print("\n" + "="*70)
        print("TIMELINE ANALYSIS")
        print("="*70)
        
        if not self.packets:
            return
        
        # Calculate activity in 60-second bins
        start_time = self.packets[0]['timestamp']
        end_time = self.packets[-1]['timestamp']
        duration = end_time - start_time
        
        print(f"  Total duration: {duration:.1f} seconds ({duration/60:.1f} minutes)")
        print(f"  Total packets: {len(self.packets)}")
        print(f"  Average rate: {len(self.packets)/max(duration, 1):.2f} pkt/s")
        
        # Find peak activity
        bin_size = 60  # 1 minute bins
        bins = defaultdict(int)
        for packet in self.packets:
            bin_idx = int((packet['timestamp'] - start_time) / bin_size)
            bins[bin_idx] += 1
        
        if bins:
            peak_bin = max(bins.items(), key=lambda x: x[1])
            peak_time = start_time + (peak_bin[0] * bin_size)
            print(f"  Peak activity: {peak_bin[1]} packets in 1-minute window")
    
    def export_json(self, output_file):
        """Export analysis results as JSON"""
        results = {
            'summary': {
                'total_packets': len(self.packets),
                'total_devices': len(self.devices),
                'capture_file': str(self.log_file)
            },
            'devices': []
        }
        
        for node_id, data in self.devices.items():
            results['devices'].append({
                'node_id': node_id,
                'packet_count': len(data['packets']),
                'protocols': list(data['protocols']),
                'avg_rssi': statistics.mean(data['rssi_values']),
                'min_rssi': min(data['rssi_values']),
                'max_rssi': max(data['rssi_values']),
                'first_seen': data['first_seen'],
                'last_seen': data['last_seen'],
                'duration': data['last_seen'] - data['first_seen']
            })
        
        with open(output_file, 'w') as f:
            json.dump(results, f, indent=2)
        
        print(f"\n✅ JSON export saved to {output_file}")
    
    def generate_report(self):
        """Generate comprehensive analysis report"""
        self.analyze_devices()
        self.analyze_protocols()
        self.analyze_timeline()

def main():
    parser = argparse.ArgumentParser(
        description='Analyze ESP32 LoRa Sniffer packet captures (CSV or JSONL)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s capture.csv                  Analyze CSV file
  %(prog)s packets.jsonl                Analyze JSONL file  
  %(prog)s logs/                        Analyze all logs in directory
  %(prog)s capture.csv --json out.json  Export analysis as JSON
"""
    )
    parser.add_argument('input', help='CSV/JSONL file or directory to analyze')
    parser.add_argument('--json', help='Export results as JSON')
    parser.add_argument('--summary', action='store_true', 
                       help='Show summary only')
    
    args = parser.parse_args()
    
    input_path = Path(args.input)
    
    # Handle directory input
    if input_path.is_dir():
        # Find all supported log files
        log_files = (list(input_path.glob('*.csv')) + 
                     list(input_path.glob('*.jsonl')) +
                     list(input_path.glob('*.ndjson')))
        
        if not log_files:
            print(f"No CSV or JSONL files found in {input_path}")
            return
        
        print(f"Found {len(log_files)} capture files:")
        for i, f in enumerate(log_files, 1):
            print(f"  {i}. {f.name}")
        
        # For now, analyze first file (TODO: merge multiple files)
        log_file = log_files[0]
        print(f"\nAnalyzing: {log_file.name}\n")
    else:
        log_file = input_path
    
    # Run analysis
    analyzer = PacketAnalyzer(log_file)
    analyzer.load_data()
    analyzer.generate_report()
    
    # Export JSON if requested
    if args.json:
        analyzer.export_json(args.json)

if __name__ == '__main__':
    main()
