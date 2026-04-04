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
import binascii
import json
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
import signal
from datetime import datetime
from pathlib import Path

try:
    import websocket
    WEBSOCKET_AVAILABLE = True
except ImportError:
    WEBSOCKET_AVAILABLE = False

# PSK decryption (optional)
sys.path.insert(0, str(Path(__file__).parent / 'meshtastic'))
try:
    from psk_decrypt import MeshtasticDecryptor
    PSK_AVAILABLE = True
except ImportError:
    PSK_AVAILABLE = False


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
    'MeshCore': Colors.CYAN,
    'Unknown': Colors.GRAY,
    'Beacon': Colors.MAGENTA,
}


class WebSocketMonitor:
    """Headless WebSocket monitor for ESP32 LoRa Sniffer"""
    
    @staticmethod
    def _extract_text(plaintext: bytes) -> str | None:
        """
        Extract UTF-8 text from a decrypted Meshtastic TEXT_MESSAGE protobuf.

        Expected structure after decryption:
          0x08 0x01         field 1 (portnum) = 1 (TEXT_MESSAGE)
          0x12 <len> <str>  field 2 (payload bytes) = the message text
        """
        if len(plaintext) < 4:
            return None
        if plaintext[0] != 0x08 or plaintext[1] != 0x01:
            return None
        if plaintext[2] != 0x12:
            return None
        text_len = plaintext[3]
        text_bytes = plaintext[4:4 + text_len]
        if len(text_bytes) < text_len:
            return None
        try:
            return text_bytes.decode('utf-8')
        except UnicodeDecodeError:
            return text_bytes.decode('utf-8', errors='replace')

    def __init__(self, host='192.168.4.1', json_output=False,
                 protocol_filter=None, quiet=False, no_color=False,
                 decrypt=False, messages=False):
        self.host = host
        self.json_output = json_output
        self.protocol_filter = protocol_filter.lower() if protocol_filter else None
        self.quiet = quiet
        self.no_color = no_color
        self.decrypt = decrypt
        self.ws = None
        self.packet_count = 0
        self.start_time = datetime.now()
        self.running = True

        self.messages = messages
        if self.messages:
            self.decrypt = True  # --messages implies --decrypt

        if self.decrypt:
            if not PSK_AVAILABLE:
                print("[!] PSK decryption unavailable - pip install cryptography")
                self.decrypt = False
                self.messages = False
            else:
                self._decryptor = MeshtasticDecryptor()
        
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
        
        # --messages mode: attempt decrypt, skip non-text packets entirely
        if self.messages and protocol == 'Meshtastic':
            raw_hex = data.get('dataHex') or data.get('data_hex') or data.get('rawHex')
            if not raw_hex:
                return  # no raw bytes — can't decrypt, skip
            try:
                raw_bytes = binascii.unhexlify(raw_hex)
                result = self._decryptor.try_decrypt(raw_bytes)
                if result and result.portnum_name == 'TEXT_MESSAGE':
                    text = self._extract_text(result.plaintext) or ''.join(
                        chr(b) if 32 <= b < 127 else '.' for b in result.plaintext[4:]
                    )
                    key_label = self._color(f"[key#{result.key_index}]", Colors.YELLOW)
                    print(f"💬 {timestamp} {node_str} {rssi_str}  {key_label} {self._color(text, Colors.GREEN)}")
                    self.packet_count += 1
            except Exception:
                pass
            return  # always skip packet-header line in --messages mode

        self.packet_count += 1
        print(f"📦 {timestamp} {proto_str} {node_str} {rssi_str}  SNR:{snr:5.1f}  {length:3}B")

        # Show message if decrypted by firmware
        message = data.get('message')
        if message:
            print(f"   💬 {self._color(message, Colors.GREEN)}")

        # Attempt PSK decryption on raw bytes if --decrypt and no firmware message
        if self.decrypt and not message and protocol == 'Meshtastic':
            raw_hex = data.get('dataHex') or data.get('data_hex') or data.get('rawHex')
            if raw_hex:
                try:
                    raw_bytes = binascii.unhexlify(raw_hex)
                    result = self._decryptor.try_decrypt(raw_bytes)
                    if result:
                        portnum = f" [{result.portnum_name}]" if result.portnum_name else ""
                        key_label = self._color(f"key#{result.key_index} {result.key_name}", Colors.YELLOW)
                        plaintext_printable = ''.join(
                            chr(b) if 32 <= b < 127 else '.' for b in result.plaintext
                        )
                        print(f"   🔓 {key_label}{portnum}: {self._color(plaintext_printable[:120], Colors.GREEN)}")
                    else:
                        print(f"   🔒 {self._color('no default PSK matched', Colors.GRAY)}")
                except Exception:
                    pass
    
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


# Canonical demo nodes — same IDs used by topology, reveal, and visualizer demos
_DEMO_NODES = [
    ('401ACD4E', 'PRESENTER', 'Meshtastic', -58.7,  9.2),
    ('598B29CE', 'ATTENDEE',  'Meshtastic', -71.3,  7.1),
    ('B3F42A10', 'MOBILE',    'Meshtastic', -79.1,  5.3),
    ('7C891DEF', 'HIDDEN',    'Meshtastic', -88.4,  2.1),
    ('A42B8C56', 'ROUTER',    'Meshtastic', -42.1, 14.3),
    ('260B1234', 'SENSOR',    'LoRaWAN',   -91.2,  1.1),
]

_DEMO_PSK_RESULTS = ['LongFast public channel', 'none', 'none',
                     'LongFast public channel', 'Admin channel (legacy)', 'none']


def _run_demo(args):
    """Simulate a live WebSocket packet stream for offline presentations."""
    import time
    import random

    monitor = WebSocketMonitor(
        host='demo',
        json_output=args.json,
        protocol_filter=args.filter,
        quiet=args.quiet,
        no_color=args.no_color,
        decrypt=False,   # demo generates pre-formatted output
        messages=False,
    )

    if not args.json and not args.quiet:
        print(f"{Colors.GREEN}[DEMO MODE] Simulated packet stream — Ctrl+C to stop{Colors.RESET}")
        print(f"{'='*70}")

    rng = random.Random(42)
    packet_num = 0

    try:
        while True:
            idx = rng.choices(range(len(_DEMO_NODES)),
                              weights=[3, 2, 2, 1, 3, 1])[0]
            node_hex, name, protocol, base_rssi, base_snr = _DEMO_NODES[idx]
            psk_result = _DEMO_PSK_RESULTS[idx]

            rssi = base_rssi + rng.uniform(-3, 3)
            snr  = base_snr  + rng.uniform(-1, 1)
            length = rng.randint(24, 180)
            packet_num += 1

            pkt = {
                'protocol':  protocol,
                'nodeId':    node_hex,
                'rssi':      round(rssi, 1),
                'snr':       round(snr, 1),
                'length':    length,
                'encrypted': 1 if protocol == 'Meshtastic' else 0,
                'pskResult': psk_result,
                'frequency': 906.875 if protocol == 'Meshtastic' else 903.9,
                'timestamp': int(time.time() * 1000),
            }

            if args.json:
                print(json.dumps(pkt))
            else:
                monitor._print_packet(pkt)

            delay = rng.uniform(0.3, 1.8)
            time.sleep(delay)

    except KeyboardInterrupt:
        if not args.json:
            monitor._print_summary()


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
  %(prog)s --decrypt                 Attempt PSK decryption on Meshtastic packets
  %(prog)s --messages                Decrypted text messages only (quiet everything else)

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
    parser.add_argument('--decrypt', action='store_true',
                       help='Attempt PSK decryption on Meshtastic packets (requires: pip install cryptography)')
    parser.add_argument('--messages', action='store_true',
                       help='Show decrypted text messages only — implies --decrypt, suppresses all other output')
    parser.add_argument('--demo', action='store_true',
                       help='Demo mode: simulate live packet stream (no hardware needed)')

    args = parser.parse_args()

    if args.demo:
        _run_demo(args)
        return

    monitor = WebSocketMonitor(
        host=args.host,
        json_output=args.json,
        protocol_filter=args.filter,
        quiet=args.quiet,
        no_color=args.no_color,
        decrypt=args.decrypt,
        messages=args.messages,
    )

    if not WEBSOCKET_AVAILABLE:
        print("Error: websocket-client library required.")
        print("Install with: pip install websocket-client")
        sys.exit(1)

    monitor.run()


if __name__ == '__main__':
    main()
