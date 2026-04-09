#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer — Message Reveal Theater

Presents decrypted text messages one at a time with maximum drama:
  hex ciphertext → PSK database scan → plaintext reveal.

Designed as a conference set piece.  Each message fills the screen.
Press ENTER to advance, or use --auto for timed progression.

Usage:
    python message_theater.py                         # hotel CSV if present, else demo
    python message_theater.py capture.csv             # from a capture file
    python message_theater.py capture.csv --auto 6    # auto-advance every 6 s
    python message_theater.py capture.csv --all       # include non-text packets too

Requirements:
    pip install rich cryptography
"""

import argparse
import csv
import random
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent / 'meshtastic'))
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')

try:
    from rich.console import Console
    from rich.panel import Panel
    from rich.text import Text
    from rich.align import Align
    from rich.rule import Rule
    from rich import box
except ImportError:
    print("pip install rich")
    sys.exit(1)

try:
    from psk_auditor import PSKAuditor
    AUDITOR_OK = True
except ImportError:
    AUDITOR_OK = False

_HOTEL = Path("c:/Users/tim/foedata/Hotel logs/lora_capture_1774798318443.csv")

# ─── Demo messages (no CSV needed) ───────────────────────────────────────────

_DEMO = [
    {
        'node':    '0xA42B8C56',
        'ts':      '00:04:12',
        'psk':     'Admin Channel (pre-2.5)',
        'risk':    'CRITICAL',
        'rssi':    -42.1,
        'freq':    906.875,
        'raw_hex': 'a3 c2 f0 11 56 8c 2b a4 77 3e 19 d0 01 00 00 00 b1 4c 88 21 f3 99 0a 17',
        'text':    'set_owner name: admin pw: h0tel2024',
    },
    {
        'node':    '0x401ACD4E',
        'ts':      '00:07:31',
        'psk':     'LongFast Preset',
        'risk':    'INFO',
        'rssi':    -58.7,
        'freq':    906.875,
        'raw_hex': '08 01 12 1b 61 6e 79 6f 6e 65 20 61 74 20 74 68 65 20 68 6f 74 65 6c 20 62 61 72 20 79 65 74 3f',
        'text':    'anyone at the hotel bar yet?',
    },
    {
        'node':    '0x598B29CE',
        'ts':      '00:13:44',
        'psk':     'LongFast Preset',
        'risk':    'INFO',
        'rssi':    -71.3,
        'freq':    906.875,
        'raw_hex': '08 01 12 1a 72 6f 6f 6d 20 34 31 32 2c 20 68 65 61 64 69 6e 67 20 64 6f 77 6e 20 6e 6f 77',
        'text':    'room 412, heading down now',
    },
    {
        'node':    '0x401ACD4E',
        'ts':      '00:20:09',
        'psk':     'LongFast Preset',
        'risk':    'INFO',
        'rssi':    -59.1,
        'freq':    906.875,
        'raw_hex': '08 01 12 2b 64 6f 6e 27 74 20 73 61 79 20 73 74 75 66 66 20 6c 69 6b 65 20 74 68 61 74 20 6f 6e 20 61 20 70 75 62 6c 69 63 20 63 68 61 6e 6e 65 6c',
        'text':    "don't say stuff like that on a public channel",
    },
    {
        'node':    '0x598B29CE',
        'ts':      '00:24:52',
        'psk':     'LongFast Preset',
        'risk':    'INFO',
        'rssi':    -72.5,
        'freq':    906.875,
        'raw_hex': '08 01 12 28 67 72 61 62 20 6d 79 20 62 61 64 67 65 2c 20 6c 65 61 76 69 6e 67 20 69 74 20 61 74 20 74 68 65 20 66 72 6f 6e 74 20 64 65 73 6b',
        'text':    'grab my badge, leaving it at the front desk',
    },
]


# ─── Load messages from CSV ───────────────────────────────────────────────────

def load_messages(csv_path):
    """Run PSKAuditor over CSV and return messages as theater dicts."""
    if not AUDITOR_OK:
        return None

    auditor  = PSKAuditor(verbose=False, no_color=True)
    rows_by_ts = {}

    with open(csv_path, 'r', encoding='utf-8', errors='replace') as f:
        for row in csv.DictReader(f):
            try:
                ts = int(float(row.get('timestamp_ms', 0) or 0))
            except Exception:
                continue
            rows_by_ts[ts] = row

    messages = []
    for ts in sorted(rows_by_ts):
        row = rows_by_ts[ts]
        raw = row.get('raw_hex', '') or ''
        if not raw:
            continue
        prev = len(auditor.text_messages)
        try:
            raw_bytes = bytes.fromhex(raw.replace(' ', ''))
            auditor.audit_packet(raw_bytes, ts / 1000)
        except Exception:
            continue
        if len(auditor.text_messages) > prev:
            node, ts_str, psk_name, text = auditor.text_messages[-1]
            risk = _classify_psk_risk(psk_name)
            # Reconstruct relative timestamp
            messages.append({
                'node':    node,
                'ts':      ts_str,
                'psk':     psk_name,
                'risk':    risk,
                'rssi':    float(row.get('rssi_dbm', -100) or -100),
                'freq':    float(row.get('frequency_mhz', 906.875) or 906.875),
                'raw_hex': raw[:72] + ('…' if len(raw) > 72 else ''),
                'text':    text,
            })

    return messages


# ─── Theater presentation ─────────────────────────────────────────────────────

_RISK_COLOR = {
    'CRITICAL': 'bold red',
    'HIGH':     'bold orange3',
    'MEDIUM':   'bold yellow',
    'INFO':     'dim cyan',
}

_PUBLIC_CHANNEL_KEYS = ('LongFast', 'LongSlow', 'MediumFast', 'MediumSlow',
                        'ShortFast', 'ShortSlow', 'MeshCore public')

def _classify_psk_risk(psk_name):
    if 'admin' in psk_name.lower():
        return 'CRITICAL'
    if any(k in psk_name for k in _PUBLIC_CHANNEL_KEYS):
        return 'INFO'
    return 'HIGH'

_HEX_CHARS = '0123456789abcdef'

def _glitch_hex(length=48):
    """Return a string of plausible-looking hex bytes."""
    pairs = [f'{random.choice(_HEX_CHARS)}{random.choice(_HEX_CHARS)}' for _ in range(length // 3)]
    return ' '.join(pairs)


def _typewriter(console, text, style='bold white', delay=0.03):
    """Print text character by character."""
    for ch in text:
        console.print(ch, end='', style=style, highlight=False)
        time.sleep(delay)
    console.print()


def present_message(console, msg, idx, total, auto_delay=None):
    risk       = msg['risk']
    risk_color = _RISK_COLOR.get(risk, 'white')

    # ── Screen clear ─────────────────────────────────────────────────────────
    console.clear()

    # ── Header ───────────────────────────────────────────────────────────────
    console.print()
    console.rule(f'[bold green]  ESP32 LORA RECON  //  MESSAGE THEATER  [{idx}/{total}]  [/]',
                 style='green')
    console.print()

    # ── Packet metadata ──────────────────────────────────────────────────────
    meta = Text()
    meta.append('  Node:      ', style='dim')
    meta.append(f'{msg["node"]}\n', style='bold cyan')
    meta.append('  Timestamp: ', style='dim')
    meta.append(f'{msg["ts"]}\n', style='white')
    meta.append('  RSSI:      ', style='dim')
    rssi = msg['rssi']
    rssi_color = 'green' if rssi > -70 else ('yellow' if rssi > -85 else 'red')
    meta.append(f'{rssi:.1f} dBm\n', style=rssi_color)
    meta.append('  Frequency: ', style='dim')
    meta.append(f'{msg["freq"]:.3f} MHz\n', style='dim white')
    console.print(Panel(meta, title='[dim]PACKET METADATA[/]', border_style='dim',
                        box=box.SIMPLE_HEAD, expand=False, padding=(0, 2)))
    console.print()

    # ── Captured payload ─────────────────────────────────────────────────────
    console.print('  [dim]Captured over-the-air payload:[/]')
    console.print()

    raw_display = msg.get('raw_hex', _glitch_hex())
    hex_lines = [raw_display[i:i+48] for i in range(0, len(raw_display), 49)]
    for line in hex_lines[:3]:
        console.print(f'    {line}', style='dim green')
    console.print()
    time.sleep(0.5)

    psk_name = msg['psk']

    if risk == 'INFO':
        # ── Public channel path ───────────────────────────────────────────────
        # No "cracking" happened — the channel key is factory default and public.
        console.print('  [dim]Channel identification:[/]')
        console.print()
        console.print(f'    [cyan]{psk_name}[/]')
        time.sleep(0.4)
        console.print()
        console.print('  [dim]This is a public broadcast channel.[/]')
        console.print('  [dim]The same 128-bit key ships on every Meshtastic device.[/]')
        console.print('  [dim]No encryption was broken. No private key was required.[/]')
        time.sleep(0.8)
        console.print()

        banner = Text()
        banner.append('  PUBLIC BROADCAST  ', style='bold white on blue')
        banner.append(f'  {psk_name}', style='dim')
        console.print(banner)
        console.print()
        console.print('  [dim]Message content:[/]')
        console.print()

    else:
        # ── Exploit path (CRITICAL / HIGH) ────────────────────────────────────
        # A private channel key that happens to be publicly known was in use.
        console.print('  [dim]Scanning PSK database for known keys...[/]')
        console.print()

        decoys = [
            'Default (0x01)         — no match',
            'All Zeros              — no match',
            'Legacy 0x02            — no match',
        ]
        if 'LongFast' not in psk_name:
            decoys.append('LongFast Preset        — no match')

        for d in decoys:
            console.print(f'    [dim]{d}[/]')
            time.sleep(0.18)

        time.sleep(0.3)
        console.print(f'    [bold yellow]{psk_name:<23} — [/][bold red]MATCH[/]')
        time.sleep(0.6)
        console.print()

        if risk == 'CRITICAL':
            console.print('  [bold red]This key is published in Meshtastic firmware source.[/]')
            console.print('  [bold red]It allows remote configuration of any device using it.[/]')
        else:
            console.print('  [dim]Known key. Private channel — but readable by anyone with the database.[/]')
        time.sleep(0.5)
        console.print()

        banner = Text()
        bg = 'red' if risk == 'CRITICAL' else 'dark_orange'
        banner.append(f'  {risk}  ', style=f'bold white on {bg}')
        banner.append(f'  known key: {psk_name}', style='dim')
        console.print(banner)
        console.print()
        console.print('  [dim]Decrypted message content:[/]')
        console.print()

    # ── Typewriter reveal ─────────────────────────────────────────────────────
    for ch in msg['text']:
        console.print(ch, end='', style='bold white', highlight=False)
        time.sleep(max(0.01, 0.05 - len(msg['text']) * 0.0005))
    console.print('\n')

    console.print(Panel(
        Align(Text(f'"{msg["text"]}"', style='bold white'), align='center'),
        border_style=risk_color, box=box.DOUBLE,
        padding=(1, 4),
    ))
    console.print()

    # ── Advance prompt ───────────────────────────────────────────────────────
    if auto_delay:
        console.print(f'  [dim]Auto-advancing in {auto_delay}s…  (Ctrl+C to stop)[/]')
        time.sleep(auto_delay)
    else:
        try:
            console.print('  [dim]Press ENTER for next message   (q = quit)[/]')
            ans = input()
            if ans.strip().lower() == 'q':
                return False
        except (EOFError, KeyboardInterrupt):
            return False

    return True


def run_summary(console, messages, total_packets=None):
    """Show a summary after all messages have been presented."""
    console.clear()
    console.print()
    console.rule('[bold green]  ASSESSMENT SUMMARY  [/]', style='green')
    console.print()

    by_risk = {}
    for m in messages:
        by_risk.setdefault(m['risk'], []).append(m)

    n_public   = len(by_risk.get('INFO', []))
    n_findings = sum(len(v) for k, v in by_risk.items() if k != 'INFO')

    t = Text()
    if total_packets:
        t.append(f'  Packets captured:        ', style='dim')
        t.append(f'{total_packets:,}\n', style='bold white')
    t.append(f'  Messages readable:       ', style='dim')
    t.append(f'{len(messages)}\n', style='bold white')
    t.append(f'\n  Public channel traffic:  ', style='dim')
    t.append(f'{n_public}\n', style='cyan')
    t.append(f'  (LongFast is a broadcast channel — no private expectation)\n\n', style='dim')
    t.append(f'  Known-key findings:      ', style='dim')
    t.append(f'{n_findings}\n', style='bold red' if n_findings else 'bold white')
    t.append(f'  (private channels using keys found in firmware source)\n\n', style='dim')

    for risk in ('CRITICAL', 'HIGH', 'MEDIUM'):
        if risk in by_risk:
            t.append(f'    {risk:<10}  ', style=_RISK_COLOR.get(risk, 'white'))
            t.append(f'{len(by_risk[risk])} messages\n', style='dim white')

    console.print(Panel(t, title='[bold]PASSIVE CAPTURE RESULTS[/]',
                        border_style='dim green', box=box.DOUBLE, padding=(1, 2)))
    console.print()
    console.print('  [dim]No transmissions were made. No active attack. No noise on the network.[/]')
    console.print('  [dim]Everything readable here was readable by any receiver in range.[/]')
    console.print()


# ─── Entry point ──────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(
        description='ESP32 LoRa Sniffer — Message Reveal Theater',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python message_theater.py                        # auto-find hotel CSV or demo
  python message_theater.py capture.csv            # from a capture file
  python message_theater.py capture.csv --auto 5   # auto-advance every 5 s
  python message_theater.py --demo                 # use built-in demo messages
""")
    ap.add_argument('file',   nargs='?', help='CSV capture file')
    ap.add_argument('--auto', type=float, metavar='SEC',
                    help='Auto-advance every N seconds (default: manual)')
    ap.add_argument('--demo', action='store_true', help='Use built-in demo messages')
    ap.add_argument('--max',  type=int, default=None,
                    help='Show only first N messages')
    args = ap.parse_args()

    console = Console()
    total_packets = None

    # ── Data source ───────────────────────────────────────────────────────────
    if args.demo:
        messages = _DEMO
    elif args.file:
        csv_path = Path(args.file)
        if not csv_path.exists():
            console.print(f'[red]File not found: {csv_path}[/]')
            return 1
        console.print(f'[dim]Loading {csv_path.name}…[/]')
        messages = load_messages(csv_path)
        if messages is None:
            console.print('[red]cryptography not installed — pip install cryptography[/]')
            return 1
        # Count total rows for summary
        with open(csv_path, 'r', encoding='utf-8', errors='replace') as f:
            total_packets = sum(1 for _ in f) - 1
        console.print(f'[dim]Found {len(messages)} text messages in {total_packets:,} packets.[/]')
    elif _HOTEL.exists():
        console.print(f'[dim]Auto-using hotel CSV: {_HOTEL.name}[/]')
        messages = load_messages(_HOTEL)
        if messages is None:
            messages = _DEMO
        with open(_HOTEL, 'r', encoding='utf-8', errors='replace') as f:
            total_packets = sum(1 for _ in f) - 1
        console.print(f'[dim]Found {len(messages)} text messages in {total_packets:,} packets.[/]')
    else:
        console.print('[dim]No CSV found — using demo data.[/]')
        messages = _DEMO

    if args.max:
        messages = messages[:args.max]

    if not messages:
        console.print('[yellow]No decrypted text messages found.[/]')
        return 0

    # ── Brief intro ───────────────────────────────────────────────────────────
    console.clear()
    console.print()
    console.rule('[bold green]  ESP32 LORA RECON  //  MESSAGE REVEAL  [/]', style='green')
    console.print()
    console.print(f'  [dim]{len(messages)} messages intercepted.[/]')
    if total_packets:
        console.print(f'  [dim]{total_packets:,} packets captured, all passively.[/]')
    console.print()
    console.print('  [dim]Each message was encrypted.  We decrypted every one.[/]')
    console.print()
    if not args.auto:
        try:
            input('  Press ENTER to begin…')
        except (EOFError, KeyboardInterrupt):
            return 0

    # ── Present each message ──────────────────────────────────────────────────
    for i, msg in enumerate(messages, 1):
        try:
            ok = present_message(console, msg, i, len(messages), auto_delay=args.auto)
        except KeyboardInterrupt:
            break
        if not ok:
            break

    run_summary(console, messages, total_packets)
    return 0


if __name__ == '__main__':
    sys.exit(main())
