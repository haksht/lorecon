#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - Mesh Topology Visualizer

Builds a graph of LoRa mesh network topology from captured data.
Supports Meshtastic (neighborinfo / traceroute) and MeshCore (path arrays).
Outputs a PNG image of the network graph.

Usage:
    python topology.py capture.csv
    python topology.py capture.csv -o graph.png
    python topology.py capture.pcap
    python topology.py --api 192.168.4.1

Requirements:
    pip install networkx matplotlib
    pip install cryptography   (for Meshtastic PSK decryption)
"""

import argparse
import base64
import csv
import hashlib
import hmac as _hmac
import struct
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Optional, Tuple

try:
    import networkx as nx
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    RENDER_AVAILABLE = True
except ImportError:
    RENDER_AVAILABLE = False

try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    from cryptography.hazmat.backends import default_backend
    CRYPTO_AVAILABLE = True
except ImportError:
    CRYPTO_AVAILABLE = False

try:
    import requests
    REQUESTS_AVAILABLE = True
except ImportError:
    REQUESTS_AVAILABLE = False

# ---------------------------------------------------------------------------
# Meshtastic PSK decryption
# ---------------------------------------------------------------------------

DEFAULT_PSKS = [
    "AQ==", "1PG7OiApB1nwvP+rz05pAQ==", "Ag==", "Aw==", "BA==", "BQ==",
    "Bg==", "Bw==", "CA==", "CQ==", "AAAAAAAAAAAAAAAAAAAAAA==",
    "MTIzNDU2Nzg5MDEyMzQ1Ng==", "dGVzdHRlc3R0ZXN0dGVzdA==",
    "bWVzaHRhc3RpY21lc2h0YXN0", "PKdTs51e4EB0BoOevIN0Dw==",
    "shmLkA9H74gAeLH3eGCqsw==", "ogDPnKVRN7wz/VF8nt6LkA==",
    "ZQ+HdKKbbAU4dSCGt66Qqw==", "d1iq21lNSh7BP6MOkP6cQA==",
    "/u7k03L8N3Q=", "ZiETcjFHbk+ygALDp8tB3g==",
    "LiDWCBX5RWCGy3T9dKiTpw==", "8LQW/RhCEWKyV9HfSF3osA==",
]

def _expand_psk(b64_key: str) -> bytes:
    key = base64.b64decode(b64_key)
    n = len(key)
    if n == 1:
        return key * 16          # single byte repeated
    if n == 8:
        return key + key         # doubled to 16 bytes
    if n == 16 or n == 32:
        return key
    if n < 16:
        return key + b'\x00' * (16 - n)
    return key[:32]


def _build_nonce(packet_id: int, node_id_int: int) -> bytes:
    # Meshtastic nonce: packet_id at [0:4], zeros at [4:8], node_id at [8:12], zeros at [12:16]
    return struct.pack('<IIII', packet_id, 0, node_id_int, 0)


def _meshtastic_try_decrypt(header: Dict, payload: bytes) -> Optional[Tuple[str, bytes]]:
    """Try all known PSKs. Returns (key_name, plaintext) or None."""
    if not CRYPTO_AVAILABLE or not payload:
        return None
    packet_id = header['packet_id']
    node_id_int = int(header['node_id'][1:], 16)
    nonce = _build_nonce(packet_id, node_id_int)
    for b64 in DEFAULT_PSKS:
        key = _expand_psk(b64)
        try:
            cipher = Cipher(algorithms.AES(key), modes.CTR(nonce), backend=default_backend())
            dec = cipher.decryptor()
            pt = dec.update(payload) + dec.finalize()
            # Validity: first tag must be field 1 (portnum) varint, value in Meshtastic range
            if not pt or pt[0] != 0x08:  # field 1, wire type 0 (varint)
                continue
            portnum = pt[1] if len(pt) > 1 else 0
            if portnum == 0 or portnum > 512:
                continue
            return (b64, pt)
        except Exception:
            continue
    return None


# ---------------------------------------------------------------------------
# Meshtastic protobuf helpers (no protobuf dependency)
# ---------------------------------------------------------------------------

def _decode_varint(data: bytes, offset: int) -> Tuple[int, int]:
    result, shift, consumed = 0, 0, 0
    while offset + consumed < len(data):
        b = data[offset + consumed]
        consumed += 1
        result |= (b & 0x7F) << shift
        shift += 7
        if not (b & 0x80):
            break
    return result, consumed


def _parse_meshtastic_header(raw: bytes) -> Optional[Dict]:
    if len(raw) < 16:
        return None
    dest = struct.unpack('<I', raw[0:4])[0]
    src  = struct.unpack('<I', raw[4:8])[0]
    pid  = struct.unpack('<I', raw[8:12])[0]
    if src == 0:
        return None
    return {
        'dest':      f"!{dest:08x}",
        'node_id':   f"!{src:08x}",
        'packet_id': pid,
        'hop_limit': raw[12] & 0x07,
        'payload':   raw[16:],
    }


def _parse_data_portnum(payload: bytes) -> Optional[int]:
    """Extract portnum (field 1) from decrypted Data protobuf."""
    offset = 0
    while offset < len(payload):
        tag, c = _decode_varint(payload, offset); offset += c
        fn, wt = tag >> 3, tag & 0x07
        if fn == 1 and wt == 0:
            v, c = _decode_varint(payload, offset); return v
        if wt == 0:
            _, c = _decode_varint(payload, offset); offset += c
        elif wt == 2:
            l, c = _decode_varint(payload, offset); offset += c + l
        elif wt == 5:
            offset += 4
        elif wt == 1:
            offset += 8
        else:
            break
    return None


def _parse_inner_payload(payload: bytes) -> Optional[bytes]:
    """Extract inner payload bytes (field 2) from decrypted Data protobuf."""
    offset = 0
    while offset < len(payload):
        tag, c = _decode_varint(payload, offset); offset += c
        fn, wt = tag >> 3, tag & 0x07
        if fn == 2 and wt == 2:
            l, c = _decode_varint(payload, offset); offset += c
            return payload[offset:offset + l]
        if wt == 0:
            _, c = _decode_varint(payload, offset); offset += c
        elif wt == 2:
            l, c = _decode_varint(payload, offset); offset += c + l
        elif wt == 5:
            offset += 4
        elif wt == 1:
            offset += 8
        else:
            break
    return None


def _parse_traceroute(inner: bytes) -> List[str]:
    """
    RouteDiscovery.route: packed repeated uint32 (field 1, wire type 2).
    The payload bytes are raw 4-byte little-endian node IDs concatenated.
    """
    route, offset = [], 0
    while offset < len(inner):
        tag, c = _decode_varint(inner, offset); offset += c
        fn, wt = tag >> 3, tag & 0x07
        if fn == 1 and wt == 2:  # packed bytes
            l, c = _decode_varint(inner, offset); offset += c
            # Each 4 bytes is one node ID (little-endian uint32)
            for i in range(0, l, 4):
                if offset + 4 <= len(inner):
                    nid = struct.unpack('<I', inner[offset:offset+4])[0]
                    route.append(f"!{nid:08x}")
                    offset += 4
        elif wt == 0:
            _, c = _decode_varint(inner, offset); offset += c
        elif wt == 2:
            l, c = _decode_varint(inner, offset); offset += c + l
        elif wt == 5:
            offset += 4
        else:
            break
    return route


def _parse_neighborinfo(inner: bytes) -> List[Dict]:
    """
    NeighborInfo wrapper → repeated Neighbor messages (field 4).
    Neighbor sub-message: node_id = field 1 varint (wire 0), snr = field 2 float (wire 5).
    """
    neighbors, offset = [], 0
    while offset < len(inner):
        tag, c = _decode_varint(inner, offset); offset += c
        fn, wt = tag >> 3, tag & 0x07
        if fn == 4 and wt == 2:
            l, c = _decode_varint(inner, offset); offset += c
            sub = inner[offset:offset + l]; offset += l
            nb = {'node_id': None, 'snr': None}
            si = 0
            while si < len(sub):
                st, sc = _decode_varint(sub, si); si += sc
                sf, sw = st >> 3, st & 0x07
                if sf == 1 and sw == 0:          # node_id: varint
                    v, sc = _decode_varint(sub, si); si += sc
                    nb['node_id'] = f"!{v:08x}"
                elif sf == 2 and sw == 5 and si + 4 <= len(sub):  # SNR: float32
                    nb['snr'] = struct.unpack('<f', sub[si:si+4])[0]
                    si += 4
                elif sw == 0:
                    _, sc = _decode_varint(sub, si); si += sc
                elif sw == 2:
                    ll, sc = _decode_varint(sub, si); si += sc + ll
                elif sw == 5:
                    si += 4
                else:
                    break
            if nb['node_id']:
                neighbors.append(nb)
        elif wt == 0:
            _, c = _decode_varint(inner, offset); offset += c
        elif wt == 2:
            l, c = _decode_varint(inner, offset); offset += c + l
        elif wt == 5:
            offset += 4
        else:
            break
    return neighbors


PORT_TRACEROUTE   = 70
PORT_NEIGHBORINFO = 71

# ---------------------------------------------------------------------------
# MeshCore packet parsing
# ---------------------------------------------------------------------------

MC_PUBLIC_KEY_B64 = "izOH6cXN6mrJ5e26oRXNcg=="
MC_HASHTAG_ROOMS  = ["#general", "#emergency", "#local", "#mesh", "#public",
                     "#chat", "#help", "#info", "#sos", "#weather"]

def _mc_keys():
    keys = [(base64.b64decode(MC_PUBLIC_KEY_B64), "public")]
    for room in MC_HASHTAG_ROOMS:
        keys.append((hashlib.sha256(room.encode()).digest()[:16], room))
    return keys

MC_ALL_KEYS = _mc_keys()


def _parse_meshcore_packet(data: bytes) -> Optional[Dict]:
    """Parse a MeshCore packet, return path list or None."""
    if len(data) < 3:
        return None
    hdr          = data[0]
    version      = (hdr >> 6) & 0x03
    payload_type = (hdr >> 2) & 0x0F
    route_type   = hdr & 0x03
    if version > 1 or payload_type > 11:
        return None
    path_len_offset = 5 if route_type in (2, 3) else 1
    if path_len_offset + 1 > len(data):
        return None
    pl_byte  = data[path_len_offset]
    hop_count = pl_byte & 0x3F
    hash_size = ((pl_byte >> 6) & 0x03) + 1
    path_start = path_len_offset + 1
    path_end   = path_start + hop_count * hash_size
    if path_end > len(data):
        return None
    path = []
    for i in range(hop_count):
        off = path_start + i * hash_size
        path.append(data[off:off + hash_size].hex())
    return {'path': path, 'payload_type': payload_type, 'route_type': route_type}


# ---------------------------------------------------------------------------
# PCAP reader
# ---------------------------------------------------------------------------

LORA_HEADER_FMT  = '<fffBIBH'
LORA_HEADER_SIZE = struct.calcsize(LORA_HEADER_FMT)

def _read_pcap(filepath: str):
    """Yield (freq_mhz, rssi, snr, raw_payload_bytes) per packet."""
    try:
        with open(filepath, 'rb') as fh:
            magic = struct.unpack('<I', fh.read(4))[0]
            if magic not in (0xa1b2c3d4, 0xa1b23c4d):
                return
            fh.read(20)  # skip rest of global header
            while True:
                rec = fh.read(16)
                if not rec or len(rec) < 16:
                    break
                _ts_s, _ts_us, cap_len, _ = struct.unpack('<IIII', rec)
                pkt = fh.read(cap_len)
                if len(pkt) < LORA_HEADER_SIZE:
                    continue
                freq, rssi, snr, sf, bw, cr, _ = struct.unpack(LORA_HEADER_FMT, pkt[:LORA_HEADER_SIZE])
                yield freq, rssi, snr, pkt[LORA_HEADER_SIZE:]
    except Exception:
        return


# ---------------------------------------------------------------------------
# Graph builder
# ---------------------------------------------------------------------------

class TopologyGraph:
    def __init__(self):
        # nodes: id -> {label, color, packets, is_router}
        self.nodes: Dict[str, Dict] = {}
        # edges: (a,b) -> {weight, snr_sum, snr_count, directed}
        self.edges: Dict[Tuple, Dict] = {}
        self.protocol = 'Unknown'

    def _node(self, nid: str, **attrs):
        if nid not in self.nodes:
            self.nodes[nid] = {'label': nid[-6:], 'color': '#4682b4',
                               'packets': 0, 'is_router': False}
        self.nodes[nid].update(attrs)

    def _edge(self, a: str, b: str, snr: Optional[float] = None, directed=False):
        key = (a, b) if directed else tuple(sorted([a, b]))
        if key not in self.edges:
            self.edges[key] = {'weight': 0, 'snr_sum': 0.0, 'snr_count': 0, 'directed': directed}
        self.edges[key]['weight'] += 1
        if snr is not None:
            self.edges[key]['snr_sum'] += snr
            self.edges[key]['snr_count'] += 1

    def load_csv(self, filepath: str, decrypt: bool = True):
        rows_by_protocol: Dict[str, List] = defaultdict(list)
        with open(filepath, 'r', encoding='utf-8') as fh:
            for row in csv.DictReader(fh):
                proto = row.get('protocol', '')
                rows_by_protocol[proto].append(row)

        for proto, rows in rows_by_protocol.items():
            if 'MeshCore' in proto or 'meshcore' in proto.lower():
                self._load_meshcore_rows(rows)
            elif 'Meshtastic' in proto or 'meshtastic' in proto.lower():
                self._load_meshtastic_rows(rows, decrypt)

        if self.nodes:
            protocols = list(rows_by_protocol.keys())
            self.protocol = protocols[0] if len(protocols) == 1 else 'Mixed'

    def _load_meshcore_rows(self, rows: List):
        for row in rows:
            nid = row.get('node_id_hex', '') or ''
            if not nid:
                continue
            self._node(nid, packets=self.nodes.get(nid, {}).get('packets', 0) + 1,
                       is_router=row.get('is_router', '0') == '1')

            raw_hex = row.get('raw_hex', '')
            if not raw_hex:
                continue
            try:
                raw = bytes.fromhex(raw_hex)
            except ValueError:
                continue

            parsed = _parse_meshcore_packet(raw)
            if not parsed or len(parsed['path']) < 2:
                continue

            path = parsed['path']
            snr = float(row.get('snr_db', 0) or 0)
            for i in range(len(path) - 1):
                self._node(path[i])
                self._node(path[i+1])
                self._edge(path[i], path[i+1], snr=snr, directed=True)

    def _load_meshtastic_rows(self, rows: List, decrypt: bool):
        for row in rows:
            nid = row.get('node_id_hex', '') or ''
            if not nid:
                continue
            if not nid.startswith('!'):
                nid = '!' + nid
            self._node(nid, packets=self.nodes.get(nid, {}).get('packets', 0) + 1,
                       is_router=row.get('is_router', '0') == '1',
                       label=nid[-6:])

            ptype = row.get('packet_type', '')
            raw_hex = row.get('raw_hex', '')
            if not raw_hex or not decrypt:
                continue
            try:
                raw = bytes.fromhex(raw_hex)
            except ValueError:
                continue

            header = _parse_meshtastic_header(raw)
            if not header:
                continue

            src = header['node_id']
            self._node(src)

            # Attempt decryption on any encrypted packet — portnum tells us what's inside
            encrypted = row.get('encrypted', '0') in ('1', 'true', 'True')
            if not encrypted:
                continue

            result = _meshtastic_try_decrypt(header, header['payload'])
            if not result:
                continue
            _, pt = result

            portnum = _parse_data_portnum(pt)
            inner   = _parse_inner_payload(pt)
            if inner is None:
                continue

            snr = float(row.get('snr_db', 0) or 0)

            if portnum == PORT_TRACEROUTE:
                dest = header['dest']
                route = _parse_traceroute(inner)
                # Even an empty route means src↔dest are directly connected
                full_path = [src] + route + [dest]
                for i in range(len(full_path) - 1):
                    self._node(full_path[i])
                    self._node(full_path[i+1])
                    self._edge(full_path[i], full_path[i+1], snr=snr)

            elif portnum == PORT_NEIGHBORINFO:
                neighbors = _parse_neighborinfo(inner)
                for nb in neighbors:
                    nb_id = nb['node_id']
                    self._node(nb_id)
                    self._edge(src, nb_id, snr=nb.get('snr'))

    def load_pcap(self, filepath: str, decrypt: bool = True):
        for freq, rssi, snr, payload in _read_pcap(filepath):
            # Try MeshCore first (simpler structure)
            parsed = _parse_meshcore_packet(payload)
            if parsed and len(parsed['path']) >= 2:
                path = parsed['path']
                for i in range(len(path) - 1):
                    self._node(path[i])
                    self._node(path[i+1])
                    self._edge(path[i], path[i+1], snr=snr, directed=True)
                self.protocol = 'MeshCore'
                continue

            # Try Meshtastic
            if decrypt:
                header = _parse_meshtastic_header(payload)
                if header:
                    self._node(header['node_id'])
                    result = _meshtastic_try_decrypt(header, header['payload'])
                    if result:
                        _, pt = result
                        portnum = _parse_data_portnum(pt)
                        inner   = _parse_inner_payload(pt)
                        if inner:
                            src = header['node_id']
                            if portnum == PORT_TRACEROUTE:
                                route = _parse_traceroute(inner)
                                dest  = header['dest']
                                full  = [src] + route + [dest]
                                for i in range(len(full) - 1):
                                    self._node(full[i])
                                    self._node(full[i+1])
                                    self._edge(full[i], full[i+1], snr=snr)
                            elif portnum == PORT_NEIGHBORINFO:
                                for nb in _parse_neighborinfo(inner):
                                    self._node(nb['node_id'])
                                    self._edge(src, nb['node_id'], snr=nb.get('snr'))
                        self.protocol = 'Meshtastic'

    def load_api(self, host: str):
        if not REQUESTS_AVAILABLE:
            print("ERROR: pip install requests")
            return
        try:
            r = requests.get(f"http://{host}/api/devices", timeout=10)
            for d in r.json().get('devices', []):
                nid = d.get('nodeId', '')
                if nid:
                    self._node(nid, label=nid[-6:], packets=d.get('packetCount', 0),
                               is_router=d.get('isRouter', False))
            print(f"[API] Loaded {len(self.nodes)} devices from {host}")
        except Exception as e:
            print(f"[API] Error: {e}")

    def classify(self):
        """Color nodes by role: high-degree = green (gateway), low = grey (edge)."""
        degrees = defaultdict(int)
        for (a, b) in self.edges:
            degrees[a] += 1
            degrees[b] += 1
        for nid, node in self.nodes.items():
            d = degrees[nid]
            if d >= 4:
                node['color'] = '#2ecc71'  # gateway
                node['shape'] = 's'
            elif d <= 1:
                node['color'] = '#666666'  # edge
                node['shape'] = 'o'
            else:
                node['color'] = '#4682b4'  # relay
                node['shape'] = 'o'

    def render(self, output: Optional[str] = None, title: Optional[str] = None):
        if not RENDER_AVAILABLE:
            print("ERROR: pip install networkx matplotlib")
            return

        self.classify()
        G = nx.DiGraph()

        # Only include nodes that appear in at least one edge — isolated nodes
        # add nothing to the graph and make dense captures unreadable.
        connected = set()
        for (a, b) in self.edges:
            connected.add(a)
            connected.add(b)

        for nid, attrs in self.nodes.items():
            if nid in connected:
                G.add_node(nid, **attrs)

        for (a, b), edata in self.edges.items():
            G.add_edge(a, b, weight=edata['weight'],
                       snr=(edata['snr_sum'] / edata['snr_count']) if edata['snr_count'] else None)

        isolated = len(self.nodes) - len(connected)
        if len(G.nodes) == 0:
            print("No topology data found. Capture needs neighborinfo, traceroute, or MeshCore path packets.")
            print(f"({len(self.nodes)} nodes seen but none with link data)")
            return
        if isolated:
            print(f"Skipping {isolated} isolated nodes (no link data) — showing {len(connected)} connected nodes")

        fig, ax = plt.subplots(figsize=(14, 9))
        ax.set_facecolor('#1a1a2e')
        fig.patch.set_facecolor('#1a1a2e')

        try:
            pos = nx.spring_layout(G, seed=42, k=2.5 / (len(G.nodes) ** 0.5 + 0.1))
        except Exception:
            pos = nx.circular_layout(G)

        for shape in ('s', 'o'):
            group = [n for n, d in G.nodes(data=True) if d.get('shape', 'o') == shape]
            if not group:
                continue
            colors = [G.nodes[n]['color'] for n in group]
            nx.draw_networkx_nodes(G, pos, nodelist=group, node_color=colors,
                                   node_shape=shape, node_size=800, ax=ax, alpha=0.92)

        weights = [0.8 + G.edges[e].get('weight', 1) * 0.4 for e in G.edges()]
        nx.draw_networkx_edges(G, pos, width=weights, edge_color='#7799bb',
                               arrows=True, arrowsize=16, ax=ax,
                               connectionstyle='arc3,rad=0.07')

        labels = {n: n[-9:] for n in G.nodes()}  # last 9 chars e.g. !b03cb82c
        nx.draw_networkx_labels(G, pos, labels, font_size=8,
                                font_color='white', ax=ax)

        edge_labels = {}
        seen = set()
        for u, v, d in G.edges(data=True):
            key = tuple(sorted([u, v]))
            if key not in seen:
                seen.add(key)
                lbl = f"{d.get('weight', 1)}p"
                if d.get('snr') is not None:
                    lbl += f"\n{d['snr']:.1f}dB"
                edge_labels[(u, v)] = lbl
        nx.draw_networkx_edge_labels(G, pos, edge_labels, font_size=6,
                                     font_color='#aaccee', ax=ax)

        legend_items = [
            mpatches.Patch(color='#2ecc71', label='Gateway (≥4 links)'),
            mpatches.Patch(color='#4682b4', label='Relay'),
            mpatches.Patch(color='#666666', label='Edge node (≤1 link)'),
        ]
        ax.legend(handles=legend_items, loc='upper left',
                  facecolor='#2a2a4e', labelcolor='white', fontsize=9)

        proto = title or self.protocol or 'LoRa Mesh'
        node_count = len(self.nodes)
        edge_count = len(self.edges)
        ax.set_title(f'{proto} Network Topology  —  {node_count} nodes, {edge_count} links',
                     color='white', fontsize=13, pad=14)
        ax.axis('off')
        plt.tight_layout()

        if output:
            plt.savefig(output, dpi=150, bbox_inches='tight',
                        facecolor=fig.get_facecolor())
            print(f"Saved: {output}")
        else:
            plt.show()
        plt.close(fig)

    def summary(self):
        print(f"Nodes: {len(self.nodes)}")
        print(f"Links: {len(self.edges)}")
        if not self.nodes:
            print("No topology found — need traceroute, neighborinfo, or MeshCore path packets.")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description='Render LoRa mesh topology as a PNG graph image.')
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument('file', nargs='?',
                     help='CSV or PCAP capture file')
    src.add_argument('--api', metavar='HOST',
                     help='Pull device list from live ESP32 API')
    ap.add_argument('-o', '--output', metavar='FILE',
                    help='Output PNG file (default: show window)')
    ap.add_argument('--no-decrypt', action='store_true',
                    help='Skip Meshtastic PSK decryption (faster)')
    args = ap.parse_args()

    g = TopologyGraph()

    if args.api:
        g.load_api(args.api)
    else:
        p = Path(args.file)
        if not p.exists():
            sys.exit(f"File not found: {args.file}")
        if p.suffix.lower() == '.pcap':
            print(f"Loading PCAP: {p}")
            g.load_pcap(str(p), decrypt=not args.no_decrypt)
        else:
            print(f"Loading CSV: {p}")
            g.load_csv(str(p), decrypt=not args.no_decrypt)

    g.summary()

    out = args.output
    if not out and args.file:
        out = Path(args.file).stem + '_topology.png'
    g.render(output=out)


if __name__ == '__main__':
    main()
