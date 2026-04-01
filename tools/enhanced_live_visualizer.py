#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - Enhanced Live Visualization Tool (Conference Demo Edition)

NEW FEATURES FOR CONFERENCE DEMOS:
- 5-panel dashboard including GPS map
- Dual-mode parsing (human-readable + JSON)
- Audio feedback ("Geiger counter" effect)
- Protocol-specific colors matching web UI
- Screen recording capability
- Auto-screenshot on interesting events
- DEMO MODE: Simulated traffic for offline presentations

Usage:
    python enhanced_live_visualizer.py COM3
    python enhanced_live_visualizer.py COM3 --json --audio --record
    python enhanced_live_visualizer.py --demo         # Simulated traffic for presentations
    python enhanced_live_visualizer.py --demo --audio # Demo with sound effects

Requirements:
    pip install pyserial matplotlib folium sounddevice numpy
"""

import warnings
# Suppress matplotlib font/glyph warnings
warnings.filterwarnings('ignore', message='.*Glyph.*missing from.*font.*')
warnings.filterwarnings('ignore', category=UserWarning, module='matplotlib')

import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from matplotlib.patches import Circle
from matplotlib.ticker import MaxNLocator
from collections import deque, defaultdict
import re
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
import time
import json
import argparse
from datetime import datetime
from pathlib import Path

# Try optional audio support
try:
    import sounddevice as sd
    import numpy as np
    AUDIO_AVAILABLE = True
except ImportError:
    AUDIO_AVAILABLE = False
    print("[!] Audio feedback disabled - pip install sounddevice numpy")

# Try optional folium for interactive maps
try:
    import folium
    FOLIUM_AVAILABLE = True
except ImportError:
    FOLIUM_AVAILABLE = False
    print("[!] HTML map export disabled - pip install folium")

# Configuration
SERIAL_BAUDRATE = 115200
UPDATE_INTERVAL = 200  # milliseconds
MAX_HISTORY_POINTS = 300
WINDOW_SIZE = (16, 10)  # Larger for 5-panel layout

# Protocol colors matching web UI
PROTOCOL_COLORS = {
    'Meshtastic': '#3498db',  # Blue
    'LoRaWAN': '#e67e22',     # Orange
    'Helium': '#2ecc71',      # Green
    'Unknown': '#95a5a6'      # Gray
}

# Audio frequencies for "Geiger counter" effect
AUDIO_FREQS = {
    'Meshtastic': 800,  # Hz
    'LoRaWAN': 600,
    'Helium': 500,
    'Unknown': 400
}

class AudioFeedback:
    """Geiger counter style audio feedback"""
    def __init__(self, enabled=True):
        self.enabled = enabled and AUDIO_AVAILABLE
        self.sample_rate = 44100
        self.duration = 0.05  # 50ms click
        
    def play_click(self, protocol='Unknown'):
        """Play protocol-specific click sound"""
        if not self.enabled:
            return
            
        try:
            freq = AUDIO_FREQS.get(protocol, 400)
            t = np.linspace(0, self.duration, int(self.sample_rate * self.duration))
            # Quick envelope to avoid clicks
            envelope = np.exp(-t * 20)
            wave = envelope * np.sin(2 * np.pi * freq * t)
            sd.play(wave, self.sample_rate, blocking=False)
        except Exception:
            pass  # Silently fail if audio not available


class DemoDataGenerator:
    """Generate realistic simulated LoRa traffic for conference demos"""
    
    # Realistic node IDs (Meshtastic style)
    DEMO_NODES = [
        '401ACD4E',  # Presenter node
        '598B29CE',  # Nearby attendee
        'B3F42A10',  # Mobile node (moving)
        '7C891DEF',  # Hidden node
        'A42B8C56',  # Router node
    ]

    # Demo GPS positions (Research Triangle Park, NC — Sheraton Imperial area)
    DEMO_POSITIONS = {
        '401ACD4E': (35.8690, -78.8450, 'stationary'),   # Conference venue
        '598B29CE': (35.8683, -78.8441, 'stationary'),   # Nearby attendee
        'B3F42A10': (35.8697, -78.8458, 'mobile'),       # Mobile node (moves)
        '7C891DEF': (35.8672, -78.8445, 'stationary'),   # Distant node
        'A42B8C56': (35.8693, -78.8463, 'stationary'),   # Router node
    }
    
    def __init__(self, seed=42):
        import random
        self.rng = random.Random(seed)
        self.start_time = time.time()
        self.packet_count = 0
        self.last_packet_time = 0
        self.mobile_offset = 0
        
    def get_next_event(self):
        """Generate a realistic packet event with variable timing"""
        import random
        
        current_time = time.time()
        # Packets arrive every 0.3-2.0 seconds (realistic for LoRa mesh)
        if current_time - self.last_packet_time < self.rng.uniform(0.3, 2.0):
            return None
        
        self.last_packet_time = current_time
        self.packet_count += 1
        
        # Pick node (weighted toward active ones)
        weights = [3, 2, 2, 1, 1]  # First node most active
        node_id = self.rng.choices(self.DEMO_NODES, weights=weights)[0]
        
        # Protocol distribution: 70% Meshtastic, 20% LoRaWAN, 10% Other
        protocol = self.rng.choices(
            ['Meshtastic', 'LoRaWAN', 'Helium', 'Unknown'],
            weights=[70, 20, 5, 5]
        )[0]
        
        # Realistic RSSI values (-40 to -120 dBm)
        base_rssi = {
            '401ACD4E': -55,  # Closest
            '598B29CE': -68,
            'B3F42A10': -75,
            '7C891DEF': -95,  # Farthest
            'A42B8C56': -62,
        }
        rssi = base_rssi.get(node_id, -80) + self.rng.uniform(-8, 8)
        snr = 5 + (rssi + 80) / 10 + self.rng.uniform(-2, 2)  # SNR correlates with RSSI
        
        event = {
            'type': 'packet',
            'node_id': node_id,
            'rssi': round(rssi, 1),
            'snr': round(snr, 1),
            'protocol': protocol,
            'length': self.rng.randint(24, 256),
            'timestamp': current_time
        }
        
        # 15% chance to include GPS data
        if self.rng.random() < 0.15 and node_id in self.DEMO_POSITIONS:
            base_lat, base_lon, mobility = self.DEMO_POSITIONS[node_id]
            if mobility == 'mobile':
                # Mobile node moves around
                self.mobile_offset += 0.0001 * self.rng.uniform(-1, 1)
                lat = base_lat + self.mobile_offset + self.rng.uniform(-0.0002, 0.0002)
                lon = base_lon + self.mobile_offset * 0.5 + self.rng.uniform(-0.0002, 0.0002)
            else:
                lat = base_lat + self.rng.uniform(-0.0001, 0.0001)
                lon = base_lon + self.rng.uniform(-0.0001, 0.0001)
            
            return [event, {
                'type': 'gps',
                'node_id': node_id,
                'lat': round(lat, 6),
                'lon': round(lon, 6),
                'timestamp': current_time
            }]
        
        return [event]


class EnhancedLoRaVisualizer:
    def __init__(self, port, json_mode=False, audio=False, record=False, demo_mode=False):
        self.port = port
        self.json_mode = json_mode
        self.ser = None
        self.audio = AudioFeedback(enabled=audio)
        self.record = record
        self.demo_mode = demo_mode
        self.demo_generator = DemoDataGenerator() if demo_mode else None
        self.screenshot_dir = Path('screenshots')
        if record:
            self.screenshot_dir.mkdir(exist_ok=True)
        
        # Data storage
        self.devices = defaultdict(lambda: {
            'rssi': deque(maxlen=MAX_HISTORY_POINTS),
            'time': deque(maxlen=MAX_HISTORY_POINTS),
            'packet_count': 0,
            'last_seen': time.time(),
            'device_type': 'Unknown',
            'lat': None,
            'lon': None,
            'positions': []  # Track movement
        })
        
        self.start_time = time.time()
        self.total_packets = 0
        self.packet_times = deque(maxlen=100)
        self.protocol_counts = defaultdict(int)
        
        # Setup matplotlib with 5 panels
        plt.style.use('dark_background')
        self.fig = plt.figure(figsize=WINDOW_SIZE)
        
        # Create grid: 2 rows, 3 columns
        gs = self.fig.add_gridspec(2, 3, hspace=0.3, wspace=0.3)
        
        self.ax_rssi = self.fig.add_subplot(gs[0, 0])      # Top-left: RSSI
        self.ax_devices = self.fig.add_subplot(gs[0, 1])   # Top-middle: Device list
        self.ax_protocols = self.fig.add_subplot(gs[0, 2]) # Top-right: Protocol pie
        self.ax_packets = self.fig.add_subplot(gs[1, 0])   # Bottom-left: Packet histogram
        self.ax_map = self.fig.add_subplot(gs[1, 1:])      # Bottom-middle+right: GPS map
        
        title = 'ESP32 LoRa Sniffer — Demo (RTP, NC)' if demo_mode else 'ESP32 LoRa Sniffer — Live'
        self.fig.suptitle(title, fontsize=16, fontweight='bold')
        
        # Connect to serial port (unless in demo mode)
        if not self.demo_mode:
            self.connect()
        else:
            print("[*] DEMO MODE: Using simulated LoRa traffic")
            print("[*] Starting visualization with realistic packet generation...")
            print()
        
    def connect(self):
        """Connect to ESP32 via serial port"""
        if self.demo_mode:
            return  # Skip connection in demo mode
        try:
            print(f"[*] Connecting to {self.port} at {SERIAL_BAUDRATE} baud...")
            # Use explicit port configuration to prevent DTR reset
            self.ser = serial.Serial()
            self.ser.port = self.port
            self.ser.baudrate = SERIAL_BAUDRATE
            self.ser.timeout = 1
            self.ser.dtr = False  # Prevent ESP32 reset on connect
            self.ser.rts = False
            self.ser.open()
            time.sleep(0.5)  # Brief settle time (no reboot wait needed)
            print(f"[✓] Connected successfully!")
            
            if self.json_mode:
                print(f"[*] Enabling JSON output mode on ESP32...")
                self.ser.write(b'j')  # Send 'j' command to enable JSON mode
                time.sleep(0.5)
            
            print(f"[*] Starting live visualization...\n")
        except serial.SerialException as e:
            print(f"[✗] Failed to connect to {self.port}: {e}")
            sys.exit(1)
    
    def parse_serial_line(self, line):
        """Parse serial output - supports both JSON and human-readable"""
        try:
            # Try JSON first
            if line.startswith('{'):
                return json.loads(line)
            
            # Fall back to regex parsing
            # Pattern with node ID: [RECON] or [SMALL] Packet #42: Meshtastic, 0x401ACD4E, 123 bytes, -68.5 dBm, 8.2 dB SNR
            match = re.search(
                r'\[(RECON|CAPTURE|SMALL)\]\s*Packet\s*#(\d+):\s*(\w+),\s*0x([0-9A-Fa-f]+),\s*(\d+)\s*bytes,\s*(-?\d+\.?\d*)\s*dBm,\s*(-?\d+\.?\d*)\s*dB',
                line
            )
            
            if match:
                packet_num = int(match.group(2))
                protocol = match.group(3)
                node_id = match.group(4).upper()
                length = int(match.group(5))
                rssi = float(match.group(6))
                snr = float(match.group(7))
                
                return {
                    'type': 'packet',
                    'node_id': node_id,
                    'rssi': rssi,
                    'snr': snr,
                    'protocol': protocol,
                    'length': length,
                    'timestamp': time.time()
                }
            
            # Format without node ID: [CAPTURE] Packet #1: Meshtastic, 56 bytes, -64.0 dBm, 5.8 dB SNR
            # Store packet info, wait for PSK line with NodeID
            match_capture = re.search(
                r'\[(RECON|CAPTURE)\]\s*Packet\s*#(\d+):\s*(\w+),\s*(\d+)\s*bytes,\s*(-?\d+\.?\d*)\s*dBm,\s*(-?\d+\.?\d*)\s*dB',
                line
            )
            if match_capture:
                # Store pending packet info for next PSK line to complete
                self._pending_packet = {
                    'packet_num': int(match_capture.group(2)),
                    'protocol': match_capture.group(3),
                    'length': int(match_capture.group(4)),
                    'rssi': float(match_capture.group(5)),
                    'snr': float(match_capture.group(6)),
                }
                return None  # Wait for PSK line with NodeID
            
            # PSK line with NodeID: [PSK] Testing packet: NodeID=0x598B29CE, PacketID=0xE9A17147, Flags=0x82, Chan=8
            psk_match = re.search(r'\[PSK\].*NodeID=0x([0-9A-Fa-f]+)', line)
            if psk_match and hasattr(self, '_pending_packet') and self._pending_packet:
                node_id = psk_match.group(1).upper()
                pending = self._pending_packet
                self._pending_packet = None  # Clear pending
                
                return {
                    'type': 'packet',
                    'node_id': node_id,
                    'rssi': pending['rssi'],
                    'snr': pending['snr'],
                    'protocol': pending['protocol'],
                    'length': pending['length'],
                    'timestamp': time.time()
                }
            
            # GPS extraction - multi-line format from firmware:
            # 📍 GPS POSITION EXTRACTED!
            #    Node: 0x401ACD4E
            #    Lat:  35.730228° N
            #    Lon:  78.879128° W
            
            # Detect GPS block start
            if 'GPS POSITION EXTRACTED' in line:
                self._pending_gps = {'node_id': None, 'lat': None, 'lon': None}
                return None
            
            # Parse GPS node ID line
            if hasattr(self, '_pending_gps') and self._pending_gps is not None:
                node_match = re.search(r'Node:\s*0x([0-9A-Fa-f]+)', line)
                if node_match:
                    self._pending_gps['node_id'] = node_match.group(1).upper()
                    return None
                
                # Parse latitude line: "   Lat:  35.730228° N"
                lat_match = re.search(r'Lat:\s*(-?\d+\.?\d*)[°\s]*([NS])?', line)
                if lat_match:
                    lat = float(lat_match.group(1))
                    if lat_match.group(2) == 'S':
                        lat = -lat
                    self._pending_gps['lat'] = lat
                    return None
                
                # Parse longitude line: "   Lon:  78.879128° W"
                lon_match = re.search(r'Lon:\s*(-?\d+\.?\d*)[°\s]*([EW])?', line)
                if lon_match:
                    lon = float(lon_match.group(1))
                    if lon_match.group(2) == 'W':
                        lon = -lon
                    self._pending_gps['lon'] = lon
                    
                    # Complete GPS event
                    if self._pending_gps['lat'] is not None:
                        result = {
                            'type': 'gps',
                            'node_id': self._pending_gps.get('node_id'),
                            'lat': self._pending_gps['lat'],
                            'lon': self._pending_gps['lon'],
                            'timestamp': time.time()
                        }
                        self._pending_gps = None
                        return result
                    return None
            
            # Single-line GPS format fallback: GPS: 35.730228, -78.879128
            gps_match = re.search(r'(?:lat[=:]\s*)?(-?\d+\.\d+)[°,\s]+(?:lon[=:]\s*)?(-?\d+\.\d+)', line, re.IGNORECASE)
            if gps_match and ('GEO' in line or 'GPS' in line or 'Position' in line):
                lat = float(gps_match.group(1))
                lon = float(gps_match.group(2))
                # Handle "W" meaning negative longitude
                if 'W' in line and lon > 0:
                    lon = -lon
                return {
                    'type': 'gps',
                    'lat': lat,
                    'lon': lon,
                    'timestamp': time.time()
                }
                
        except Exception as e:
            pass
        
        return None
    
    def update_data(self, event):
        """Update internal data structures with new event"""
        if event['type'] == 'packet':
            node_id = event.get('node_id')
            if not node_id:
                return
                
            current_time = time.time() - self.start_time
            protocol = event.get('protocol', 'Unknown')
            
            # Update device data
            device = self.devices[node_id]
            device['rssi'].append(event['rssi'])
            device['time'].append(current_time)
            device['packet_count'] += 1
            device['last_seen'] = time.time()
            device['device_type'] = protocol
            
            # Update global stats
            self.total_packets += 1
            self.packet_times.append(time.time())
            self.protocol_counts[protocol] += 1
            
            # Audio feedback
            self.audio.play_click(protocol)
            
        elif event['type'] == 'gps':
            # Associate GPS with specific node if node_id provided
            node_id = event.get('node_id')
            if node_id and node_id in self.devices:
                device = self.devices[node_id]
                device['lat'] = event['lat']
                device['lon'] = event['lon']
                device['positions'].append((event['lat'], event['lon'], time.time()))
            elif self.devices:
                # Fallback: associate with most recent device
                last_device = list(self.devices.values())[-1]
                last_device['lat'] = event['lat']
                last_device['lon'] = event['lon']
                last_device['positions'].append((event['lat'], event['lon'], time.time()))
    
    def update_plots(self, frame):
        """Update all 5 panels"""
        # Get data from demo generator OR serial port
        if self.demo_mode and self.demo_generator:
            events = self.demo_generator.get_next_event()
            if events:
                for event in events:
                    self.update_data(event)
        else:
            # Read serial data
            while self.ser and self.ser.in_waiting:
                try:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        event = self.parse_serial_line(line)
                        if event:
                            self.update_data(event)
                except Exception:
                    pass
        
        # Clear all axes
        self.ax_rssi.clear()
        self.ax_devices.clear()
        self.ax_protocols.clear()
        self.ax_packets.clear()
        self.ax_map.clear()
        
        # Panel 1: RSSI over time
        for node_id, data in self.devices.items():
            if len(data['rssi']) > 0:
                color = PROTOCOL_COLORS.get(data['device_type'], '#95a5a6')
                label = f"{node_id[:6]}.. ({data['device_type']})"
                self.ax_rssi.plot(data['time'], data['rssi'], 
                                color=color, marker='o', markersize=2, 
                                linewidth=1.5, label=label, alpha=0.8)
        
        self.ax_rssi.set_xlabel('Time (seconds)', fontsize=9)
        self.ax_rssi.set_ylabel('RSSI (dBm)', fontsize=9)
        self.ax_rssi.set_title('Signal Strength', fontweight='bold', fontsize=10)
        self.ax_rssi.grid(True, alpha=0.3)
        # Auto-scale Y axis with some padding, typical LoRa range is -140 to -40 dBm
        all_rssi = [r for d in self.devices.values() for r in d['rssi']]
        if all_rssi:
            min_rssi = min(all_rssi) - 5
            max_rssi = max(all_rssi) + 5
            self.ax_rssi.set_ylim(min_rssi, max_rssi)
        self.ax_rssi.axhline(y=-80, color='yellow', linestyle='--', linewidth=1, alpha=0.5)
        self.ax_rssi.axhline(y=-60, color='green', linestyle='--', linewidth=1, alpha=0.5)
        if 0 < len(self.devices) <= 4:
            self.ax_rssi.legend(loc='lower left', fontsize=7)
        
        # Panel 2: Device list
        self.ax_devices.axis('off')
        stats_text = "DEVICES\n" + "="*20 + "\n\n"
        
        if len(self.devices) == 0:
            stats_text += "Scanning...\n"
        else:
            sorted_devices = sorted(
                self.devices.items(),
                key=lambda x: x[1]['packet_count'],
                reverse=True
            )
            
            # Limit to top 5 to fit in panel
            for i, (node_id, data) in enumerate(sorted_devices[:5], 1):
                last_rssi = data['rssi'][-1] if data['rssi'] else 0
                bars = int(max(1, (last_rssi + 120) / 10))
                quality = "|" * min(bars, 8)
                
                stats_text += f"{i}. {node_id}\n"
                stats_text += f"   {data['device_type']}\n"
                stats_text += f"   {quality} {last_rssi:.0f}dBm\n"
                stats_text += f"   {data['packet_count']} pkts\n\n"
        
        self.ax_devices.text(0.05, 0.95, stats_text,
                           verticalalignment='top',
                           fontfamily='monospace',
                           fontsize=8,
                           transform=self.ax_devices.transAxes)
        
        # Panel 3: Protocol distribution pie chart
        if self.protocol_counts:
            protocols = list(self.protocol_counts.keys())
            counts = list(self.protocol_counts.values())
            colors = [PROTOCOL_COLORS.get(p, '#95a5a6') for p in protocols]
            
            self.ax_protocols.pie(counts, labels=protocols, colors=colors,
                                autopct='%1.0f%%', startangle=90)
            self.ax_protocols.set_title('Protocols', fontweight='bold', fontsize=10)
        else:
            self.ax_protocols.text(0.5, 0.5, 'Waiting...', ha='center', va='center',
                                 transform=self.ax_protocols.transAxes)
            self.ax_protocols.set_title('Protocols', fontweight='bold', fontsize=10)
        
        # Panel 4: Packet histogram
        if len(self.devices) > 0:
            node_ids = [f"0x{nid[:4]}" for nid in self.devices.keys()]
            packet_counts = [data['packet_count'] for data in self.devices.values()]
            colors_list = [PROTOCOL_COLORS.get(data['device_type'], '#95a5a6') 
                          for data in self.devices.values()]
            
            # Limit to top 8 devices by packet count
            sorted_indices = sorted(range(len(packet_counts)), key=lambda i: packet_counts[i], reverse=True)[:8]
            node_ids = [node_ids[i] for i in sorted_indices]
            packet_counts = [packet_counts[i] for i in sorted_indices]
            colors_list = [colors_list[i] for i in sorted_indices]
            
            bars = self.ax_packets.bar(range(len(node_ids)), packet_counts, 
                                      color=colors_list, alpha=0.8)
            self.ax_packets.set_xlabel('Device', fontsize=9)
            self.ax_packets.set_ylabel('Packets', fontsize=9)
            shown = len(sorted_indices)
            total = len(self.devices)
            title = 'Packet Captures'
            if total > shown:
                title += f' (top {shown} of {total} devices)'
            self.ax_packets.set_title(title, fontweight='bold', fontsize=10)
            self.ax_packets.set_xticks(range(len(node_ids)))
            self.ax_packets.set_xticklabels(node_ids, rotation=45, ha='right', fontsize=7)
            self.ax_packets.grid(True, alpha=0.3, axis='y')
            # Auto-scale Y with some headroom, force integer ticks
            if max(packet_counts) > 0:
                self.ax_packets.set_ylim(0, max(packet_counts) * 1.2)
            self.ax_packets.yaxis.set_major_locator(MaxNLocator(integer=True))
        
        # Panel 5: GPS Map
        def has_valid_gps(d):
            return d['lat'] is not None and not (d['lat'] == 0.0 and d['lon'] == 0.0)

        has_gps = any(has_valid_gps(d) for d in self.devices.values())

        if has_gps:
            for node_id, data in self.devices.items():
                if has_valid_gps(data):
                    color = PROTOCOL_COLORS.get(data['device_type'], '#95a5a6')
                    self.ax_map.scatter(data['lon'], data['lat'], 
                                      c=color, s=100, alpha=0.7, 
                                      edgecolors='white', linewidth=1)
                    self.ax_map.text(data['lon'], data['lat'], f"0x{node_id[:4]}", 
                                   fontsize=7, ha='center', va='bottom')
                    
                    # Draw trail if device has moved
                    if len(data['positions']) > 1:
                        lats = [p[0] for p in data['positions']]
                        lons = [p[1] for p in data['positions']]
                        self.ax_map.plot(lons, lats, color=color, alpha=0.3, linewidth=1)
            
            self.ax_map.set_xlabel('Longitude', fontsize=9)
            self.ax_map.set_ylabel('Latitude', fontsize=9)
            self.ax_map.set_title('GPS Positions', fontweight='bold', fontsize=10)
            self.ax_map.grid(True, alpha=0.3)
        else:
            self.ax_map.text(0.5, 0.5, 'No GPS data captured yet', 
                           ha='center', va='center', fontsize=12,
                           transform=self.ax_map.transAxes)
            self.ax_map.set_title('GPS Positions', fontweight='bold', fontsize=10)
        
        # Update main title with stats
        elapsed = time.time() - self.start_time
        rate = len(self.packet_times) / 10.0 if len(self.packet_times) > 0 else 0.0
        status = f"LIVE | {len(self.devices)} devices | {self.total_packets} pkts | {rate:.1f} pkt/s | {elapsed:.0f}s"
        self.fig.suptitle(f'ESP32 LoRa Sniffer - {status}',
                         fontsize=14, fontweight='bold')
        
        # Auto-screenshot on milestones
        if self.record and self.total_packets in [10, 50, 100, 500]:
            self.save_screenshot(f"milestone_{self.total_packets}pkts")
    
    def save_screenshot(self, name):
        """Save screenshot for presentation"""
        filename = self.screenshot_dir / f"{name}_{int(time.time())}.png"
        self.fig.savefig(filename, dpi=150, bbox_inches='tight')
        print(f"[📸] Screenshot saved: {filename}")
    
    def export_interactive_map(self, filename='lora_map.html'):
        """Export interactive HTML map using folium"""
        if not FOLIUM_AVAILABLE:
            print("[!] Folium not available - install with: pip install folium")
            return False
        
        # Check if we have GPS data (exclude 0,0 — no valid fix)
        devices_with_gps = {nid: data for nid, data in self.devices.items()
                           if data['lat'] is not None and not (data['lat'] == 0.0 and data['lon'] == 0.0)}
        
        if not devices_with_gps:
            print("[!] No GPS data to export")
            return False
        
        # Calculate map center (average of all positions)
        avg_lat = sum(d['lat'] for d in devices_with_gps.values()) / len(devices_with_gps)
        avg_lon = sum(d['lon'] for d in devices_with_gps.values()) / len(devices_with_gps)
        
        # Create map
        m = folium.Map(
            location=[avg_lat, avg_lon],
            zoom_start=13,
            tiles='OpenStreetMap'
        )
        
        # Add devices as markers
        for node_id, data in devices_with_gps.items():
            # Choose marker color based on protocol
            color_map = {
                'Meshtastic': 'blue',
                'LoRaWAN': 'orange',
                'Helium': 'green',
                'Unknown': 'gray'
            }
            color = color_map.get(data['device_type'], 'gray')
            
            # Create popup content
            last_rssi = data['rssi'][-1] if data['rssi'] else 'N/A'
            popup_html = f"""
                <b>Device: 0x{node_id[:8]}</b><br>
                Type: {data['device_type']}<br>
                Packets: {data['packet_count']}<br>
                RSSI: {last_rssi} dBm<br>
                Lat: {data['lat']:.6f}<br>
                Lon: {data['lon']:.6f}
            """
            
            folium.Marker(
                location=[data['lat'], data['lon']],
                popup=folium.Popup(popup_html, max_width=250),
                tooltip=f"0x{node_id[:8]} ({data['device_type']})",
                icon=folium.Icon(color=color, icon='info-sign')
            ).add_to(m)
            
            # Add trail if device has moved
            if len(data['positions']) > 1:
                trail_coords = [[p[0], p[1]] for p in data['positions']]
                folium.PolyLine(
                    trail_coords,
                    color=PROTOCOL_COLORS.get(data['device_type'], '#95a5a6'),
                    weight=2,
                    opacity=0.6,
                    popup=f"0x{node_id[:4]} trail"
                ).add_to(m)
        
        # Add legend
        legend_html = """
        <div style="position: fixed; 
                    top: 10px; right: 10px; 
                    background-color: white;
                    border: 2px solid grey;
                    border-radius: 5px;
                    padding: 10px;
                    font-size: 14px;
                    z-index: 9999;">
            <p><b>ESP32 LoRa Sniffer</b></p>
            <p>🔵 Meshtastic</p>
            <p>🟠 LoRaWAN</p>
            <p>🟢 Helium</p>
            <p>⚪ Unknown</p>
        </div>
        """
        m.get_root().html.add_child(folium.Element(legend_html))
        
        # Save map
        output_path = self.screenshot_dir / filename if self.record else Path(filename)
        m.save(str(output_path))
        print(f"[🗺️] Interactive map saved: {output_path}")
        
        # Try to open in browser
        try:
            import webbrowser
            webbrowser.open(f'file://{output_path.absolute()}')
            print(f"[🌐] Map opened in browser")
        except Exception:
            print(f"[!] Could not auto-open browser. Manually open: {output_path}")
        
        return True
    
    def on_key(self, event):
        """Handle keypress events"""
        if event.key == 'm':
            print("\n[*] Exporting map...")
            if not FOLIUM_AVAILABLE:
                print("[!] Install folium: pip install folium")
                return
            devices_with_gps = {nid: d for nid, d in self.devices.items()
                                if d['lat'] is not None and not (d['lat'] == 0.0 and d['lon'] == 0.0)}
            if not devices_with_gps:
                print("[!] No GPS data yet")
                return
            # Temporarily enable record path so export_interactive_map works
            orig_record = self.record
            orig_dir = self.screenshot_dir
            self.record = False
            self.export_interactive_map(filename='lora_map.html')
            self.record = orig_record
            self.screenshot_dir = orig_dir

    def run(self):
        """Start the live visualization"""
        ani = FuncAnimation(self.fig, self.update_plots,
                           interval=UPDATE_INTERVAL,
                           cache_frame_data=False)
        self.fig.canvas.mpl_connect('key_press_event', self.on_key)
        print("[*] Press 'm' in the plot window (not terminal) to export GPS map to browser")

        try:
            plt.show()
        except KeyboardInterrupt:
            print("\n[*] Visualization stopped by user")
        finally:
            if self.record:
                self.save_screenshot("final")
                # Export interactive map if GPS data available
                self.export_interactive_map()
            if self.ser:
                self.ser.close()

def list_serial_ports():
    """List available serial ports"""
    try:
        import serial.tools.list_ports
        ports = list(serial.tools.list_ports.comports())
        
        if not ports:
            print("[!] No serial ports found")
            return []
        
        print("\n📡 Available Serial Ports:\n")
        for i, port in enumerate(ports, 1):
            print(f"  {i}. {port.device}")
            print(f"     Description: {port.description}")
            print(f"     Manufacturer: {port.manufacturer or 'Unknown'}")
            print()
        
        return ports
    except ImportError:
        print("[!] pyserial not installed")
        return []


def detect_esp32():
    """Try to auto-detect ESP32 port"""
    try:
        import serial.tools.list_ports
        ports = list(serial.tools.list_ports.comports())
        
        # Look for common ESP32 identifiers
        for port in ports:
            desc_lower = (port.description or '').lower()
            mfr_lower = (port.manufacturer or '').lower()
            
            if any(kw in desc_lower for kw in ['cp210', 'ch340', 'esp32', 'uart', 'silicon']):
                return port.device
            if any(kw in mfr_lower for kw in ['silicon labs', 'wch', 'espressif']):
                return port.device
        
        return None
    except ImportError:
        return None


def open_web_ui(esp32_ip='192.168.4.1'):
    """Open web UI in default browser"""
    try:
        import webbrowser
        url = f"http://{esp32_ip}"
        webbrowser.open(url)
        print(f"[✓] Web UI opened: {url}")
        return True
    except Exception as e:
        print(f"[!] Could not open browser: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description='ESP32 LoRa Sniffer - Conference Demo Visualizer',
        epilog='Example: python enhanced_live_visualizer.py COM3 --audio --record --web'
    )
    parser.add_argument('port', nargs='?', help='Serial port (e.g., COM3 or /dev/ttyUSB0)')
    parser.add_argument('--list-ports', action='store_true',
                       help='List available serial ports and exit')
    parser.add_argument('--auto-detect', action='store_true',
                       help='Auto-detect ESP32 port')
    parser.add_argument('--json', action='store_true', 
                       help='Enable JSON output mode on ESP32')
    parser.add_argument('--audio', action='store_true',
                       help='Enable audio feedback (Geiger counter effect)')
    parser.add_argument('--record', action='store_true',
                       help='Auto-save screenshots at milestones')
    parser.add_argument('--demo', action='store_true',
                       help='Demo mode: generate simulated traffic (no hardware needed)')
    parser.add_argument('--web', action='store_true',
                       help='Open web UI in browser')
    parser.add_argument('--web-ip', default='192.168.4.1',
                       help='ESP32 web UI IP address (default: 192.168.4.1)')
    parser.add_argument('--duration', type=int, metavar='SECONDS',
                       help='Auto-exit after duration (for scripted demos)')
    
    args = parser.parse_args()
    
    print("="*70)
    print("  ESP32 LoRa Sniffer - Conference Demo Visualizer")
    print("="*70)
    print()
    
    # Handle --list-ports
    if args.list_ports:
        list_serial_ports()
        return 0
    
    # Determine port (not required in demo mode)
    port = args.port
    
    # Demo mode doesn't need hardware
    if args.demo:
        print("[*] 🎮 DEMO MODE ACTIVATED")
        print("[*] Simulating realistic LoRa traffic for conference presentation")
        print()
    elif args.auto_detect:
        print("[*] Auto-detecting ESP32...")
        detected = detect_esp32()
        if detected:
            print(f"[✓] Found ESP32 on: {detected}")
            port = detected
        else:
            print("[!] Auto-detection failed")
            if not port:
                list_serial_ports()
                return 1
    
    if not port and not args.demo:
        print("[!] No port specified. Use --list-ports to see available ports.")
        print()
        print("Usage examples:")
        print("  python enhanced_live_visualizer.py COM3")
        print("  python enhanced_live_visualizer.py /dev/ttyUSB0 --audio --record")
        print("  python enhanced_live_visualizer.py --auto-detect --web")
        print("  python enhanced_live_visualizer.py --demo              # No hardware needed!")
        return 1
    
    # Open web UI if requested (only if hardware is connected)
    if args.web and not args.demo:
        open_web_ui(args.web_ip)
    
    if args.audio and not AUDIO_AVAILABLE:
        print("[!] Audio requested but dependencies missing")
        print("[!] Install: pip install sounddevice numpy")
        print()
    
    print(f"[*] Features enabled:")
    if args.demo:
        print("    🎮 Demo mode (simulated traffic)")
    if args.audio and AUDIO_AVAILABLE:
        print("    🔊 Audio feedback")
    if args.record:
        print("    📸 Screenshot recording")
    if args.web and not args.demo:
        print(f"    🌐 Web UI at http://{args.web_ip}")
    if args.duration:
        print(f"    ⏱️  Auto-exit after {args.duration}s")
    print()
    
    visualizer = EnhancedLoRaVisualizer(
        port, 
        json_mode=args.json,
        audio=args.audio,
        record=args.record,
        demo_mode=args.demo
    )
    
    if args.duration:
        # Run with timeout
        import threading
        def timeout_handler():
            import time
            time.sleep(args.duration)
            print(f"\n[*] Duration ({args.duration}s) complete, exiting...")
            plt.close('all')
        
        timer = threading.Thread(target=timeout_handler, daemon=True)
        timer.start()
    
    visualizer.run()
    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main() or 0)
