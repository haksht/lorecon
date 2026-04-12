#!/usr/bin/env python3
"""ESP32 LoRa Sniffer - Unified CLI"""

import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
import subprocess
from pathlib import Path

TOOLS_DIR = Path(__file__).parent
REPO_ROOT = TOOLS_DIR.parent

# Each entry: (module path for `python -m`, description).
COMMANDS = {
    # Analysis — the three output types
    'map':       ('tools.map',       'GPS positions → interactive HTML map'),
    'topology':  ('tools.topology',  'Mesh graph → PNG image (Meshtastic + MeshCore)'),
    'report':    ('tools.report',    'Security assessment → HTML report'),
    'merge':     ('tools.merge',     'Cross-capture identity linker (2+ CSVs)'),
    # Live / ops
    'monitor':   ('tools.monitor',   'Live WebSocket packet stream (headless)'),
    'wireshark': ('tools.wireshark', 'Convert ESP32 PCAP to Wireshark LoRaTap format'),
    # Developer
    'api':       ('tools.dev.api_client', 'REST API client (~30 subcommands)'),
}


def print_help():
    print("ESP32 LoRa Sniffer - Unified CLI")
    print()
    print("Usage:  python sniffer.py <command> [options]")
    print("        All extra arguments are forwarded to the underlying tool.")
    print()
    w = max(len(k) for k in COMMANDS)
    sections = [
        ("Analysis", ['map', 'topology', 'report', 'merge']),
        ("Live / Ops", ['monitor', 'wireshark']),
        ("Developer", ['api']),
    ]
    for title, keys in sections:
        print(f"{title}:")
        for k in keys:
            script, desc = COMMANDS[k]
            print(f"  {k:<{w}}  {desc}")
        print()
    print("Examples:")
    print("  python sniffer.py map       capture.csv")
    print("  python sniffer.py topology  capture.csv")
    print("  python sniffer.py report    capture.csv --pcap capture.pcap")
    print("  python sniffer.py monitor   --host 192.168.4.1")
    print("  python sniffer.py monitor   --messages")
    print("  python sniffer.py wireshark capture.pcap")
    print("  python sniffer.py api       status")
    print()
    print("Pass --help to any command for full options:")
    print("  python sniffer.py topology --help")
    print()


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ('-h', '--help', 'help'):
        print_help()
        return 0

    command = sys.argv[1].lower()
    if command not in COMMANDS:
        print(f"Unknown command: '{command}'")
        print()
        print_help()
        return 1

    module, _ = COMMANDS[command]
    # Run the module with repo root as CWD so `tools.core` imports resolve.
    result = subprocess.run(
        [sys.executable, '-m', module] + sys.argv[2:],
        cwd=str(REPO_ROOT),
    )
    return result.returncode


if __name__ == '__main__':
    sys.exit(main())
