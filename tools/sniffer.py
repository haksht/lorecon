#!/usr/bin/env python3
"""ESP32 LoRa Sniffer - Unified CLI"""

import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
import subprocess
from pathlib import Path

TOOLS_DIR = Path(__file__).parent

# Commands grouped for presentation clarity.
# Each entry: name -> (script_path, description)
STORY_COMMANDS = {
    'story':    (None,                                                          'Guided presenter mode — start here'),
    'demo':     (TOOLS_DIR / 'enhanced_live_visualizer.py',                    '5-panel live dashboard (no hardware needed)'),
    'topology': (TOOLS_DIR / 'meshtastic' / 'mesh_topology_analyzer.py',       'Mesh network map (--demo or file input)'),
    'reveal':   (TOOLS_DIR / 'meshtastic' / 'make_reveal.py',                  'Decrypt reveal presentation (--demo or pcap input)'),
    'lorawan':  (TOOLS_DIR / 'join_parser.py',                                 'LoRaWAN join request / DevEUI scanner (--demo)'),
    'assess':   (TOOLS_DIR / 'run_assessment.py',                              'Full pipeline: capture -> audit -> report (--demo)'),
}

ANALYSIS_COMMANDS = {
    'monitor':  (TOOLS_DIR / 'ws_monitor.py',                                  'Headless live WebSocket packet stream (--demo)'),
    'audit':    (TOOLS_DIR / 'meshtastic' / 'psk_auditor.py',                  'PSK vulnerability scan (--demo, --dramatic)'),
    'report':   (TOOLS_DIR / 'recon_report.py',                                'Security assessment report generator'),
}

DEV_COMMANDS = {
    'analyze':  (TOOLS_DIR / 'pcap_analyzer.py',                               'Raw PCAP inspector'),
    'decode':   (TOOLS_DIR / 'meshtastic' / 'meshtastic_decoder.py',           'Batch Meshtastic decryptor'),
    'api':      (TOOLS_DIR / 'dev' / 'api_client.py',                          'REST API client (~30 subcommands)'),
    'visualize':(TOOLS_DIR / 'enhanced_live_visualizer.py',                    '5-panel dashboard (serial or WebSocket)'),
}

# Flat lookup for dispatch (story handled separately)
ALL_COMMANDS = {**STORY_COMMANDS, **ANALYSIS_COMMANDS, **DEV_COMMANDS}

# demo alias -> visualize --demo
DEMO_ALIAS = 'demo'


def print_help():
    print("ESP32 LoRa Sniffer - Unified CLI")
    print()
    print("Usage: python sniffer.py <command> [options]")
    print("       All extra arguments are passed to the underlying tool.")
    print()

    def _section(title, cmds):
        print(f"{title}:")
        width = max(len(k) for k in cmds)
        for name, (_, desc) in cmds.items():
            print(f"  {name:<{width}}  {desc}")
        print()

    _section("CONFERENCE STORY", STORY_COMMANDS)
    _section("ANALYSIS TOOLS", ANALYSIS_COMMANDS)
    _section("DEVELOPER TOOLS", DEV_COMMANDS)

    print("Examples:")
    print("  python sniffer.py story")
    print("  python sniffer.py demo")
    print("  python sniffer.py topology --demo")
    print("  python sniffer.py reveal --demo")
    print("  python sniffer.py reveal capture.pcap --max-messages 4")
    print("  python sniffer.py lorawan --demo")
    print("  python sniffer.py assess --demo")
    print()


# ── Story mode ──────────────────────────────────────────────────────────────

STORY_STEPS = [
    ("ACT 1  LIVE CAPTURE",
     "demo",
     ["--demo"],
     "5-panel live dashboard — simulated traffic, audio optional"),
    ("ACT 2  DEVICE INVENTORY",
     "topology",
     ["--demo"],
     "ASCII mesh map — who is routing for whom"),
    ("ACT 3  DECRYPT REVEAL",
     "reveal",
     ["--demo"],
     "Browser reveal page — hex glitch -> plaintext"),
    ("ACT 4  LORAWAN SCAN",
     "lorawan",
     ["--demo"],
     "DevEUI inventory — device IDs broadcast in plaintext by design"),
    ("ACT 5  ASSESSMENT REPORT",
     "assess",
     ["--demo", "--format", "html"],
     "Full pipeline — deliverable-quality HTML report"),
]


def story_mode():
    """Interactive presenter checklist. Enter to launch each act."""
    import os

    print()
    print("=" * 62)
    print("  ESP32 LoRa Sniffer  //  CONFERENCE STORY MODE")
    print("=" * 62)
    print()
    print("  Each step runs a demo — no hardware or files needed.")
    print("  Press ENTER to launch, or type the command to run manually.")
    print()

    for i, (label, cmd, extra, hint) in enumerate(STORY_STEPS, 1):
        script_entry = ALL_COMMANDS.get(cmd)
        if cmd == DEMO_ALIAS:
            script = ALL_COMMANDS['visualize'][0]
            run_args = ['--demo'] + extra
        else:
            script = script_entry[0] if script_entry else None
            run_args = extra

        # Build the display command string
        display = f"python sniffer.py {cmd} {' '.join(extra)}"

        print(f"  [{i}] {label}")
        print(f"       {hint}")
        print(f"       > {display}")
        print()

        try:
            ans = input("       Press ENTER to launch (s=skip, q=quit): ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            print()
            break

        if ans == 'q':
            break
        if ans == 's':
            print("       Skipped.\n")
            continue

        if script and script.exists():
            print()
            subprocess.run([sys.executable, str(script)] + run_args)
            print()
        else:
            print(f"       [!] Script not found: {script}\n")

    print("  Story complete.")
    print()


# ── Main ────────────────────────────────────────────────────────────────────

def main():
    if len(sys.argv) < 2 or sys.argv[1] in ('-h', '--help', 'help'):
        print_help()
        return 0

    command = sys.argv[1].lower()
    extra_args = sys.argv[2:]

    # story is handled inline
    if command == 'story':
        story_mode()
        return 0

    # demo alias -> visualize --demo
    if command == DEMO_ALIAS:
        script = ALL_COMMANDS['visualize'][0]
        extra_args = ['--demo'] + extra_args
    elif command in ALL_COMMANDS:
        entry = ALL_COMMANDS[command]
        script = entry[0]
        if script is None:
            # Shouldn't happen for non-story commands
            print(f"'{command}' has no associated script.")
            return 1
    else:
        print(f"Unknown command: '{command}'")
        print()
        print_help()
        return 1

    if not script.exists():
        print(f"Tool not found: {script}")
        return 1

    result = subprocess.run([sys.executable, str(script)] + extra_args)
    return result.returncode


if __name__ == '__main__':
    sys.exit(main())
