#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - Security Assessment Report Generator

Generates professional security assessment reports from captured data.
Outputs Markdown, HTML, or JSON format suitable for conference papers or client deliverables.

Usage:
    python recon_report.py capture.csv
    python recon_report.py capture.csv --format html --output report.html
    python recon_report.py --api 192.168.4.1  # Pull live data from ESP32

Requirements:
    pip install requests jinja2 markdown
"""

import argparse
import csv
import json
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
from collections import defaultdict
from datetime import datetime
from pathlib import Path
from statistics import mean, stdev

try:
    import requests
    REQUESTS_AVAILABLE = True
except ImportError:
    REQUESTS_AVAILABLE = False

try:
    import markdown
    MARKDOWN_AVAILABLE = True
except ImportError:
    MARKDOWN_AVAILABLE = False


class SecurityAssessment:
    """Analyze captured data for security findings"""
    
    def __init__(self):
        self.devices = defaultdict(lambda: {
            'packets': 0,
            'rssi_values': [],
            'protocols': set(),
            'first_seen': None,
            'last_seen': None,
            'encrypted_count': 0,
            'decrypted_count': 0,
            'psk_used': set(),
            'has_gps': False,
            'positions': [],
        })
        self.total_packets = 0
        self.protocol_counts = defaultdict(int)
        self.psk_success = defaultdict(int)
        self.vulnerabilities = []
        self.findings = []
        self.start_time = None
        self.end_time = None
    
    def load_csv(self, filepath):
        """Load data from CSV capture file"""
        with open(filepath, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                self._process_row(row)
    
    def load_api(self, host):
        """Load data from live ESP32 API"""
        if not REQUESTS_AVAILABLE:
            raise ImportError("requests library required")
        
        base = f"http://{host}"
        
        # Get devices
        try:
            r = requests.get(f"{base}/api/devices", timeout=10)
            devices = r.json().get('devices', [])
            for d in devices:
                node_id = d.get('nodeId', 'Unknown')
                self.devices[node_id]['packets'] = d.get('packetCount', 0)
                self.devices[node_id]['protocols'].add(d.get('protocol', 'Unknown'))
                if 'rssi' in d:
                    self.devices[node_id]['rssi_values'].append(d['rssi'])
        except Exception as e:
            print(f"Warning: Could not fetch devices: {e}")
        
        # Get statistics
        try:
            r = requests.get(f"{base}/api/statistics", timeout=10)
            stats = r.json()
            self.total_packets = stats.get('totalPackets', 0)
        except Exception:
            pass
        
        # Get security assessment
        try:
            r = requests.get(f"{base}/api/recon/security", timeout=10)
            sec = r.json()
            if 'vulnerabilities' in sec:
                self.vulnerabilities.extend(sec['vulnerabilities'])
        except Exception:
            pass
    
    def _process_row(self, row):
        """Process a single CSV row"""
        self.total_packets += 1
        
        timestamp = int(row.get('timestamp_ms', row.get('timestamp', 0))) / 1000
        if self.start_time is None or timestamp < self.start_time:
            self.start_time = timestamp
        if self.end_time is None or timestamp > self.end_time:
            self.end_time = timestamp
        
        node_id = row.get('node_id_hex', row.get('nodeId', row.get('node_id', 'Unknown')))
        protocol = row.get('protocol', 'Unknown')
        rssi = float(row.get('rssi_dbm', row.get('rssi', -100)) or -100)
        encrypted = row.get('encrypted', '0') in ('1', 'true', 'True')
        psk_result = row.get('psk_result', 'none')
        
        device = self.devices[node_id]
        device['packets'] += 1
        device['rssi_values'].append(rssi)
        device['protocols'].add(protocol)
        
        if device['first_seen'] is None:
            device['first_seen'] = timestamp
        device['last_seen'] = timestamp
        
        if encrypted:
            device['encrypted_count'] += 1
        
        if psk_result and psk_result not in ('none', 'failed', ''):
            device['decrypted_count'] += 1
            device['psk_used'].add(psk_result)
            self.psk_success[psk_result] += 1
        
        lat = row.get('lat_deg')
        lon = row.get('lon_deg')
        if lat and lon:
            device['has_gps'] = True
            device['positions'].append((float(lat), float(lon)))
        
        self.protocol_counts[protocol] += 1
    
    def analyze(self):
        """Perform security analysis"""
        self.findings = []
        
        # Check for default PSK usage
        total_decrypted = sum(self.psk_success.values())
        if total_decrypted > 0:
            psk_names = list(self.psk_success.keys())
            self.findings.append({
                'severity': 'HIGH',
                'title': 'Default PSK Encryption Detected',
                'description': f'{total_decrypted} packets decrypted with known default keys: {", ".join(psk_names)}',
                'recommendation': 'Configure unique channel PSKs. Do not use factory defaults.',
                'cwe': 'CWE-1392: Use of Default Credentials',
            })
        
        # Check for legacy admin key
        if 'ADMIN CHANNEL' in str(self.psk_success.keys()):
            self.findings.append({
                'severity': 'CRITICAL',
                'title': 'Legacy Admin Key Vulnerability',
                'description': 'Devices using pre-2.5 admin channel default key detected. Remote configuration attack possible.',
                'recommendation': 'Update firmware to 2.5+ and regenerate admin channel key.',
                'cwe': 'CWE-798: Use of Hard-coded Credentials',
            })
        
        # Check for unencrypted traffic
        unencrypted_devices = [nid for nid, d in self.devices.items() 
                               if d['encrypted_count'] == 0 and d['packets'] > 0]
        if unencrypted_devices:
            self.findings.append({
                'severity': 'MEDIUM',
                'title': 'Unencrypted Traffic Detected',
                'description': f'{len(unencrypted_devices)} devices transmitting without encryption.',
                'recommendation': 'Enable channel encryption on all devices.',
                'cwe': 'CWE-319: Cleartext Transmission of Sensitive Information',
            })
        
        # Check for GPS position exposure
        gps_devices = [nid for nid, d in self.devices.items() if d['has_gps']]
        if gps_devices:
            self.findings.append({
                'severity': 'MEDIUM',
                'title': 'GPS Position Broadcast Exposure',
                'description': f'{len(gps_devices)} devices broadcasting GPS coordinates.',
                'recommendation': 'Disable position broadcasts if operational security required.',
                'cwe': 'CWE-359: Exposure of Private Personal Information',
            })
        
        # Check for high-value targets (routers, base stations)
        high_rssi = [nid for nid, d in self.devices.items() 
                    if d['rssi_values'] and max(d['rssi_values']) > -50]
        if high_rssi:
            self.findings.append({
                'severity': 'INFO',
                'title': 'Close-Proximity Devices Detected',
                'description': f'{len(high_rssi)} devices with strong signal (>-50 dBm) - likely base stations or fixed infrastructure.',
                'recommendation': 'Review physical security of high-power installations.',
                'cwe': None,
            })
        
        return self.findings
    
    def generate_markdown(self):
        """Generate Markdown report"""
        lines = []
        
        # Header
        lines.append("# LoRa Network Security Assessment Report")
        lines.append("")
        lines.append(f"**Generated:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        lines.append(f"**Tool:** ESP32 LoRa Sniffer v2.2.0")
        lines.append("")
        
        # Executive Summary
        lines.append("## Executive Summary")
        lines.append("")
        
        critical = len([f for f in self.findings if f['severity'] == 'CRITICAL'])
        high = len([f for f in self.findings if f['severity'] == 'HIGH'])
        medium = len([f for f in self.findings if f['severity'] == 'MEDIUM'])
        
        lines.append(f"| Metric | Value |")
        lines.append(f"|--------|-------|")
        lines.append(f"| Devices Discovered | {len(self.devices)} |")
        lines.append(f"| Total Packets | {self.total_packets} |")
        lines.append(f"| Critical Findings | {critical} |")
        lines.append(f"| High Findings | {high} |")
        lines.append(f"| Medium Findings | {medium} |")
        lines.append("")
        
        # Duration
        if self.start_time and self.end_time:
            duration = self.end_time - self.start_time
            lines.append(f"**Scan Duration:** {duration/60:.1f} minutes")
            lines.append("")
        
        # Findings
        lines.append("## Security Findings")
        lines.append("")
        
        severity_order = ['CRITICAL', 'HIGH', 'MEDIUM', 'LOW', 'INFO']
        sorted_findings = sorted(self.findings, 
                                 key=lambda f: severity_order.index(f['severity']))
        
        for i, finding in enumerate(sorted_findings, 1):
            emoji = {'CRITICAL': '🔴', 'HIGH': '🟠', 'MEDIUM': '🟡', 
                    'LOW': '🔵', 'INFO': 'ℹ️'}.get(finding['severity'], '⚪')
            
            lines.append(f"### {i}. {emoji} {finding['title']}")
            lines.append("")
            lines.append(f"**Severity:** {finding['severity']}")
            lines.append("")
            lines.append(f"**Description:** {finding['description']}")
            lines.append("")
            lines.append(f"**Recommendation:** {finding['recommendation']}")
            if finding.get('cwe'):
                lines.append("")
                lines.append(f"**Reference:** {finding['cwe']}")
            lines.append("")
        
        # Protocol Distribution
        lines.append("## Protocol Distribution")
        lines.append("")
        lines.append("| Protocol | Packets | Percentage |")
        lines.append("|----------|---------|------------|")
        for proto, count in sorted(self.protocol_counts.items(), 
                                   key=lambda x: x[1], reverse=True):
            pct = count / max(self.total_packets, 1) * 100
            lines.append(f"| {proto} | {count} | {pct:.1f}% |")
        lines.append("")
        
        # Device Inventory
        lines.append("## Device Inventory")
        lines.append("")
        lines.append("| Node ID | Protocol | Packets | Avg RSSI | Encrypted | GPS |")
        lines.append("|---------|----------|---------|----------|-----------|-----|")
        
        for node_id, data in sorted(self.devices.items(), 
                                    key=lambda x: x[1]['packets'], reverse=True):
            proto = ', '.join(data['protocols'])
            avg_rssi = mean(data['rssi_values']) if data['rssi_values'] else 'N/A'
            if isinstance(avg_rssi, float):
                avg_rssi = f"{avg_rssi:.1f}"
            encrypted = '✅' if data['encrypted_count'] > 0 else '❌'
            gps = '📍' if data['has_gps'] else ''
            lines.append(f"| {node_id} | {proto} | {data['packets']} | {avg_rssi} | {encrypted} | {gps} |")
        lines.append("")
        
        # GPS Observations
        gps_devices = {nid: d for nid, d in self.devices.items() if d['has_gps'] and d['positions']}
        if gps_devices:
            lines.append("## GPS Observations")
            lines.append("")
            lines.append("The following devices broadcast GPS coordinates during the observation window.")
            lines.append("Coordinates are extracted from unencrypted position packets.")
            lines.append("")
            lines.append("| Node ID | Fixes | Lat Range | Lon Range |")
            lines.append("|---------|-------|-----------|-----------|")
            for node_id, data in sorted(gps_devices.items()):
                positions = data['positions']
                lats = [p[0] for p in positions]
                lons = [p[1] for p in positions]
                if len(positions) == 1:
                    lat_str = f"{lats[0]:.6f}"
                    lon_str = f"{lons[0]:.6f}"
                else:
                    lat_str = f"{min(lats):.6f} – {max(lats):.6f}"
                    lon_str = f"{min(lons):.6f} – {max(lons):.6f}"
                lines.append(f"| {node_id} | {len(positions)} | {lat_str} | {lon_str} |")
            lines.append("")

        # PSK Analysis
        if self.psk_success:
            lines.append("## PSK Decryption Analysis")
            lines.append("")
            lines.append("| PSK Name | Packets Decrypted |")
            lines.append("|----------|-------------------|")
            for psk, count in sorted(self.psk_success.items(), 
                                     key=lambda x: x[1], reverse=True):
                lines.append(f"| {psk} | {count} |")
            lines.append("")
        
        # Assessment Methodology
        lines.append("## Assessment Methodology")
        lines.append("")
        lines.append("Security findings are based on passive observation of over-the-air traffic only.")
        lines.append("No active probing or injection was performed.")
        lines.append("")
        lines.append("**Severity criteria:**")
        lines.append("")
        lines.append("| Severity | Criteria |")
        lines.append("|----------|----------|")
        lines.append("| CRITICAL | Exploitable with public tools, immediate risk (e.g. legacy admin key) |")
        lines.append("| HIGH | Default credential in use, decryption confirmed |")
        lines.append("| MEDIUM | Information exposure, missing encryption |")
        lines.append("| INFO | Observation only, no direct risk |")
        lines.append("")
        lines.append("**Limitations:** Findings are constrained by observation time and frequency coverage.")
        lines.append("Devices with fewer than 5 packets captured may be under-characterized —")
        lines.append("a low packet count does not indicate low risk.")
        lines.append("")

        # Recommendations
        lines.append("## Recommendations Summary")
        lines.append("")
        lines.append("1. **Change Default PSKs** - Configure unique channel encryption keys")
        lines.append("2. **Update Firmware** - Ensure all devices run firmware 2.5.0+")
        lines.append("3. **Review Position Broadcasts** - Disable if operational security needed")
        lines.append("4. **Use DMs for Sensitive Info** - Direct messages use PKC encryption")
        lines.append("5. **Monitor Network** - Regular passive scanning to detect unauthorized devices")
        lines.append("")
        
        # Footer
        lines.append("---")
        lines.append("")
        lines.append("*Report generated by ESP32 LoRa Sniffer - https://github.com/your-repo*")
        
        return '\n'.join(lines)
    
    def generate_html(self):
        """Generate HTML report"""
        md_content = self.generate_markdown()
        
        if MARKDOWN_AVAILABLE:
            html_body = markdown.markdown(md_content, extensions=['tables'])
        else:
            # Fallback: wrap markdown in pre tag
            html_body = f"<pre>{md_content}</pre>"
        
        html = f"""<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>LoRa Security Assessment Report</title>
    <style>
        :root {{
            --bg: #0d1117;
            --bg2: #161b22;
            --bg3: #21262d;
            --border: #30363d;
            --text: #c9d1d9;
            --text-muted: #8b949e;
            --green: #3fb950;
            --blue: #58a6ff;
            --yellow: #d29922;
            --orange: #db6d28;
            --red: #f85149;
        }}
        * {{ box-sizing: border-box; }}
        body {{
            font-family: 'Courier New', 'Consolas', monospace;
            background-color: var(--bg);
            color: var(--text);
            max-width: 1000px;
            margin: 0 auto;
            padding: 24px;
            line-height: 1.6;
        }}
        h1 {{
            color: var(--green);
            border-bottom: 1px solid var(--border);
            padding-bottom: 10px;
            letter-spacing: 0.05em;
        }}
        h2 {{
            color: var(--blue);
            margin-top: 32px;
            border-left: 3px solid var(--blue);
            padding-left: 10px;
        }}
        h3 {{ color: var(--text-muted); }}
        a {{ color: var(--blue); text-decoration: none; }}
        a:hover {{ text-decoration: underline; }}
        table {{
            border-collapse: collapse;
            width: 100%;
            margin: 15px 0;
            font-size: 0.9em;
        }}
        th, td {{
            border: 1px solid var(--border);
            padding: 8px 12px;
            text-align: left;
        }}
        th {{
            background-color: var(--bg3);
            color: var(--blue);
            font-weight: bold;
            letter-spacing: 0.04em;
        }}
        tr:nth-child(even) {{ background-color: var(--bg2); }}
        tr:hover {{ background-color: var(--bg3); }}
        code {{
            background-color: var(--bg3);
            color: var(--green);
            padding: 2px 6px;
            border-radius: 3px;
            font-family: 'Courier New', monospace;
            font-size: 0.88em;
        }}
        pre {{
            background-color: var(--bg2);
            border: 1px solid var(--border);
            padding: 16px;
            overflow-x: auto;
            border-radius: 4px;
            color: var(--text);
        }}
        blockquote {{
            border-left: 3px solid var(--border);
            margin-left: 0;
            padding-left: 16px;
            color: var(--text-muted);
        }}
        .severity-critical {{ color: var(--red); font-weight: bold; }}
        .severity-high {{ color: var(--orange); font-weight: bold; }}
        .severity-medium {{ color: var(--yellow); font-weight: bold; }}
        .severity-low {{ color: var(--green); }}
        hr {{ border: none; border-top: 1px solid var(--border); margin: 24px 0; }}
    </style>
</head>
<body>
{html_body}
</body>
</html>"""
        return html
    
    def generate_json(self):
        """Generate JSON report"""
        return json.dumps({
            'metadata': {
                'generated': datetime.now().isoformat(),
                'tool': 'ESP32 LoRa Sniffer v2.4.1',
                'total_packets': self.total_packets,
                'total_devices': len(self.devices),
                'scan_start': self.start_time,
                'scan_end': self.end_time,
            },
            'findings': self.findings,
            'protocol_distribution': dict(self.protocol_counts),
            'psk_analysis': dict(self.psk_success),
            'devices': {
                nid: {
                    'packets': d['packets'],
                    'protocols': list(d['protocols']),
                    'avg_rssi': mean(d['rssi_values']) if d['rssi_values'] else None,
                    'has_gps': d['has_gps'],
                    'encrypted_count': d['encrypted_count'],
                    'decrypted_count': d['decrypted_count'],
                }
                for nid, d in self.devices.items()
            }
        }, indent=2)


def main():
    parser = argparse.ArgumentParser(
        description='Generate Security Assessment Reports from LoRa Capture Data',
        epilog='Example: python recon_report.py capture.csv --format html -o report.html'
    )
    parser.add_argument('input', nargs='?', help='Input CSV file')
    parser.add_argument('--api', metavar='HOST', 
                       help='Pull live data from ESP32 API (e.g., 192.168.4.1)')
    parser.add_argument('--format', '-f', choices=['markdown', 'html', 'json'],
                       default='markdown', help='Output format (default: markdown)')
    parser.add_argument('--output', '-o', help='Output file (default: stdout)')
    
    args = parser.parse_args()
    
    if not args.input and not args.api:
        parser.print_help()
        return 1
    
    assessment = SecurityAssessment()
    
    # Load data
    if args.api:
        print(f"Fetching data from {args.api}...", file=sys.stderr)
        assessment.load_api(args.api)
    else:
        filepath = Path(args.input)
        if not filepath.exists():
            print(f"Error: File not found: {filepath}", file=sys.stderr)
            return 1
        print(f"Loading {filepath}...", file=sys.stderr)
        assessment.load_csv(filepath)
    
    print(f"Loaded {assessment.total_packets} packets from {len(assessment.devices)} devices", 
          file=sys.stderr)
    
    # Analyze
    assessment.analyze()
    print(f"Found {len(assessment.findings)} security findings", file=sys.stderr)
    
    # Generate report
    if args.format == 'html':
        output = assessment.generate_html()
    elif args.format == 'json':
        output = assessment.generate_json()
    else:
        output = assessment.generate_markdown()
    
    # Write output
    if args.output:
        with open(args.output, 'w', encoding='utf-8') as f:
            f.write(output)
        print(f"Report saved to {args.output}", file=sys.stderr)
    else:
        print(output)
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
