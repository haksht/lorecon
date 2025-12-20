#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - Duty Cycle Compliance Monitor

Monitors LoRa transmissions for regulatory compliance:
- Duty cycle limits (EU: 1%, US: varies by band)
- Airtime calculations
- Per-device transmission budgets
- Hourly/daily compliance reports

Regulatory Limits:
- EU868: 1% duty cycle (36s/hour max airtime)
- US915: No strict duty cycle, but FCC dwell time limits
- AU915: Similar to US915

Usage:
    python duty_cycle_monitor.py capture.csv
    python duty_cycle_monitor.py capture.csv --region EU868 --threshold 1.0
    python duty_cycle_monitor.py --live 192.168.4.1 --region US915

Requirements:
    pip install requests (for live monitoring)
"""

import argparse
import csv
import json
import sys
from collections import defaultdict
from datetime import datetime, timedelta
from pathlib import Path
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple

try:
    import requests
    REQUESTS_AVAILABLE = True
except ImportError:
    REQUESTS_AVAILABLE = False


# Regulatory limits by region (duty cycle as percentage)
REGION_LIMITS = {
    'EU868': {
        'duty_cycle_pct': 1.0,        # 1% max
        'max_airtime_per_hour_ms': 36000,  # 36 seconds/hour
        'description': 'European 868 MHz ISM band',
        'bands': {
            (868.0, 868.6): 1.0,   # g band
            (868.7, 869.2): 0.1,   # g1 band
            (869.4, 869.65): 10.0, # g2 band (higher limit)
            (869.7, 870.0): 1.0,   # g3 band
        }
    },
    'US915': {
        'duty_cycle_pct': None,       # No strict limit
        'max_dwell_time_ms': 400,     # FCC dwell time for FHSS
        'description': 'US 915 MHz ISM band',
        'max_power_dbm': 30,          # 1W EIRP
    },
    'AU915': {
        'duty_cycle_pct': None,
        'description': 'Australian 915 MHz ISM band',
        'max_power_dbm': 30,
    },
    'AS923': {
        'duty_cycle_pct': 1.0,
        'max_airtime_per_hour_ms': 36000,
        'description': 'Asia 923 MHz band',
    },
}

# Airtime calculation parameters
# Time on Air (ToA) = (8 + max(ceil((8*PL - 4*SF + 28 + 16 - 20*H)/(4*(SF-2*DE))), 0) * (CR+4)) * Tsym + Tpreamble
# Simplified: we estimate from packet length and SF


@dataclass
class TransmissionRecord:
    """Single transmission record"""
    timestamp_ms: int
    node_id: str
    frequency_mhz: float
    length_bytes: int
    sf: int
    bw_khz: float
    estimated_airtime_ms: float


class DutyCycleMonitor:
    """Monitor and analyze duty cycle compliance"""
    
    def __init__(self, region='EU868'):
        self.region = region
        self.limits = REGION_LIMITS.get(region, REGION_LIMITS['EU868'])
        
        # Per-device tracking
        self.device_stats: Dict[str, Dict] = defaultdict(lambda: {
            'transmissions': [],
            'total_airtime_ms': 0,
            'hourly_airtime': defaultdict(float),  # hour -> airtime_ms
            'violations': [],
        })
        
        # Global stats
        self.total_transmissions = 0
        self.total_airtime_ms = 0
        self.violations = []
    
    def estimate_airtime(self, length_bytes: int, sf: int = 7, bw_khz: float = 125) -> float:
        """
        Estimate airtime in milliseconds using LoRa formula
        
        Simplified formula based on SF and packet length.
        For accurate results, would need coding rate and header mode.
        """
        # Symbol time in ms
        t_sym = (2 ** sf) / (bw_khz * 1000) * 1000
        
        # Preamble (8 symbols + 4.25 sync) 
        t_preamble = (8 + 4.25) * t_sym
        
        # Payload symbols (simplified, assuming CR=4/5, explicit header)
        payload_bits = 8 * length_bytes
        symbols = 8 + max(0, (payload_bits - 4 * sf + 28 + 16) // (4 * sf)) * 5
        
        t_payload = symbols * t_sym
        
        return t_preamble + t_payload
    
    def add_transmission(self, node_id: str, timestamp_ms: int, 
                        frequency_mhz: float, length_bytes: int,
                        sf: int = 7, bw_khz: float = 125):
        """Add a transmission record and check compliance"""
        
        airtime = self.estimate_airtime(length_bytes, sf, bw_khz)
        
        record = TransmissionRecord(
            timestamp_ms=timestamp_ms,
            node_id=node_id,
            frequency_mhz=frequency_mhz,
            length_bytes=length_bytes,
            sf=sf,
            bw_khz=bw_khz,
            estimated_airtime_ms=airtime
        )
        
        # Update device stats
        stats = self.device_stats[node_id]
        stats['transmissions'].append(record)
        stats['total_airtime_ms'] += airtime
        
        # Track hourly airtime
        hour_key = timestamp_ms // 3600000  # milliseconds to hours
        stats['hourly_airtime'][hour_key] += airtime
        
        # Global stats
        self.total_transmissions += 1
        self.total_airtime_ms += airtime
        
        # Check for violations
        self._check_violations(node_id, hour_key)
    
    def _check_violations(self, node_id: str, hour_key: int):
        """Check if device exceeds duty cycle limits"""
        
        stats = self.device_stats[node_id]
        hourly_airtime = stats['hourly_airtime'][hour_key]
        
        # Check duty cycle limit
        if self.limits.get('max_airtime_per_hour_ms'):
            max_airtime = self.limits['max_airtime_per_hour_ms']
            if hourly_airtime > max_airtime:
                duty_cycle = (hourly_airtime / 3600000) * 100
                violation = {
                    'type': 'DUTY_CYCLE_EXCEEDED',
                    'node_id': node_id,
                    'hour': hour_key,
                    'airtime_ms': hourly_airtime,
                    'limit_ms': max_airtime,
                    'duty_cycle_pct': duty_cycle,
                    'severity': 'HIGH' if duty_cycle > 5 else 'MEDIUM',
                }
                
                # Avoid duplicate violations for same hour
                existing = [v for v in stats['violations'] 
                           if v['hour'] == hour_key and v['type'] == 'DUTY_CYCLE_EXCEEDED']
                if not existing:
                    stats['violations'].append(violation)
                    self.violations.append(violation)
    
    def load_csv(self, filepath: Path):
        """Load transmission data from CSV"""
        
        with open(filepath, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                try:
                    timestamp = int(row.get('timestamp_ms', row.get('timestamp', 0)))
                    node_id = row.get('node_id_hex', row.get('nodeId', 'Unknown'))
                    freq = float(row.get('frequency_mhz', row.get('frequency', 915.0)) or 915.0)
                    length = int(row.get('length_bytes', row.get('length', 20)) or 20)
                    
                    # Try to get SF/BW from config or use defaults
                    sf = 7  # Default
                    bw = 125.0  # Default
                    
                    self.add_transmission(node_id, timestamp, freq, length, sf, bw)
                except (ValueError, KeyError):
                    continue
    
    def get_device_report(self, node_id: str) -> Dict:
        """Get compliance report for a specific device"""
        
        stats = self.device_stats[node_id]
        
        # Calculate overall duty cycle
        if stats['transmissions']:
            first_ts = min(t.timestamp_ms for t in stats['transmissions'])
            last_ts = max(t.timestamp_ms for t in stats['transmissions'])
            duration_hours = max((last_ts - first_ts) / 3600000, 1/60)  # At least 1 minute
            avg_duty_cycle = (stats['total_airtime_ms'] / (duration_hours * 3600000)) * 100
        else:
            duration_hours = 0
            avg_duty_cycle = 0
        
        # Peak hour
        if stats['hourly_airtime']:
            peak_hour = max(stats['hourly_airtime'].items(), key=lambda x: x[1])
            peak_duty_cycle = (peak_hour[1] / 3600000) * 100
        else:
            peak_hour = (0, 0)
            peak_duty_cycle = 0
        
        return {
            'node_id': node_id,
            'total_transmissions': len(stats['transmissions']),
            'total_airtime_ms': stats['total_airtime_ms'],
            'duration_hours': duration_hours,
            'avg_duty_cycle_pct': avg_duty_cycle,
            'peak_duty_cycle_pct': peak_duty_cycle,
            'peak_hour_airtime_ms': peak_hour[1],
            'violations': stats['violations'],
            'compliant': len(stats['violations']) == 0,
        }
    
    def generate_report(self) -> str:
        """Generate full compliance report"""
        
        lines = []
        lines.append("=" * 70)
        lines.append("📡 DUTY CYCLE COMPLIANCE REPORT")
        lines.append("=" * 70)
        lines.append(f"\nRegion: {self.region} - {self.limits.get('description', '')}")
        
        if self.limits.get('duty_cycle_pct'):
            lines.append(f"Duty Cycle Limit: {self.limits['duty_cycle_pct']}%")
            lines.append(f"Max Airtime/Hour: {self.limits.get('max_airtime_per_hour_ms', 'N/A')} ms")
        else:
            lines.append("Duty Cycle Limit: No strict limit for this region")
        
        lines.append(f"\n📊 SUMMARY")
        lines.append("-" * 70)
        lines.append(f"Total Transmissions: {self.total_transmissions}")
        lines.append(f"Total Airtime: {self.total_airtime_ms:.1f} ms ({self.total_airtime_ms/1000:.2f} sec)")
        lines.append(f"Devices Monitored: {len(self.device_stats)}")
        lines.append(f"Total Violations: {len(self.violations)}")
        
        # Compliance status
        if self.violations:
            lines.append(f"\n⚠️  COMPLIANCE ISSUES DETECTED!")
        else:
            lines.append(f"\n✅ All devices within compliance limits")
        
        # Per-device breakdown
        lines.append(f"\n📱 DEVICE BREAKDOWN")
        lines.append("-" * 70)
        lines.append(f"{'Node ID':<18} {'TX Count':>8} {'Airtime':>10} {'Duty Cycle':>12} {'Status':>10}")
        lines.append("-" * 70)
        
        # Sort by airtime (highest first)
        sorted_devices = sorted(
            self.device_stats.keys(),
            key=lambda x: self.device_stats[x]['total_airtime_ms'],
            reverse=True
        )
        
        for node_id in sorted_devices[:20]:  # Top 20
            report = self.get_device_report(node_id)
            status = "⚠️ VIOLATION" if report['violations'] else "✅ OK"
            
            lines.append(f"{node_id:<18} {report['total_transmissions']:>8} "
                        f"{report['total_airtime_ms']:>8.1f}ms "
                        f"{report['avg_duty_cycle_pct']:>10.3f}% "
                        f"{status:>10}")
        
        if len(self.device_stats) > 20:
            lines.append(f"  ... and {len(self.device_stats) - 20} more devices")
        
        # Violations detail
        if self.violations:
            lines.append(f"\n🚨 VIOLATIONS")
            lines.append("-" * 70)
            
            for v in self.violations[:10]:
                lines.append(f"  [{v['severity']}] {v['node_id']}: "
                            f"Duty cycle {v['duty_cycle_pct']:.2f}% "
                            f"(limit: {self.limits.get('duty_cycle_pct', 'N/A')}%)")
        
        # Recommendations
        lines.append(f"\n💡 RECOMMENDATIONS")
        lines.append("-" * 70)
        
        if self.violations:
            lines.append("1. Review high-airtime devices for misconfiguration")
            lines.append("2. Increase spreading factor to reduce airtime")
            lines.append("3. Reduce packet sizes where possible")
            lines.append("4. Implement adaptive data rate (ADR)")
        else:
            lines.append("All devices operating within regulatory limits.")
            lines.append("Continue monitoring for sustained compliance.")
        
        return '\n'.join(lines)
    
    def export_json(self) -> str:
        """Export report as JSON"""
        
        return json.dumps({
            'region': self.region,
            'limits': self.limits,
            'summary': {
                'total_transmissions': self.total_transmissions,
                'total_airtime_ms': self.total_airtime_ms,
                'device_count': len(self.device_stats),
                'violation_count': len(self.violations),
            },
            'devices': {
                node_id: self.get_device_report(node_id)
                for node_id in self.device_stats.keys()
            },
            'violations': self.violations,
        }, indent=2)


def monitor_live(host: str, region: str, duration: int = None):
    """Monitor live from ESP32 API"""
    
    if not REQUESTS_AVAILABLE:
        print("Error: requests library required")
        return
    
    monitor = DutyCycleMonitor(region=region)
    
    print(f"Monitoring {host} for duty cycle compliance...")
    print(f"Region: {region}")
    if duration:
        print(f"Duration: {duration} seconds")
    print("Press Ctrl+C to stop\n")
    
    import time
    start = time.time()
    last_count = 0
    
    try:
        while True:
            if duration and (time.time() - start) > duration:
                break
            
            # Fetch current stats
            try:
                r = requests.get(f"http://{host}/api/statistics", timeout=5)
                stats = r.json()
                
                total = stats.get('totalPackets', 0)
                if total > last_count:
                    # New packets - would need packet data for accurate tracking
                    print(f"[{datetime.now().strftime('%H:%M:%S')}] "
                          f"Packets: {total}, Devices: {stats.get('deviceCount', '?')}")
                    last_count = total
            except Exception as e:
                print(f"Error fetching: {e}")
            
            time.sleep(5)
    
    except KeyboardInterrupt:
        print("\nStopped.")
    
    print(monitor.generate_report())


def main():
    parser = argparse.ArgumentParser(
        description='Monitor LoRa transmissions for duty cycle compliance',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Regions:
  EU868   European 868 MHz (1% duty cycle)
  US915   US 915 MHz (no strict limit)
  AU915   Australian 915 MHz
  AS923   Asia 923 MHz (1% duty cycle)

Examples:
  %(prog)s capture.csv                    Check compliance (default EU868)
  %(prog)s capture.csv --region US915     US region
  %(prog)s capture.csv --json             JSON output
  %(prog)s --live 192.168.4.1             Live monitoring
"""
    )
    
    parser.add_argument('input', nargs='?', help='Input CSV file')
    parser.add_argument('--region', '-r', default='EU868',
                       choices=list(REGION_LIMITS.keys()),
                       help='Regulatory region (default: EU868)')
    parser.add_argument('--live', metavar='HOST',
                       help='Monitor live from ESP32')
    parser.add_argument('--duration', type=int,
                       help='Duration for live monitoring (seconds)')
    parser.add_argument('--json', action='store_true',
                       help='Output as JSON')
    parser.add_argument('--threshold', type=float,
                       help='Custom duty cycle threshold (%%)')
    
    args = parser.parse_args()
    
    if args.live:
        monitor_live(args.live, args.region, args.duration)
        return 0
    
    if not args.input:
        parser.print_help()
        return 1
    
    filepath = Path(args.input)
    if not filepath.exists():
        print(f"Error: File not found: {filepath}")
        return 1
    
    # Override threshold if specified
    if args.threshold:
        REGION_LIMITS[args.region]['duty_cycle_pct'] = args.threshold
        REGION_LIMITS[args.region]['max_airtime_per_hour_ms'] = args.threshold * 36000
    
    monitor = DutyCycleMonitor(region=args.region)
    
    print(f"Loading {filepath}...")
    monitor.load_csv(filepath)
    print(f"Loaded {monitor.total_transmissions} transmissions")
    
    if args.json:
        print(monitor.export_json())
    else:
        print(monitor.generate_report())
    
    return 0 if not monitor.violations else 1


if __name__ == '__main__':
    sys.exit(main())
