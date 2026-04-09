#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer — Mission Control

Full-screen Rich terminal dashboard.  Replays a CSV capture chronologically
so the audience watches packets arrive, devices accumulate, and text messages
get intercepted in real time.

Usage:
    python demo_live.py                          # built-in demo data
    python demo_live.py capture.csv              # replay a capture file
    python demo_live.py capture.csv --duration 90   # finish in 90 seconds
    python demo_live.py capture.csv --speed 200     # 200× faster than real time
    python demo_live.py capture.csv --instant        # skip to final state

Live mode (with hardware):
    python demo_live.py --host 192.168.4.1       # stream from ESP32 WebSocket

Requirements:
    pip install rich cryptography websocket-client
"""

import argparse
import csv
import struct
import sys
import time
import threading
from collections import deque, defaultdict
from datetime import datetime
from io import StringIO
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent / 'meshtastic'))
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')

try:
    from rich.console import Console
    from rich.layout import Layout
    from rich.live import Live
    from rich.panel import Panel
    from rich.table import Table
    from rich.text import Text
    from rich import box
except ImportError:
    print("pip install rich")
    sys.exit(1)

try:
    from psk_auditor import PSKAuditor
    AUDITOR_OK = True
except ImportError:
    AUDITOR_OK = False

try:
    import websocket
    WS_OK = True
except ImportError:
    WS_OK = False

# Default hotel demo file (Tim's machine — ignored if not present)
_HOTEL = Path("c:/Users/tim/foedata/Hotel logs/lora_capture_1774798318443.csv")

PROTO_STYLE = {
    'Meshtastic': 'cyan',
    'MeshCore':   'magenta',
    'LoRaWAN':    'yellow',
    'Unknown':    'dim white',
}

# ─── Demo data ────────────────────────────────────────────────────────────────

_DEMO_PACKETS_CSV = """\
timestamp_ms,node_id_hex,protocol,rssi_dbm,snr_db,frequency_mhz,encrypted,psk_result,raw_hex
1700000001000,0x401ACD4E,Meshtastic,-58.7,9.2,906.875,1,LongFast Preset,
1700000002500,0x598B29CE,Meshtastic,-71.3,7.1,906.875,1,LongFast Preset,
1700000003000,0xB3F42A10,Meshtastic,-79.1,5.3,906.875,1,LongFast Preset,
1700000004000,0x401ACD4E,Meshtastic,-58.9,9.0,906.875,1,LongFast Preset,
1700000005000,0x7C891DEF,Meshtastic,-88.4,2.1,906.875,1,LongFast Preset,
1700000006000,0xA42B8C56,Meshtastic,-42.1,14.3,906.875,1,Admin Channel (pre-2.5),
1700000008000,0x260B1234,LoRaWAN,-91.2,1.1,903.900,0,none,
1700000009000,0x401ACD4E,Meshtastic,-58.0,9.4,906.875,1,LongFast Preset,
1700000010500,0x1FA3C2B1,Meshtastic,-83.5,3.2,906.875,1,LongFast Preset,
1700000012000,0xA42B8C56,Meshtastic,-41.8,14.5,906.875,1,Admin Channel (pre-2.5),
1700000013000,0x2DE91B77,Meshtastic,-76.2,5.8,906.875,1,LongFast Preset,
1700000015000,0x598B29CE,Meshtastic,-71.8,7.2,906.875,1,LongFast Preset,
1700000016000,0x7C891DEF,Meshtastic,-89.2,1.8,906.875,1,LongFast Preset,
1700000017000,0x260B1234,LoRaWAN,-90.8,1.3,903.900,0,none,
1700000018000,0xB3F42A10,Meshtastic,-78.8,5.5,906.875,1,LongFast Preset,
1700000019500,0x3CC8A290,MeshCore,-66.1,6.9,906.875,1,MeshCore public,
1700000020000,0xA42B8C56,Meshtastic,-42.5,14.1,906.875,1,Admin Channel (pre-2.5),
1700000021500,0x4B712E5D,Meshtastic,-80.9,4.1,906.875,1,LongFast Preset,
1700000022000,0x401ACD4E,Meshtastic,-59.5,8.8,906.875,1,LongFast Preset,
1700000023500,0x598B29CE,Meshtastic,-72.1,7.0,906.875,1,LongFast Preset,
1700000025000,0x598B29CE,Meshtastic,-72.5,6.7,906.875,1,LongFast Preset,
1700000026000,0x3CC8A290,MeshCore,-65.4,7.2,906.875,1,MeshCore public,
1700000028000,0x401ACD4E,Meshtastic,-58.2,9.1,906.875,1,LongFast Preset,
1700000030000,0x5F94AB22,Meshtastic,-85.0,2.9,906.875,0,none,
"""

# Injected messages for demo mode (no raw_hex = can't actually decrypt)
_DEMO_MESSAGES = [
    (1700000006000, '0xA42B8C56', 'Admin Channel (pre-2.5)', 'CRITICAL',
     'set_owner admin pw: h0tel2024'),
    (1700000009000, '0x401ACD4E', 'LongFast Preset', 'INFO',
     'anyone at the hotel bar yet?'),
    (1700000015000, '0x598B29CE', 'LongFast Preset', 'INFO',
     'room 412, heading down now'),
    (1700000022000, '0x401ACD4E', 'LongFast Preset', 'INFO',
     "don't say stuff like that on a public channel lol"),
    (1700000025000, '0x598B29CE', 'LongFast Preset', 'INFO',
     'grab my badge, leaving it at the front desk'),
]

_DEMO_ALERTS = [
    ('INFO',     'LongFast Preset: public broadcast channel — same key on every device by design'),
    ('CRITICAL', 'Admin Channel (pre-2.5) key is public — allows remote device config'),
    ('MEDIUM',   'Unencrypted LoRaWAN device 0x260B1234 — plaintext join requests'),
    ('MEDIUM',   'Device 0x5F94AB22 transmitting without encryption'),
]


# ─── Data loading ─────────────────────────────────────────────────────────────

def _parse_csv_rows(reader):
    packets = []
    for row in reader:
        try:
            ts = int(float(row.get('timestamp_ms', row.get('timestamp', 0)) or 0))
        except Exception:
            continue
        packets.append({
            'ts':        ts,
            'node':      row.get('node_id_hex', row.get('nodeId', 'Unknown')),
            'protocol':  row.get('protocol', 'Unknown'),
            'rssi':      float(row.get('rssi_dbm', row.get('rssi', -100)) or -100),
            'snr':       float(row.get('snr_db', row.get('snr', 0)) or 0),
            'freq':      float(row.get('frequency_mhz', row.get('frequency', 906.875)) or 906.875),
            'encrypted': row.get('encrypted', '0') in ('1', 'true', 'True'),
            'psk_result': row.get('psk_result', 'none') or 'none',
            'raw_hex':   row.get('raw_hex', '') or '',
        })
    packets.sort(key=lambda p: p['ts'])
    return packets


def load_csv(path):
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        return _parse_csv_rows(csv.DictReader(f))


def load_demo():
    return _parse_csv_rows(csv.DictReader(StringIO(_DEMO_PACKETS_CSV)))


def preprocess_messages(packets):
    """Run PSKAuditor over all packets; return ts_ms → (node, psk_name, risk, text)."""
    if not AUDITOR_OK:
        return {}

    auditor = PSKAuditor(verbose=False, no_color=True)
    msg_map = {}

    for pkt in packets:
        raw = pkt.get('raw_hex', '')
        if not raw:
            continue
        prev = len(auditor.text_messages)
        try:
            raw_bytes = bytes.fromhex(raw.replace(' ', ''))
            auditor.audit_packet(raw_bytes, pkt['ts'] / 1000)
        except Exception:
            continue
        if len(auditor.text_messages) > prev:
            node, ts_str, psk_name, text = auditor.text_messages[-1]
            msg_map[pkt['ts']] = (node, psk_name, _classify_psk_risk(psk_name), text)

    return msg_map


# ─── PSK risk classification ─────────────────────────────────────────────────

# Factory preset public channels — same key on every device, no privacy expectation.
_PUBLIC_CHANNEL_KEYS = ('LongFast', 'LongSlow', 'MediumFast', 'MediumSlow',
                        'ShortFast', 'ShortSlow', 'MeshCore public')

def _classify_psk_risk(psk_name):
    if 'admin' in psk_name.lower():
        return 'CRITICAL'
    if any(k in psk_name for k in _PUBLIC_CHANNEL_KEYS):
        return 'INFO'
    return 'HIGH'   # private channel using a known/weak key


def _psk_alert_text(psk_name, risk):
    if risk == 'CRITICAL':
        return f'Admin channel key in use — remote config exploitable ({psk_name})'
    if risk == 'INFO':
        return f'Public broadcast channel ({psk_name}) — readable by design'
    return f'Known PSK "{psk_name}" — private channel readable passively'


# ─── Rich rendering helpers ──────────────────────────────────────────────────

def _rssi_bar(rssi):
    if rssi > -70:
        bars, color = '▓▓▓', 'bold green'
    elif rssi > -85:
        bars, color = '▓▓░', 'yellow'
    else:
        bars, color = '▓░░', 'red'
    t = Text()
    t.append(f'{bars} ', style=color)
    t.append(f'{rssi:6.1f}', style='dim white')
    return t


def _risk_style(risk):
    return {'CRITICAL': 'bold red', 'HIGH': 'bold orange3',
            'MEDIUM': 'bold yellow', 'LOW': 'green', 'INFO': 'dim'}.get(risk, 'white')


# ─── Mission Control state machine ───────────────────────────────────────────

class MissionControl:
    MAX_FEED = 28   # rows in the packet feed table

    def __init__(self, packets, msg_map, demo_msgs=None):
        self.all_packets  = packets
        self.msg_map      = msg_map           # ts -> (node, psk_name, risk, text)
        self.demo_msgs    = {m[0]: m for m in (demo_msgs or [])}  # ts -> tuple

        self.feed         = deque(maxlen=self.MAX_FEED)
        self.devices      = {}                # node -> {packets, protocols, rssi_vals, last_seen}
        self.intercepted  = []                # (rel_ts_str, node, psk_name, risk, text)
        self.alerts       = []                # (risk, text) — deduplicated
        self._alert_seen  = set()

        self.processed    = 0
        self.total        = len(packets)
        self.start_ts     = packets[0]['ts'] if packets else 0
        self.end_ts       = packets[-1]['ts'] if packets else 1
        self.data_elapsed = 0                 # seconds of data time elapsed

        self.flash_until  = 0.0              # real time — flash messages border until then
        self.live_mode    = False

    # ── Packet processing ────────────────────────────────────────────────────

    def process(self, pkt):
        """Ingest one packet and update all state."""
        node = pkt['node']
        proto = pkt['protocol']
        ts = pkt['ts']

        # Feed row
        rel_sec = (ts - self.start_ts) / 1000
        h, m, s = int(rel_sec // 3600), int((rel_sec % 3600) // 60), int(rel_sec % 60)
        ts_str = f'{h:02d}:{m:02d}:{s:02d}'

        row_text = Text()
        row_text.append(f'{ts_str}  ', style='dim')
        row_text.append(f'{node:<14}', style='cyan')
        row_text.append(f'{proto:<12}', style=PROTO_STYLE.get(proto, 'white'))
        rssi_part = _rssi_bar(pkt['rssi'])
        snr_str = f'  SNR {pkt["snr"]:+.1f}'
        enc_mark = ' 🔒' if pkt['encrypted'] else ' ░░'
        self.feed.append((row_text, rssi_part, snr_str, enc_mark))

        # Device tracking
        if node not in self.devices:
            self.devices[node] = {'packets': 0, 'protos': set(), 'rssi': [], 'ts': ts_str}
        d = self.devices[node]
        d['packets'] += 1
        d['protos'].add(proto)
        d['rssi'].append(pkt['rssi'])
        d['ts'] = ts_str

        self.data_elapsed = rel_sec
        self.processed += 1

        # Message interception (real data)
        if ts in self.msg_map:
            node2, psk, risk, text = self.msg_map[ts]
            self.intercepted.append((ts_str, node2, psk, risk, text))
            if risk not in ('INFO', 'LOW'):
                self.flash_until = time.time() + 1.2
            self._maybe_add_alert(risk, _psk_alert_text(psk, risk))

        # Message injection (demo data)
        elif ts in self.demo_msgs:
            _, node2, psk, risk, text = self.demo_msgs[ts]
            self.intercepted.append((ts_str, node2, psk, risk, text))
            if risk not in ('INFO', 'LOW'):
                self.flash_until = time.time() + 1.2
            self._maybe_add_alert(risk, _psk_alert_text(psk, risk))

        # PSK alert from CSV field
        psk_result = pkt.get('psk_result', 'none')
        if psk_result and psk_result not in ('none', 'failed', ''):
            risk = _classify_psk_risk(psk_result)
            self._maybe_add_alert(risk, _psk_alert_text(psk_result, risk))

        # Unencrypted alert
        if not pkt['encrypted']:
            self._maybe_add_alert('MEDIUM', f'Unencrypted device: {node}')

    def ingest_ws(self, pkt):
        """Ingest a packet from WebSocket (live mode)."""
        self.total = self.processed + 1
        self.process(pkt)

    def _maybe_add_alert(self, risk, msg):
        key = (risk, msg[:50])
        if key not in self._alert_seen:
            self._alert_seen.add(key)
            self.alerts.append((risk, msg))

    # ── Layout builders ──────────────────────────────────────────────────────

    def _header(self):
        pct = self.processed / max(self.total, 1)
        bar_w = 38
        filled = int(bar_w * pct)
        bar = '█' * filled + '░' * (bar_w - filled)

        h = int(self.data_elapsed // 3600)
        m = int((self.data_elapsed % 3600) // 60)
        s = int(self.data_elapsed % 60)
        elapsed_str = f'{h:02d}:{m:02d}:{s:02d}'

        mode = 'LIVE' if self.live_mode else f'{self.processed:,}/{self.total:,} pkts'
        t = Text()
        t.append('  ESP32 LORA RECON  ', style='bold green')
        t.append('// ', style='dim')
        t.append('MISSION CONTROL', style='bold white')
        t.append(f'    [{bar}] ', style='green')
        t.append(f'{pct*100:5.1f}%  ', style='bold white')
        t.append(f'{mode}', style='dim cyan')
        t.append(f'  T+{elapsed_str}', style='dim')
        return Panel(t, style='green', box=box.HORIZONTALS, padding=(0, 1))

    def _packet_feed(self):
        tbl = Table(box=None, show_header=True, header_style='bold dim',
                    pad_edge=False, expand=True, show_edge=False)
        tbl.add_column('TIME     NODE            PROTOCOL    ', no_wrap=True, ratio=5)
        tbl.add_column('RSSI', no_wrap=True, ratio=2)
        tbl.add_column('SNR / ENC', no_wrap=True, ratio=2)

        for row_text, rssi_part, snr_str, enc_mark in self.feed:
            suffix = Text()
            suffix.append(snr_str, style='dim')
            suffix.append(enc_mark, style='dim green' if enc_mark.strip() == '🔒' else 'dim red')
            tbl.add_row(row_text, rssi_part, suffix)

        return Panel(tbl, title='[bold]PACKET FEED[/]', border_style='dim green',
                     box=box.SIMPLE_HEAD)

    def _stats(self):
        n_decrypted = len(self.intercepted)
        n_critical  = sum(1 for a in self.alerts if a[0] == 'CRITICAL')
        avg_rssi    = (sum(d['rssi'][-1] for d in self.devices.values() if d['rssi'])
                       / max(len(self.devices), 1))

        t = Text()
        t.append(f'  Devices seen    ', style='dim')
        t.append(f'{len(self.devices):>6}\n', style='bold white')
        t.append(f'  Packets         ', style='dim')
        t.append(f'{self.processed:>6,}\n', style='bold white')
        t.append(f'  Msgs intercepted', style='dim')
        t.append(f'{n_decrypted:>6}\n', style='bold red' if n_decrypted else 'bold white')
        t.append(f'  Security alerts ', style='dim')
        t.append(f'{len(self.alerts):>6}\n', style='bold orange3' if self.alerts else 'bold white')
        t.append(f'  CRITICAL        ', style='dim')
        t.append(f'{n_critical:>6}\n', style='bold red' if n_critical else 'bold white')
        t.append(f'  Avg RSSI        ', style='dim')
        t.append(f'{avg_rssi:>5.1f} dBm\n', style='dim white')

        return Panel(t, title='[bold]STATISTICS[/]', border_style='dim cyan',
                     box=box.SIMPLE_HEAD)

    def _messages(self):
        flash = time.time() < self.flash_until
        border = 'bold red' if flash else 'dim white'
        title  = '[bold red blink] *** EXPLOIT TRAFFIC *** [/]' if flash else '[bold]READABLE TRAFFIC[/]'

        t = Text()
        visible = self.intercepted[-6:]
        if not visible:
            t.append('  (none yet)', style='dim')
        else:
            for ts_str, node, psk, risk, text in visible:
                t.append(f'\n  {ts_str} ', style='dim')
                t.append(f'{node}', style='cyan')
                label = 'PUBLIC' if risk == 'INFO' else risk
                t.append(f'  [{label}: {psk}]\n', style=_risk_style(risk))
                # Wrap text at 42 chars
                wrapped = [text[i:i+42] for i in range(0, len(text), 42)]
                for line in wrapped:
                    t.append(f'  "{line}"\n', style='bold white')

        return Panel(t, title=title, border_style=border, box=box.SIMPLE_HEAD)

    def _alerts(self):
        t = Text()
        for risk, msg in self.alerts[-6:]:
            marker = {'CRITICAL': '■', 'HIGH': '▲', 'MEDIUM': '◆', 'LOW': '●'}.get(risk, '·')
            t.append(f'  {marker} ', style=_risk_style(risk))
            t.append(f'{msg}\n', style='dim white')
        if not self.alerts:
            t.append('  (scanning...)', style='dim')
        return Panel(t, title='[bold]SECURITY ALERTS[/]', border_style='dim red',
                     box=box.SIMPLE_HEAD)

    def make_layout(self):
        layout = Layout()
        layout.split_column(
            Layout(name='header', size=3),
            Layout(name='body'),
        )
        layout['body'].split_row(
            Layout(name='feed', ratio=3),
            Layout(name='sidebar', ratio=2),
        )
        layout['sidebar'].split_column(
            Layout(name='stats', size=10),
            Layout(name='messages', ratio=1),
            Layout(name='alerts', size=9),
        )
        layout['header'].update(self._header())
        layout['feed'].update(self._packet_feed())
        layout['stats'].update(self._stats())
        layout['messages'].update(self._messages())
        layout['alerts'].update(self._alerts())
        return layout

    # ── Run loops ────────────────────────────────────────────────────────────

    def run_replay(self, duration_sec, instant=False):
        """Replay mode: process packets at a speed timed to finish in duration_sec."""
        if not self.all_packets:
            print("No packets to replay.")
            return

        data_span = max((self.end_ts - self.start_ts) / 1000, 1)
        speed = data_span / max(duration_sec, 1)   # data-seconds per real-second

        console = Console()
        with Live(self.make_layout(), refresh_per_second=10, screen=True,
                  console=console) as live:
            if instant:
                for pkt in self.all_packets:
                    self.process(pkt)
                live.update(self.make_layout())
                time.sleep(5)
                return

            start_real = time.time()
            idx = 0
            while idx < len(self.all_packets):
                now_real = time.time() - start_real
                # Drain all packets that should have arrived by now
                while idx < len(self.all_packets):
                    pkt = self.all_packets[idx]
                    data_time = (pkt['ts'] - self.start_ts) / 1000
                    if data_time / speed > now_real + 0.08:
                        break
                    self.process(pkt)
                    idx += 1
                live.update(self.make_layout())
                time.sleep(0.07)

            # Hold final state
            time.sleep(6)

    def run_live(self, host):
        """Live mode: stream from ESP32 WebSocket and display in real time."""
        if not WS_OK:
            print("pip install websocket-client")
            return

        import json as _json
        self.live_mode = True
        self.total = 0

        console = Console()

        def on_message(ws, message):
            try:
                data = _json.loads(message)
                if data.get('type') != 'packet':
                    return
                node = data.get('nodeId', 'Unknown')
                if node and not node.startswith('0x'):
                    node = f'0x{node}'
                raw_hex = data.get('dataHex') or data.get('rawHex') or data.get('data_hex') or ''
                pkt = {
                    'ts':        int(time.time() * 1000),
                    'node':      node,
                    'protocol':  data.get('protocol', 'Unknown'),
                    'rssi':      float(data.get('rssi', -100)),
                    'snr':       float(data.get('snr', 0)),
                    'freq':      float(data.get('frequency', 906.875)),
                    'encrypted': bool(data.get('encrypted', False)),
                    'psk_result': data.get('psk_result', 'none') or 'none',
                    'raw_hex':   raw_hex,
                }
                self.ingest_ws(pkt)
            except Exception:
                pass

        def on_error(ws, error):
            pass

        def on_close(ws, *a):
            pass

        ws = websocket.WebSocketApp(
            f'ws://{host}/ws',
            on_message=on_message,
            on_error=on_error,
            on_close=on_close,
        )
        t = threading.Thread(target=ws.run_forever, daemon=True)
        t.start()

        with Live(self.make_layout(), refresh_per_second=8, screen=True,
                  console=console) as live:
            try:
                while True:
                    live.update(self.make_layout())
                    time.sleep(0.1)
            except KeyboardInterrupt:
                pass
        ws.close()


# ─── Entry point ──────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(
        description='ESP32 LoRa Sniffer — Mission Control Dashboard',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python demo_live.py                        # built-in demo data
  python demo_live.py capture.csv            # replay capture (default 120s)
  python demo_live.py capture.csv --duration 60   # replay in 60 seconds
  python demo_live.py capture.csv --instant       # jump to final state
  python demo_live.py --host 192.168.4.1     # live from hardware
""")
    ap.add_argument('file', nargs='?', help='CSV capture file to replay')
    ap.add_argument('--host',     metavar='IP', help='ESP32 IP for live mode')
    ap.add_argument('--duration', type=int, default=60,
                    help='Playback duration in seconds (default 60)')
    ap.add_argument('--speed',    type=float, default=None,
                    help='Speed multiplier — overrides --duration')
    ap.add_argument('--instant',  action='store_true',
                    help='Process all packets instantly, then hold final state')
    args = ap.parse_args()

    # ── Live mode ────────────────────────────────────────────────────────────
    if args.host:
        mc = MissionControl([], {})
        mc.run_live(args.host)
        return 0

    # ── File selection ───────────────────────────────────────────────────────
    csv_path = None
    if args.file:
        csv_path = Path(args.file)
        if not csv_path.exists():
            print(f"File not found: {csv_path}")
            return 1
    elif _HOTEL.exists():
        csv_path = _HOTEL
        print(f"[auto] Using hotel capture: {csv_path.name}", file=sys.stderr)

    # ── Load data ────────────────────────────────────────────────────────────
    demo_msgs = None
    if csv_path:
        packets = load_csv(csv_path)
        print(f"Loading {len(packets):,} packets…", file=sys.stderr)
        msg_map = preprocess_messages(packets)
        print(f"Pre-processed: {len(msg_map)} text messages found.", file=sys.stderr)
    else:
        packets  = load_demo()
        msg_map  = {}
        demo_msgs = _DEMO_MESSAGES
        print("Using built-in demo data (no CSV specified).", file=sys.stderr)

    if not packets:
        print("No packets loaded.")
        return 1

    # ── Speed calculation ────────────────────────────────────────────────────
    if args.speed:
        data_span = (packets[-1]['ts'] - packets[0]['ts']) / 1000
        duration = max(int(data_span / args.speed), 10)
    else:
        duration = args.duration

    mc = MissionControl(packets, msg_map, demo_msgs=demo_msgs)
    mc.run_replay(duration, instant=args.instant)
    return 0


if __name__ == '__main__':
    sys.exit(main())
