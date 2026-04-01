#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - Wireshark Exporter

Converts ESP32 LoRa Sniffer PCAP files to standard LoRaTap format
for native Wireshark LoRaWAN dissector support.

LoRaTap pseudo-header format (per-packet):
- Version (1 byte): 0x01
- Padding (1 byte): 0x00
- Length (2 bytes): Total header length
- Channel Type (4 bytes): Frequency in Hz
- RSSI (4 bytes): Signal strength in dBm * 1000
- SNR (4 bytes): SNR in dB * 1000
- SF (1 byte): Spreading factor
- BW (1 byte): Bandwidth index
- Timestamp (4 bytes): microseconds

Also exports cracked PSKs to Wireshark UAT format for session decryption.

Usage:
    python wireshark_exporter.py capture.pcap -o output_loratap.pcap
    python wireshark_exporter.py --export-keys keys.txt
    python wireshark_exporter.py capture.pcap --loratap --open

Requirements:
    pip install scapy (optional, for advanced features)
"""

import argparse
import struct
import json
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
from pathlib import Path
from datetime import datetime

# PCAP constants
PCAP_MAGIC = 0xa1b2c3d4
PCAP_VERSION_MAJOR = 2
PCAP_VERSION_MINOR = 4
PCAP_SNAPLEN = 65535

# Link types
DLT_USER0 = 147          # ESP32 Sniffer custom format
DLT_LORATAP = 270        # LoRaTap pseudo-header (Wireshark compatible)
DLT_IEEE802_15_4 = 195   # Alternative for some tools

# Custom ESP32 LoRa pseudo-header format (from pcap_logger.h LoRaPseudoHeader struct)
# struct LoRaPseudoHeader {
#     float frequencyMHz;      // 4 bytes
#     float rssiDbm;           // 4 bytes
#     float snrDb;             // 4 bytes
#     uint8_t spreadingFactor; // 1 byte
#     uint32_t bandwidth;      // 4 bytes (Hz)
#     uint8_t codingRate;      // 1 byte
#     uint16_t reserved;       // 2 bytes
# } __attribute__((packed));   // Total: 20 bytes
ESP32_HEADER_FORMAT = '<fffBIBH'  # frequencyMHz, rssiDbm, snrDb, sf, bandwidth_hz, codingRate, reserved
ESP32_HEADER_SIZE = struct.calcsize(ESP32_HEADER_FORMAT)  # 20 bytes

# LoRaTap header format
# Reference: https://github.com/rpp0/gr-lora/wiki/LoRaTap-Header
LORATAP_VERSION = 0x01
LORATAP_HEADER_SIZE = 24


class LoRaTapHeader:
    """LoRaTap pseudo-header for Wireshark compatibility"""
    
    def __init__(self, frequency_hz=915000000, rssi=-100.0, snr=0.0, 
                 sf=7, bw=125, timestamp_us=0):
        self.version = LORATAP_VERSION
        self.padding = 0
        self.length = LORATAP_HEADER_SIZE
        self.frequency_hz = frequency_hz
        # RSSI and SNR in milli-units for precision
        self.rssi_mdb = int(rssi * 1000)
        self.snr_mdb = int(snr * 1000)
        self.sf = sf
        self.bw = self._bw_to_index(bw)
        self.timestamp_us = timestamp_us
    
    def _bw_to_index(self, bw_khz):
        """Convert bandwidth in kHz to LoRaTap index"""
        bw_map = {125: 0, 250: 1, 500: 2, 62.5: 3, 41.67: 4, 31.25: 5,
                  20.83: 6, 15.63: 7, 10.42: 8, 7.81: 9}
        return bw_map.get(bw_khz, 0)
    
    def pack(self):
        """Pack header to bytes"""
        return struct.pack('<BBHIiiIBBBB',
            self.version,
            self.padding,
            self.length,
            self.frequency_hz,
            self.rssi_mdb,
            self.snr_mdb,
            self.timestamp_us,
            self.sf,
            self.bw,
            0,  # Sync word (reserved)
            0   # Reserved
        )


def parse_esp32_pcap(filepath):
    """Parse ESP32 Sniffer PCAP file"""
    packets = []
    
    with open(filepath, 'rb') as f:
        # Read global header
        global_header = f.read(24)
        magic, ver_maj, ver_min, thiszone, sigfigs, snaplen, linktype = \
            struct.unpack('<IHHIIII', global_header)
        
        if magic not in (PCAP_MAGIC, 0xa1b23c4d):
            raise ValueError(f"Invalid PCAP magic: 0x{magic:08x}")
        
        print(f"Input PCAP: v{ver_maj}.{ver_min}, linktype={linktype}")
        
        # Read packets
        while True:
            pkt_hdr = f.read(16)
            if len(pkt_hdr) < 16:
                break
            
            ts_sec, ts_usec, incl_len, orig_len = struct.unpack('<IIII', pkt_hdr)
            pkt_data = f.read(incl_len)
            
            if len(pkt_data) < incl_len:
                break
            
            # Parse ESP32 custom pseudo-header (20 bytes: freq, rssi, snr, sf, bw, cr, pad)
            if len(pkt_data) >= ESP32_HEADER_SIZE:
                header = struct.unpack(ESP32_HEADER_FORMAT, pkt_data[:ESP32_HEADER_SIZE])
                frequency_mhz, rssi, snr, spreading_factor, bandwidth_hz, coding_rate, _ = header
                payload = pkt_data[ESP32_HEADER_SIZE:]

                packets.append({
                    'ts_sec': ts_sec,
                    'ts_usec': ts_usec,
                    'timestamp_ms': int(ts_sec * 1000 + ts_usec / 1000),
                    'rssi': rssi,
                    'snr': snr,
                    'frequency_hz': int(frequency_mhz * 1e6),
                    'spreading_factor': spreading_factor,
                    'bandwidth_khz': bandwidth_hz / 1000.0,
                    'coding_rate': coding_rate,
                    'payload': payload,
                })
            else:
                # No custom header — use defaults
                packets.append({
                    'ts_sec': ts_sec,
                    'ts_usec': ts_usec,
                    'timestamp_ms': int(ts_sec * 1000 + ts_usec / 1000),
                    'rssi': 0,
                    'snr': 0,
                    'frequency_hz': 915000000,
                    'spreading_factor': 7,
                    'bandwidth_khz': 125.0,
                    'coding_rate': 5,
                    'payload': pkt_data,
                })
    
    return packets


def write_loratap_pcap(packets, output_path):
    """Write packets to LoRaTap format PCAP"""
    
    with open(output_path, 'wb') as f:
        # Write global header
        global_header = struct.pack('<IHHIIII',
            PCAP_MAGIC,
            PCAP_VERSION_MAJOR,
            PCAP_VERSION_MINOR,
            0,  # thiszone
            0,  # sigfigs
            PCAP_SNAPLEN,
            DLT_LORATAP
        )
        f.write(global_header)
        
        # Write packets
        for pkt in packets:
            # Create LoRaTap header using actual radio parameters from pseudo-header
            lt = LoRaTapHeader(
                frequency_hz=pkt['frequency_hz'],
                rssi=pkt['rssi'],
                snr=pkt['snr'],
                sf=pkt.get('spreading_factor', 7),
                bw=pkt.get('bandwidth_khz', 125.0),
                timestamp_us=pkt['ts_usec']
            )
            
            loratap_header = lt.pack()
            packet_data = loratap_header + pkt['payload']
            
            # Packet header
            incl_len = len(packet_data)
            pkt_hdr = struct.pack('<IIII',
                pkt['ts_sec'],
                pkt['ts_usec'],
                incl_len,
                incl_len
            )
            
            f.write(pkt_hdr)
            f.write(packet_data)
    
    print(f"✅ Wrote {len(packets)} packets to {output_path}")
    print(f"   Format: LoRaTap (DLT {DLT_LORATAP})")


def get_sf_from_config(config_idx):
    """Get spreading factor from config index"""
    # Based on recon_state.cpp scanConfigs
    sf_map = {
        0: 11, 1: 11, 2: 10, 3: 9,   # Meshtastic
        4: 10, 5: 9, 6: 8, 7: 11, 8: 10, 9: 9,  # Meshtastic variants
        10: 7, 11: 8, 12: 9, 13: 10,  # TTN
        14: 7, 15: 8, 16: 9,  # LoRaWAN
        17: 12,  # LongSlow
        18: 9, 19: 10,  # Helium UL
        20: 10, 21: 10, 22: 9, 23: 8,  # Helium DL
        24: 7, 25: 8,  # ISM
    }
    return sf_map.get(config_idx, 7)


def get_bw_from_config(config_idx):
    """Get bandwidth in kHz from config index"""
    bw_map = {
        0: 250, 1: 250, 2: 250, 3: 250,  # Meshtastic
        4: 250, 5: 250, 6: 250, 7: 250, 8: 250, 9: 250,
        10: 125, 11: 125, 12: 125, 13: 125,  # TTN
        14: 125, 15: 125, 16: 125,  # LoRaWAN
        17: 125,  # LongSlow
        18: 125, 19: 125,  # Helium UL
        20: 500, 21: 500, 22: 500, 23: 500,  # Helium DL
        24: 125, 25: 250,  # ISM
    }
    return bw_map.get(config_idx, 125)


def export_wireshark_keys(output_path, keys=None):
    """
    Export PSK keys to Wireshark UAT format
    
    For LoRaWAN, Wireshark uses UAT files:
    - lorawan_keys: DevEUI,JoinEUI,AppKey
    - lorawan_session_keys: DevAddr,NwkSKey,AppSKey
    
    For Meshtastic, we export a reference file since
    Wireshark doesn't natively support Meshtastic.
    """
    
    # Known Meshtastic PSKs (for reference/custom dissector)
    meshtastic_keys = [
        ("AQ==", "Default (0x01)", "CRITICAL"),
        ("1PG7OiApB1nwvP+rz05pAQ==", "LongFast", "HIGH"),
        ("PKdTs51e4EB0BoOevIN0Dw==", "Admin (legacy)", "CRITICAL"),
        ("AAAAAAAAAAAAAAAAAAAAAA==", "AllZeros", "HIGH"),
        ("d1iq21lNSh7BP6MOkP6cQA==", "MediumFast", "MEDIUM"),
        ("GGC5DDnv8FKFm7WCZ5rXBA==", "LongSlow", "MEDIUM"),
    ]
    
    lines = [
        "# Meshtastic PSK Reference File",
        "# For use with custom Wireshark Lua dissector",
        "#",
        "# Format: base64_key,name,risk_level",
        "#",
    ]
    
    for b64, name, risk in meshtastic_keys:
        lines.append(f"{b64},{name},{risk}")
    
    with open(output_path, 'w') as f:
        f.write('\n'.join(lines))
    
    print(f"✅ Exported {len(meshtastic_keys)} keys to {output_path}")
    print("   Use with custom Meshtastic Wireshark dissector")


def export_lorawan_uat(output_path, devices=None):
    """
    Export LoRaWAN session keys to Wireshark UAT format
    
    File format for lorawan_session_keys:
    DevAddr,NwkSKey,AppSKey
    
    Example:
    "260B1234","01020304050607080910111213141516","16151413121110090807060504030201"
    """
    
    # Placeholder - would be populated from cracked keys
    example = [
        "# LoRaWAN Session Keys for Wireshark",
        "# Import via: Edit → Preferences → Protocols → LoRaWAN → Session Keys",
        "#",
        "# Format: DevAddr,NwkSKey,AppSKey",
        "#",
        '# Example:',
        '# "260B1234","01020304050607080910111213141516","16151413121110090807060504030201"',
    ]
    
    with open(output_path, 'w') as f:
        f.write('\n'.join(example))
    
    print(f"✅ Created LoRaWAN UAT template: {output_path}")


def open_wireshark(filepath):
    """Open file in Wireshark"""
    import subprocess
    import shutil
    
    wireshark = shutil.which('wireshark')
    if not wireshark:
        common = [
            r'C:\Program Files\Wireshark\Wireshark.exe',
            '/usr/bin/wireshark',
            '/Applications/Wireshark.app/Contents/MacOS/Wireshark'
        ]
        for p in common:
            if Path(p).exists():
                wireshark = p
                break
    
    if wireshark:
        print(f"Opening {filepath} in Wireshark...")
        subprocess.Popen([wireshark, str(filepath)])
    else:
        print("❌ Wireshark not found. Install or add to PATH.")


def main():
    parser = argparse.ArgumentParser(
        description='Convert ESP32 LoRa Sniffer PCAP to Wireshark-compatible formats',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s capture.pcap -o output.pcap          Convert to LoRaTap format
  %(prog)s capture.pcap --loratap --open        Convert and open in Wireshark
  %(prog)s --export-keys meshtastic_keys.txt    Export PSK reference file
  %(prog)s --export-uat lorawan_keys.txt        Create LoRaWAN UAT template

The LoRaTap format preserves RF metadata (RSSI, SNR, frequency) and is
recognized by Wireshark's native LoRaWAN dissector.
"""
    )
    
    parser.add_argument('input', nargs='?', help='Input PCAP file (ESP32 format)')
    parser.add_argument('-o', '--output', help='Output PCAP file (LoRaTap format)')
    parser.add_argument('--loratap', action='store_true', 
                       help='Force LoRaTap output format')
    parser.add_argument('--open', action='store_true',
                       help='Open output in Wireshark')
    parser.add_argument('--export-keys', metavar='FILE',
                       help='Export Meshtastic PSK reference file')
    parser.add_argument('--export-uat', metavar='FILE',
                       help='Create LoRaWAN UAT template')
    parser.add_argument('--stats', action='store_true',
                       help='Show conversion statistics')
    
    args = parser.parse_args()
    
    # Handle key export
    if args.export_keys:
        export_wireshark_keys(args.export_keys)
        return 0
    
    if args.export_uat:
        export_lorawan_uat(args.export_uat)
        return 0
    
    if not args.input:
        parser.print_help()
        return 1
    
    input_path = Path(args.input)
    if not input_path.exists():
        print(f"❌ File not found: {input_path}")
        return 1
    
    # Determine output path
    if args.output:
        output_path = Path(args.output)
    else:
        output_path = input_path.with_stem(input_path.stem + '_loratap')
    
    print(f"📂 Loading {input_path} ({input_path.stat().st_size:,} bytes)")
    
    # Parse input
    try:
        packets = parse_esp32_pcap(input_path)
    except Exception as e:
        print(f"❌ Failed to parse PCAP: {e}")
        return 1
    
    print(f"✅ Parsed {len(packets)} packets")
    
    # Show stats
    if args.stats and packets:
        rssi_vals = [p['rssi'] for p in packets if p['rssi'] != 0]
        snr_vals = [p['snr'] for p in packets if p['snr'] != 0]
        freqs = set(p['frequency_hz'] for p in packets)
        
        print(f"\n📊 Statistics:")
        if rssi_vals:
            print(f"   RSSI: {min(rssi_vals):.1f} to {max(rssi_vals):.1f} dBm")
        if snr_vals:
            print(f"   SNR:  {min(snr_vals):.1f} to {max(snr_vals):.1f} dB")
        print(f"   Frequencies: {len(freqs)} unique")
    
    # Convert to LoRaTap
    write_loratap_pcap(packets, output_path)
    
    # Open in Wireshark
    if args.open:
        open_wireshark(output_path)
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
