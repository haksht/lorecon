#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - GPS Position Tracker

Dedicated tool for tracking and visualizing device positions from captured data.
Creates animated timelines, movement trails, and interactive maps.

Usage:
    python position_tracker.py capture.csv
    python position_tracker.py capture.csv --animate --output tracking.gif
    python position_tracker.py --live 192.168.4.1 --map live_map.html

Requirements:
    pip install matplotlib folium requests websocket-client
"""

import argparse
import csv
import json
import sys
from collections import defaultdict
from datetime import datetime, timedelta
from pathlib import Path

try:
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation
    import matplotlib.dates as mdates
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False

try:
    import folium
    from folium.plugins import TimestampedGeoJson, AntPath
    FOLIUM_AVAILABLE = True
except ImportError:
    FOLIUM_AVAILABLE = False

try:
    import requests
    REQUESTS_AVAILABLE = True
except ImportError:
    REQUESTS_AVAILABLE = False


# Protocol colors
COLORS = {
    'Meshtastic': '#3498db',
    'LoRaWAN': '#e67e22',
    'Helium': '#2ecc71',
    'Unknown': '#95a5a6',
}


class PositionTracker:
    """Track and visualize GPS positions from LoRa captures"""
    
    def __init__(self):
        self.devices = defaultdict(lambda: {
            'positions': [],  # [(lat, lon, timestamp, rssi), ...]
            'protocol': 'Unknown',
            'packet_count': 0,
        })
        self.all_positions = []
        self.my_location = None  # (lat, lon) - sniffer device location
        self.bounds = {
            'min_lat': None, 'max_lat': None,
            'min_lon': None, 'max_lon': None,
        }
    
    def add_position(self, node_id, lat, lon, timestamp=None, rssi=None, protocol='Unknown'):
        """Add a position record"""
        if timestamp is None:
            timestamp = datetime.now()
        elif isinstance(timestamp, (int, float)):
            timestamp = datetime.fromtimestamp(timestamp)
        
        pos = (lat, lon, timestamp, rssi)
        self.devices[node_id]['positions'].append(pos)
        self.devices[node_id]['protocol'] = protocol
        self.devices[node_id]['packet_count'] += 1
        self.all_positions.append((node_id, lat, lon, timestamp, rssi))
        
        # Update bounds
        if self.bounds['min_lat'] is None or lat < self.bounds['min_lat']:
            self.bounds['min_lat'] = lat
        if self.bounds['max_lat'] is None or lat > self.bounds['max_lat']:
            self.bounds['max_lat'] = lat
        if self.bounds['min_lon'] is None or lon < self.bounds['min_lon']:
            self.bounds['min_lon'] = lon
        if self.bounds['max_lon'] is None or lon > self.bounds['max_lon']:
            self.bounds['max_lon'] = lon
    
    def load_csv(self, filepath):
        """Load positions from CSV capture file"""
        with open(filepath, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                lat = row.get('lat_deg') or row.get('latitude')
                lon = row.get('lon_deg') or row.get('longitude')
                
                if not lat or not lon:
                    continue
                
                try:
                    lat = float(lat)
                    lon = float(lon)
                except ValueError:
                    continue
                
                node_id = row.get('node_id_hex') or row.get('nodeId') or row.get('node_id', 'Unknown')
                timestamp = float(row.get('timestamp_ms', row.get('timestamp', 0))) / 1000
                rssi = float(row.get('rssi_dbm', row.get('rssi', 0)) or 0)
                protocol = row.get('protocol', 'Unknown')
                
                self.add_position(node_id, lat, lon, timestamp, rssi, protocol)
    
    def load_api(self, host):
        """Load positions from ESP32 API"""
        if not REQUESTS_AVAILABLE:
            raise ImportError("requests library required")
        
        r = requests.get(f"http://{host}/api/positions", timeout=10)
        data = r.json()
        
        for pos in data.get('positions', []):
            self.add_position(
                pos.get('nodeId', 'Unknown'),
                pos['lat'],
                pos['lon'],
                pos.get('timestamp', 0) / 1000,
                pos.get('rssi'),
                pos.get('protocol', 'Unknown')
            )
    
    def get_center(self):
        """Get map center point"""
        if not self.all_positions:
            return (0, 0)
        
        avg_lat = (self.bounds['min_lat'] + self.bounds['max_lat']) / 2
        avg_lon = (self.bounds['min_lon'] + self.bounds['max_lon']) / 2
        return (avg_lat, avg_lon)
    
    def get_zoom(self):
        """Estimate appropriate zoom level"""
        if not self.all_positions:
            return 13
        
        lat_range = self.bounds['max_lat'] - self.bounds['min_lat']
        lon_range = self.bounds['max_lon'] - self.bounds['min_lon']
        max_range = max(lat_range, lon_range)
        
        if max_range < 0.01:
            return 17
        elif max_range < 0.05:
            return 15
        elif max_range < 0.2:
            return 13
        elif max_range < 1.0:
            return 11
        else:
            return 9
    
    def generate_static_map(self, output_path):
        """Generate static matplotlib map"""
        if not MATPLOTLIB_AVAILABLE:
            print("Error: matplotlib required")
            return False
        
        fig, ax = plt.subplots(figsize=(12, 10))
        ax.set_facecolor('#1a1a2e')
        fig.patch.set_facecolor('#1a1a2e')
        
        # Plot each device
        for node_id, data in self.devices.items():
            if not data['positions']:
                continue
            
            color = COLORS.get(data['protocol'], COLORS['Unknown'])
            lats = [p[0] for p in data['positions']]
            lons = [p[1] for p in data['positions']]
            
            # Plot trail
            if len(lats) > 1:
                ax.plot(lons, lats, color=color, alpha=0.5, linewidth=2)
            
            # Plot points
            ax.scatter(lons, lats, c=color, s=50, alpha=0.8, 
                      edgecolors='white', linewidth=0.5,
                      label=f"{node_id[:8]} ({data['protocol']})")
            
            # Mark start and end
            ax.scatter([lons[0]], [lats[0]], c='green', s=100, 
                      marker='^', edgecolors='white', linewidth=1, zorder=10)
            ax.scatter([lons[-1]], [lats[-1]], c='red', s=100,
                      marker='v', edgecolors='white', linewidth=1, zorder=10)
        
        ax.set_xlabel('Longitude', color='white')
        ax.set_ylabel('Latitude', color='white')
        ax.set_title('GPS Position Tracking', color='white', fontsize=14, fontweight='bold')
        ax.tick_params(colors='white')
        ax.grid(True, alpha=0.2, color='white')
        
        # Legend
        ax.legend(loc='upper right', facecolor='#2a2a4e', edgecolor='white',
                 labelcolor='white', fontsize=8)
        
        plt.tight_layout()
        plt.savefig(output_path, dpi=150, facecolor=fig.get_facecolor())
        plt.close()
        
        print(f"Static map saved to {output_path}")
        return True
    
    def generate_animated_map(self, output_path, fps=2):
        """Generate animated GIF showing position changes over time"""
        if not MATPLOTLIB_AVAILABLE:
            print("Error: matplotlib required")
            return False
        
        if not self.all_positions:
            print("No positions to animate")
            return False
        
        # Sort by timestamp
        sorted_positions = sorted(self.all_positions, key=lambda x: x[3])
        
        fig, ax = plt.subplots(figsize=(12, 10))
        
        # Track what's been plotted
        plotted = defaultdict(list)
        
        def init():
            ax.clear()
            ax.set_facecolor('#1a1a2e')
            return []
        
        def animate(frame):
            ax.clear()
            ax.set_facecolor('#1a1a2e')
            
            # Plot positions up to this frame
            for i in range(min(frame + 1, len(sorted_positions))):
                node_id, lat, lon, ts, rssi = sorted_positions[i]
                color = COLORS.get(self.devices[node_id]['protocol'], COLORS['Unknown'])
                
                plotted[node_id].append((lon, lat))
                
                # Plot trail
                if len(plotted[node_id]) > 1:
                    lons = [p[0] for p in plotted[node_id]]
                    lats = [p[1] for p in plotted[node_id]]
                    ax.plot(lons, lats, color=color, alpha=0.5, linewidth=2)
                
                # Plot current position
                ax.scatter([lon], [lat], c=color, s=80, alpha=0.9,
                          edgecolors='white', linewidth=1)
            
            ax.set_xlabel('Longitude', color='white')
            ax.set_ylabel('Latitude', color='white')
            
            if frame < len(sorted_positions):
                ts = sorted_positions[frame][3]
                time_str = ts.strftime('%Y-%m-%d %H:%M:%S') if isinstance(ts, datetime) else str(ts)
                ax.set_title(f'Position Tracking - {time_str}', color='white', fontsize=12)
            
            ax.tick_params(colors='white')
            ax.grid(True, alpha=0.2, color='white')
            
            # Set consistent bounds
            if self.bounds['min_lat']:
                ax.set_xlim(self.bounds['min_lon'] - 0.001, self.bounds['max_lon'] + 0.001)
                ax.set_ylim(self.bounds['min_lat'] - 0.001, self.bounds['max_lat'] + 0.001)
            
            return []
        
        anim = FuncAnimation(fig, animate, init_func=init,
                            frames=len(sorted_positions),
                            interval=1000//fps, blit=True)
        
        anim.save(output_path, writer='pillow', fps=fps)
        plt.close()
        
        print(f"Animated map saved to {output_path}")
        return True
    
    def generate_interactive_map(self, output_path, open_browser=True):
        """Generate interactive HTML map with Folium"""
        if not FOLIUM_AVAILABLE:
            print("Error: folium required. Install: pip install folium")
            return False
        
        if not self.all_positions:
            print("No positions to map")
            return False
        
        center = self.get_center()
        zoom = self.get_zoom()
        
        m = folium.Map(location=center, zoom_start=zoom, tiles='CartoDB dark_matter')
        
        # Add sniffer/observer location if set
        if self.my_location:
            folium.Marker(
                location=self.my_location,
                popup='<b>📡 Sniffer Location</b><br>Your device',
                icon=folium.Icon(color='purple', icon='tower-broadcast', prefix='fa'),
            ).add_to(m)
            
            # Add circle showing approximate range
            folium.Circle(
                location=self.my_location,
                radius=500,  # 500m typical LoRa urban range
                color='purple',
                fill=True,
                fill_opacity=0.1,
                popup='~500m range estimate'
            ).add_to(m)
        
        # Add each device
        for node_id, data in self.devices.items():
            if not data['positions']:
                continue
            
            color = COLORS.get(data['protocol'], COLORS['Unknown'])
            
            # Create trail with animated ants
            if len(data['positions']) > 1:
                trail_coords = [[p[0], p[1]] for p in data['positions']]
                
                AntPath(
                    locations=trail_coords,
                    color=color,
                    weight=3,
                    opacity=0.7,
                    delay=1000,
                    dash_array=[10, 20],
                    pulse_color='white'
                ).add_to(m)
            
            # Add markers for each position
            for i, (lat, lon, ts, rssi) in enumerate(data['positions']):
                ts_str = ts.strftime('%H:%M:%S') if isinstance(ts, datetime) else str(ts)
                
                popup_html = f"""
                    <b>Device:</b> {node_id}<br>
                    <b>Protocol:</b> {data['protocol']}<br>
                    <b>Time:</b> {ts_str}<br>
                    <b>RSSI:</b> {rssi or 'N/A'} dBm<br>
                    <b>Position {i+1}/{len(data['positions'])}</b>
                """
                
                # Different icons for start/end/middle
                if i == 0:
                    icon = folium.Icon(color='green', icon='play')
                elif i == len(data['positions']) - 1:
                    icon = folium.Icon(color='red', icon='stop')
                else:
                    icon = folium.Icon(color='blue', icon='map-marker')
                
                folium.Marker(
                    location=[lat, lon],
                    popup=folium.Popup(popup_html, max_width=250),
                    tooltip=f"{node_id[:8]} @ {ts_str}",
                    icon=icon
                ).add_to(m)
        
        # Add legend
        legend_html = """
        <div style="position: fixed; 
                    bottom: 50px; left: 50px; 
                    background-color: rgba(0,0,0,0.8);
                    border: 2px solid white;
                    border-radius: 5px;
                    padding: 10px;
                    font-size: 14px;
                    color: white;
                    z-index: 9999;">
            <p><b>📍 Position Tracker</b></p>
            <p>🟢 Start position</p>
            <p>🔴 Last position</p>
            <p>🔵 Intermediate</p>
            <hr style="border-color: #444;">
            <p style="color: #3498db;">● Meshtastic</p>
            <p style="color: #e67e22;">● LoRaWAN</p>
            <p style="color: #2ecc71;">● Helium</p>
        </div>
        """
        m.get_root().html.add_child(folium.Element(legend_html))
        
        # Add title
        title_html = f"""
        <div style="position: fixed; 
                    top: 10px; left: 50%; transform: translateX(-50%);
                    background-color: rgba(0,0,0,0.8);
                    border: 2px solid white;
                    border-radius: 5px;
                    padding: 10px 20px;
                    font-size: 18px;
                    font-weight: bold;
                    color: white;
                    z-index: 9999;">
            🛰️ LoRa Position Tracker - {len(self.devices)} devices, {len(self.all_positions)} positions
        </div>
        """
        m.get_root().html.add_child(folium.Element(title_html))
        
        # Add "Locate Me" button with browser geolocation
        locate_me_html = """
        <button id="locateBtn" onclick="locateMe()" style="
            position: fixed;
            bottom: 20px;
            right: 20px;
            padding: 12px 20px;
            background: #9b59b6;
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            cursor: pointer;
            z-index: 9999;
            box-shadow: 0 2px 10px rgba(0,0,0,0.3);
        ">📍 Locate Me</button>
        <script>
        var myMarker = null;
        var myCircle = null;
        function locateMe() {
            var btn = document.getElementById('locateBtn');
            btn.textContent = '⏳ Locating...';
            if (navigator.geolocation) {
                navigator.geolocation.getCurrentPosition(function(pos) {
                    var lat = pos.coords.latitude;
                    var lon = pos.coords.longitude;
                    var acc = pos.coords.accuracy;
                    
                    // Remove old marker if exists
                    if (myMarker) { myMarker.remove(); }
                    if (myCircle) { myCircle.remove(); }
                    
                    // Add marker at my location
                    myMarker = L.marker([lat, lon], {
                        icon: L.divIcon({
                            html: '<div style="background:#9b59b6;color:white;padding:8px 12px;border-radius:50%;font-size:20px;border:3px solid white;box-shadow:0 2px 10px rgba(0,0,0,0.5);">📡</div>',
                            iconSize: [40, 40],
                            iconAnchor: [20, 20]
                        })
                    }).addTo(window.map || Object.values(window).find(v => v instanceof L.Map));
                    myMarker.bindPopup('<b>📡 You are here</b><br>Accuracy: ' + Math.round(acc) + 'm');
                    
                    // Add accuracy circle
                    var map = window.map || Object.values(window).find(v => v instanceof L.Map);
                    myCircle = L.circle([lat, lon], {radius: acc, color: '#9b59b6', fillOpacity: 0.1}).addTo(map);
                    
                    map.setView([lat, lon], 16);
                    btn.textContent = '📍 Located!';
                    setTimeout(function() { btn.textContent = '📍 Locate Me'; }, 2000);
                }, function(err) {
                    btn.textContent = '❌ ' + err.message;
                    setTimeout(function() { btn.textContent = '📍 Locate Me'; }, 3000);
                }, {enableHighAccuracy: true, timeout: 10000});
            } else {
                btn.textContent = '❌ Not supported';
            }
        }
        </script>
        """
        m.get_root().html.add_child(folium.Element(locate_me_html))
        
        m.save(output_path)
        print(f"Interactive map saved to {output_path}")
        
        # Try to open in browser (only if requested)
        if open_browser:
            try:
                import webbrowser
                webbrowser.open(f'file://{Path(output_path).absolute()}')
            except Exception:
                pass
        
        return True
    
    def print_summary(self):
        """Print tracking summary"""
        print("\n" + "="*60)
        print("📍 POSITION TRACKING SUMMARY")
        print("="*60)
        
        print(f"\nDevices tracked: {len(self.devices)}")
        print(f"Total positions: {len(self.all_positions)}")
        
        if self.all_positions:
            times = [p[3] for p in self.all_positions if isinstance(p[3], datetime)]
            if times:
                duration = max(times) - min(times)
                print(f"Time span: {duration}")
        
        print(f"\nDevice Details:")
        print("-"*60)
        
        for node_id, data in sorted(self.devices.items(), 
                                    key=lambda x: len(x[1]['positions']), 
                                    reverse=True):
            positions = data['positions']
            if not positions:
                continue
            
            print(f"\n  {node_id}")
            print(f"    Protocol: {data['protocol']}")
            print(f"    Positions: {len(positions)}")
            
            if len(positions) >= 2:
                # Calculate distance traveled (simple)
                total_dist = 0
                for i in range(1, len(positions)):
                    lat1, lon1 = positions[i-1][0], positions[i-1][1]
                    lat2, lon2 = positions[i][0], positions[i][1]
                    # Approximate km (at equator)
                    dist = ((lat2-lat1)**2 + (lon2-lon1)**2)**0.5 * 111
                    total_dist += dist
                print(f"    Est. distance: {total_dist:.2f} km")
            
            if positions:
                latest = positions[-1]
                print(f"    Last position: {latest[0]:.6f}, {latest[1]:.6f}")


def monitor_live(host, tracker, output_path, update_interval=5):
    """Monitor positions live from ESP32"""
    if not REQUESTS_AVAILABLE:
        print("Error: requests library required")
        return
    
    print(f"Monitoring live positions from {host}...")
    print(f"Map will be updated every {update_interval} seconds")
    print("Press Ctrl+C to stop\n")
    
    import time
    browser_opened = False
    
    try:
        while True:
            try:
                tracker.load_api(host)
                
                if tracker.all_positions:
                    # Only open browser on first successful update
                    tracker.generate_interactive_map(output_path, open_browser=not browser_opened)
                    if not browser_opened:
                        browser_opened = True
                    print(f"[{datetime.now().strftime('%H:%M:%S')}] "
                          f"Map updated: {len(tracker.devices)} devices, "
                          f"{len(tracker.all_positions)} positions")
                else:
                    print(f"[{datetime.now().strftime('%H:%M:%S')}] "
                          f"No positions yet...")
                
                time.sleep(update_interval)
                
            except requests.exceptions.ConnectionError:
                print(f"Connection lost, retrying...")
                time.sleep(2)
            except Exception as e:
                print(f"Error: {e}")
                time.sleep(2)
                
    except KeyboardInterrupt:
        print("\nStopped monitoring")
        tracker.print_summary()


def generate_demo_data(tracker, center=None):
    """Generate simulated GPS tracks for demo presentations
    
    Creates realistic movement patterns around a center point
    with multiple devices showing different behaviors.
    
    Args:
        tracker: PositionTracker instance
        center: (lat, lon) tuple, defaults to Raleigh NC if not provided
    """
    import random
    import math
    
    # Default to Raleigh NC (user's area) if no center provided
    if center:
        CENTER_LAT, CENTER_LON = center
        print(f"  📍 Centering demo around: {CENTER_LAT:.4f}, {CENTER_LON:.4f}")
    else:
        CENTER_LAT = 35.7796
        CENTER_LON = -78.6382
        print(f"  📍 Using default location: Raleigh, NC")
    
    # Simulated devices with different behaviors
    # Node IDs are formatted like actual Meshtastic IDs for realism
    devices = [
        # (node_id, behavior, protocol)
        ("!a1b2c3d4", "wander", "Meshtastic"),     # Attendee wandering exhibit hall
        ("!f5e6d7c8", "patrol", "Meshtastic"),     # Security patrolling perimeter
        ("!9a8b7c6d", "stationary", "Meshtastic"), # Fixed booth transmitter
        ("!1234abcd", "fast_move", "Meshtastic"),  # Vehicle in parking area
        ("!deadbeef", "scheduled", "Meshtastic"),  # Speaker moving between talks
    ]
    
    base_time = datetime.now()
    behavior_desc = {
        "wander": "Attendee wandering exhibit hall",
        "patrol": "Security patrolling perimeter",
        "stationary": "Fixed booth transmitter",
        "fast_move": "Vehicle in parking area",
        "scheduled": "Speaker moving between talks",
    }
    
    for node_id, behavior, protocol in devices:
        print(f"  📍 {node_id}: {behavior_desc[behavior]}")
        
        # Generate 30-60 positions per device over "2 hours"
        num_positions = random.randint(30, 60)
        
        if behavior == "stationary":
            # Fixed position with GPS jitter
            base_lat = CENTER_LAT + random.uniform(-0.002, 0.002)
            base_lon = CENTER_LON + random.uniform(-0.002, 0.002)
            
            for i in range(num_positions):
                lat = base_lat + random.gauss(0, 0.00002)
                lon = base_lon + random.gauss(0, 0.00002)
                ts = base_time + timedelta(minutes=i*4)
                tracker.add_position(node_id, lat, lon, ts, protocol=protocol)
                
        elif behavior == "wander":
            # Random walk through venue
            lat = CENTER_LAT + random.uniform(-0.001, 0.001)
            lon = CENTER_LON + random.uniform(-0.001, 0.001)
            
            for i in range(num_positions):
                lat += random.gauss(0, 0.0003)
                lon += random.gauss(0, 0.0003)
                # Keep within venue bounds
                lat = max(CENTER_LAT - 0.004, min(CENTER_LAT + 0.004, lat))
                lon = max(CENTER_LON - 0.004, min(CENTER_LON + 0.004, lon))
                ts = base_time + timedelta(minutes=i*2 + random.randint(0, 30))
                tracker.add_position(node_id, lat, lon, ts, protocol=protocol)
                
        elif behavior == "patrol":
            # Circular patrol pattern
            radius = 0.003
            for i in range(num_positions):
                angle = (i / num_positions) * 2 * math.pi * 3  # 3 loops
                lat = CENTER_LAT + radius * math.sin(angle) + random.gauss(0, 0.0001)
                lon = CENTER_LON + radius * math.cos(angle) + random.gauss(0, 0.0001)
                ts = base_time + timedelta(minutes=i*3)
                tracker.add_position(node_id, lat, lon, ts, protocol=protocol)
                
        elif behavior == "fast_move":
            # Vehicle moving in parking lot / nearby streets
            lat = CENTER_LAT + 0.005  # Start offset
            lon = CENTER_LON - 0.003
            direction = random.uniform(0, 2 * math.pi)
            
            for i in range(num_positions):
                # Occasionally change direction
                if random.random() < 0.15:
                    direction += random.uniform(-0.5, 0.5)
                lat += 0.0008 * math.sin(direction) + random.gauss(0, 0.0001)
                lon += 0.0008 * math.cos(direction) + random.gauss(0, 0.0001)
                ts = base_time + timedelta(minutes=i*1)
                tracker.add_position(node_id, lat, lon, ts, protocol=protocol)
                
        elif behavior == "scheduled":
            # Moves between fixed "talk locations" then stays
            talk_locations = [
                (CENTER_LAT + 0.001, CENTER_LON - 0.001),  # Hall A
                (CENTER_LAT - 0.001, CENTER_LON + 0.001),  # Hall B
                (CENTER_LAT + 0.002, CENTER_LON + 0.002),  # Main stage
            ]
            
            current_loc = 0
            positions_per_loc = num_positions // len(talk_locations)
            
            for i in range(num_positions):
                loc_idx = min(i // positions_per_loc, len(talk_locations) - 1)
                target_lat, target_lon = talk_locations[loc_idx]
                
                # Add some movement noise
                lat = target_lat + random.gauss(0, 0.00015)
                lon = target_lon + random.gauss(0, 0.00015)
                ts = base_time + timedelta(minutes=i*2)
                tracker.add_position(node_id, lat, lon, ts, protocol=protocol)
    
    # Set "our" location at the demo booth (if not already set by user)
    if not tracker.my_location:
        tracker.my_location = (CENTER_LAT - 0.001, CENTER_LON)
    
    print(f"\n✅ Generated {len(tracker.all_positions)} positions for {len(tracker.devices)} devices")


def main():
    parser = argparse.ArgumentParser(
        description='GPS Position Tracker for LoRa Device Movements',
        epilog='Example: python position_tracker.py capture.csv --map tracking.html'
    )
    parser.add_argument('input', nargs='?', help='Input CSV file')
    parser.add_argument('--live', metavar='HOST',
                       help='Monitor live from ESP32 API')
    parser.add_argument('--map', metavar='FILE',
                       help='Generate interactive HTML map')
    parser.add_argument('--static', metavar='FILE',
                       help='Generate static PNG map')
    parser.add_argument('--animate', metavar='FILE',
                       help='Generate animated GIF')
    parser.add_argument('--fps', type=int, default=2,
                       help='Animation FPS (default: 2)')
    parser.add_argument('--update-interval', type=int, default=5,
                       help='Live monitoring update interval in seconds')
    parser.add_argument('--my-location', metavar='LAT,LON',
                       help='Your sniffer location (e.g., "35.7796,-78.6382")')
    parser.add_argument('--demo', action='store_true',
                       help='Demo mode with simulated GPS tracks (no input needed)')
    
    args = parser.parse_args()
    
    tracker = PositionTracker()
    
    # Parse and set sniffer location if provided
    if args.my_location:
        try:
            lat, lon = map(float, args.my_location.split(','))
            tracker.my_location = (lat, lon)
            print(f"📡 Sniffer location set: {lat:.6f}, {lon:.6f}")
        except ValueError:
            print(f"Warning: Invalid --my-location format. Use: LAT,LON (e.g., 35.7796,-78.6382)")
    
    # Demo mode - generate synthetic tracks
    if args.demo:
        print("\n🎮 DEMO MODE: Generating simulated GPS tracks...\n")
        # Use user's location if provided, otherwise default to Raleigh NC
        center = tracker.my_location if tracker.my_location else (35.7796, -78.6382)
        generate_demo_data(tracker, center)
        
        output = args.map or 'demo_tracking.html'
        tracker.generate_interactive_map(output)
        return 0
    
    if not args.input and not args.live:
        parser.print_help()
        return 1
    
    if args.live:
        output = args.map or 'live_tracking.html'
        monitor_live(args.live, tracker, output, args.update_interval)
    else:
        filepath = Path(args.input)
        if not filepath.exists():
            print(f"Error: File not found: {filepath}")
            return 1
        
        print(f"Loading {filepath}...")
        tracker.load_csv(filepath)
        
        if not tracker.all_positions:
            print("No GPS positions found in capture file")
            return 1
        
        tracker.print_summary()
        
        # Generate outputs
        if args.map:
            tracker.generate_interactive_map(args.map)
        
        if args.static:
            tracker.generate_static_map(args.static)
        
        if args.animate:
            tracker.generate_animated_map(args.animate, args.fps)
        
        # Default: generate interactive map if no output specified
        if not (args.map or args.static or args.animate):
            output = filepath.stem + '_tracking.html'
            tracker.generate_interactive_map(output)
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
