#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - Unified CLI

Single entry point for all analysis and monitoring tools.

Usage:
    python sniffer.py <command> [options]

Commands:
    monitor     Live WebSocket packet stream (headless)
    visualize   5-panel live dashboard with GPS map
    analyze     Analyze a PCAP file
    audit       PSK vulnerability audit on a CSV capture
    report      Generate security assessment report
    topology    Mesh network topology reconstruction
    decode      Batch Meshtastic decryption from PCAP
    demo        Launch visualizer in demo mode (no hardware needed)
    help        Show this help

All extra arguments are passed through to the underlying tool.

Examples:
    python sniffer.py monitor --host 192.168.4.1 --decrypt
    python sniffer.py visualize --demo --audio
    python sniffer.py analyze capture.pcap --wireshark
    python sniffer.py audit capture.csv --verbose
    python sniffer.py report capture.csv --format html -o report.html
    python sniffer.py topology capture.csv --ascii
    python sniffer.py decode capture.pcap
    python sniffer.py demo
"""

import sys
import subprocess
from pathlib import Path

TOOLS_DIR = Path(__file__).parent

COMMANDS = {
    'monitor':   (TOOLS_DIR / 'ws_monitor.py',                          'Live WebSocket packet stream (headless)'),
    'visualize': (TOOLS_DIR / 'enhanced_live_visualizer.py',            '5-panel live dashboard with GPS map'),
    'analyze':   (TOOLS_DIR / 'pcap_analyzer.py',                       'Analyze a PCAP file'),
    'audit':     (TOOLS_DIR / 'meshtastic' / 'psk_auditor.py',          'PSK vulnerability audit on a CSV capture'),
    'report':    (TOOLS_DIR / 'recon_report.py',                        'Generate security assessment report'),
    'topology':  (TOOLS_DIR / 'meshtastic' / 'mesh_topology_analyzer.py', 'Mesh network topology reconstruction'),
    'decode':    (TOOLS_DIR / 'meshtastic' / 'meshtastic_decoder.py',   'Batch Meshtastic decryption from PCAP'),
    'api':       (TOOLS_DIR / 'api_client.py',                          'REST API client (~30 subcommands)'),
    'lorawan':   (TOOLS_DIR / 'lorawan' / 'join_parser.py',            'LoRaWAN join request / DevEUI scanner'),
    'uplink':    (TOOLS_DIR / 'lorawan' / 'uplink_parser.py',          'LoRaWAN uplink frame parser + decryptor'),
    'assess':    (TOOLS_DIR / 'run_assessment.py',                      'End-to-end capture + audit + report pipeline'),
}

DEMO_ALIAS = 'demo'


def print_help():
    print(__doc__)
    print("Commands:")
    print()
    width = max(len(k) for k in list(COMMANDS) + [DEMO_ALIAS])
    for name, (_, desc) in COMMANDS.items():
        print(f"  {name:<{width}}  {desc}")
    print(f"  {DEMO_ALIAS:<{width}}  Launch visualizer in demo mode (no hardware needed)")
    print()


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ('-h', '--help', 'help'):
        print_help()
        return 0

    command = sys.argv[1].lower()
    extra_args = sys.argv[2:]

    # demo alias
    if command == DEMO_ALIAS:
        script = COMMANDS['visualize'][0]
        extra_args = ['--demo'] + extra_args
    elif command in COMMANDS:
        script = COMMANDS[command][0]
    else:
        print(f"Unknown command: '{command}'")
        print()
        print_help()
        return 1

    if not script.exists():
        print(f"❌ Tool not found: {script}")
        return 1

    result = subprocess.run([sys.executable, str(script)] + extra_args)
    return result.returncode


if __name__ == '__main__':
    sys.exit(main())
