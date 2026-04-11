#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - Geographic Map

Plots captured node positions on an interactive map.
The webapp shows a node connectivity diagram — this tool shows real geography.

Nodes with GPS coordinates (lat_deg / lon_deg in the CSV) are plotted as
circle markers. Click a marker to see node ID, protocol, packet count, RSSI.

Output: interactive HTML file (opens in any browser).
Fallback: static PNG via matplotlib if folium is not installed.

Usage:
    python map.py capture.csv
    python map.py capture.csv -o field_map.html
    python map.py --api 192.168.4.1

Requirements:
    pip install folium            (interactive HTML map — recommended)
    pip install matplotlib        (static PNG fallback)
    pip install requests          (for --api mode)
"""

import argparse
import base64
import csv
import struct
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Optional, Tuple

try:
    import folium
    from folium.plugins import MarkerCluster
    FOLIUM_AVAILABLE = True
except ImportError:
    FOLIUM_AVAILABLE = False

try:
    import matplotlib.pyplot as plt
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False

try:
    import requests
    REQUESTS_AVAILABLE = True
except ImportError:
    REQUESTS_AVAILABLE = False

try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    from cryptography.hazmat.backends import default_backend
    CRYPTO_AVAILABLE = True
except ImportError:
    CRYPTO_AVAILABLE = False

# ---------------------------------------------------------------------------
# Meshtastic PSK decryption + Position protobuf parser
# ---------------------------------------------------------------------------

# Keys tried in order: private/weak first, public channels last.
_PSKS = [
    "PKdTs51e4EB0BoOevIN0Dw==",  # Admin Channel (pre-2.5)
    "AAAAAAAAAAAAAAAAAAAAAA==",  # All Zeros
    "MTIzNDU2Nzg5MDEyMzQ1Ng==",  # 1234567890123456
    "d1iq21lNSh7BP6MOkP6cQA==",  # MediumFast
    "/u7k03L8N3Q=",              # ShortFast
    "GGC5DDnv8FKFm7WCZ5rXBA==",  # LongSlow
    "LHrwq5nxPIJlqFU/K5dKKQ==",  # MediumSlow
    "sb6GxC62sdwGXxJz2sERuQ==",  # ShortSlow
    "ZQ+HdKKbbAU4dSCGt66Qqw==",  # EU868 Regional
    "dGVzdHRlc3R0ZXN0dGVzdA==",  # testtesttesttest
    "shmLkA9H74gAeLH3eGCqsw==",  # Secondary Default
    "ogDPnKVRN7wz/VF8nt6LkA==",  # Debug Key
    "AQ==",                       # Default (0x01)  — public channel
    "1PG7OiApB1nwvP+rz05pAQ==",  # LongFast Preset — public channel
]

def _expand_psk(b64: str) -> bytes:
    k = base64.b64decode(b64)
    n = len(k)
    if n == 1:  return k * 16
    if n == 8:  return k + k
    if n in (16, 32): return k
    return (k + b'\x00' * (16 - n)) if n < 16 else k[:32]

_COMPILED_PSKS = [_expand_psk(b) for b in _PSKS]


def _decode_varint(data: bytes, offset: int) -> Tuple[int, int]:
    result, shift, consumed = 0, 0, 0
    while offset + consumed < len(data):
        b = data[offset + consumed]; consumed += 1
        result |= (b & 0x7F) << shift; shift += 7
        if not (b & 0x80): break
    return result, consumed


def _try_decrypt(raw: bytes) -> Optional[bytes]:
    """
    Try all PSKs against a raw Meshtastic packet.
    Returns decrypted Data protobuf bytes if a key works, else None.
    Validity check: first byte must be 0x08 (field 1 varint) and portnum in 1–127.
    """
    if not CRYPTO_AVAILABLE or len(raw) < 20:
        return None
    src_int = struct.unpack('<I', raw[4:8])[0]
    pid     = struct.unpack('<I', raw[8:12])[0]
    payload = raw[16:]
    if not payload:
        return None
    nonce = struct.pack('<IIII', pid, 0, src_int, 0)
    for key in _COMPILED_PSKS:
        try:
            pt = Cipher(algorithms.AES(key), modes.CTR(nonce),
                        backend=default_backend()).decryptor().update(payload)
            if not pt or pt[0] != 0x08:
                continue
            portnum = pt[1] if len(pt) > 1 else 0
            if portnum == 0 or portnum > 127:
                continue
            return pt
        except Exception:
            pass
    return None


def _parse_position(inner: bytes) -> Optional[Tuple[float, float, float]]:
    """
    Parse a Meshtastic Position protobuf.
    Returns (lat_deg, lon_deg, alt_m) or None.
    Position fields: lat_i=1 sfixed32, lon_i=2 sfixed32, alt=3 int32.
    """
    lat_i = lon_i = alt = None
    offset = 0
    while offset < len(inner):
        tag, c = _decode_varint(inner, offset); offset += c
        fn, wt = tag >> 3, tag & 0x07
        if fn == 1 and wt == 5 and offset + 4 <= len(inner):
            lat_i = struct.unpack('<i', inner[offset:offset+4])[0]; offset += 4
        elif fn == 2 and wt == 5 and offset + 4 <= len(inner):
            lon_i = struct.unpack('<i', inner[offset:offset+4])[0]; offset += 4
        elif fn == 3 and wt == 0:
            alt, c = _decode_varint(inner, offset); offset += c
        elif wt == 0:
            _, c = _decode_varint(inner, offset); offset += c
        elif wt == 2:
            l, c = _decode_varint(inner, offset); offset += c + l
        elif wt == 5:
            offset += 4
        elif wt == 1:
            offset += 8
        else:
            break
    if lat_i is not None and lon_i is not None:
        lat = lat_i / 1e7
        lon = lon_i / 1e7
        if lat != 0.0 or lon != 0.0:
            return (lat, lon, float(alt or 0))
    return None


def _extract_inner(pt: bytes) -> Optional[bytes]:
    """Extract field 2 (payload bytes) from a decrypted Data protobuf."""
    offset = 0
    while offset < len(pt):
        tag, c = _decode_varint(pt, offset); offset += c
        fn, wt = tag >> 3, tag & 0x07
        if fn == 2 and wt == 2:
            l, c = _decode_varint(pt, offset); offset += c
            return pt[offset:offset + l]
        if wt == 0:
            _, c = _decode_varint(pt, offset); offset += c
        elif wt == 2:
            l, c = _decode_varint(pt, offset); offset += c + l
        elif wt == 5:
            offset += 4
        elif wt == 1:
            offset += 8
        else:
            break
    return None


PORT_POSITION = 3


def _enrich_positions(filepath: str, nodes: Dict) -> int:
    """
    Second pass over a CSV: decrypt Meshtastic packets, find portnum=3 (POSITION),
    parse the inner Position proto, and inject real node GPS into NodeTrack objects.
    Returns count of position packets successfully decrypted.
    """
    if not CRYPTO_AVAILABLE:
        return 0
    found = 0
    # Track which nodes got real positions so we can clear sniffer positions after.
    enriched = set()
    with open(filepath, 'r', encoding='utf-8') as fh:
        for row in csv.DictReader(fh):
            if row.get('protocol') != 'Meshtastic':
                continue
            raw_hex = row.get('raw_hex', '')
            if not raw_hex:
                continue
            try:
                raw = bytes.fromhex(raw_hex)
            except ValueError:
                continue
            pt = _try_decrypt(raw)
            if not pt:
                continue
            portnum = pt[1] if len(pt) > 1 else 0
            if portnum != PORT_POSITION:
                continue
            inner = _extract_inner(pt)
            if not inner:
                continue
            pos = _parse_position(inner)
            if not pos:
                continue
            nid = row.get('node_id_hex', '') or 'unknown'
            if nid in nodes:
                if nid not in enriched:
                    # First real position for this node: clear sniffer positions
                    nodes[nid].positions = []
                    enriched.add(nid)
                nodes[nid].positions.append(pos)
                found += 1
    return found

# Protocol → marker color (folium colors)
PROTOCOL_COLORS = {
    'Meshtastic': 'blue',
    'MeshCore':   'red',
    'LoRaWAN':    'green',
    'ISM':        'gray',
    'Helium':     'purple',
    'Unknown':    'lightgray',
}


class NodeTrack:
    """Aggregated info for one node across all its packets."""
    def __init__(self, node_id: str):
        self.node_id   = node_id
        self.protocol  = 'Unknown'
        self.packets   = 0
        self.rssi_vals: List[float] = []
        self.positions: List[Tuple[float, float, float]] = []  # lat, lon, alt

    def add(self, row: Dict):
        self.packets += 1
        proto = row.get('protocol', '')
        if proto:
            self.protocol = proto
        try:
            rssi = float(row.get('rssi_dbm', '') or '')
            self.rssi_vals.append(rssi)
        except (ValueError, TypeError):
            pass
        try:
            lat = float(row.get('lat_deg', '') or '')
            lon = float(row.get('lon_deg', '') or '')
            alt = float(row.get('alt_m', '') or 0)
            if lat != 0.0 or lon != 0.0:
                self.positions.append((lat, lon, alt))
        except (ValueError, TypeError):
            pass

    @property
    def best_position(self) -> Optional[Tuple[float, float, float]]:
        return self.positions[-1] if self.positions else None

    @property
    def avg_rssi(self) -> Optional[float]:
        if not self.rssi_vals:
            return None
        return sum(self.rssi_vals) / len(self.rssi_vals)

    @property
    def color(self) -> str:
        for key, col in PROTOCOL_COLORS.items():
            if key in self.protocol:
                return col
        return 'lightgray'

    def popup_html(self) -> str:
        rssi_str = f"{self.avg_rssi:.1f} dBm" if self.avg_rssi is not None else "—"
        pos = self.best_position
        alt_str = f"{pos[2]:.0f} m" if pos and pos[2] else "—"
        lat_str = f"{pos[0]:.6f}" if pos else "—"
        lon_str = f"{pos[1]:.6f}" if pos else "—"
        return (
            f"<b>{self.node_id}</b><br>"
            f"Protocol: {self.protocol}<br>"
            f"Packets: {self.packets}<br>"
            f"Avg RSSI: {rssi_str}<br>"
            f"Lat: {lat_str}  Lon: {lon_str}<br>"
            f"Altitude: {alt_str}<br>"
            f"Position fixes: {len(self.positions)}"
        )


def load_csv(filepath: str) -> Dict[str, NodeTrack]:
    nodes: Dict[str, NodeTrack] = {}
    with open(filepath, 'r', encoding='utf-8') as fh:
        for row in csv.DictReader(fh):
            nid = row.get('node_id_hex', '') or 'unknown'
            if nid not in nodes:
                nodes[nid] = NodeTrack(nid)
            nodes[nid].add(row)

    # Detect sniffer-GPS-only data: all node positions cluster tightly together
    # (within ~200m), meaning the CSV's lat/lon is the sniffer location, not the nodes'.
    positioned = [n for n in nodes.values() if n.best_position]
    if positioned:
        lats = [n.best_position[0] for n in positioned]
        lons = [n.best_position[1] for n in positioned]
        lat_spread = max(lats) - min(lats)
        lon_spread = max(lons) - min(lons)
        # ~0.002 degrees ≈ 200m — any real deployment spans more than this
        if lat_spread < 0.002 and lon_spread < 0.002:
            print(f"Sniffer-GPS-only data detected (all {len(positioned)} nodes within "
                  f"{lat_spread*111000:.0f}m × {lon_spread*111000:.0f}m).")
            print("Decrypting Meshtastic POSITION packets for real node locations...")
            found = _enrich_positions(filepath, nodes)
            if found:
                enriched_nodes = sum(1 for n in nodes.values() if n.best_position)
                print(f"Decrypted {found} position packets → {enriched_nodes} nodes with real GPS")
            else:
                print("No Meshtastic POSITION packets decrypted. Map cannot be rendered.")
                for n in nodes.values():
                    n.positions = []

    return nodes


def load_api(host: str) -> Dict[str, NodeTrack]:
    if not REQUESTS_AVAILABLE:
        sys.exit("ERROR: pip install requests")
    nodes: Dict[str, NodeTrack] = {}
    try:
        r = requests.get(f"http://{host}/api/devices", timeout=10)
        for d in r.json().get('devices', []):
            nid = d.get('nodeId', 'unknown')
            nt = NodeTrack(nid)
            nt.protocol = d.get('protocol', 'Unknown')
            nt.packets  = d.get('packetCount', 0)
            if d.get('rssi'):
                nt.rssi_vals = [d['rssi']]
            lat = d.get('lat') or d.get('latitude')
            lon = d.get('lon') or d.get('longitude')
            if lat and lon:
                nt.positions.append((float(lat), float(lon), 0.0))
            nodes[nid] = nt
        print(f"[API] {len(nodes)} devices from {host}")
    except Exception as e:
        sys.exit(f"[API] Error: {e}")
    return nodes


def render_folium(nodes: Dict[str, NodeTrack], output: str):
    positioned = {nid: n for nid, n in nodes.items() if n.best_position}
    if not positioned:
        print("No GPS positions available. Cannot render map.")
        return

    lats = [n.best_position[0] for n in positioned.values()]
    lons = [n.best_position[1] for n in positioned.values()]
    center = (sum(lats) / len(lats), sum(lons) / len(lons))

    m = folium.Map(location=center, zoom_start=14,
                   tiles='CartoDB dark_matter')

    cluster = MarkerCluster(name='LoRa Nodes').add_to(m)

    for nid, node in positioned.items():
        lat, lon, _ = node.best_position
        folium.CircleMarker(
            location=(lat, lon),
            radius=8,
            color=node.color,
            fill=True,
            fill_color=node.color,
            fill_opacity=0.75,
            popup=folium.Popup(node.popup_html(), max_width=240),
            tooltip=f"{nid[-8:]} ({node.protocol})",
        ).add_to(cluster)

        # Draw path if node moved significantly
        if len(node.positions) > 1:
            path_coords = [(lat, lon) for lat, lon, _ in node.positions]
            folium.PolyLine(path_coords, color=node.color,
                            weight=1.5, opacity=0.5).add_to(m)

    # Protocol legend (as a simple HTML layer)
    proto_counts: Dict[str, int] = defaultdict(int)
    for node in positioned.values():
        proto_counts[node.protocol] += 1

    legend_html = (
        '<div style="position:fixed;bottom:30px;left:30px;z-index:1000;'
        'background:#1a1a2e;color:white;padding:10px 16px;'
        'border-radius:8px;font-size:13px;border:1px solid #444;">'
        f'<b>ESP32 LoRa Sniffer</b><br>'
        f'<small>{len(positioned)} nodes with GPS / {len(nodes)} total</small><br><br>'
    )
    for proto, count in sorted(proto_counts.items(), key=lambda x: -x[1]):
        col = PROTOCOL_COLORS.get(proto, 'lightgray')
        legend_html += (
            f'<span style="display:inline-block;width:12px;height:12px;'
            f'border-radius:50%;background:{col};margin-right:6px;"></span>'
            f'{proto}: {count}<br>'
        )
    legend_html += '</div>'
    m.get_root().html.add_child(folium.Element(legend_html))

    folium.LayerControl().add_to(m)
    m.save(output)
    print(f"Map saved: {output}  ({len(positioned)} nodes plotted)")
    print(f"Open in browser: {Path(output).resolve().as_uri()}")


def render_matplotlib(nodes: Dict[str, NodeTrack], output: str):
    positioned = {nid: n for nid, n in nodes.items() if n.best_position}
    if not positioned:
        print("No GPS positions in data.")
        return

    fig, ax = plt.subplots(figsize=(12, 8))
    ax.set_facecolor('#1a1a2e')
    fig.patch.set_facecolor('#1a1a2e')

    # Group by protocol for legend
    proto_groups: Dict[str, List] = defaultdict(list)
    for nid, node in positioned.items():
        proto_groups[node.protocol].append(node)

    for proto, group in proto_groups.items():
        lons = [n.best_position[1] for n in group]
        lats = [n.best_position[0] for n in group]
        sizes = [30 + n.packets * 2 for n in group]
        # Map folium color name to matplotlib color
        col_map = {'blue': '#4682b4', 'red': '#e74c3c', 'green': '#2ecc71',
                   'gray': '#888888', 'purple': '#9b59b6', 'lightgray': '#555555'}
        col = col_map.get(PROTOCOL_COLORS.get(proto, 'lightgray'), '#4682b4')
        ax.scatter(lons, lats, s=sizes, c=col, alpha=0.8, label=proto, zorder=5)

        for node in group:
            _, lon, _ = node.best_position  # using lon as x
            lat, _, _ = node.best_position
            ax.annotate(node.node_id[-6:], (lon, lat),
                        fontsize=6, color='white', alpha=0.7,
                        xytext=(3, 3), textcoords='offset points')

    ax.set_xlabel('Longitude', color='white')
    ax.set_ylabel('Latitude', color='white')
    ax.set_title(f'LoRa Node Positions  —  {len(positioned)} nodes with GPS',
                 color='white', fontsize=13)
    ax.tick_params(colors='white')
    for spine in ax.spines.values():
        spine.set_edgecolor('#444444')
    legend = ax.legend(facecolor='#2a2a4e', labelcolor='white', fontsize=9)
    plt.tight_layout()

    if output.endswith('.html'):
        output = output.replace('.html', '.png')
    plt.savefig(output, dpi=150, bbox_inches='tight', facecolor=fig.get_facecolor())
    print(f"Map saved: {output}")
    plt.close(fig)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description='Plot captured LoRa node GPS positions on an interactive map.')
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument('file', nargs='?', help='CSV capture file')
    src.add_argument('--api', metavar='HOST', help='Pull live data from ESP32 API')
    ap.add_argument('-o', '--output', metavar='FILE',
                    help='Output file (.html for interactive, .png for static)')
    ap.add_argument('--min-packets', type=int, default=1, metavar='N',
                    help='Only show nodes with at least N packets (default: 1)')
    args = ap.parse_args()

    if args.api:
        nodes = load_api(args.api)
        out = args.output or f"map_{args.api.replace('.', '_')}.html"
    else:
        p = Path(args.file)
        if not p.exists():
            sys.exit(f"File not found: {args.file}")
        nodes = load_csv(str(p))
        print(f"Loaded {len(nodes)} nodes from {p.name}")
        out = args.output or (p.stem + '_map.html')

    if args.min_packets > 1:
        before = len(nodes)
        nodes = {nid: n for nid, n in nodes.items() if n.packets >= args.min_packets}
        print(f"Filtered to {len(nodes)} nodes with >= {args.min_packets} packets (dropped {before - len(nodes)})")

    positioned = sum(1 for n in nodes.values() if n.best_position)
    print(f"Nodes with GPS: {positioned} / {len(nodes)}")

    if FOLIUM_AVAILABLE and not out.endswith('.png'):
        render_folium(nodes, out)
    elif MATPLOTLIB_AVAILABLE:
        if FOLIUM_AVAILABLE is False:
            print("Tip: pip install folium  for interactive HTML maps")
        render_matplotlib(nodes, out)
    else:
        sys.exit("ERROR: pip install folium  or  pip install matplotlib")


if __name__ == '__main__':
    main()
