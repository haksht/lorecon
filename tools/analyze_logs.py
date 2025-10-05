#!/usr/bin/env python3
"""
ESP32 LoRa Recon - Log Analysis Tool

Analyzes JSON logs from the ESP32 LoRa reconnaissance tool.
Provides statistical analysis and visualization of captured LoRa traffic.

Usage:
    python analyze_logs.py packets.jsonl
    python analyze_logs.py packets.jsonl --format csv
    python analyze_logs.py packets.jsonl --plot
"""

import json
import argparse
import pandas as pd
import matplotlib.pyplot as plt
from collections import defaultdict, Counter
from datetime import datetime
import sys

def load_packets(filename):
    """Load packets from JSONL log file"""
    packets = []
    try:
        with open(filename, 'r') as f:
            for line in f:
                if line.strip():
                    packets.append(json.loads(line))
        return packets
    except FileNotFoundError:
        print(f"Error: File {filename} not found")
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"Error parsing JSON: {e}")
        sys.exit(1)

def analyze_protocols(packets):
    """Analyze protocol distribution"""
    protocols = Counter(p['protocol'] for p in packets)
    
    print("=== Protocol Distribution ===")
    for protocol, count in protocols.most_common():
        percentage = (count / len(packets)) * 100
        print(f"{protocol:12}: {count:4} packets ({percentage:5.1f}%)")
    print()
    
    return protocols

def analyze_frequencies(packets):
    """Analyze frequency usage"""
    frequencies = Counter(p['frequency'] for p in packets)
    
    print("=== Frequency Distribution ===")
    for freq, count in sorted(frequencies.items()):
        percentage = (count / len(packets)) * 100
        print(f"{freq:8.3f} MHz: {count:4} packets ({percentage:5.1f}%)")
    print()
    
    return frequencies

def analyze_signal_quality(packets):
    """Analyze signal strength and quality"""
    rssi_values = [p['rssi'] for p in packets if 'rssi' in p]
    snr_values = [p['snr'] for p in packets if 'snr' in p]
    
    print("=== Signal Quality Analysis ===")
    if rssi_values:
        print(f"RSSI: min={min(rssi_values):6.1f} dBm, "
              f"max={max(rssi_values):6.1f} dBm, "
              f"avg={sum(rssi_values)/len(rssi_values):6.1f} dBm")
    
    if snr_values:
        print(f"SNR:  min={min(snr_values):6.1f} dB, "
              f"max={max(snr_values):6.1f} dB, "
              f"avg={sum(snr_values)/len(snr_values):6.1f} dB")
    print()
    
    return rssi_values, snr_values

def analyze_temporal_patterns(packets):
    """Analyze temporal patterns in traffic"""
    if not packets:
        return
    
    # Convert timestamps to relative times (seconds from first packet)
    first_ts = min(p['timestamp'] for p in packets)
    rel_times = [(p['timestamp'] - first_ts) / 1000.0 for p in packets]
    
    # Group by protocol
    protocol_times = defaultdict(list)
    for packet, time in zip(packets, rel_times):
        protocol_times[packet['protocol']].append(time)
    
    print("=== Temporal Analysis ===")
    print(f"Capture duration: {max(rel_times):.1f} seconds")
    print(f"Total packets: {len(packets)}")
    print(f"Average rate: {len(packets) / max(rel_times):.2f} packets/sec")
    
    for protocol, times in protocol_times.items():
        if len(times) > 1:
            intervals = [times[i] - times[i-1] for i in range(1, len(times))]
            avg_interval = sum(intervals) / len(intervals)
            print(f"{protocol} avg interval: {avg_interval:.1f}s")
    print()

def analyze_packet_sizes(packets):
    """Analyze packet size distribution"""
    sizes = [p['length'] for p in packets if 'length' in p]
    
    print("=== Packet Size Analysis ===")
    if sizes:
        print(f"Size: min={min(sizes):3} bytes, "
              f"max={max(sizes):3} bytes, "
              f"avg={sum(sizes)/len(sizes):5.1f} bytes")
        
        # Size distribution
        size_ranges = [(0, 10), (11, 50), (51, 100), (101, 200), (201, 999)]
        for min_size, max_size in size_ranges:
            count = sum(1 for s in sizes if min_size <= s <= max_size)
            if count > 0:
                pct = (count / len(sizes)) * 100
                print(f"{min_size:3}-{max_size:3} bytes: {count:4} packets ({pct:5.1f}%)")
    print()

def extract_node_info(packets):
    """Extract and analyze node information from packet data"""
    nodes = defaultdict(list)
    
    for packet in packets:
        # Simple node extraction from Meshtastic packets
        if packet['protocol'] == 'Meshtastic' and 'data' in packet:
            data = packet['data']
            if len(data) >= 16:  # At least 8 bytes (16 hex chars)
                # Extract potential node ID from bytes 4-7 (positions 8-15 in hex)
                node_hex = data[8:16]
                if len(node_hex) == 8:
                    try:
                        node_id = int(node_hex, 16)
                        nodes[node_id].append({
                            'timestamp': packet['timestamp'],
                            'rssi': packet.get('rssi', 0),
                            'frequency': packet['frequency']
                        })
                    except ValueError:
                        pass
    
    print("=== Node Analysis ===")
    print(f"Unique nodes detected: {len(nodes)}")
    
    for node_id, packets_list in list(nodes.items())[:10]:  # Show top 10
        avg_rssi = sum(p['rssi'] for p in packets_list) / len(packets_list)
        frequencies = set(p['frequency'] for p in packets_list)
        print(f"Node 0x{node_id:08X}: {len(packets_list):3} packets, "
              f"avg RSSI {avg_rssi:6.1f} dBm, "
              f"freqs: {sorted(frequencies)}")
    
    if len(nodes) > 10:
        print(f"... and {len(nodes) - 10} more nodes")
    print()

def export_csv(packets, filename):
    """Export packets to CSV format"""
    df = pd.DataFrame(packets)
    csv_filename = filename.replace('.jsonl', '.csv')
    df.to_csv(csv_filename, index=False)
    print(f"Exported {len(packets)} packets to {csv_filename}")

def plot_analysis(packets):
    """Create visualization plots"""
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not available for plotting")
        return
    
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(12, 8))
    
    # Protocol distribution
    protocols = Counter(p['protocol'] for p in packets)
    ax1.pie(protocols.values(), labels=protocols.keys(), autopct='%1.1f%%')
    ax1.set_title('Protocol Distribution')
    
    # RSSI distribution
    rssi_values = [p['rssi'] for p in packets if 'rssi' in p]
    if rssi_values:
        ax2.hist(rssi_values, bins=20, alpha=0.7)
        ax2.set_xlabel('RSSI (dBm)')
        ax2.set_ylabel('Count')
        ax2.set_title('Signal Strength Distribution')
    
    # Frequency usage
    frequencies = [p['frequency'] for p in packets]
    ax3.hist(frequencies, bins=15, alpha=0.7)
    ax3.set_xlabel('Frequency (MHz)')
    ax3.set_ylabel('Count')
    ax3.set_title('Frequency Usage')
    
    # Temporal pattern
    if packets:
        first_ts = min(p['timestamp'] for p in packets)
        rel_times = [(p['timestamp'] - first_ts) / 1000.0 for p in packets]
        ax4.scatter(rel_times, rssi_values[:len(rel_times)], alpha=0.6)
        ax4.set_xlabel('Time (seconds)')
        ax4.set_ylabel('RSSI (dBm)')
        ax4.set_title('Signal Strength Over Time')
    
    plt.tight_layout()
    plt.savefig('lora_analysis.png', dpi=150, bbox_inches='tight')
    plt.show()
    print("Plot saved as lora_analysis.png")

def main():
    parser = argparse.ArgumentParser(description='Analyze ESP32 LoRa reconnaissance logs')
    parser.add_argument('logfile', help='JSONL log file from ESP32 tool')
    parser.add_argument('--format', choices=['csv'], help='Export format')
    parser.add_argument('--plot', action='store_true', help='Generate plots')
    
    args = parser.parse_args()
    
    print(f"Loading packets from {args.logfile}...")
    packets = load_packets(args.logfile)
    
    if not packets:
        print("No packets found in log file")
        return
    
    print(f"Loaded {len(packets)} packets\n")
    
    # Run analysis
    analyze_protocols(packets)
    analyze_frequencies(packets)
    analyze_signal_quality(packets)
    analyze_temporal_patterns(packets)
    analyze_packet_sizes(packets)
    extract_node_info(packets)
    
    # Export if requested
    if args.format == 'csv':
        export_csv(packets, args.logfile)
    
    # Plot if requested
    if args.plot:
        plot_analysis(packets)

if __name__ == '__main__':
    main()