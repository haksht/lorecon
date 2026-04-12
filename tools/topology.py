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

import base64
import hashlib
import struct
import sys
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from tools.core import capture as capture_loader
from tools.core import cli, decode
from tools.core.models import Capture, CapturedPacket

try:
    import networkx as nx
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    RENDER_AVAILABLE = True
except ImportError:
    RENDER_AVAILABLE = False

CRYPTO_AVAILABLE = decode.CRYPTO_AVAILABLE


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
    route, offset = [], 0
    while offset < len(inner):
        tag, c = _decode_varint(inner, offset); offset += c
        fn, wt = tag >> 3, tag & 0x07
        if fn == 1 and wt == 2:
            l, c = _decode_varint(inner, offset); offset += c
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
                if sf == 1 and sw == 0:
                    v, sc = _decode_varint(sub, si); si += sc
                    nb['node_id'] = f"!{v:08x}"
                elif sf == 2 and sw == 5 and si + 4 <= len(sub):
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


PORT_NODEINFO     = 4
PORT_TRACEROUTE   = 70
PORT_NEIGHBORINFO = 71


def _parse_user_proto(inner: bytes) -> Optional[Tuple[str, str]]:
    short_name = long_name = None
    offset = 0
    while offset < len(inner):
        tag, c = _decode_varint(inner, offset); offset += c
        fn, wt = tag >> 3, tag & 0x07
        if wt == 2:
            l, c = _decode_varint(inner, offset); offset += c
            val = inner[offset:offset + l]; offset += l
            if fn == 2:
                try: long_name = val.decode('utf-8', errors='replace').strip()
                except Exception: pass
            elif fn == 3:
                try: short_name = val.decode('utf-8', errors='replace').strip()
                except Exception: pass
        elif wt == 0:
            _, c = _decode_varint(inner, offset); offset += c
        elif wt == 5:
            offset += 4
        elif wt == 1:
            offset += 8
        else:
            break
    if short_name or long_name:
        return (short_name or '', long_name or '')
    return None

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
# ESP32 PCAP reader (custom LoRa header — same as report.py)
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
            fh.read(20)
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
        self.nodes: Dict[str, Dict] = {}
        self.edges: Dict[Tuple, Dict] = {}
        self.names: Dict[str, Tuple[str, str]] = {}
        self.protocol = 'Unknown'
        self.traceroute_count   = 0
        self.neighborinfo_count = 0
        self.total_packets      = 0

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

    def ingest_capture(self, cap: Capture, decrypt: bool = True) -> None:
        by_protocol: Dict[str, List[CapturedPacket]] = defaultdict(list)
        for p in cap:
            by_protocol[p.protocol].append(p)

        for proto, packets in by_protocol.items():
            if 'MeshCore' in proto or 'meshcore' in proto.lower():
                self._load_meshcore_packets(packets)
            elif 'Meshtastic' in proto or 'meshtastic' in proto.lower():
                self._load_meshtastic_packets(packets, decrypt)
                self._infer_relay_edges(packets)

        if self.nodes:
            protos = list(by_protocol.keys())
            self.protocol = protos[0] if len(protos) == 1 else 'Mixed'

    def _load_meshcore_packets(self, packets: List[CapturedPacket]):
        self.total_packets += len(packets)
        for p in packets:
            nid = p.node_id_hex or ''
            if not nid or nid == 'unknown':
                continue
            self._node(nid, packets=self.nodes.get(nid, {}).get('packets', 0) + 1,
                       is_router=p.is_router)

            if not p.raw_hex:
                continue
            try:
                raw = p.raw_bytes
            except ValueError:
                continue

            parsed = _parse_meshcore_packet(raw)
            if not parsed or len(parsed['path']) < 2:
                continue

            path = parsed['path']
            for i in range(len(path) - 1):
                self._node(path[i])
                self._node(path[i+1])
                self._edge(path[i], path[i+1], snr=p.snr_db, directed=True)

    def _load_meshtastic_packets(self, packets: List[CapturedPacket], decrypt: bool):
        self.total_packets += len(packets)
        for p in packets:
            nid = p.node_id_hex or ''
            if not nid or nid == 'unknown':
                continue
            if not nid.startswith('!'):
                nid = '!' + nid
            self._node(nid, packets=self.nodes.get(nid, {}).get('packets', 0) + 1,
                       is_router=p.is_router, label=nid[-6:])

            if not p.raw_hex or not decrypt:
                continue
            try:
                raw = p.raw_bytes
            except ValueError:
                continue

            header = _parse_meshtastic_header(raw)
            if not header:
                continue

            src = header['node_id']
            self._node(src)

            if not p.encrypted:
                continue

            result = decode.try_decrypt(raw)
            if not result:
                continue
            pt = result.plaintext
            portnum = result.portnum
            inner   = _parse_inner_payload(pt)
            if inner is None:
                continue

            snr = p.snr_db

            if portnum == PORT_TRACEROUTE:
                self.traceroute_count += 1
                dest = header['dest']
                route = _parse_traceroute(inner)
                full_path = [src] + route + [dest]
                for i in range(len(full_path) - 1):
                    self._node(full_path[i])
                    self._node(full_path[i+1])
                    self._edge(full_path[i], full_path[i+1], snr=snr)

            elif portnum == PORT_NEIGHBORINFO:
                self.neighborinfo_count += 1
                neighbors = _parse_neighborinfo(inner)
                for nb in neighbors:
                    nb_id = nb['node_id']
                    self._node(nb_id)
                    self._edge(src, nb_id, snr=nb.get('snr'))

            elif portnum == PORT_NODEINFO:
                names = _parse_user_proto(inner)
                if names:
                    self.names[src] = names

    def _infer_relay_edges(self, packets: List[CapturedPacket]):
        """
        Infer mesh links from the relay byte (byte 15) in Meshtastic headers.

        Prefers typed CapturedPacket fields (packet_id / relay_byte / hop_count
        from the 2026-04-12 CSV columns). Falls back to raw_hex parsing for
        older captures that lack those columns.
        """
        # Pass 1: build last-byte → node map. Normalize node IDs to the same
        # "!aabbccdd" (8-char hex, no 0x prefix) format used by edge creation,
        # so self._node(relay_node) merges with nodes already created that way.
        node_by_last: Dict[int, List[str]] = defaultdict(list)
        node_pkts: Dict[str, int] = defaultdict(int)
        for p in packets:
            nid_hex = p.node_id_hex or ''
            if not nid_hex or nid_hex == 'unknown':
                continue
            try:
                src = int(nid_hex.lstrip('!'), 16)
            except ValueError:
                continue
            src_hex = f"!{src:08x}"
            node_pkts[src_hex] += 1
            lb = src & 0xFF
            if src_hex not in node_by_last[lb]:
                node_by_last[lb].append(src_hex)

        lb_to_node: Dict[int, str] = {}
        for lb, nids in node_by_last.items():
            lb_to_node[lb] = max(nids, key=lambda n: node_pkts[n])

        # Pass 2: group copies by (src, packet_id)
        pid_copies: Dict[Tuple[int, int], List[Tuple[int, int, float]]] = defaultdict(list)
        for p in packets:
            nid_hex = p.node_id_hex or ''
            if not nid_hex or nid_hex == 'unknown':
                continue
            pid = p.packet_id
            hop_limit = p.hop_count
            b15 = p.relay_byte
            if pid is not None and hop_limit is not None:
                # Typed-field path: packet_id came in from CSV as hex; we already
                # parsed it to int. relay_byte may be None (missing column on
                # older rows) — fall back to (src & 0xFF) to match original.
                try:
                    src = int(nid_hex.lstrip('!'), 16)
                except ValueError:
                    continue
                if b15 is None:
                    b15 = src & 0xFF
            else:
                # Fallback: parse from raw hex
                if not p.raw_hex or len(p.raw_hex) < 32:
                    continue
                try:
                    raw = p.raw_bytes
                except ValueError:
                    continue
                if len(raw) < 16:
                    continue
                src = struct.unpack('<I', raw[4:8])[0]
                pid = struct.unpack('<I', raw[8:12])[0]
                hop_limit = raw[12] & 0x07
                b15 = raw[15]
            pid_copies[(src, pid)].append((hop_limit, b15, p.snr_db))

        relay_edge_count = 0
        for (src, pid), copies in pid_copies.items():
            src_last = src & 0xFF
            max_hop = max(hl for hl, _, _ in copies)
            for hl, b15, snr in copies:
                if b15 == src_last:
                    continue
                if max_hop - hl != 1:
                    continue
                if b15 not in lb_to_node:
                    continue
                relay_node = lb_to_node[b15]
                src_hex = f"!{src:08x}"
                self._node(src_hex)
                self._node(relay_node)
                self._edge(src_hex, relay_node, snr=snr)
                relay_edge_count += 1
        if relay_edge_count:
            print(f"Inferred {relay_edge_count} relay observations")

    def load_pcap(self, filepath: str, decrypt: bool = True):
        meshtastic_payloads: List[Tuple[bytes, float]] = []
        for freq, rssi, snr, payload in _read_pcap(filepath):
            parsed = _parse_meshcore_packet(payload)
            if parsed and len(parsed['path']) >= 2:
                path = parsed['path']
                for i in range(len(path) - 1):
                    self._node(path[i])
                    self._node(path[i+1])
                    self._edge(path[i], path[i+1], snr=snr, directed=True)
                self.protocol = 'MeshCore'
                continue

            if decrypt:
                header = _parse_meshtastic_header(payload)
                if header:
                    meshtastic_payloads.append((payload, snr))
                    self._node(header['node_id'])
                    result = decode.try_decrypt(payload)
                    if result:
                        pt = result.plaintext
                        portnum = result.portnum
                        inner   = _parse_inner_payload(pt)
                        if inner:
                            src = header['node_id']
                            if portnum == PORT_TRACEROUTE:
                                self.traceroute_count += 1
                                route = _parse_traceroute(inner)
                                dest  = header['dest']
                                full  = [src] + route + [dest]
                                for i in range(len(full) - 1):
                                    self._node(full[i])
                                    self._node(full[i+1])
                                    self._edge(full[i], full[i+1], snr=snr)
                            elif portnum == PORT_NEIGHBORINFO:
                                self.neighborinfo_count += 1
                                for nb in _parse_neighborinfo(inner):
                                    self._node(nb['node_id'])
                                    self._edge(src, nb['node_id'], snr=nb.get('snr'))
                            elif portnum == PORT_NODEINFO:
                                names = _parse_user_proto(inner)
                                if names:
                                    self.names[src] = names
                        self.protocol = 'Meshtastic'

        if meshtastic_payloads:
            self._infer_relay_edges_raw(meshtastic_payloads)

    def _infer_relay_edges_raw(self, payloads: List[Tuple[bytes, float]]):
        node_by_last: Dict[int, List[str]] = defaultdict(list)
        node_pkts: Dict[str, int] = defaultdict(int)
        pid_copies: Dict[Tuple[int, int], List[Tuple[int, int, float]]] = defaultdict(list)

        for payload, snr in payloads:
            if len(payload) < 16:
                continue
            src = struct.unpack('<I', payload[4:8])[0]
            pid = struct.unpack('<I', payload[8:12])[0]
            src_hex = f"!{src:08x}"
            node_pkts[src_hex] += 1
            lb = src & 0xFF
            if src_hex not in node_by_last[lb]:
                node_by_last[lb].append(src_hex)
            hop_limit = payload[12] & 0x07
            b15 = payload[15]
            pid_copies[(src, pid)].append((hop_limit, b15, snr))

        lb_to_node: Dict[int, str] = {}
        for lb, nids in node_by_last.items():
            lb_to_node[lb] = max(nids, key=lambda n: node_pkts[n])

        relay_edge_count = 0
        for (src, pid), copies in pid_copies.items():
            src_last = src & 0xFF
            max_hop = max(hl for hl, _, _ in copies)
            for hl, b15, snr in copies:
                if b15 == src_last or max_hop - hl != 1 or b15 not in lb_to_node:
                    continue
                relay_node = lb_to_node[b15]
                src_hex = f"!{src:08x}"
                self._node(src_hex)
                self._node(relay_node)
                self._edge(src_hex, relay_node, snr=snr)
                relay_edge_count += 1
        if relay_edge_count:
            print(f"Inferred {relay_edge_count} relay observations")

    def ingest_devices_api(self, host: str):
        try:
            devices = capture_loader.load_devices(host)
        except Exception as e:
            print(f"[API] Error: {e}")
            return
        for d in devices:
            nid = d.node_id
            if nid:
                self._node(nid, label=nid[-6:], packets=d.packet_count,
                           is_router=d.raw.get('isRouter', False))
        print(f"[API] Loaded {len(self.nodes)} devices from {host}")

    def classify(self):
        degrees = defaultdict(int)
        for (a, b) in self.edges:
            degrees[a] += 1
            degrees[b] += 1
        for nid, node in self.nodes.items():
            d = degrees[nid]
            if d >= 4:
                node['color'] = '#2ecc71'
                node['shape'] = 's'
            elif d <= 1:
                node['color'] = '#666666'
                node['shape'] = 'o'
            else:
                node['color'] = '#4682b4'
                node['shape'] = 'o'

    def render(self, output: Optional[str] = None, title: Optional[str] = None):
        if not RENDER_AVAILABLE:
            print("ERROR: pip install networkx matplotlib")
            return

        self.classify()
        G = nx.DiGraph()

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

        n_graph = len(G.nodes)
        fig_w = 14 if n_graph <= 20 else min(22, 14 + n_graph // 10)
        fig_h = int(fig_w * 0.65)
        fig, ax = plt.subplots(figsize=(fig_w, fig_h))
        ax.set_facecolor('#1a1a2e')
        fig.patch.set_facecolor('#1a1a2e')

        try:
            if n_graph > 15:
                pos = nx.forceatlas2_layout(G, seed=42, max_iter=1000,
                                           strong_gravity=True, gravity=5.0)
            else:
                pos = nx.spring_layout(G, seed=42,
                                       k=2.5 / (n_graph ** 0.5 + 0.1))
        except Exception:
            pos = nx.circular_layout(G)

        node_size = 800 if n_graph <= 20 else max(250, 700 - n_graph * 8)
        font_size = 8 if n_graph <= 20 else max(6, 9 - n_graph // 15)
        arrow_size = 16 if n_graph <= 20 else 10

        for shape in ('s', 'o'):
            group = [n for n, d in G.nodes(data=True) if d.get('shape', 'o') == shape]
            if not group:
                continue
            colors = [G.nodes[n]['color'] for n in group]
            nx.draw_networkx_nodes(G, pos, nodelist=group, node_color=colors,
                                   node_shape=shape, node_size=node_size,
                                   ax=ax, alpha=0.92)

        max_weight = max((G.edges[e].get('weight', 1) for e in G.edges()), default=1)
        weights = [0.5 + G.edges[e].get('weight', 1) / max(max_weight, 1) * 3
                   for e in G.edges()]
        nx.draw_networkx_edges(G, pos, width=weights, edge_color='#7799bb',
                               arrows=True, arrowsize=arrow_size, ax=ax,
                               connectionstyle='arc3,rad=0.07', alpha=0.6)

        def _strip_emoji(s: str) -> str:
            return ''.join(c for c in s if ord(c) < 0x10000)

        def _node_label(nid: str) -> str:
            if nid in self.names:
                short, long = self.names[nid]
                name = short if short else (long[:10] if long else '')
                name = _strip_emoji(name).strip()
                if name:
                    return name
            return nid[-9:]

        labels = {n: _node_label(n) for n in G.nodes()}
        nx.draw_networkx_labels(G, pos, labels, font_size=font_size,
                                font_color='white', ax=ax)

        if len(G.nodes) <= 20:
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

        stats_lines = []
        if self.total_packets:
            stats_lines.append(f'Packets analyzed : {self.total_packets:,}')
        stats_lines.append(f'Nodes seen       : {len(self.nodes):,}')
        stats_lines.append(f'In topology      : {len(connected)}')
        stats_lines.append(f'Links            : {len(self.edges)}')
        sources = []
        if self.traceroute_count:
            sources.append(f'{self.traceroute_count} traceroute')
        if self.neighborinfo_count:
            sources.append(f'{self.neighborinfo_count} neighborinfo')
        relay_inferred = len(self.edges) - (self.traceroute_count + self.neighborinfo_count)
        if relay_inferred > 0:
            sources.append(f'{relay_inferred} relay-inferred')
        if sources:
            stats_lines.append(f'Sources          : {", ".join(sources)}')
        named = len(self.names)
        if named:
            stats_lines.append(f'Named nodes      : {named}')
        stats_text = '\n'.join(stats_lines)
        ax.text(0.99, 0.02, stats_text,
                transform=ax.transAxes,
                ha='right', va='bottom',
                fontsize=8, family='monospace',
                color='#ccddee',
                bbox=dict(boxstyle='round,pad=0.5', facecolor='#1a1a2e',
                          edgecolor='#445566', alpha=0.9))

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
    ap = cli.base_parser('Render LoRa mesh topology as a PNG graph image.')
    ap.add_argument('--no-decrypt', action='store_true',
                    help='Skip Meshtastic PSK decryption (faster)')
    args = ap.parse_args()

    g = TopologyGraph()

    if args.api:
        g.ingest_devices_api(args.api)
        out = args.output
    else:
        if not args.input:
            sys.exit("ERROR: provide a capture file or --api HOST")
        p = Path(args.input)
        if not p.exists():
            sys.exit(f"File not found: {args.input}")
        if p.suffix.lower() == '.pcap':
            print(f"Loading PCAP: {p}")
            g.load_pcap(str(p), decrypt=not args.no_decrypt)
        else:
            print(f"Loading CSV: {p}")
            cap = capture_loader.load(str(p))
            g.ingest_capture(cap, decrypt=not args.no_decrypt)
        out = args.output or (p.stem + '_topology.png')

    g.summary()
    g.render(output=out)


if __name__ == '__main__':
    main()
