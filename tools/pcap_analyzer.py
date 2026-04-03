#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - PCAP Analyzer

Analyzes PCAP files exported from ESP32 LoRa Sniffer.
Extracts LoRa metadata from custom pseudo-header and provides analysis.

The ESP32 exports PCAP files with a custom link layer type that includes:
- RSSI (signal strength in dBm)
- SNR (signal-to-noise ratio in dB)
- Frequency (in MHz)
- Timestamp

Usage:
    python pcap_analyzer.py capture.pcap
    python pcap_analyzer.py capture.pcap --json
    python pcap_analyzer.py capture.pcap --export-csv output.csv
    python pcap_analyzer.py capture.pcap --wireshark  # Open in Wireshark

Requirements:
    pip install scapy  (optional, for full analysis)
    
Note: Basic analysis works without scapy using raw PCAP parsing.
"""

import argparse
import json
import struct
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
from collections import defaultdict
from datetime import datetime
from pathlib import Path

# Wireshark exporter (same repo, dev/ subdirectory)
sys.path.insert(0, str(Path(__file__).parent / 'dev'))
try:
    from wireshark_exporter import parse_esp32_pcap, write_loratap_pcap, open_wireshark as _ws_open
    EXPORTER_AVAILABLE = True
except ImportError:
    EXPORTER_AVAILABLE = False

sys.path.insert(0, str(Path(__file__).parent / 'meshtastic'))
try:
    from psk_decrypt import MeshtasticDecryptor
    _decryptor = MeshtasticDecryptor()
    PSK_AVAILABLE = True
except Exception:
    PSK_AVAILABLE = False

# Try optional scapy import
try:
    from scapy.all import rdpcap, raw, Packet
    SCAPY_AVAILABLE = True
except ImportError:
    SCAPY_AVAILABLE = False


# PCAP file format constants
PCAP_MAGIC = 0xa1b2c3d4
PCAP_MAGIC_NANO = 0xa1b23c4d
PCAP_LINKTYPE_USER0 = 147  # DLT_USER0 for custom protocol

# Custom LoRa pseudo-header format (from pcap_logger.h)
# struct LoRaPseudoHeader {
#     float frequencyMHz;      // 4 bytes
#     float rssiDbm;           // 4 bytes
#     float snrDb;             // 4 bytes
#     uint8_t spreadingFactor; // 1 byte
#     uint32_t bandwidth;      // 4 bytes
#     uint8_t codingRate;      // 1 byte
#     uint16_t reserved;       // 2 bytes (padding)
# } __attribute__((packed));   // Total: 20 bytes
LORA_HEADER_FORMAT = '<fffBIBH'  # Little-endian: 3 floats, byte, uint32, byte, uint16
LORA_HEADER_SIZE = struct.calcsize(LORA_HEADER_FORMAT)  # Should be 20 bytes


class LoRaPacket:
    """Represents a LoRa packet with metadata"""
    
    def __init__(self, timestamp_ms, rssi, snr, frequency_mhz, 
                 config_index, data, pcap_timestamp=None):
        self.timestamp_ms = timestamp_ms
        self.rssi = rssi
        self.snr = snr
        self.frequency_mhz = frequency_mhz
        self.config_index = config_index
        self.data = data
        self.pcap_timestamp = pcap_timestamp
        
        # Extended radio metadata (populated by parser)
        self.spreading_factor = 0
        self.bandwidth = 0
        self.coding_rate = 0
        
        # Derived fields
        self.length = len(data)
        self.protocol = self._identify_protocol()
        self.node_id = self._extract_node_id()
        self.lat = None
        self.lon = None
        self._extract_gps()
    
    def _identify_protocol(self):
        """Identify protocol from packet signature"""
        if len(self.data) < 4:
            return 'Short'
        
        # Meshtastic: 4-byte magic header 0xFFFFFFFF
        if self.data[0:4] == b'\xff\xff\xff\xff':
            return 'Meshtastic'
        
        # LoRaWAN: Check MHDR mtype field
        if 12 <= len(self.data) <= 51:
            mtype = (self.data[0] >> 5) & 0x07
            if mtype <= 0x07:
                return 'LoRaWAN'
        
        if len(self.data) <= 8:
            return 'Beacon'
        
        return 'Unknown'
    
    def _extract_node_id(self):
        """Extract node ID if present"""
        if self.protocol == 'Meshtastic' and len(self.data) >= 8:
            # Node ID at bytes 4-7 (little-endian)
            node_id = struct.unpack('<I', self.data[4:8])[0]
            return f"0x{node_id:08X}"
        
        if self.protocol == 'LoRaWAN' and len(self.data) >= 8:
            # DevAddr at bytes 1-4 (little-endian)
            dev_addr = struct.unpack('<I', self.data[1:5])[0]
            return f"0x{dev_addr:08X}"
        
        return None
    
    def _extract_gps(self):
        """
        Extract GPS coordinates from Meshtastic position packets.

        Tries PSK decryption first (covers encrypted packets using default keys),
        then falls back to checking if the payload is already plaintext.
        Parses the decrypted Data protobuf for portnum POSITION (0x03) and
        extracts latitude_i / longitude_i (signed int32, scaled by 1e7).
        """
        if self.protocol != 'Meshtastic' or len(self.data) < 20:
            return

        plaintext = None

        # Try PSK decryption (handles encrypted packets with default keys)
        if PSK_AVAILABLE:
            try:
                result = _decryptor.try_decrypt(self.data)
                if result and result.portnum_name == 'POSITION':
                    plaintext = result.plaintext
            except Exception:
                pass

        # Fall back: check if payload is already unencrypted
        if plaintext is None:
            payload = self.data[20:]  # skip 4-byte magic + 16-byte header
            if len(payload) >= 2 and payload[0] == 0x08 and payload[1] == 0x03:
                plaintext = payload

        if plaintext is None:
            return

        # plaintext is a Data protobuf. Find field 2 (0x12) = the Position bytes.
        # Structure: 0x08 0x03 0x12 <len> <Position protobuf>
        try:
            i = 0
            pos_data = None
            while i < len(plaintext) - 1:
                tag = plaintext[i]; i += 1
                field = tag >> 3
                wire  = tag & 0x07
                if wire == 2 and field == 2:          # length-delimited, field 2 = payload
                    length = plaintext[i]; i += 1
                    pos_data = plaintext[i:i + length]
                    break
                elif wire == 0:                        # skip varint
                    while i < len(plaintext) and (plaintext[i] & 0x80):
                        i += 1
                    i += 1
                else:
                    break

            if not pos_data:
                return

            # Parse Position protobuf: field 1 = latitude_i, field 2 = longitude_i (varints)
            i = 0
            lat_i = lon_i = None
            while i < len(pos_data):
                tag = pos_data[i]; i += 1
                field = tag >> 3
                wire  = tag & 0x07
                if wire == 0:
                    val = 0; shift = 0
                    while i < len(pos_data):
                        b = pos_data[i]; i += 1
                        val |= (b & 0x7F) << shift
                        shift += 7
                        if not (b & 0x80):
                            break
                    if val >= (1 << 31):
                        val -= (1 << 32)
                    if field == 1:
                        lat_i = val
                    elif field == 2:
                        lon_i = val
                elif wire == 2:                        # skip length-delimited
                    length = pos_data[i]; i += 1
                    i += length
                else:
                    break

            if lat_i and lon_i:
                self.lat = lat_i / 1e7
                self.lon = lon_i / 1e7
        except Exception:
            pass

    def to_dict(self):
        """Convert to dictionary"""
        result = {
            'timestamp_ms': self.timestamp_ms,
            'pcap_timestamp': self.pcap_timestamp.isoformat() if self.pcap_timestamp else None,
            'rssi': round(self.rssi, 1),
            'snr': round(self.snr, 1),
            'frequency_mhz': round(self.frequency_mhz, 3),
            'spreading_factor': self.spreading_factor,
            'bandwidth_hz': self.bandwidth,
            'coding_rate': self.coding_rate,
            'length': self.length,
            'protocol': self.protocol,
            'node_id': self.node_id,
            'data_hex': self.data.hex(),
            'lat': self.lat,
            'lon': self.lon,
        }
        return result


def parse_pcap_native(pcap_path):
    """Parse PCAP file without scapy (native implementation)"""
    packets = []
    
    with open(pcap_path, 'rb') as f:
        # Read global header (24 bytes)
        global_header = f.read(24)
        if len(global_header) < 24:
            raise ValueError("Invalid PCAP file: too short")
        
        magic, version_major, version_minor, thiszone, sigfigs, snaplen, linktype = \
            struct.unpack('<IHHIIII', global_header)
        
        # Check magic number
        if magic == PCAP_MAGIC:
            ts_resolution = 'microseconds'
        elif magic == PCAP_MAGIC_NANO:
            ts_resolution = 'nanoseconds'
        elif magic == 0xd4c3b2a1:  # Big-endian
            raise ValueError("Big-endian PCAP not supported")
        else:
            raise ValueError(f"Invalid PCAP magic: 0x{magic:08x}")
        
        print(f"PCAP Version: {version_major}.{version_minor}")
        print(f"Link type: {linktype} {'(USER0/Custom)' if linktype == PCAP_LINKTYPE_USER0 else ''}")
        print(f"Timestamp resolution: {ts_resolution}")
        print()
        
        # Read packets
        while True:
            # Read packet header (16 bytes)
            pkt_header = f.read(16)
            if len(pkt_header) < 16:
                break  # EOF
            
            ts_sec, ts_usec, incl_len, orig_len = struct.unpack('<IIII', pkt_header)
            
            # Calculate timestamp
            if ts_resolution == 'nanoseconds':
                pcap_ts = datetime.fromtimestamp(ts_sec + ts_usec / 1e9)
            else:
                pcap_ts = datetime.fromtimestamp(ts_sec + ts_usec / 1e6)
            
            # Read packet data
            pkt_data = f.read(incl_len)
            if len(pkt_data) < incl_len:
                print(f"Warning: Truncated packet at offset {f.tell()}")
                break
            
            # Parse LoRa pseudo-header if present (20 bytes)
            if len(pkt_data) >= LORA_HEADER_SIZE:
                header = struct.unpack(LORA_HEADER_FORMAT, pkt_data[:LORA_HEADER_SIZE])
                frequency_mhz, rssi, snr, spreading_factor, bandwidth, coding_rate, reserved = header
                
                # Extract payload (after pseudo-header)
                payload = pkt_data[LORA_HEADER_SIZE:]
                
                packet = LoRaPacket(
                    timestamp_ms=int(ts_sec * 1000 + ts_usec / 1000),  # Derive from PCAP timestamp
                    rssi=rssi,
                    snr=snr,
                    frequency_mhz=frequency_mhz,
                    config_index=spreading_factor,  # Use SF as config indicator
                    data=payload,
                    pcap_timestamp=pcap_ts
                )
                # Store additional metadata
                packet.spreading_factor = spreading_factor
                packet.bandwidth = bandwidth
                packet.coding_rate = coding_rate
                packets.append(packet)
            else:
                # No pseudo-header, treat entire packet as data
                packet = LoRaPacket(
                    timestamp_ms=0,
                    rssi=0,
                    snr=0,
                    frequency_mhz=0,
                    config_index=0,
                    data=pkt_data,
                    pcap_timestamp=pcap_ts
                )
                packets.append(packet)
    
    return packets


def analyze_packets(packets):
    """Generate analysis report from packets"""
    if not packets:
        print("No packets found in PCAP file")
        return
    
    print(f"\n{'='*60}")
    print(f"📊 PCAP ANALYSIS REPORT")
    print(f"{'='*60}")
    
    # Basic statistics
    print(f"\n📦 Packet Statistics")
    print(f"   Total packets: {len(packets)}")
    
    # Time range
    timestamps = [p.pcap_timestamp for p in packets if p.pcap_timestamp]
    if timestamps:
        duration = (max(timestamps) - min(timestamps)).total_seconds()
        print(f"   Time range: {min(timestamps)} - {max(timestamps)}")
        print(f"   Duration: {duration:.1f} seconds")
        if duration > 0:
            print(f"   Average rate: {len(packets) / duration:.2f} packets/sec")
    
    # Protocol distribution
    print(f"\n📡 Protocol Distribution")
    protocol_counts = defaultdict(int)
    for p in packets:
        protocol_counts[p.protocol] += 1
    
    for protocol, count in sorted(protocol_counts.items(), key=lambda x: -x[1]):
        pct = (count / len(packets)) * 100
        print(f"   {protocol:<15} {count:5} ({pct:5.1f}%)")
    
    # Signal quality
    rssi_values = [p.rssi for p in packets if p.rssi != 0]
    snr_values = [p.snr for p in packets if p.snr != 0]
    
    if rssi_values:
        print(f"\n📶 Signal Quality")
        print(f"   RSSI: min={min(rssi_values):.1f}, max={max(rssi_values):.1f}, "
              f"avg={sum(rssi_values)/len(rssi_values):.1f} dBm")
    
    if snr_values:
        print(f"   SNR:  min={min(snr_values):.1f}, max={max(snr_values):.1f}, "
              f"avg={sum(snr_values)/len(snr_values):.1f} dB")
    
    # Frequency distribution
    print(f"\n📻 Frequency Distribution")
    freq_counts = defaultdict(int)
    for p in packets:
        if p.frequency_mhz > 0:
            freq_counts[f"{p.frequency_mhz:.3f}"] += 1
    
    for freq, count in sorted(freq_counts.items(), key=lambda x: -x[1])[:10]:
        pct = (count / len(packets)) * 100
        print(f"   {freq} MHz: {count:5} ({pct:5.1f}%)")
    
    # Radio configuration analysis
    print(f"\n⚙️  Radio Configurations")
    sf_counts = defaultdict(int)
    bw_counts = defaultdict(int)
    for p in packets:
        if hasattr(p, 'spreading_factor') and p.spreading_factor > 0:
            sf_counts[p.spreading_factor] += 1
        if hasattr(p, 'bandwidth') and p.bandwidth > 0:
            bw_counts[p.bandwidth] += 1
    
    if sf_counts:
        print(f"   Spreading Factors:")
        for sf, count in sorted(sf_counts.items()):
            print(f"      SF{sf}: {count} packets")
    
    if bw_counts:
        print(f"   Bandwidths:")
        for bw, count in sorted(bw_counts.items()):
            bw_khz = bw / 1000 if bw > 1000 else bw
            print(f"      {bw_khz:.0f} kHz: {count} packets")
    
    # Device analysis
    print(f"\n📱 Device Analysis")
    device_stats = defaultdict(lambda: {'count': 0, 'rssi': [], 'protocols': set()})
    
    for p in packets:
        if p.node_id:
            stats = device_stats[p.node_id]
            stats['count'] += 1
            if p.rssi != 0:
                stats['rssi'].append(p.rssi)
            stats['protocols'].add(p.protocol)
    
    print(f"   Unique devices: {len(device_stats)}")
    
    # Top devices
    sorted_devices = sorted(device_stats.items(), key=lambda x: -x[1]['count'])[:10]
    if sorted_devices:
        print(f"\n   Top devices:")
        for node_id, stats in sorted_devices:
            avg_rssi = sum(stats['rssi']) / len(stats['rssi']) if stats['rssi'] else 0
            protos = ', '.join(stats['protocols'])
            print(f"   {node_id}: {stats['count']:4} pkts, "
                  f"avg RSSI {avg_rssi:6.1f} dBm ({protos})")
    
    # GPS observations
    gps_packets = [(p.node_id, p.lat, p.lon) for p in packets if p.lat is not None]
    if gps_packets:
        print(f"\n📍 GPS Observations")
        gps_by_node = defaultdict(list)
        for node_id, lat, lon in gps_packets:
            gps_by_node[node_id or 'Unknown'].append((lat, lon))
        print(f"   Nodes with GPS data: {len(gps_by_node)}")
        for node_id, positions in sorted(gps_by_node.items()):
            lats = [p[0] for p in positions]
            lons = [p[1] for p in positions]
            if len(positions) == 1:
                print(f"   {node_id}: {lats[0]:.6f}, {lons[0]:.6f}")
            else:
                lat_range = f"{min(lats):.6f} to {max(lats):.6f}"
                lon_range = f"{min(lons):.6f} to {max(lons):.6f}"
                print(f"   {node_id}: {len(positions)} fixes  lat {lat_range}  lon {lon_range}")

    # Packet sizes
    sizes = [p.length for p in packets]
    print(f"\n📐 Packet Sizes")
    print(f"   Min: {min(sizes)} bytes, Max: {max(sizes)} bytes, "
          f"Avg: {sum(sizes)/len(sizes):.1f} bytes")


def export_csv(packets, output_path):
    """Export packets to CSV"""
    import csv
    
    fieldnames = ['timestamp_ms', 'pcap_timestamp', 'rssi', 'snr',
                  'frequency_mhz', 'spreading_factor', 'bandwidth_hz', 'coding_rate',
                  'length', 'protocol', 'node_id', 'lat', 'lon', 'data_hex']
    
    with open(output_path, 'w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        
        for packet in packets:
            writer.writerow(packet.to_dict())
    
    print(f"✅ Exported {len(packets)} packets to {output_path}")


def export_json(packets, output_path):
    """Export packets to JSON"""
    data = [p.to_dict() for p in packets]
    
    with open(output_path, 'w') as f:
        json.dump(data, f, indent=2)
    
    print(f"✅ Exported {len(packets)} packets to {output_path}")


def open_wireshark(pcap_path):
    """Open PCAP file in Wireshark"""
    import subprocess
    import shutil
    
    wireshark = shutil.which('wireshark')
    if not wireshark:
        # Try common locations
        common_paths = [
            r'C:\Program Files\Wireshark\Wireshark.exe',
            r'C:\Program Files (x86)\Wireshark\Wireshark.exe',
            '/usr/bin/wireshark',
            '/Applications/Wireshark.app/Contents/MacOS/Wireshark'
        ]
        for path in common_paths:
            if Path(path).exists():
                wireshark = path
                break
    
    if wireshark:
        print(f"Opening {pcap_path} in Wireshark...")
        subprocess.Popen([wireshark, str(pcap_path)])
    else:
        print("❌ Wireshark not found. Install Wireshark or add it to PATH.")


def main():
    parser = argparse.ArgumentParser(
        description='Analyze PCAP files from ESP32 LoRa Sniffer',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s capture.pcap              Basic analysis
  %(prog)s capture.pcap --json       Output as JSON
  %(prog)s capture.pcap -o out.csv   Export to CSV
  %(prog)s capture.pcap --wireshark  Open in Wireshark
"""
    )
    
    parser.add_argument('pcap_file', help='PCAP file to analyze')
    parser.add_argument('--json', action='store_true',
                       help='Output analysis as JSON')
    parser.add_argument('-o', '--output', metavar='FILE',
                       help='Export to CSV file')
    parser.add_argument('--export-json', metavar='FILE',
                       help='Export to JSON file')
    parser.add_argument('--wireshark', action='store_true',
                       help='Open file in Wireshark')
    parser.add_argument('--raw', action='store_true',
                       help='Show raw packet hex dumps')
    
    args = parser.parse_args()
    
    pcap_path = Path(args.pcap_file)
    if not pcap_path.exists():
        print(f"❌ File not found: {pcap_path}")
        sys.exit(1)
    
    print(f"📂 Loading {pcap_path} ({pcap_path.stat().st_size:,} bytes)")
    
    # Parse PCAP file
    try:
        packets = parse_pcap_native(pcap_path)
    except Exception as e:
        print(f"❌ Failed to parse PCAP: {e}")
        sys.exit(1)
    
    print(f"✅ Parsed {len(packets)} packets")
    
    # Handle outputs
    if args.wireshark:
        if EXPORTER_AVAILABLE:
            loratap_path = pcap_path.with_name(pcap_path.stem + '_loratap.pcap')
            raw_packets = parse_esp32_pcap(pcap_path)
            write_loratap_pcap(raw_packets, loratap_path)
            print(f"✅ LoRaTap PCAP created: {loratap_path}")
            _ws_open(loratap_path)
        else:
            open_wireshark(pcap_path)
    
    if args.output:
        export_csv(packets, args.output)
    
    if args.export_json:
        export_json(packets, args.export_json)
    
    if args.json:
        data = {
            'file': str(pcap_path),
            'packet_count': len(packets),
            'packets': [p.to_dict() for p in packets]
        }
        print(json.dumps(data, indent=2))
    else:
        analyze_packets(packets)
    
    if args.raw:
        print(f"\n{'='*60}")
        print("RAW PACKET DUMPS")
        print(f"{'='*60}")
        for i, p in enumerate(packets[:20]):  # Limit to first 20
            print(f"\nPacket {i+1}: {p.protocol} {p.length}B RSSI:{p.rssi:.1f}")
            # Format hex dump
            hex_str = p.data.hex()
            for j in range(0, len(hex_str), 32):
                print(f"  {hex_str[j:j+32]}")


if __name__ == '__main__':
    main()
