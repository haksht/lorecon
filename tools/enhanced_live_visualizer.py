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

Usage:
    python enhanced_live_visualizer.py COM3
    python enhanced_live_visualizer.py COM3 --json --audio --record

Requirements:
    pip install pyserial matplotlib folium sounddevice numpy
"""

import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from matplotlib.patches import Circle
from collections import deque, defaultdict
import re
import sys
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
    print("[!] Audio feedback disabled (install sounddevice + numpy to enable)")

# Try optional folium for interactive maps
try:
    import folium
    FOLIUM_AVAILABLE = True
except ImportError:
    FOLIUM_AVAILABLE = False
    print("[!] Interactive maps disabled (install folium to enable)")

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

class EnhancedLoRaVisualizer:
    def __init__(self, port, json_mode=False, audio=False, record=False):
        self.port = port
        self.json_mode = json_mode
        self.ser = None
        self.audio = AudioFeedback(enabled=audio)
        self.record = record
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
        
        self.fig.suptitle('🎯 ESP32 LoRa Sniffer - Conference Demo Mode', 
                         fontsize=16, fontweight='bold')
        
        # Connect to serial port
        self.connect()
        
    def connect(self):
        """Connect to ESP32 via serial port"""
        try:
            print(f"[*] Connecting to {self.port} at {SERIAL_BAUDRATE} baud...")
            self.ser = serial.Serial(self.port, SERIAL_BAUDRATE, timeout=1)
            time.sleep(2)
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
            # Pattern: [RECON] Packet #42: Meshtastic, 0x401ACD4E, -68.5 dBm, 8.2 dB SNR
            match = re.search(
                r'\[(PACKET|RECON|CAPTURE)\].*?(?:0x([0-9A-Fa-f]{8}))?.*?(-?\d+\.\d+)\s*dBm',
                line
            )
            
            if match:
                node_id = match.group(2)
                rssi = float(match.group(3))
                
                # Extract protocol
                protocol = 'Unknown'
                for p in PROTOCOL_COLORS.keys():
                    if p in line:
                        protocol = p
                        break
                
                return {
                    'type': 'packet',
                    'node_id': node_id.upper() if node_id else None,
                    'rssi': rssi,
                    'protocol': protocol,
                    'timestamp': time.time()
                }
            
            # GPS extraction: [GEO] 📍 Position: 35.730228° N, 78.879128° W
            gps_match = re.search(r'\[GEO\].*?(-?\d+\.\d+)°.*?(-?\d+\.\d+)°', line)
            if gps_match:
                return {
                    'type': 'gps',
                    'lat': float(gps_match.group(1)),
                    'lon': float(gps_match.group(2)),
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
            # Associate GPS with most recent device
            # (In production, JSON would include node_id with GPS)
            if self.devices:
                last_device = list(self.devices.values())[-1]
                last_device['lat'] = event['lat']
                last_device['lon'] = event['lon']
                last_device['positions'].append((event['lat'], event['lon'], time.time()))
    
    def update_plots(self, frame):
        """Update all 5 panels"""
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
        self.ax_rssi.set_title('📡 Signal Strength', fontweight='bold', fontsize=10)
        self.ax_rssi.grid(True, alpha=0.3)
        self.ax_rssi.axhline(y=-80, color='yellow', linestyle='--', linewidth=1, alpha=0.5)
        self.ax_rssi.axhline(y=-60, color='green', linestyle='--', linewidth=1, alpha=0.5)
        if 0 < len(self.devices) <= 4:
            self.ax_rssi.legend(loc='lower left', fontsize=7)
        
        # Panel 2: Device list
        self.ax_devices.axis('off')
        stats_text = "🎯 DEVICES\n" + "="*25 + "\n\n"
        
        if len(self.devices) == 0:
            stats_text += "Scanning...\n"
        else:
            sorted_devices = sorted(
                self.devices.items(),
                key=lambda x: x[1]['packet_count'],
                reverse=True
            )
            
            for i, (node_id, data) in enumerate(sorted_devices[:6], 1):
                last_rssi = data['rssi'][-1] if data['rssi'] else 0
                quality = "█" * max(1, int((last_rssi + 100) / 10))
                
                stats_text += f"{i}. 0x{node_id[:8]}\n"
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
            self.ax_protocols.set_title('📊 Protocols', fontweight='bold', fontsize=10)
        else:
            self.ax_protocols.text(0.5, 0.5, 'Waiting...', ha='center', va='center',
                                 transform=self.ax_protocols.transAxes)
            self.ax_protocols.set_title('📊 Protocols', fontweight='bold', fontsize=10)
        
        # Panel 4: Packet histogram
        if len(self.devices) > 0:
            node_ids = [f"0x{nid[:4]}" for nid in self.devices.keys()]
            packet_counts = [data['packet_count'] for data in self.devices.values()]
            colors_list = [PROTOCOL_COLORS.get(data['device_type'], '#95a5a6') 
                          for data in self.devices.values()]
            
            bars = self.ax_packets.bar(range(len(node_ids)), packet_counts, 
                                      color=colors_list, alpha=0.8)
            self.ax_packets.set_xlabel('Device', fontsize=9)
            self.ax_packets.set_ylabel('Packets', fontsize=9)
            self.ax_packets.set_title('📦 Captures', fontweight='bold', fontsize=10)
            self.ax_packets.set_xticks(range(len(node_ids)))
            self.ax_packets.set_xticklabels(node_ids, rotation=45, ha='right', fontsize=7)
            self.ax_packets.grid(True, alpha=0.3, axis='y')
        
        # Panel 5: GPS Map
        has_gps = any(d['lat'] is not None for d in self.devices.values())
        
        if has_gps:
            for node_id, data in self.devices.items():
                if data['lat'] is not None:
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
            self.ax_map.set_title('🗺️ Geographic Intelligence', fontweight='bold', fontsize=10)
            self.ax_map.grid(True, alpha=0.3)
        else:
            self.ax_map.text(0.5, 0.5, 'No GPS data captured yet', 
                           ha='center', va='center', fontsize=12,
                           transform=self.ax_map.transAxes)
            self.ax_map.set_title('🗺️ Geographic Intelligence', fontweight='bold', fontsize=10)
        
        # Update main title with stats
        elapsed = time.time() - self.start_time
        rate = len(self.packet_times) / 10.0 if len(self.packet_times) > 0 else 0.0
        status = f"LIVE | {len(self.devices)} devices | {self.total_packets} pkts | {rate:.1f} pkt/s | {elapsed:.0f}s"
        self.fig.suptitle(f'🎯 ESP32 LoRa Sniffer - {status}',
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
        
        # Check if we have GPS data
        devices_with_gps = {nid: data for nid, data in self.devices.items() 
                           if data['lat'] is not None}
        
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
    
    def run(self):
        """Start the live visualization"""
        ani = FuncAnimation(self.fig, self.update_plots,
                           interval=UPDATE_INTERVAL,
                           cache_frame_data=False)
        
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

def main():
    parser = argparse.ArgumentParser(
        description='Enhanced Live Visualization for Conference Demos'
    )
    parser.add_argument('port', help='Serial port (e.g., COM3 or /dev/ttyUSB0)')
    parser.add_argument('--json', action='store_true', 
                       help='Enable JSON output mode on ESP32')
    parser.add_argument('--audio', action='store_true',
                       help='Enable audio feedback (Geiger counter effect)')
    parser.add_argument('--record', action='store_true',
                       help='Auto-save screenshots at milestones')
    
    args = parser.parse_args()
    
    print("="*70)
    print("  ESP32 LoRa Sniffer - Enhanced Live Visualization")
    print("  Conference Demo Edition")
    print("="*70)
    print()
    
    if args.audio and not AUDIO_AVAILABLE:
        print("[!] Audio requested but dependencies missing")
        print("[!] Install: pip install sounddevice numpy")
        print()
    
    visualizer = EnhancedLoRaVisualizer(
        args.port, 
        json_mode=args.json,
        audio=args.audio,
        record=args.record
    )
    visualizer.run()

if __name__ == '__main__':
    main()
