#!/usr/bin/env python3
"""ESP32 LoRa Sniffer - Unified CLI

Three primary outputs: map, topology, report.
Everything else lives under `sniffer.py dev <cmd>`.
"""

import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
import subprocess
from pathlib import Path

# Headline outputs — the three things this toolkit produces.
OUTPUTS = {
    'map':      ('tools.map',      'GPS positions \u2192 interactive HTML map'),
    'topology': ('tools.topology', 'Mesh graph \u2192 SVG (zoomable in browser)'),
    'report':   ('tools.report',   'Security assessment \u2192 HTML report'),
}

# Utilities — still available, just not the headline.
DEV = {
    'monitor':   ('tools.monitor',        'Live WebSocket packet stream'),
    'wireshark': ('tools.wireshark',      'Convert ESP32 PCAP to Wireshark LoRaTap'),
    'merge':     ('tools.merge',          'Cross-capture identity linker'),
    'api':       ('tools.dev.api_client', 'REST API client'),
}


def _invoked_as():
    """Use 'lorecon' when launched as the installed console script,
    otherwise 'python sniffer.py' when run directly."""
    prog = Path(sys.argv[0]).name.lower()
    if prog.startswith("lorecon"):
        return "lorecon"
    return "python sniffer.py"


def print_help():
    name = _invoked_as()
    print("LoRecon - Unified CLI")
    print()
    print(f"Usage:  {name} <map|topology|report> <capture.csv> [-o OUT]")
    print(f"        {name} dev <cmd> [options]")
    print()
    w = max(len(k) for k in list(OUTPUTS) + list(DEV))
    print("Outputs:")
    for k, (_, desc) in OUTPUTS.items():
        print(f"  {k:<{w}}  {desc}")
    print()
    print(f"Dev utilities  ({name} dev <cmd>):")
    for k, (_, desc) in DEV.items():
        print(f"  {k:<{w}}  {desc}")
    print()
    print("Examples:")
    print(f"  {name} map      capture.csv")
    print(f"  {name} topology capture.csv -o mesh.png")
    print(f"  {name} report   capture.csv --pcap capture.pcap")
    print(f"  {name} dev monitor --host 192.168.4.1")
    print()
    print("Pass --help to any command for full options.")
    print()


def run_module(module, argv):
    result = subprocess.run(
        [sys.executable, '-m', module] + argv,
    )
    return result.returncode


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ('-h', '--help', 'help'):
        print_help()
        return 0

    command = sys.argv[1].lower()

    if command == 'dev':
        if len(sys.argv) < 3 or sys.argv[2] in ('-h', '--help', 'help'):
            print_help()
            return 0
        sub = sys.argv[2].lower()
        if sub not in DEV:
            print(f"Unknown dev command: '{sub}'")
            print()
            print_help()
            return 1
        return run_module(DEV[sub][0], sys.argv[3:])

    if command in OUTPUTS:
        return run_module(OUTPUTS[command][0], sys.argv[2:])

    print(f"Unknown command: '{command}'")
    print()
    print_help()
    return 1


if __name__ == '__main__':
    sys.exit(main())
