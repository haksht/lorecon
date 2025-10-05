#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - Live Visualization Tool

Real-time visualization of LoRa reconnaissance data from ESP32 sniffer.
Connects via serial port and displays live graphs of:
- Device signal strength (RSSI) over time
- Packet rate histogram
- Device discovery timeline

Usage:
    python live_visualizer.py [COM_PORT]
    
Example:
    python live_visualizer.py COM3
    python live_visualizer.py /dev/ttyUSB0

Requirements:
    pip install pyserial matplotlib
"""

import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from collections import deque, defaultdict
import re
import sys
import time
from datetime import datetime

# Configuration
SERIAL_BAUDRATE = 115200
UPDATE_INTERVAL = 200  # milliseconds
MAX_HISTORY_POINTS = 200
WINDOW_SIZE = (14, 8)

class LoRaVisualizer:
    def __init__(self, port):
        self.port = port
        self.ser = None
        
        # Data storage
        self.devices = defaultdict(lambda: {
            'rssi': deque(maxlen=MAX_HISTORY_POINTS),
            'time': deque(maxlen=MAX_HISTORY_POINTS),
            'packet_count': 0,
            'last_seen': time.time(),
            'device_type': 'Unknown'
        })
        
        self.start_time = time.time()
        self.total_packets = 0
        self.packet_times = deque(maxlen=100)  # For packet rate calculation
        
        # Setup matplotlib
        plt.style.use('dark_background')
        self.fig, self.axes = plt.subplots(2, 2, figsize=WINDOW_SIZE)
        self.fig.suptitle('🔐 ESP32 LoRa Sniffer - Live Reconnaissance', 
                         fontsize=16, fontweight='bold')
        
        # Connect to serial port
        self.connect()
        
    def connect(self):
        """Connect to ESP32 via serial port"""
        try:
            print(f"[*] Connecting to {self.port} at {SERIAL_BAUDRATE} baud...")
            self.ser = serial.Serial(self.port, SERIAL_BAUDRATE, timeout=1)
            time.sleep(2)  # Wait for connection to stabilize
            print(f"[✓] Connected successfully!")
            print(f"[*] Starting live visualization...\n")
        except serial.SerialException as e:
            print(f"[✗] Failed to connect to {self.port}: {e}")
            print(f"[!] Available ports: {self.list_ports()}")
            sys.exit(1)
    
    @staticmethod
    def list_ports():
        """List available serial ports"""
        import serial.tools.list_ports
        ports = serial.tools.list_ports.comports()
        return [port.device for port in ports]
    
    def parse_serial_line(self, line):
        """Parse serial output for packet information"""
        try:
            # Pattern 1: [PACKET] or [RECON] or [CAPTURE] lines with RSSI
            # Example: [PACKET] Meshtastic, 14 bytes, -45.0 dBm, 8.2 dB SNR
            packet_match = re.search(
                r'\[(PACKET|RECON|CAPTURE)\].*?(-?\d+\.\d+)\s*dBm',
                line
            )
            
            if packet_match:
                rssi = float(packet_match.group(2))
                
                # Try to extract node ID
                # Example: Node ID: 0x401ACD4E or just 0x401ACD4E
                node_match = re.search(r'0x([0-9A-Fa-f]{8})', line)
                if node_match:
                    node_id = node_match.group(1).upper()
                    
                    # Extract device type if present
                    type_match = re.search(r'(Meshtastic|LoRaWAN|Beacon)', line)
                    device_type = type_match.group(1) if type_match else 'Unknown'
                    
                    return {
                        'node_id': node_id,
                        'rssi': rssi,
                        'device_type': device_type,
                        'timestamp': time.time()
                    }
            
            # Pattern 2: NODE tracking lines
            # Example: [NODE] New: 0x401ACD4E (Meshtastic)
            node_new_match = re.search(r'\[NODE\].*?0x([0-9A-Fa-f]{8})', line)
            if node_new_match:
                node_id = node_new_match.group(1).upper()
                type_match = re.search(r'\((.*?)\)', line)
                device_type = type_match.group(1) if type_match else 'Unknown'
                
                return {
                    'node_id': node_id,
                    'rssi': -75.0,  # Default placeholder
                    'device_type': device_type,
                    'timestamp': time.time(),
                    'is_new': True
                }
                
        except Exception as e:
            # Silently ignore parsing errors
            pass
        
        return None
    
    def update_data(self, packet_info):
        """Update internal data structures with new packet info"""
        node_id = packet_info['node_id']
        current_time = time.time() - self.start_time
        
        # Update device data
        device = self.devices[node_id]
        device['rssi'].append(packet_info['rssi'])
        device['time'].append(current_time)
        device['packet_count'] += 1
        device['last_seen'] = time.time()
        device['device_type'] = packet_info['device_type']
        
        # Update global stats
        self.total_packets += 1
        self.packet_times.append(time.time())
    
    def update_plots(self, frame):
        """Update all plots with current data"""
        # Read and parse any available serial data
        while self.ser and self.ser.in_waiting:
            try:
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    packet_info = self.parse_serial_line(line)
                    if packet_info:
                        self.update_data(packet_info)
            except Exception as e:
                pass  # Ignore read errors
        
        # Clear all axes
        for ax in self.axes.flat:
            ax.clear()
        
        # Plot 1: RSSI over time (top left)
        ax1 = self.axes[0, 0]
        for node_id, data in self.devices.items():
            if len(data['rssi']) > 0:
                label = f"{node_id[:6]}.. ({data['device_type']})"
                ax1.plot(data['time'], data['rssi'], 
                        marker='o', markersize=2, linewidth=1.5,
                        label=label, alpha=0.8)
        
        ax1.set_xlabel('Time (seconds)', fontsize=10)
        ax1.set_ylabel('RSSI (dBm)', fontsize=10)
        ax1.set_title('📡 Signal Strength Over Time', fontweight='bold')
        ax1.grid(True, alpha=0.3)
        ax1.axhline(y=-80, color='yellow', linestyle='--', linewidth=1, alpha=0.5, label='Weak')
        ax1.axhline(y=-60, color='green', linestyle='--', linewidth=1, alpha=0.5, label='Good')
        if len(self.devices) > 0 and len(self.devices) <= 5:
            ax1.legend(loc='lower left', fontsize=8)
        
        # Plot 2: Device list with stats (top right)
        ax2 = self.axes[0, 1]
        ax2.axis('off')
        
        stats_text = "🎯 DISCOVERED DEVICES\n" + "="*40 + "\n\n"
        
        if len(self.devices) == 0:
            stats_text += "No devices detected yet...\n"
            stats_text += "Waiting for packets...\n"
        else:
            # Sort by packet count (most active first)
            sorted_devices = sorted(
                self.devices.items(),
                key=lambda x: x[1]['packet_count'],
                reverse=True
            )
            
            for i, (node_id, data) in enumerate(sorted_devices[:8], 1):  # Show top 8
                last_rssi = data['rssi'][-1] if data['rssi'] else 0
                age = time.time() - data['last_seen']
                
                # RSSI quality indicator
                if last_rssi > -60:
                    quality = "████████"
                elif last_rssi > -80:
                    quality = "█████░░░"
                elif last_rssi > -100:
                    quality = "███░░░░░"
                else:
                    quality = "█░░░░░░░"
                
                stats_text += f"{i}. 0x{node_id[:8]}\n"
                stats_text += f"   Type: {data['device_type']}\n"
                stats_text += f"   RSSI: {quality} {last_rssi:.1f} dBm\n"
                stats_text += f"   Pkts: {data['packet_count']}\n"
                stats_text += f"   Age:  {age:.0f}s ago\n\n"
        
        ax2.text(0.05, 0.95, stats_text,
                verticalalignment='top',
                fontfamily='monospace',
                fontsize=9,
                transform=ax2.transAxes)
        
        # Plot 3: Packet histogram (bottom left)
        ax3 = self.axes[1, 0]
        
        if len(self.devices) > 0:
            node_ids = [f"0x{nid[:6]}" for nid in self.devices.keys()]
            packet_counts = [data['packet_count'] for data in self.devices.values()]
            
            colors = plt.cm.viridis([i/len(node_ids) for i in range(len(node_ids))])
            bars = ax3.bar(range(len(node_ids)), packet_counts, color=colors, alpha=0.8)
            
            ax3.set_xlabel('Device', fontsize=10)
            ax3.set_ylabel('Packet Count', fontsize=10)
            ax3.set_title('📊 Packet Capture Statistics', fontweight='bold')
            ax3.set_xticks(range(len(node_ids)))
            ax3.set_xticklabels(node_ids, rotation=45, ha='right', fontsize=8)
            ax3.grid(True, alpha=0.3, axis='y')
            
            # Add value labels on bars
            for i, (bar, count) in enumerate(zip(bars, packet_counts)):
                height = bar.get_height()
                ax3.text(bar.get_x() + bar.get_width()/2., height,
                        f'{int(count)}',
                        ha='center', va='bottom', fontsize=8)
        else:
            ax3.text(0.5, 0.5, 'Waiting for packets...',
                    ha='center', va='center',
                    fontsize=12, transform=ax3.transAxes)
            ax3.set_title('📊 Packet Capture Statistics', fontweight='bold')
        
        # Plot 4: Overall stats (bottom right)
        ax4 = self.axes[1, 1]
        ax4.axis('off')
        
        elapsed_time = time.time() - self.start_time
        
        # Calculate packet rate (packets per second)
        recent_packets = sum(1 for t in self.packet_times if time.time() - t < 10)
        packet_rate = recent_packets / 10.0 if recent_packets > 0 else 0.0
        
        summary_text = "📈 RECONNAISSANCE SUMMARY\n"
        summary_text += "="*40 + "\n\n"
        summary_text += f"⏱️  Runtime:       {elapsed_time:.0f} seconds\n"
        summary_text += f"📦 Total Packets:  {self.total_packets}\n"
        summary_text += f"📡 Devices Found:  {len(self.devices)}\n"
        summary_text += f"⚡ Packet Rate:    {packet_rate:.2f} pkt/sec\n\n"
        
        if len(self.devices) > 0:
            summary_text += "🏆 TOP DEVICE:\n"
            top_device = max(self.devices.items(), key=lambda x: x[1]['packet_count'])
            node_id, data = top_device
            summary_text += f"   ID: 0x{node_id[:8]}\n"
            summary_text += f"   Type: {data['device_type']}\n"
            summary_text += f"   Packets: {data['packet_count']}\n"
            last_rssi = data['rssi'][-1] if data['rssi'] else 0
            summary_text += f"   RSSI: {last_rssi:.1f} dBm\n"
        
        ax4.text(0.05, 0.95, summary_text,
                verticalalignment='top',
                fontfamily='monospace',
                fontsize=10,
                transform=ax4.transAxes)
        
        # Update figure title with status
        status = f"LIVE - {len(self.devices)} devices | {self.total_packets} packets | {packet_rate:.1f} pkt/s"
        self.fig.suptitle(f'🔐 ESP32 LoRa Sniffer - {status}',
                         fontsize=14, fontweight='bold')
        
        plt.tight_layout()
    
    def run(self):
        """Start the live visualization"""
        ani = FuncAnimation(self.fig, self.update_plots,
                           interval=UPDATE_INTERVAL,
                           cache_frame_data=False)
        plt.show()
        
        # Cleanup
        if self.ser:
            self.ser.close()

def main():
    """Main entry point"""
    print("="*60)
    print("  ESP32 LoRa Sniffer - Live Visualization Tool")
    print("="*60)
    print()
    
    # Parse command line arguments
    if len(sys.argv) > 1:
        port = sys.argv[1]
    else:
        # Try to auto-detect
        import serial.tools.list_ports
        ports = list(serial.tools.list_ports.comports())
        
        if not ports:
            print("[✗] No serial ports found!")
            print("[!] Usage: python live_visualizer.py [COM_PORT]")
            sys.exit(1)
        
        print("[*] Available serial ports:")
        for i, port_info in enumerate(ports, 1):
            print(f"    {i}. {port_info.device} - {port_info.description}")
        
        # Use first port as default
        port = ports[0].device
        print(f"\n[*] Using default port: {port}")
        print(f"[!] To specify different port: python live_visualizer.py <PORT>\n")
    
    # Create and run visualizer
    try:
        visualizer = LoRaVisualizer(port)
        visualizer.run()
    except KeyboardInterrupt:
        print("\n[*] Visualization stopped by user")
    except Exception as e:
        print(f"\n[✗] Error: {e}")
        import traceback
        traceback.print_exc()

if __name__ == '__main__':
    main()
