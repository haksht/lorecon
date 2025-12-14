#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - WebSocket Monitor (Headless)

Lightweight terminal-based monitor for real-time packet events.
No GUI dependencies - perfect for SSH sessions or headless servers.

Usage:
    python ws_monitor.py
    python ws_monitor.py --host 192.168.4.1
    python ws_monitor.py --json
    python ws_monitor.py --filter meshtastic
    python ws_monitor.py --quiet  # Only show packets, no status updates

Requirements:
    pip install websocket-client
"""

import argparse
import json
import sys
import signal
from datetime import datetime

try:
    import websocket
except ImportError:
    print("Error: websocket-client library required.")
    print("Install with: pip install websocket-client")
    sys.exit(1)


# ANSI color codes
class Colors:
    RESET = '\033[0m'
    BOLD = '\033[1m'
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    GRAY = '\033[90m'


# Protocol colors
PROTOCOL_COLORS = {
    'Meshtastic': Colors.BLUE,
    'LoRaWAN': Colors.YELLOW,
    'Helium': Colors.GREEN,
    'Unknown': Colors.GRAY,
    'Beacon': Colors.MAGENTA,
}


class WebSocketMonitor:
    """Headless WebSocket monitor for ESP32 LoRa Sniffer"""
    
    def __init__(self, host='192.168.4.1', json_output=False, 
                 protocol_filter=None, quiet=False, no_color=False):
        self.host = host
        self.json_output = json_output
        self.protocol_filter = protocol_filter.lower() if protocol_filter else None
        self.quiet = quiet
        self.no_color = no_color
        self.ws = None
        self.packet_count = 0
        self.start_time = datetime.now()
        self.running = True
        
        # Handle Ctrl+C gracefully
        signal.signal(signal.SIGINT, self._signal_handler)
    
    def _signal_handler(self, signum, frame):
        """Handle Ctrl+C"""
        self.running = False
        print(f"\n{Colors.YELLOW}Shutting down...{Colors.RESET}")
        if self.ws:
            self.ws.close()
        self._print_summary()
        sys.exit(0)
    
    def _color(self, text, color):
        """Apply color if enabled"""
        if self.no_color:
            return text
        return f"{color}{text}{Colors.RESET}"
    
    def _format_timestamp(self):
        """Format current timestamp"""
        return datetime.now().strftime('%H:%M:%S')
    
    def _format_rssi(self, rssi):
        """Format RSSI with color based on signal strength"""
        if rssi > -70:
            color = Colors.GREEN
            icon = '▓▓▓'
        elif rssi > -90:
            color = Colors.YELLOW
            icon = '▓▓░'
        else:
            color = Colors.RED
            icon = '▓░░'
        
        return self._color(f"{icon} {rssi:6.1f} dBm", color)
    
    def _print_packet(self, data):
        """Print packet event"""
        if self.json_output:
            print(json.dumps(data))
            return
        
        protocol = data.get('protocol', 'Unknown')
        
        # Apply protocol filter
        if self.protocol_filter:
            if self.protocol_filter not in protocol.lower():
                return
        
        self.packet_count += 1
        
        node_id = data.get('nodeId', 'N/A')
        if node_id and node_id != 'N/A':
            node_id = f"0x{node_id}" if not node_id.startswith('0x') else node_id
        
        rssi = data.get('rssi', 0)
        snr = data.get('snr', 0)
        length = data.get('length', 0)
        
        # Get protocol color
        proto_color = PROTOCOL_COLORS.get(protocol, Colors.GRAY)
        
        timestamp = self._color(self._format_timestamp(), Colors.GRAY)
        proto_str = self._color(f"{protocol:<12}", proto_color)
        node_str = self._color(f"{node_id:<12}", Colors.CYAN)
        rssi_str = self._format_rssi(rssi)
        
        print(f"📦 {timestamp} {proto_str} {node_str} {rssi_str}  SNR:{snr:5.1f}  {length:3}B")
        
        # Show message if decrypted
        message = data.get('message')
        if message:
            print(f"   💬 {self._color(message, Colors.GREEN)}")
    
    def _print_status(self, data):
        """Print status update"""
        if self.quiet or self.json_output:
            return
        
        mode = data.get('mode', 'Unknown')
        devices = data.get('devices', 0)
        packets = data.get('totalPackets', 0)
        dropped = data.get('droppedPackets', 0)
        
        status_line = (
            f"📡 Mode: {self._color(mode, Colors.CYAN)} | "
            f"Devices: {self._color(str(devices), Colors.GREEN)} | "
            f"Packets: {packets}"
        )
        
        if dropped > 0:
            drop_pct = (dropped / (packets + dropped)) * 100 if packets else 0
            status_line += f" | {self._color(f'Dropped: {dropped} ({drop_pct:.1f}%)', Colors.RED)}"
        
        print(f"\r{status_line}", end='', flush=True)
        print()  # New line after status
    
    def _print_summary(self):
        """Print session summary"""
        if self.json_output:
            return
        
        duration = datetime.now() - self.start_time
        print(f"\n{'='*50}")
        print(f"📊 Session Summary")
        print(f"   Duration: {duration}")
        print(f"   Packets received: {self.packet_count}")
        print(f"{'='*50}")
    
    def on_message(self, ws, message):
        """Handle WebSocket message"""
        try:
            data = json.loads(message)
            msg_type = data.get('type', '')
            
            if msg_type == 'packet':
                self._print_packet(data)
            elif msg_type == 'status':
                self._print_status(data)
            elif self.json_output:
                print(json.dumps(data))
        except json.JSONDecodeError:
            if not self.quiet:
                print(f"[?] {message}")
    
    def on_error(self, ws, error):
        """Handle WebSocket error"""
        print(f"\n{self._color(f'❌ WebSocket error: {error}', Colors.RED)}")
    
    def on_close(self, ws, close_status_code, close_msg):
        """Handle WebSocket close"""
        if self.running:
            print(f"\n{self._color('⚠️  Connection closed', Colors.YELLOW)}")
    
    def on_open(self, ws):
        """Handle WebSocket open"""
        if not self.json_output:
            print(f"{self._color('✅ Connected to ESP32', Colors.GREEN)}")
            print(f"   Host: ws://{self.host}/ws")
            if self.protocol_filter:
                print(f"   Filter: {self.protocol_filter}")
            print(f"   Press Ctrl+C to stop\n")
            print(f"{'='*70}")
    
    def run(self):
        """Start monitoring"""
        ws_url = f"ws://{self.host}/ws"
        
        if not self.json_output:
            print(f"🔌 Connecting to {ws_url}...")
        
        self.ws = websocket.WebSocketApp(
            ws_url,
            on_open=self.on_open,
            on_message=self.on_message,
            on_error=self.on_error,
            on_close=self.on_close
        )
        
        try:
            self.ws.run_forever(ping_interval=30, ping_timeout=10)
        except Exception as e:
            print(f"\n{self._color(f'❌ Connection failed: {e}', Colors.RED)}")
            print(f"   Make sure you're connected to the ESP32's WiFi network")
            sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description='ESP32 LoRa Sniffer WebSocket Monitor (Headless)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                           Monitor with default settings
  %(prog)s --host 192.168.4.1       Specify ESP32 IP
  %(prog)s --json                    Output raw JSON (for piping)
  %(prog)s --filter meshtastic      Only show Meshtastic packets
  %(prog)s --quiet                   Suppress status updates
  %(prog)s --no-color                Disable colored output

Keyboard:
  Ctrl+C                             Stop monitoring and show summary
"""
    )
    
    parser.add_argument('--host', default='192.168.4.1',
                       help='ESP32 IP address (default: 192.168.4.1)')
    parser.add_argument('--json', action='store_true',
                       help='Output raw JSON (for piping to other tools)')
    parser.add_argument('--filter', metavar='PROTOCOL',
                       help='Filter by protocol (e.g., meshtastic, lorawan)')
    parser.add_argument('--quiet', '-q', action='store_true',
                       help='Only show packets, no status updates')
    parser.add_argument('--no-color', action='store_true',
                       help='Disable colored output')
    
    args = parser.parse_args()
    
    monitor = WebSocketMonitor(
        host=args.host,
        json_output=args.json,
        protocol_filter=args.filter,
        quiet=args.quiet,
        no_color=args.no_color
    )
    
    monitor.run()


if __name__ == '__main__':
    main()
