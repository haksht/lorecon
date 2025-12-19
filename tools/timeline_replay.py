#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - Timeline Replay (DVR Mode)

Replays captured sessions at configurable speeds for conference demos.
Perfect for showing "compressed" reconnaissance without waiting.

Features:
- Play/Pause/Speed controls (1x, 2x, 5x, 10x, 50x)
- Terminal UI with progress bar and live stats
- Optional WebSocket broadcast for visualization sync
- Event markers for interesting moments (PSK cracks, new devices)
- Auto-pause on significant events

Usage:
    python timeline_replay.py capture.csv
    python timeline_replay.py capture.csv --speed 10 --broadcast 192.168.4.1
    python timeline_replay.py capture.csv --interactive

Requirements:
    pip install rich websocket-client
"""

import argparse
import csv
import json
import sys
import time
import threading
from collections import defaultdict
from datetime import datetime, timedelta
from pathlib import Path

# Try rich for beautiful terminal UI
try:
    from rich.console import Console
    from rich.live import Live
    from rich.table import Table
    from rich.panel import Panel
    from rich.progress import Progress, SpinnerColumn, BarColumn, TextColumn, TimeElapsedColumn
    from rich.layout import Layout
    from rich.text import Text
    from rich import box
    RICH_AVAILABLE = True
except ImportError:
    RICH_AVAILABLE = False
    print("[!] Install 'rich' for beautiful terminal UI: pip install rich")

try:
    import websocket
    WS_AVAILABLE = True
except ImportError:
    WS_AVAILABLE = False


class PacketEvent:
    """Represents a single packet event from capture"""
    
    def __init__(self, row, base_timestamp=0):
        self.timestamp_ms = int(row.get('timestamp_ms', row.get('timestamp', 0)) or 0)
        self.relative_ms = self.timestamp_ms - base_timestamp
        self.node_id = row.get('node_id_hex') or row.get('nodeId') or 'Unknown'
        self.protocol = row.get('protocol', 'Unknown')
        self.rssi = float(row.get('rssi_dbm', row.get('rssi', -100)) or -100)
        self.snr = float(row.get('snr_db', row.get('snr', 0)) or 0)
        self.length = int(row.get('length_bytes', row.get('length', 0)) or 0)
        self.frequency = float(row.get('frequency_mhz', row.get('frequency', 0)) or 0)
        self.encrypted = row.get('encrypted', '0') in ('1', 'true', 'True')
        self.psk_result = row.get('psk_result', 'none') or 'none'
        self.lat = row.get('lat_deg')
        self.lon = row.get('lon_deg')
        
        # Determine event significance
        self.is_significant = False
        self.significance_reason = None
        
        if self.psk_result not in ('none', 'failed', ''):
            self.is_significant = True
            self.significance_reason = f"🔓 PSK Cracked: {self.psk_result}"
        
        if self.lat and self.lon:
            self.is_significant = True
            self.significance_reason = "📍 GPS Position"
    
    def to_dict(self):
        return {
            'timestamp_ms': self.timestamp_ms,
            'nodeId': self.node_id,
            'protocol': self.protocol,
            'rssi': self.rssi,
            'snr': self.snr,
            'length': self.length,
            'frequency': self.frequency,
            'encrypted': self.encrypted,
            'psk_result': self.psk_result,
            'type': 'packet'
        }


class TimelineReplay:
    """DVR-style replay of captured sessions"""
    
    def __init__(self, csv_path, speed=1.0, broadcast_host=None, 
                 auto_pause=True, interactive=False):
        self.csv_path = Path(csv_path)
        self.speed = speed
        self.broadcast_host = broadcast_host
        self.auto_pause = auto_pause
        self.interactive = interactive
        
        self.events = []
        self.current_idx = 0
        self.playing = False
        self.paused = False
        
        # Stats
        self.devices_seen = set()
        self.protocol_counts = defaultdict(int)
        self.psk_cracks = 0
        self.gps_positions = 0
        self.total_packets = 0
        
        # Timing
        self.playback_start = None
        self.real_elapsed = 0
        self.capture_duration_ms = 0
        
        # WebSocket for broadcast
        self.ws = None
        
        # Rich console
        self.console = Console() if RICH_AVAILABLE else None
        
        # Load events
        self._load_events()
    
    def _load_events(self):
        """Load and parse CSV file"""
        if not self.csv_path.exists():
            print(f"Error: File not found: {self.csv_path}")
            sys.exit(1)
        
        with open(self.csv_path, 'r') as f:
            reader = csv.DictReader(f)
            rows = list(reader)
        
        if not rows:
            print("Error: Empty CSV file")
            sys.exit(1)
        
        # Find base timestamp
        base_ts = int(rows[0].get('timestamp_ms', rows[0].get('timestamp', 0)) or 0)
        
        for row in rows:
            event = PacketEvent(row, base_ts)
            self.events.append(event)
        
        # Sort by timestamp
        self.events.sort(key=lambda e: e.relative_ms)
        
        # Calculate duration
        if self.events:
            self.capture_duration_ms = self.events[-1].relative_ms
        
        print(f"✓ Loaded {len(self.events)} packets from {self.csv_path.name}")
        print(f"  Capture duration: {self._format_duration(self.capture_duration_ms)}")
    
    def _format_duration(self, ms):
        """Format milliseconds as human-readable duration"""
        seconds = ms / 1000
        if seconds < 60:
            return f"{seconds:.1f}s"
        elif seconds < 3600:
            return f"{int(seconds // 60)}m {int(seconds % 60)}s"
        else:
            hours = int(seconds // 3600)
            mins = int((seconds % 3600) // 60)
            return f"{hours}h {mins}m"
    
    def _connect_broadcast(self):
        """Connect to ESP32 WebSocket for broadcast"""
        if not self.broadcast_host or not WS_AVAILABLE:
            return
        
        try:
            self.ws = websocket.create_connection(
                f"ws://{self.broadcast_host}/ws",
                timeout=5
            )
            print(f"✓ Broadcasting to {self.broadcast_host}")
        except Exception as e:
            print(f"⚠ Could not connect to {self.broadcast_host}: {e}")
            self.ws = None
    
    def _broadcast_event(self, event):
        """Send event to connected WebSocket"""
        if self.ws:
            try:
                self.ws.send(json.dumps(event.to_dict()))
            except:
                self.ws = None
    
    def _update_stats(self, event):
        """Update statistics from event"""
        self.total_packets += 1
        self.devices_seen.add(event.node_id)
        self.protocol_counts[event.protocol] += 1
        
        if event.psk_result not in ('none', 'failed', ''):
            self.psk_cracks += 1
        
        if event.lat and event.lon:
            self.gps_positions += 1
    
    def run_simple(self):
        """Simple text-mode playback (no rich)"""
        print("\n" + "="*60)
        print("🎬 TIMELINE REPLAY - DVR MODE")
        print("="*60)
        print(f"Speed: {self.speed}x | Duration: {self._format_duration(self.capture_duration_ms)}")
        print("="*60 + "\n")
        
        self._connect_broadcast()
        self.playing = True
        self.playback_start = time.time()
        
        last_print_time = 0
        
        for idx, event in enumerate(self.events):
            if not self.playing:
                break
            
            # Calculate sleep time
            if idx > 0:
                delta_ms = event.relative_ms - self.events[idx - 1].relative_ms
                sleep_time = (delta_ms / 1000) / self.speed
                if sleep_time > 0:
                    time.sleep(sleep_time)
            
            self._update_stats(event)
            self._broadcast_event(event)
            
            # Print significant events immediately
            if event.is_significant:
                print(f"\n{'='*40}")
                print(f"⚡ {event.significance_reason}")
                print(f"   Device: 0x{event.node_id} | Protocol: {event.protocol}")
                print(f"{'='*40}\n")
            
            # Progress update every second
            current_time = time.time()
            if current_time - last_print_time >= 1.0:
                progress = (idx + 1) / len(self.events) * 100
                elapsed = self._format_duration(event.relative_ms)
                print(f"\r[{progress:5.1f}%] {elapsed} | "
                      f"Pkts: {self.total_packets} | "
                      f"Devices: {len(self.devices_seen)} | "
                      f"PSK: {self.psk_cracks}", end='', flush=True)
                last_print_time = current_time
        
        print("\n\n" + "="*60)
        print("✓ REPLAY COMPLETE")
        print("="*60)
        self._print_summary()
    
    def run_rich(self):
        """Rich terminal UI playback"""
        self._connect_broadcast()
        self.playing = True
        self.playback_start = time.time()
        
        def make_layout():
            """Create the display layout"""
            layout = Layout()
            layout.split_column(
                Layout(name="header", size=3),
                Layout(name="main", ratio=1),
                Layout(name="footer", size=3)
            )
            layout["main"].split_row(
                Layout(name="stats", ratio=1),
                Layout(name="events", ratio=2)
            )
            return layout
        
        def generate_content(current_event, idx):
            """Generate display content"""
            layout = make_layout()
            
            # Header
            progress = (idx + 1) / len(self.events) * 100
            capture_time = self._format_duration(current_event.relative_ms if current_event else 0)
            total_time = self._format_duration(self.capture_duration_ms)
            
            header = Text()
            header.append("🎬 TIMELINE REPLAY ", style="bold magenta")
            header.append(f"[{self.speed}x] ", style="cyan")
            header.append(f"│ {capture_time} / {total_time} ", style="green")
            header.append(f"│ {progress:.1f}%", style="yellow")
            layout["header"].update(Panel(header, box=box.ROUNDED))
            
            # Stats panel
            stats_table = Table(show_header=False, box=box.SIMPLE, padding=(0, 1))
            stats_table.add_column("Label", style="dim")
            stats_table.add_column("Value", style="bold")
            
            stats_table.add_row("📦 Packets", str(self.total_packets))
            stats_table.add_row("📡 Devices", str(len(self.devices_seen)))
            stats_table.add_row("🔓 PSK Cracks", f"[red]{self.psk_cracks}[/red]" if self.psk_cracks else "0")
            stats_table.add_row("📍 GPS Points", str(self.gps_positions))
            stats_table.add_row("", "")
            
            # Protocol breakdown
            for proto, count in sorted(self.protocol_counts.items(), key=lambda x: -x[1])[:5]:
                color = "blue" if proto == "Meshtastic" else "yellow" if proto == "LoRaWAN" else "white"
                stats_table.add_row(f"  {proto}", f"[{color}]{count}[/{color}]")
            
            layout["stats"].update(Panel(stats_table, title="Statistics", border_style="blue"))
            
            # Events panel - show last 10 events
            events_table = Table(box=box.SIMPLE, padding=(0, 1))
            events_table.add_column("Time", style="dim", width=8)
            events_table.add_column("Device", style="cyan", width=12)
            events_table.add_column("Protocol", width=12)
            events_table.add_column("RSSI", justify="right", width=8)
            events_table.add_column("Event", width=20)
            
            start_idx = max(0, idx - 9)
            for i in range(start_idx, idx + 1):
                if i >= len(self.events):
                    break
                ev = self.events[i]
                t = self._format_duration(ev.relative_ms)
                node = f"..{ev.node_id[-4:]}" if len(ev.node_id) > 4 else ev.node_id
                
                event_text = ""
                event_style = ""
                if ev.psk_result not in ('none', 'failed', ''):
                    event_text = f"🔓 {ev.psk_result[:15]}"
                    event_style = "bold red"
                elif ev.lat and ev.lon:
                    event_text = "📍 GPS"
                    event_style = "green"
                
                rssi_color = "green" if ev.rssi > -70 else "yellow" if ev.rssi > -90 else "red"
                
                events_table.add_row(
                    t,
                    f"0x{node}",
                    ev.protocol,
                    f"[{rssi_color}]{ev.rssi:.0f}[/{rssi_color}]",
                    f"[{event_style}]{event_text}[/{event_style}]" if event_text else ""
                )
            
            layout["events"].update(Panel(events_table, title="Event Stream", border_style="green"))
            
            # Footer - controls
            footer = Text()
            footer.append("Controls: ", style="dim")
            footer.append("[Q]uit  ", style="bold red")
            footer.append("[Space]Pause  ", style="bold yellow")
            footer.append("[+/-]Speed  ", style="bold cyan")
            if self.paused:
                footer.append("  ⏸️  PAUSED", style="bold yellow")
            layout["footer"].update(Panel(footer, box=box.ROUNDED))
            
            return layout
        
        # Main playback loop with rich Live display
        with Live(generate_content(None, 0), refresh_per_second=10, console=self.console) as live:
            idx = 0
            while idx < len(self.events) and self.playing:
                event = self.events[idx]
                
                # Calculate sleep time
                if idx > 0:
                    delta_ms = event.relative_ms - self.events[idx - 1].relative_ms
                    sleep_time = (delta_ms / 1000) / self.speed
                    
                    # Sleep in small increments for responsiveness
                    sleep_start = time.time()
                    while time.time() - sleep_start < sleep_time and self.playing:
                        if self.paused:
                            time.sleep(0.1)
                            continue
                        time.sleep(min(0.05, sleep_time))
                
                if self.paused:
                    live.update(generate_content(event, idx))
                    time.sleep(0.1)
                    continue
                
                self._update_stats(event)
                self._broadcast_event(event)
                
                # Auto-pause on significant events
                if self.auto_pause and event.is_significant:
                    self.paused = True
                
                live.update(generate_content(event, idx))
                idx += 1
        
        # Summary
        self.console.print("\n")
        self._print_summary_rich()
    
    def _print_summary(self):
        """Print final summary (simple mode)"""
        print(f"\nTotal Packets: {self.total_packets}")
        print(f"Unique Devices: {len(self.devices_seen)}")
        print(f"PSK Cracks: {self.psk_cracks}")
        print(f"GPS Positions: {self.gps_positions}")
        print("\nProtocol Breakdown:")
        for proto, count in sorted(self.protocol_counts.items(), key=lambda x: -x[1]):
            print(f"  {proto}: {count}")
    
    def _print_summary_rich(self):
        """Print final summary (rich mode)"""
        table = Table(title="📊 Replay Summary", box=box.ROUNDED)
        table.add_column("Metric", style="cyan")
        table.add_column("Value", style="bold")
        
        table.add_row("Total Packets", str(self.total_packets))
        table.add_row("Unique Devices", str(len(self.devices_seen)))
        table.add_row("PSK Cracks", f"[red]{self.psk_cracks}[/red]")
        table.add_row("GPS Positions", str(self.gps_positions))
        table.add_row("Capture Duration", self._format_duration(self.capture_duration_ms))
        
        self.console.print(table)
        
        if self.protocol_counts:
            proto_table = Table(title="Protocol Breakdown", box=box.SIMPLE)
            proto_table.add_column("Protocol")
            proto_table.add_column("Count", justify="right")
            proto_table.add_column("Percentage", justify="right")
            
            for proto, count in sorted(self.protocol_counts.items(), key=lambda x: -x[1]):
                pct = count / self.total_packets * 100
                proto_table.add_row(proto, str(count), f"{pct:.1f}%")
            
            self.console.print(proto_table)
    
    def run(self):
        """Run playback with best available UI"""
        if RICH_AVAILABLE and self.interactive:
            self.run_rich()
        else:
            self.run_simple()


def main():
    parser = argparse.ArgumentParser(
        description='Timeline Replay - DVR mode for LoRa captures',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python timeline_replay.py capture.csv                    # Simple playback at 1x
  python timeline_replay.py capture.csv --speed 10        # 10x speed
  python timeline_replay.py capture.csv -i                # Interactive mode (rich UI)
  python timeline_replay.py capture.csv --broadcast 192.168.4.1  # Sync with visualizer
        """
    )
    
    parser.add_argument('csv_file', help='CSV capture file to replay')
    parser.add_argument('--speed', '-s', type=float, default=1.0,
                        help='Playback speed multiplier (default: 1.0)')
    parser.add_argument('--broadcast', '-b', metavar='HOST',
                        help='Broadcast events to WebSocket (e.g., 192.168.4.1)')
    parser.add_argument('--no-auto-pause', action='store_true',
                        help='Disable auto-pause on significant events')
    parser.add_argument('--interactive', '-i', action='store_true',
                        help='Use interactive rich terminal UI')
    
    args = parser.parse_args()
    
    replay = TimelineReplay(
        csv_path=args.csv_file,
        speed=args.speed,
        broadcast_host=args.broadcast,
        auto_pause=not args.no_auto_pause,
        interactive=args.interactive
    )
    
    try:
        replay.run()
    except KeyboardInterrupt:
        print("\n\nPlayback interrupted.")
        replay._print_summary()


if __name__ == '__main__':
    main()
