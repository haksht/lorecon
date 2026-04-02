#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - End-to-End Assessment Pipeline

Connects to the device, waits for capture data, then runs the full
analysis chain automatically:

    1. Connect to device API (WiFi) and wait for capture window
    2. Download CSV and PCAP
    3. Run PSK audit (psk_auditor)
    4. Generate security report (recon_report)
    5. Open report in browser

For operators who want a single command that produces a deliverable.

Usage:
    python run_assessment.py --host 192.168.4.1
    python run_assessment.py --host 192.168.4.1 --duration 10m
    python run_assessment.py --host 192.168.4.1 --duration 30m --format html
    python run_assessment.py --demo

Requirements:
    pip install requests websocket-client cryptography

Output:
    assessment_<timestamp>/
        capture.csv
        capture.pcap
        audit.txt
        report.html  (or report.md)
"""

import argparse
import json
import os
import subprocess
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
import time
import webbrowser
from datetime import datetime
from pathlib import Path

try:
    import requests
    REQUESTS_AVAILABLE = True
except ImportError:
    REQUESTS_AVAILABLE = False

TOOLS_DIR = Path(__file__).parent


def check_deps():
    missing = []
    if not REQUESTS_AVAILABLE:
        missing.append('requests')
    try:
        import websocket
    except ImportError:
        missing.append('websocket-client')
    if missing:
        print(f"❌ Missing dependencies: {', '.join(missing)}")
        print(f"   pip install {' '.join(missing)}")
        return False
    return True


def wait_for_device(host: str, timeout: int = 30) -> bool:
    """Poll until device API is reachable."""
    print(f"⏳ Waiting for device at {host}...", end='', flush=True)
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            r = requests.get(f"http://{host}/api/status", timeout=3)
            if r.status_code == 200:
                print(" connected.")
                return True
        except Exception:
            pass
        print('.', end='', flush=True)
        time.sleep(2)
    print(" timed out.")
    return False


def get_device_info(host: str) -> dict:
    try:
        r = requests.get(f"http://{host}/api/status", timeout=5)
        return r.json()
    except Exception:
        return {}


def wait_for_packets(host: str, min_packets: int, duration_sec: int) -> int:
    """
    Wait until either duration_sec has passed or min_packets is reached.
    Returns total packet count at end of wait.
    """
    start = time.time()
    deadline = start + duration_sec
    last_count = 0

    print(f"📡 Capturing for {duration_sec//60}m {duration_sec%60:02d}s "
          f"(or until {min_packets} packets)...")
    print(f"   Press Ctrl+C to stop early and proceed with current data.\n")

    try:
        while time.time() < deadline:
            elapsed = time.time() - start
            remaining = deadline - time.time()
            try:
                r = requests.get(f"http://{host}/api/statistics", timeout=3)
                stats = r.json()
                count = stats.get('totalPackets', 0)
                devices = stats.get('totalDevices', 0)
                last_count = count
                bar_len = 30
                filled = int(bar_len * elapsed / duration_sec)
                bar = '█' * filled + '░' * (bar_len - filled)
                print(f"\r   [{bar}] {elapsed:5.0f}s  {count:4} pkts  {devices} devices   ",
                      end='', flush=True)
                if count >= min_packets:
                    print(f"\n✅ Reached {min_packets} packet threshold.")
                    break
            except Exception:
                pass
            time.sleep(3)
    except KeyboardInterrupt:
        print(f"\n⚠️  Capture interrupted by user.")

    print()
    return last_count


def download_csv(host: str, token: str | None, output_path: Path) -> bool:
    headers = {'X-Auth-Token': token} if token else {}
    try:
        r = requests.get(f"http://{host}/api/export/csv", headers=headers, timeout=15)
        if r.status_code == 200:
            output_path.write_bytes(r.content)
            print(f"✅ CSV downloaded: {output_path} ({len(r.content):,} bytes)")
            return True
        print(f"❌ CSV download failed: HTTP {r.status_code}")
    except Exception as e:
        print(f"❌ CSV download error: {e}")
    return False


def download_pcap(host: str, token: str | None, output_path: Path) -> bool:
    headers = {'X-Auth-Token': token} if token else {}
    try:
        r = requests.get(f"http://{host}/api/export/pcap", headers=headers, timeout=15)
        if r.status_code == 200:
            output_path.write_bytes(r.content)
            print(f"✅ PCAP downloaded: {output_path} ({len(r.content):,} bytes)")
            return True
        print(f"❌ PCAP download failed: HTTP {r.status_code}")
    except Exception as e:
        print(f"❌ PCAP download error: {e}")
    return False


def run_tool(script: Path, args: list[str], output_file: Path | None = None) -> tuple[int, str]:
    """Run a tool subprocess, optionally capturing stdout to a file."""
    cmd = [sys.executable, str(script)] + args
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8')
        output = result.stdout + result.stderr
        if output_file:
            output_file.write_text(output, encoding='utf-8')
        return result.returncode, output
    except Exception as e:
        return 1, str(e)


def run_demo(output_dir: Path, report_format: str, no_open: bool = False):
    """Run a demo assessment with simulated data."""
    print("\n[DEMO MODE — simulated capture + analysis]\n")

    output_dir.mkdir(parents=True, exist_ok=True)

    # Generate demo CSV — canonical node IDs match the visualizer/topology/reveal demos
    demo_csv = output_dir / 'capture.csv'
    demo_rows = [
        'timestamp_ms,node_id_hex,protocol,rssi_dbm,snr_db,frequency_mhz,encrypted,psk_result,lat_deg,lon_deg',
        # 0x401ACD4E — PRESENTER node (high activity, default PSK decrypted, GPS)
        '1700000001000,0x401ACD4E,Meshtastic,-58.7,9.2,906.875,1,LongFast public channel,35.869000,-78.845000',
        '1700000004500,0x401ACD4E,Meshtastic,-59.1,9.0,906.875,1,LongFast public channel,35.869050,-78.844980',
        '1700000009000,0x401ACD4E,Meshtastic,-57.8,9.4,906.875,1,LongFast public channel,35.869100,-78.844900',
        '1700000014000,0x401ACD4E,Meshtastic,-58.3,9.1,906.875,1,LongFast public channel,,',
        '1700000022000,0x401ACD4E,Meshtastic,-59.5,8.8,906.875,1,LongFast public channel,,',
        # 0x598B29CE — ATTENDEE node (default PSK decrypted, GPS)
        '1700000002500,0x598B29CE,Meshtastic,-71.3,7.1,906.875,1,LongFast public channel,35.868300,-78.844100',
        '1700000007000,0x598B29CE,Meshtastic,-72.0,6.9,906.875,1,LongFast public channel,,',
        '1700000015000,0x598B29CE,Meshtastic,-71.8,7.2,906.875,1,LongFast public channel,35.868320,-78.844080',
        '1700000025000,0x598B29CE,Meshtastic,-72.5,6.7,906.875,1,LongFast public channel,,',
        # 0xB3F42A10 — MOBILE node (default PSK, moving GPS positions)
        '1700000003000,0xB3F42A10,Meshtastic,-79.1,5.3,906.875,1,LongFast public channel,35.869700,-78.845800',
        '1700000011000,0xB3F42A10,Meshtastic,-80.4,4.9,906.875,1,LongFast public channel,35.869820,-78.845600',
        '1700000018000,0xB3F42A10,Meshtastic,-78.8,5.5,906.875,1,LongFast public channel,35.870100,-78.845300',
        # 0x7C891DEF — HIDDEN node (weak signal, default PSK)
        '1700000005000,0x7C891DEF,Meshtastic,-88.4,2.1,906.875,1,LongFast public channel,,',
        '1700000016000,0x7C891DEF,Meshtastic,-89.2,1.8,906.875,1,LongFast public channel,,',
        # 0xA42B8C56 — ROUTER node (strong signal, CRITICAL: legacy admin key, GPS)
        '1700000006000,0xA42B8C56,Meshtastic,-42.1,14.3,906.875,1,Admin channel (legacy),35.869300,-78.846300',
        '1700000012000,0xA42B8C56,Meshtastic,-41.8,14.5,906.875,1,Admin channel (legacy),,',
        '1700000020000,0xA42B8C56,Meshtastic,-42.5,14.1,906.875,1,Admin channel (legacy),,',
        # LoRaWAN sensor (unencrypted)
        '1700000008000,0x260B1234,LoRaWAN,-91.2,1.1,903.900,0,none,,',
        '1700000017000,0x260B1234,LoRaWAN,-90.8,1.3,903.900,0,none,,',
    ]
    demo_csv.write_text('\n'.join(demo_rows))
    print(f"✅ Demo capture written: {demo_csv}")

    _run_analysis(output_dir, demo_csv, None, report_format, demo=True, no_open=no_open)


def _run_analysis(output_dir: Path, csv_path: Path, pcap_path: Path | None,
                  report_format: str, demo: bool = False, no_open: bool = False):
    """Run audit + report on downloaded (or demo) data."""

    # Step 1: PSK Audit
    print(f"\n{'─'*50}")
    print("🔍 Running PSK audit...")
    audit_out = output_dir / 'audit.txt'
    rc, output = run_tool(
        TOOLS_DIR / 'meshtastic' / 'psk_auditor.py',
        [str(csv_path)],
        audit_out
    )
    # Print a summary from the audit output
    for line in output.splitlines():
        if any(k in line for k in ['CRITICAL', 'HIGH', 'MEDIUM', 'Found', 'devices', 'Audit']):
            print(f"   {line.strip()}")
    print(f"   Full audit saved: {audit_out}")

    # Step 2: LoRaWAN analysis (if PCAP available)
    if pcap_path and pcap_path.exists():
        print(f"\n{'─'*50}")
        print("📻 Running LoRaWAN join scanner...")
        lorawan_out = output_dir / 'lorawan_joins.txt'
        rc, output = run_tool(
            TOOLS_DIR / 'lorawan' / 'join_parser.py',
            [str(pcap_path)],
            lorawan_out
        )
        join_count = sum(1 for l in output.splitlines() if 'Join Request' in l and 'found' in l)
        if join_count or 'No OTAA' not in output:
            print(f"   Saved: {lorawan_out}")

    # Step 3: Security Report
    print(f"\n{'─'*50}")
    print(f"📄 Generating {report_format.upper()} report...")
    ext = {'html': 'html', 'markdown': 'md', 'json': 'json'}.get(report_format, 'md')
    report_path = output_dir / f'report.{ext}'
    rc, output = run_tool(
        TOOLS_DIR / 'recon_report.py',
        [str(csv_path), '--format', report_format, '--output', str(report_path)]
    )
    if report_path.exists():
        print(f"✅ Report saved: {report_path} ({report_path.stat().st_size:,} bytes)")
    else:
        print(f"❌ Report generation failed:\n{output}")
        return

    # Step 4: Open report
    print(f"\n{'─'*50}")
    uri = report_path.resolve().as_uri()
    if report_format == 'html' and not no_open:
        print(f"🌐 Opening report...")
        webbrowser.open(uri)
    else:
        print(f"📄 Report: {uri}")

    # Summary
    print(f"\n{'='*50}")
    print(f"✅ Assessment complete")
    print(f"   Output directory: {output_dir.resolve()}")
    print(f"   Files:")
    for f in sorted(output_dir.iterdir()):
        print(f"     {f.name}  ({f.stat().st_size:,} bytes)")


def main():
    parser = argparse.ArgumentParser(
        description='ESP32 LoRa Sniffer - End-to-End Assessment Pipeline',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --host 192.168.4.1                    10-minute capture + report
  %(prog)s --host 192.168.4.1 --duration 30m     30-minute capture
  %(prog)s --host 192.168.4.1 --min-packets 200  Stop when 200 packets seen
  %(prog)s --host 192.168.4.1 --format html      HTML report (opens in browser)
  %(prog)s --demo                                 Demo run without hardware
"""
    )
    parser.add_argument('--host', default='192.168.4.1',
                        help='ESP32 IP address (default: 192.168.4.1)')
    parser.add_argument('--token', help='API auth token (if required)')
    parser.add_argument('--duration', default='10m',
                        help='Capture duration, e.g. 5m, 30m, 1h (default: 10m)')
    parser.add_argument('--min-packets', type=int, default=50,
                        help='Stop early when this many packets captured (default: 50)')
    parser.add_argument('--format', '-f', choices=['markdown', 'html', 'json'],
                        default='html', dest='report_format',
                        help='Report format (default: html)')
    parser.add_argument('--out', default=None,
                        help='Output directory (default: assessment_<timestamp>)')
    parser.add_argument('--demo', action='store_true',
                        help='Demo run with simulated data (no device needed)')
    parser.add_argument('--no-open', action='store_true',
                        help='Do not auto-open the report in a browser (print path instead)')

    args = parser.parse_args()

    # Parse duration
    duration_str = args.duration.lower()
    if duration_str.endswith('h'):
        duration_sec = int(duration_str[:-1]) * 3600
    elif duration_str.endswith('m'):
        duration_sec = int(duration_str[:-1]) * 60
    elif duration_str.endswith('s'):
        duration_sec = int(duration_str[:-1])
    else:
        duration_sec = int(duration_str)

    # Output directory
    ts = datetime.now().strftime('%Y%m%d_%H%M%S')
    output_dir = Path(args.out) if args.out else Path(f'assessment_{ts}')

    print(f"ESP32 LoRa Sniffer — Assessment Pipeline")
    print(f"{'='*50}")

    if args.demo:
        run_demo(output_dir, args.report_format, no_open=args.no_open)
        return 0

    if not check_deps():
        return 1

    output_dir.mkdir(parents=True, exist_ok=True)
    print(f"Output directory: {output_dir.resolve()}")

    # Connect
    if not wait_for_device(args.host):
        print(f"\nTip: Connect to the device's WiFi AP (default SSID: LoRa-Sniffer)")
        return 1

    info = get_device_info(args.host)
    if info:
        board = info.get('board', 'Unknown')
        fw = info.get('firmware', 'Unknown')
        print(f"   Board: {board}  Firmware: {fw}")

    # Capture window
    wait_for_packets(args.host, args.min_packets, duration_sec)

    # Download data
    print(f"\n{'─'*50}")
    print("⬇️  Downloading capture data...")
    csv_path = output_dir / 'capture.csv'
    pcap_path = output_dir / 'capture.pcap'
    csv_ok = download_csv(args.host, args.token, csv_path)
    download_pcap(args.host, args.token, pcap_path)

    if not csv_ok or not csv_path.exists():
        print("❌ No CSV data to analyze. Exiting.")
        return 1

    _run_analysis(output_dir, csv_path, pcap_path, args.report_format, no_open=args.no_open)
    return 0


if __name__ == '__main__':
    sys.exit(main())
