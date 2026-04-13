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

import sys
from collections import defaultdict
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple

from tools.core import capture as capture_loader
from tools.core import cli, decode
from tools.core.models import Capture, CapturedPacket, Device

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

def _enrich_positions(packets: Iterable[CapturedPacket], nodes: Dict[str, "NodeTrack"]) -> int:
    """Decrypt Meshtastic POSITION packets and inject real node GPS into NodeTrack."""
    if not decode.CRYPTO_AVAILABLE:
        return 0
    found = 0
    enriched: set[str] = set()
    for p in packets:
        if p.protocol != 'Meshtastic' or not p.raw_hex:
            continue
        result = decode.try_decrypt(p.raw_bytes)
        if not result or result.portnum != decode.PORT_POSITION:
            continue
        inner = decode.extract_inner(result.plaintext)
        if not inner:
            continue
        pos = decode.parse_position(inner)
        if not pos:
            continue
        nid = p.node_id_hex
        if nid in nodes:
            if nid not in enriched:
                nodes[nid].positions = []
                enriched.add(nid)
            nodes[nid].positions.append(pos)
            found += 1
    return found


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
        self.positions: List[Tuple[float, float, float]] = []

    def add(self, p: CapturedPacket):
        self.packets += 1
        if p.protocol:
            self.protocol = p.protocol
        if p.rssi_dbm:
            self.rssi_vals.append(p.rssi_dbm)
        if p.lat_deg is not None and p.lon_deg is not None:
            if p.lat_deg != 0.0 or p.lon_deg != 0.0:
                self.positions.append((p.lat_deg, p.lon_deg, p.alt_m or 0.0))

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


def aggregate_nodes(cap: Capture) -> Dict[str, NodeTrack]:
    """Build per-node aggregates from a Capture, enriching GPS from POSITION packets if needed."""
    nodes: Dict[str, NodeTrack] = {}
    for p in cap:
        nid = p.node_id_hex or 'unknown'
        if nid not in nodes:
            nodes[nid] = NodeTrack(nid)
        nodes[nid].add(p)

    # Sniffer-GPS-only detection: if every node position is within ~200m of
    # every other, the CSV's lat/lon is the sniffer — decrypt Meshtastic
    # POSITION packets to recover real node GPS.
    positioned = [n for n in nodes.values() if n.best_position]
    if positioned:
        lats = [n.best_position[0] for n in positioned]
        lons = [n.best_position[1] for n in positioned]
        lat_spread = max(lats) - min(lats)
        lon_spread = max(lons) - min(lons)
        if lat_spread < 0.002 and lon_spread < 0.002:
            print(f"Sniffer-GPS-only data detected (all {len(positioned)} nodes within "
                  f"{lat_spread*111000:.0f}m × {lon_spread*111000:.0f}m).")
            print("Decrypting Meshtastic POSITION packets for real node locations...")
            found = _enrich_positions(cap, nodes)
            if found:
                enriched_nodes = sum(1 for n in nodes.values() if n.best_position)
                print(f"Decrypted {found} position packets → {enriched_nodes} nodes with real GPS")
            else:
                print("No Meshtastic POSITION packets decrypted — no real node GPS recoverable.")
                for n in nodes.values():
                    n.positions = []

    return nodes


def nodes_from_devices(devices: List[Device]) -> Dict[str, NodeTrack]:
    """Build NodeTrack dict from a pre-aggregated Device list (from /api/devices)."""
    nodes: Dict[str, NodeTrack] = {}
    for d in devices:
        nt = NodeTrack(d.node_id)
        nt.protocol = d.protocol
        nt.packets  = d.packet_count
        if d.rssi_dbm is not None:
            nt.rssi_vals = [d.rssi_dbm]
        if d.lat_deg is not None and d.lon_deg is not None:
            nt.positions.append((d.lat_deg, d.lon_deg, d.alt_m or 0.0))
        nodes[d.node_id] = nt
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
                   tiles='CartoDB positron')

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

        if len(node.positions) > 1:
            path_coords = [(lat, lon) for lat, lon, _ in node.positions]
            folium.PolyLine(path_coords, color=node.color,
                            weight=1.5, opacity=0.5).add_to(m)

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

    proto_groups: Dict[str, List] = defaultdict(list)
    for nid, node in positioned.items():
        proto_groups[node.protocol].append(node)

    for proto, group in proto_groups.items():
        lons = [n.best_position[1] for n in group]
        lats = [n.best_position[0] for n in group]
        sizes = [30 + n.packets * 2 for n in group]
        col_map = {'blue': '#4682b4', 'red': '#e74c3c', 'green': '#2ecc71',
                   'gray': '#888888', 'purple': '#9b59b6', 'lightgray': '#555555'}
        col = col_map.get(PROTOCOL_COLORS.get(proto, 'lightgray'), '#4682b4')
        ax.scatter(lons, lats, s=sizes, c=col, alpha=0.8, label=proto, zorder=5)

        for node in group:
            lat, lon, _ = node.best_position
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
    ax.legend(facecolor='#2a2a4e', labelcolor='white', fontsize=9)
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
    ap = cli.base_parser('Plot captured LoRa node GPS positions on an interactive map.')
    ap.add_argument('--min-packets', type=int, default=1, metavar='N',
                    help='Only show nodes with at least N packets (default: 1)')
    args = ap.parse_args()

    auto_open = not args.output
    suffix = '.html' if FOLIUM_AVAILABLE else '.png'

    if args.api:
        # /api/devices is aggregated, not per-packet — use load_devices, not
        # capture.load (which hits /api/replay/slots, limited to 8 packets).
        devices = capture_loader.load_devices(args.api)
        print(f"[API] {len(devices)} devices from {args.api}")
        nodes = nodes_from_devices(devices)
        out = args.output or cli.temp_output(suffix, stem=f"map_{args.api.replace('.', '_')}")
    else:
        if not args.input:
            sys.exit("ERROR: provide a capture file or --api HOST")
        p = Path(args.input)
        if not p.exists():
            sys.exit(f"File not found: {args.input}")
        cap = capture_loader.load(str(p))
        nodes = aggregate_nodes(cap)
        print(f"Loaded {len(nodes)} nodes from {p.name} ({len(cap)} packets)")
        out = args.output or cli.temp_output(suffix, stem=f'map_{p.stem}')

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

    # temp_output() pre-creates an empty file — only auto-open if something
    # actually got written to it.
    if auto_open and Path(out).stat().st_size > 0:
        print(f"Opening {out} in browser...")
        cli.open_in_browser(out)


if __name__ == '__main__':
    main()
