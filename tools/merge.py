#!/usr/bin/env python3
"""ESP32 LoRa Sniffer - Cross-Capture Identity Linker.

Merges multiple captures by node_id to reveal operators who appear across
sessions (e.g. Hotel + Conference + Airport). Bubble-sorts the list by
how many captures each node was seen in — the top is your most-travelled
/ most-surveilled operators.

Usage:
    python -m tools.merge a.csv b.csv [c.csv ...]
    python -m tools.merge a.csv b.csv --json out.json
    python -m tools.merge a.csv b.csv --min-captures 2
"""
from __future__ import annotations

import argparse
import json
import sys
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Optional, Set

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')

from tools.core import capture as capture_loader
from tools.core import decode
from tools.core.models import Capture, CapturedPacket


class NodeRecord:
    __slots__ = ('node_id', 'short_name', 'long_name', 'hw_model',
                 'captures', 'protocols', 'psk_names', 'positions', 'texts')

    def __init__(self, node_id: str):
        self.node_id   = node_id
        self.short_name: Optional[str] = None
        self.long_name:  Optional[str] = None
        self.hw_model:   Optional[str] = None
        # capture_name -> {packets, first_ts, last_ts}
        self.captures: Dict[str, Dict] = {}
        self.protocols: Set[str] = set()
        self.psk_names: Set[str] = set()
        self.positions: List = []
        self.texts:     List[str] = []


def _ingest(cap: Capture, records: Dict[str, NodeRecord]) -> None:
    name = cap.source
    for p in cap:
        nid = p.node_id_hex
        if not nid or nid == 'unknown':
            continue
        rec = records.setdefault(nid, NodeRecord(nid))
        c = rec.captures.setdefault(name, {'packets': 0, 'first_ts': None, 'last_ts': None})
        c['packets'] += 1
        if p.timestamp_ms:
            ts = p.timestamp_ms / 1000.0
            if c['first_ts'] is None or ts < c['first_ts']: c['first_ts'] = ts
            if c['last_ts']  is None or ts > c['last_ts']:  c['last_ts']  = ts
        if p.protocol and p.protocol != 'Unknown':
            rec.protocols.add(p.protocol)
        if p.psk_result and p.psk_result not in ('none', 'failed'):
            rec.psk_names.add(p.psk_result)
        if p.lat_deg is not None and p.lon_deg is not None:
            if p.lat_deg != 0.0 or p.lon_deg != 0.0:
                rec.positions.append((p.lat_deg, p.lon_deg))
        # Try NodeInfo / Text / Position decrypt for identity
        if p.encrypted and decode.CRYPTO_AVAILABLE and p.raw_hex:
            try:
                r = decode.try_decrypt(p.raw_bytes)
            except Exception:
                r = None
            if r:
                rec.psk_names.add(r.entry.name)
                if r.portnum == decode.PORT_NODEINFO:
                    inner = decode.extract_inner(r.plaintext)
                    if inner:
                        user = decode.parse_user(inner)
                        if user:
                            if user.get('short_name'): rec.short_name = user['short_name']
                            if user.get('long_name'):  rec.long_name  = user['long_name']
                            if user.get('hw_model_name'): rec.hw_model = user['hw_model_name']
                elif r.portnum == decode.PORT_POSITION:
                    inner = decode.extract_inner(r.plaintext)
                    if inner:
                        pos = decode.parse_position(inner)
                        if pos:
                            rec.positions.append((pos[0], pos[1]))
                elif r.portnum == decode.PORT_TEXT_MESSAGE:
                    inner = decode.extract_inner(r.plaintext)
                    if inner:
                        try:
                            t = inner.decode('utf-8', errors='replace').strip('\x00').strip()
                        except Exception:
                            t = ''
                        if t:
                            rec.texts.append(t)


def _fmt_ts(ts: Optional[float]) -> str:
    if not ts: return '—'
    return datetime.fromtimestamp(ts, tz=timezone.utc).strftime('%Y-%m-%d %H:%M')


def _span(c: Dict) -> str:
    if not c['first_ts'] or not c['last_ts']: return ''
    mins = (c['last_ts'] - c['first_ts']) / 60
    return f'{mins:.0f}m'


def _render_text(records: Dict[str, NodeRecord], capture_names: List[str],
                 min_captures: int) -> None:
    shown = [r for r in records.values() if len(r.captures) >= min_captures]
    shown.sort(key=lambda r: (-len(r.captures), -sum(c['packets'] for c in r.captures.values())))

    print()
    print(f"Captures merged: {len(capture_names)}")
    for n in capture_names:
        print(f"  - {n}")
    print(f"Total unique nodes: {len(records)}")
    print(f"Nodes in >= {min_captures} capture(s): {len(shown)}")
    print()

    if not shown:
        return

    # Compact per-capture presence grid.
    hdr = f"{'Node':<14} {'Name':<18} {'HW':<18} " + ' '.join(f'{i+1:>3}' for i in range(len(capture_names))) + '  Total  Protos / PSK'
    print(hdr)
    print('-' * len(hdr))
    for r in shown:
        name = (r.short_name or r.long_name or '')[:17]
        hw   = (r.hw_model or '')[:17]
        cells = []
        total = 0
        for cn in capture_names:
            c = r.captures.get(cn)
            if c:
                cells.append(f'{c["packets"]:>3}'); total += c['packets']
            else:
                cells.append('  .')
        protos = ','.join(sorted(r.protocols))[:24]
        psks   = ','.join(sorted(r.psk_names))[:40]
        tag    = f'{protos}  |  {psks}' if psks else protos
        print(f"{r.node_id:<14} {name:<18} {hw:<18} {' '.join(cells)}  {total:>5}  {tag}")

    print()
    print("Capture key:")
    for i, cn in enumerate(capture_names):
        print(f"  {i+1} = {cn}")


def _render_json(records: Dict[str, NodeRecord], capture_names: List[str],
                 out_path: str, min_captures: int) -> None:
    out = {
        'captures': capture_names,
        'nodes': [],
    }
    for r in records.values():
        if len(r.captures) < min_captures:
            continue
        out['nodes'].append({
            'node_id':     r.node_id,
            'short_name':  r.short_name,
            'long_name':   r.long_name,
            'hw_model':    r.hw_model,
            'protocols':   sorted(r.protocols),
            'psk_names':   sorted(r.psk_names),
            'captures':    {k: {**v,
                                'first_ts': _fmt_ts(v['first_ts']),
                                'last_ts':  _fmt_ts(v['last_ts'])}
                            for k, v in r.captures.items()},
            'positions':   list({(round(lat, 4), round(lon, 4)) for lat, lon in r.positions}),
            'text_sample': r.texts[:3],
        })
    out['nodes'].sort(key=lambda n: -len(n['captures']))
    Path(out_path).write_text(json.dumps(out, indent=2, default=list), encoding='utf-8')
    print(f"JSON written: {out_path}  ({len(out['nodes'])} nodes)")


def main():
    ap = argparse.ArgumentParser(description='Merge multiple captures by node_id to find cross-capture operators.')
    ap.add_argument('captures', nargs='+', help='Two or more capture files (CSV / PCAP)')
    ap.add_argument('--json', metavar='FILE', help='Also write a JSON report to FILE')
    ap.add_argument('--min-captures', type=int, default=2,
                    help='Only show nodes appearing in at least N captures (default: 2)')
    args = ap.parse_args()

    if len(args.captures) < 2:
        sys.exit("ERROR: need at least 2 captures to merge.")

    records: Dict[str, NodeRecord] = {}
    capture_names: List[str] = []
    for src in args.captures:
        p = Path(src)
        if not p.exists():
            sys.exit(f"File not found: {src}")
        print(f"Loading {p.name} ...")
        cap = capture_loader.load(str(p))
        capture_names.append(cap.source)
        _ingest(cap, records)

    _render_text(records, capture_names, args.min_captures)
    if args.json:
        _render_json(records, capture_names, args.json, args.min_captures)


if __name__ == '__main__':
    sys.exit(main() or 0)
