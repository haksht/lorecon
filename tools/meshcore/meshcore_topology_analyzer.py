#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - MeshCore Topology Analyzer

Extracts relay topology from MeshCore packet captures by parsing the
path array embedded in every flooded packet.

MeshCore path layout (FLOOD route type):
  Every relayed packet carries a growing path array:
    [originator_hash][relay1_hash][relay2_hash]...
  Each relay appends its own hash as it forwards. Directed edge
  A -> B means "A's traffic was forwarded by B."

Topology evidence is stronger than Meshtastic neighborinfo because
every single flooded message contributes — no need for special
traceroute packets.

Output formats:
  ascii      - Terminal table + relay chain listing (default)
  json       - Full graph (nodes, edges, paths)
  graphviz   - Graphviz DOT for rendering with dot/neato

Usage:
    python meshcore_topology_analyzer.py capture.pcap
    python meshcore_topology_analyzer.py capture.csv --format csv
    python meshcore_topology_analyzer.py --live 192.168.4.1
    python meshcore_topology_analyzer.py --live 192.168.4.1 --watch
    python meshcore_topology_analyzer.py --format graphviz -o mesh.dot
    python meshcore_topology_analyzer.py --demo

Requirements:
    pip install cryptography         (for decryption + node name resolution)
    pip install websocket-client     (for --live mode)
"""

import argparse
import base64
import csv
import hashlib
import hmac
import json
import struct
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Optional, Tuple

try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    from cryptography.hazmat.backends import default_backend
    CRYPTO_AVAILABLE = True
except ImportError:
    CRYPTO_AVAILABLE = False

# ---------------------------------------------------------------------------
# MeshCore channel key derivation (same as meshcore_decoder.py)
# ---------------------------------------------------------------------------

PUBLIC_CHANNEL_KEY_B64 = "izOH6cXN6mrJ5e26oRXNcg=="

HASHTAG_ROOMS = [
    "#general", "#emergency", "#local", "#mesh", "#public",
    "#chat", "#help", "#info", "#sos", "#weather", "#news", "#test",
]

PAYLOAD_TYPE_NAMES = {
    0: "MSG", 1: "ACK", 2: "SIGNED_MSG", 3: "TRACE_REQ",
    4: "ADVERT", 5: "DIRECT", 6: "PING", 7: "PONG", 8: "POSITION",
}

ROUTE_TYPE_NAMES = {0: "ZERO_HOP", 1: "FLOOD", 2: "TRANSPORT_FLOOD", 3: "TRANSPORT_DIRECT"}


def _derive_keys():
    keys = [(base64.b64decode(PUBLIC_CHANNEL_KEY_B64), "public")]
    for room in HASHTAG_ROOMS:
        keys.append((hashlib.sha256(room.encode()).digest()[:16], room))
    return keys


ALL_KEYS = _derive_keys()


def _try_decrypt_payload(payload: bytes) -> Optional[Tuple[str, str]]:
    """Try all known keys. Returns (channel_name, plaintext) or None."""
    if not CRYPTO_AVAILABLE or len(payload) < 19:
        return None
    ciphertext = payload[3:]
    if not ciphertext or len(ciphertext) % 16 != 0:
        return None
    for key, name in ALL_KEYS:
        key_hash = hashlib.sha256(key).digest()
        if key_hash[0] != payload[0]:
            continue
        hmac_key = key + b'\x00' * 16
        mac = hmac.new(hmac_key, ciphertext, hashlib.sha256).digest()
        if mac[0] != payload[1] or mac[1] != payload[2]:
            continue
        try:
            cipher = Cipher(algorithms.AES(key), modes.ECB(), backend=default_backend())
            dec = cipher.decryptor()
            pt = dec.update(ciphertext) + dec.finalize()
        except Exception:
            continue
        if len(pt) < 5:
            continue
        text_bytes = pt[5:]
        chars = []
        for b in text_bytes:
            if b == 0:
                break
            if b < 9 or (13 < b < 32):
                break
            chars.append(chr(b) if b < 128 else b.to_bytes(1, 'big').decode('utf-8', errors='replace'))
        text = ''.join(chars).strip()
        if text:
            return (name, text)
    return None


# ---------------------------------------------------------------------------
# Packet parsing
# ---------------------------------------------------------------------------

def parse_packet(data: bytes) -> Optional[Dict]:
    """
    Parse a MeshCore packet. Returns a dict or None if not MeshCore.

    Returned keys:
      version, payload_type, payload_type_name, route_type, route_type_name,
      path  — list of hex strings, e.g. ["c9", "a1b2", "3c"]  (originator first)
      hop_count, hash_size,
      channel, text  (if decryptable), sender_name
    """
    if len(data) < 3:
        return None

    hdr = data[0]
    version = (hdr >> 6) & 0x03
    payload_type = (hdr >> 2) & 0x0F
    route_type = hdr & 0x03

    if version > 1 or payload_type > 11:
        return None

    # Transport types have 4 extra bytes before path_len
    path_len_offset = 5 if route_type in (2, 3) else 1
    if path_len_offset + 1 > len(data):
        return None

    path_len_byte = data[path_len_offset]
    hop_count = path_len_byte & 0x3F
    hash_size = ((path_len_byte >> 6) & 0x03) + 1

    path_start = path_len_offset + 1
    path_end = path_start + hop_count * hash_size
    if path_end > len(data):
        return None

    # Extract each node hash as a hex string
    path = []
    for i in range(hop_count):
        offset = path_start + i * hash_size
        path.append(data[offset:offset + hash_size].hex())

    payload_offset = path_end
    channel = None
    text = None
    sender_name = None

    # Only group messages (payload type 0, 2) have AES payloads
    if payload_type in (0, 2) and payload_offset < len(data):
        payload = data[payload_offset:]
        result = _try_decrypt_payload(payload)
        if result:
            channel, text = result
            # Sender name is "Name: message text" convention
            if ': ' in text:
                sender_name, text = text.split(': ', 1)
                sender_name = sender_name.strip()
                text = text.strip()

    return {
        'version': version,
        'payload_type': payload_type,
        'payload_type_name': PAYLOAD_TYPE_NAMES.get(payload_type, f'TYPE_{payload_type}'),
        'route_type': route_type,
        'route_type_name': ROUTE_TYPE_NAMES.get(route_type, f'RT_{route_type}'),
        'path': path,
        'hop_count': hop_count,
        'hash_size': hash_size,
        'channel': channel,
        'text': text,
        'sender_name': sender_name,
    }


# ---------------------------------------------------------------------------
# Graph nodes and edges
# ---------------------------------------------------------------------------

class MeshCoreNode:
    def __init__(self, node_hash: str):
        self.node_hash = node_hash       # hex string, e.g. "c954a1b2"
        self.name: Optional[str] = None  # resolved from decrypted sender name
        self.channel: Optional[str] = None
        self.originated = 0              # packets where this node is path[0]
        self.relayed = 0                 # packets where this node is path[1+]
        self.first_seen: Optional[int] = None
        self.last_seen: Optional[int] = None
        self.packet_count = 0

    @property
    def label(self) -> str:
        if self.name:
            return f"{self.name} ({self.node_hash[:6]})"
        return self.node_hash

    @property
    def role(self) -> str:
        if self.relayed >= 2:
            return "relay"
        if self.originated > 0 and self.relayed == 0:
            return "edge"
        return "unknown"

    def update_seen(self, ts_ms: Optional[int]):
        self.packet_count += 1
        if ts_ms is None:
            return
        if self.first_seen is None:
            self.first_seen = ts_ms
        self.last_seen = ts_ms


class MeshCoreEdge:
    """Directed edge: source forwarded a packet from/to target."""
    def __init__(self, src: str, dst: str):
        self.src = src
        self.dst = dst
        self.relay_count = 0   # number of times this relay hop was observed
        self.channels: Dict[str, int] = defaultdict(int)
        self.first_seen: Optional[int] = None
        self.last_seen: Optional[int] = None

    def observe(self, ts_ms: Optional[int], channel: Optional[str]):
        self.relay_count += 1
        if channel:
            self.channels[channel] += 1
        if ts_ms is None:
            return
        if self.first_seen is None:
            self.first_seen = ts_ms
        self.last_seen = ts_ms


# ---------------------------------------------------------------------------
# Topology analyzer
# ---------------------------------------------------------------------------

class MeshCoreTopologyAnalyzer:

    def __init__(self, verbose=False):
        self.verbose = verbose
        self.nodes: Dict[str, MeshCoreNode] = {}
        self.edges: Dict[Tuple[str, str], MeshCoreEdge] = {}
        self.relay_chains: List[Dict] = []   # full recorded paths
        self.stats = defaultdict(int)

    # --- node/edge helpers --------------------------------------------------

    def _node(self, h: str) -> MeshCoreNode:
        if h not in self.nodes:
            self.nodes[h] = MeshCoreNode(h)
        return self.nodes[h]

    def _edge(self, src: str, dst: str) -> MeshCoreEdge:
        key = (src, dst)
        if key not in self.edges:
            self.edges[key] = MeshCoreEdge(src, dst)
        return self.edges[key]

    # --- main ingestion -----------------------------------------------------

    def process_raw(self, data: bytes, ts_ms: Optional[int] = None):
        """Parse one raw MeshCore packet and integrate into topology graph."""
        pkt = parse_packet(data)
        if pkt is None:
            self.stats['non_meshcore'] += 1
            return

        self.stats['total'] += 1
        path = pkt['path']

        if len(path) < 1:
            self.stats['zero_hop'] += 1
            return

        # Originator
        originator_hash = path[0]
        orig_node = self._node(originator_hash)
        orig_node.originated += 1
        orig_node.update_seen(ts_ms)

        # Sender name resolution (bind to originator hash)
        if pkt['sender_name'] and not orig_node.name:
            orig_node.name = pkt['sender_name']
            if self.verbose:
                print(f"  [name] {originator_hash} = {pkt['sender_name']}")
        if pkt['channel'] and not orig_node.channel:
            orig_node.channel = pkt['channel']

        # Relay chain: path[1], path[2], ...
        channel = pkt['channel']
        for i in range(1, len(path)):
            relay_hash = path[i]
            relay_node = self._node(relay_hash)
            relay_node.relayed += 1
            relay_node.update_seen(ts_ms)

            # Edge from previous hop to this relay
            prev = path[i - 1]
            edge = self._edge(prev, relay_hash)
            edge.observe(ts_ms, channel)

        # Record the chain if multi-hop
        if len(path) >= 2:
            self.stats['chained'] += 1
            self.relay_chains.append({
                'path': path,
                'channel': channel,
                'text': pkt['text'],
                'sender': pkt['sender_name'],
                'payload_type': pkt['payload_type_name'],
                'ts': ts_ms,
            })
            if self.verbose:
                chain_str = ' -> '.join(f"{h[:6]}" for h in path)
                print(f"  [chain] {pkt['payload_type_name']} {chain_str}  ch={channel or '?'}")
        else:
            self.stats['direct'] += 1

        if pkt['channel']:
            self.stats[f'ch_{pkt["channel"]}'] += 1

    def process_websocket_event(self, msg: Dict):
        """
        Process a WebSocket packet event from the ESP32 firmware.

        The firmware broadcasts hex_data when available. If not, falls back
        to device-level fields (nodeId, hopCount, meshCoreChannel, senderName).
        """
        protocol = msg.get('protocol', '')
        if 'MeshCore' not in protocol and msg.get('type') != 'packet':
            return

        raw_hex = msg.get('hex_data') or msg.get('raw_hex')
        if raw_hex:
            try:
                data = bytes.fromhex(raw_hex.replace(' ', ''))
                ts = msg.get('timestamp_ms')
                self.process_raw(data, ts)
                return
            except ValueError:
                pass

        # Fallback: device-level summary fields (no path array, but still useful)
        node_id = msg.get('nodeId') or msg.get('node_id')
        if node_id:
            node_id = node_id.lstrip('!')
            node = self._node(node_id)
            node.update_seen(msg.get('timestamp_ms'))
            if msg.get('senderName') and not node.name:
                node.name = msg['senderName']
            if msg.get('meshCoreChannel') and not node.channel:
                node.channel = msg['meshCoreChannel']
            hop_count = msg.get('hopCount', 0)
            if hop_count == 0:
                node.originated += 1
            else:
                node.relayed += 1
            self.stats['total'] += 1

    # --- classification -----------------------------------------------------

    def classify_nodes(self):
        """Assign roles based on accumulated relay evidence."""
        pass  # Role is derived dynamically via MeshCoreNode.role property

    # --- output -------------------------------------------------------------

    def to_ascii(self) -> str:
        lines = []
        lines.append('=' * 64)
        lines.append('MESHCORE TOPOLOGY ANALYSIS')
        lines.append('=' * 64)
        lines.append('')

        total = self.stats['total']
        if total == 0:
            lines.append('No MeshCore packets found.')
            return '\n'.join(lines)

        chained = self.stats['chained']
        direct = self.stats['direct']

        lines.append(f'Packets processed : {total}')
        lines.append(f'  Multi-hop chains: {chained}')
        lines.append(f'  Direct (0 relays): {direct}')
        lines.append(f'Unique nodes      : {len(self.nodes)}')
        lines.append(f'Unique relay edges: {len(self.edges)}')
        lines.append('')

        # Channel breakdown
        ch_stats = {k[3:]: v for k, v in self.stats.items() if k.startswith('ch_')}
        if ch_stats:
            lines.append('Channel breakdown:')
            for ch, count in sorted(ch_stats.items(), key=lambda x: -x[1]):
                lines.append(f'  {ch:<20} {count} packets')
            lines.append('')

        # Node table
        lines.append('NODE TABLE:')
        lines.append(f'  {"Hash":<12} {"Name":<18} {"Role":<8} {"Channel":<12} {"Orig":>5} {"Relay":>5}')
        lines.append('  ' + '-' * 65)
        for node in sorted(self.nodes.values(), key=lambda n: -(n.originated + n.relayed)):
            role = node.role
            name = (node.name or '')[:17]
            ch = (node.channel or '')[:11]
            lines.append(f'  {node.node_hash:<12} {name:<18} {role:<8} {ch:<12} {node.originated:>5} {node.relayed:>5}')
        lines.append('')

        # Relay edge table
        if self.edges:
            lines.append('RELAY EDGES (directed: src forwarded traffic from/to dst):')
            lines.append(f'  {"Source":<12} {"->":2} {"Dest":<12} {"Count":>6}  Channels')
            lines.append('  ' + '-' * 60)
            for (src, dst), edge in sorted(self.edges.items(), key=lambda kv: -kv[1].relay_count):
                src_label = self.nodes[src].name or src[:8]
                dst_label = self.nodes[dst].name or dst[:8]
                ch_str = ', '.join(f'{c}({n})' for c, n in sorted(edge.channels.items(), key=lambda x: -x[1]))
                lines.append(f'  {src_label:<12}  -> {dst_label:<12} {edge.relay_count:>6}  {ch_str}')
            lines.append('')

        # Relay chain listing (most recent 20)
        if self.relay_chains:
            lines.append(f'RELAY CHAINS ({min(len(self.relay_chains), 20)} most recent):')
            lines.append('  (arrows show packet path through mesh — leftmost = originator)')
            lines.append('')
            for chain in self.relay_chains[-20:]:
                path = chain['path']
                chain_str = ' -> '.join(
                    (self.nodes[h].name or h[:6]) for h in path
                )
                ch = chain.get('channel') or '?'
                pt = chain.get('payload_type') or ''
                text = chain.get('text') or ''
                line = f'  [{ch}] {pt} {chain_str}'
                if text:
                    line += f'\n    "{text[:60]}"'
                lines.append(line)
            if len(self.relay_chains) > 20:
                lines.append(f'  ... and {len(self.relay_chains) - 20} more')
            lines.append('')

        lines.append('=' * 64)
        return '\n'.join(lines)

    def to_json(self) -> Dict:
        nodes_out = []
        for node in self.nodes.values():
            nodes_out.append({
                'hash': node.node_hash,
                'name': node.name,
                'channel': node.channel,
                'role': node.role,
                'originated': node.originated,
                'relayed': node.relayed,
                'packet_count': node.packet_count,
                'first_seen': node.first_seen,
                'last_seen': node.last_seen,
            })

        edges_out = []
        for (src, dst), edge in self.edges.items():
            edges_out.append({
                'source': src,
                'target': dst,
                'relay_count': edge.relay_count,
                'channels': dict(edge.channels),
                'first_seen': edge.first_seen,
                'last_seen': edge.last_seen,
            })

        return {
            'generated_at': datetime.now().isoformat(),
            'stats': dict(self.stats),
            'nodes': nodes_out,
            'edges': edges_out,
            'relay_chains': self.relay_chains[-200:],  # cap for size
        }

    def to_graphviz(self) -> str:
        lines = ['digraph MeshCoreTopology {']
        lines.append('    rankdir=LR;')
        lines.append('    node [fontname="Helvetica", fontsize=10];')
        lines.append('    edge [fontname="Helvetica", fontsize=9];')
        lines.append('')

        role_styles = {
            'relay':   ('box',     'steelblue', 'white'),
            'edge':    ('ellipse', 'gray40',    'white'),
            'unknown': ('ellipse', 'gray70',    'black'),
        }

        for node in self.nodes.values():
            shape, color, fontcolor = role_styles.get(node.role, role_styles['unknown'])
            label = node.name or node.node_hash[:8]
            # Show channel as second line if known
            if node.channel:
                label += f'\\n[{node.channel}]'
            lines.append(
                f'    "{node.node_hash}" [label="{label}", shape={shape}, '
                f'style=filled, fillcolor="{color}", fontcolor="{fontcolor}"];'
            )

        lines.append('')

        for (src, dst), edge in self.edges.items():
            label = f'{edge.relay_count}x'
            top_ch = max(edge.channels, key=edge.channels.get) if edge.channels else None
            if top_ch:
                label += f'\\n{top_ch}'
            lines.append(
                f'    "{src}" -> "{dst}" [label="{label}", '
                f'penwidth={min(1 + edge.relay_count * 0.3, 4.0):.1f}];'
            )

        lines.append('}')
        return '\n'.join(lines)


# ---------------------------------------------------------------------------
# File loaders
# ---------------------------------------------------------------------------

def load_pcap(filepath: Path) -> List[Tuple[bytes, Optional[int]]]:
    """Return list of (raw_bytes, timestamp_ms) from PCAP."""
    packets = []
    with open(filepath, 'rb') as f:
        header = f.read(24)
        if len(header) < 24:
            raise ValueError("Invalid PCAP file")
        magic = struct.unpack('<I', header[:4])[0]
        if magic not in (0xa1b2c3d4, 0xa1b23c4d):
            raise ValueError(f"Invalid PCAP magic: 0x{magic:08x}")

        while True:
            ph = f.read(16)
            if len(ph) < 16:
                break
            ts_sec, ts_usec, incl_len, _ = struct.unpack('<IIII', ph)
            data = f.read(incl_len)
            if len(data) < incl_len:
                break
            ts_ms = ts_sec * 1000 + ts_usec // 1000
            # Strip 20-byte LoRa pseudo-header if present
            if len(data) > 20 and data[0] not in (0x00, 0x02, 0x04, 0x06, 0x08, 0x0C):
                data = data[20:]
            packets.append((data, ts_ms))
    return packets


def load_csv(filepath: Path) -> List[Tuple[bytes, Optional[int]]]:
    """Return list of (raw_bytes, timestamp_ms) from CSV export."""
    packets = []
    with open(filepath, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            protocol = row.get('protocol', '')
            if protocol and 'meshcore' not in protocol.lower():
                continue
            hex_data = row.get('hex_data') or row.get('data') or row.get('raw_hex')
            if not hex_data:
                continue
            try:
                raw = bytes.fromhex(hex_data.replace(' ', ''))
            except ValueError:
                continue
            ts = None
            ts_val = row.get('timestamp_ms') or row.get('timestamp')
            if ts_val:
                try:
                    ts = int(float(ts_val))
                except (ValueError, TypeError):
                    pass
            packets.append((raw, ts))
    return packets


# ---------------------------------------------------------------------------
# Live WebSocket monitoring
# ---------------------------------------------------------------------------

def monitor_live(host: str, analyzer: MeshCoreTopologyAnalyzer, watch: bool = False):
    try:
        import websocket
    except ImportError:
        print("ERROR: websocket-client required. Install: pip install websocket-client")
        sys.exit(1)

    ws_url = f"ws://{host}/ws"
    print(f"Connecting to {ws_url} ...")
    print("Ctrl+C to stop and print results.\n")

    def on_message(ws, message):
        try:
            msg = json.loads(message)
            analyzer.process_websocket_event(msg)
            if watch and analyzer.stats['total'] % 5 == 0:
                print('\033[2J\033[H', end='')
                print(analyzer.to_ascii())
        except (json.JSONDecodeError, Exception):
            pass

    def on_error(ws, error):
        print(f"WebSocket error: {error}")

    def on_close(ws, code, msg):
        print("\nWebSocket closed.")
        print(analyzer.to_ascii())

    def on_open(ws):
        print(f"Connected. Monitoring MeshCore traffic for topology...")

    app = websocket.WebSocketApp(
        ws_url,
        on_open=on_open,
        on_message=on_message,
        on_error=on_error,
        on_close=on_close,
    )

    try:
        app.run_forever()
    except KeyboardInterrupt:
        print("\nStopped.")
        print(analyzer.to_ascii())


# ---------------------------------------------------------------------------
# Demo mode
# ---------------------------------------------------------------------------

def generate_demo(analyzer: MeshCoreTopologyAnalyzer):
    """
    Synthetic RDUMesh-style network matching the canonical demo node IDs
    used in enhanced_live_visualizer.py and mesh_topology_analyzer.py.

    Real MeshCore hashes are 4 bytes in most networks; we use truncated
    versions of the demo node IDs so they map to the same visual identity.
    """
    import random
    rng = random.Random(42)

    # node_hash, name, channel, (originated, relayed paths)
    # Hash derived from demo canonical IDs: a42b, 401a, 598b, b3f4, 7c89
    nodes_def = [
        ("a42b8c56", "N1FJB",    "public"),   # high-relay gateway
        ("401acd4e", "KI4OTK",   "public"),   # relay
        ("598b29ce", "W4RDU",    "public"),   # relay
        ("b3f42a10", "KD9XYZ",   "public"),   # originator only
        ("7c891def", None,       None),        # unknown, custom PSK
    ]

    base_ts = int(datetime.now().timestamp() * 1000)

    # Build nodes first
    for h, name, ch in nodes_def:
        node = analyzer._node(h)
        if name:
            node.name = name
        if ch:
            node.channel = ch

    # Simulate relay chains:
    # Direct: b3f42a10 (KD9XYZ) -- heard directly
    # 1-relay: 401acd4e -> a42b8c56
    # 2-relay: 598b29ce -> 401acd4e -> a42b8c56
    # 3-relay: b3f42a10 -> 598b29ce -> 401acd4e -> a42b8c56
    # Unknown device 7c891def -- only heard direct, not relaying anything

    chains = [
        (["b3f42a10"], "public", "KD9XYZ: Good morning RDU mesh"),
        (["401acd4e"], "public", "KI4OTK: Anyone near Lake Crabtree?"),
        (["401acd4e", "a42b8c56"], "public", "KI4OTK: Test 2-hop"),
        (["598b29ce", "401acd4e", "a42b8c56"], "public", "W4RDU: 3-hop to gateway"),
        (["b3f42a10", "598b29ce", "401acd4e", "a42b8c56"], "public", "KD9XYZ: 4-hop chain"),
        (["b3f42a10", "598b29ce"], "public", None),
        (["598b29ce", "401acd4e"], "public", "W4RDU: Direct link test"),
        (["7c891def"], None, None),   # custom PSK — no decrypt
        (["401acd4e", "a42b8c56"], "public", "KI4OTK: Good signal today"),
        (["598b29ce", "401acd4e", "a42b8c56"], "public", "W4RDU: Another 3-hop"),
    ]

    for path, channel, text in chains:
        ts = base_ts + rng.randint(0, 3600000)
        # Feed via internal graph directly (no raw bytes needed for demo)
        orig = analyzer._node(path[0])
        orig.originated += 1
        orig.update_seen(ts)
        if channel and not orig.channel:
            orig.channel = channel

        for i in range(1, len(path)):
            relay_node = analyzer._node(path[i])
            relay_node.relayed += 1
            relay_node.update_seen(ts)
            edge = analyzer._edge(path[i - 1], path[i])
            edge.observe(ts, channel)

        if len(path) >= 2:
            analyzer.relay_chains.append({
                'path': path, 'channel': channel,
                'text': text, 'sender': analyzer.nodes[path[0]].name,
                'payload_type': 'MSG', 'ts': ts,
            })
            analyzer.stats['chained'] += 1
        else:
            analyzer.stats['direct'] += 1
        analyzer.stats['total'] += 1
        if channel:
            analyzer.stats[f'ch_{channel}'] += 1

    print(f"Demo: {len(analyzer.nodes)} nodes, {len(analyzer.edges)} edges, "
          f"{len(analyzer.relay_chains)} relay chains\n")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='MeshCore Topology Analyzer — extract relay graphs from path arrays',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  %(prog)s capture.pcap                    # Analyze PCAP capture
  %(prog)s capture.csv --format csv        # Analyze CSV export
  %(prog)s --live 192.168.4.1              # Live WebSocket monitoring
  %(prog)s --live 192.168.4.1 --watch      # Live monitoring with auto-refresh
  %(prog)s capture.pcap --format graphviz -o mesh.dot
  %(prog)s --demo                          # Demo mode (no input needed)

Render Graphviz output:
  dot -Tpng -o mesh.png mesh.dot
  neato -Tsvg -o mesh.svg mesh.dot        # neato gives better spatial layout
''',
    )
    parser.add_argument('input', nargs='?', help='Input PCAP or CSV file')
    parser.add_argument('--format', choices=['ascii', 'json', 'graphviz'], default='ascii',
                        help='Output format (default: ascii)')
    parser.add_argument('--live', metavar='HOST',
                        help='Live mode: connect to ESP32 WebSocket at HOST')
    parser.add_argument('--watch', action='store_true',
                        help='Auto-refresh display in live mode')
    parser.add_argument('-o', '--output', metavar='FILE', help='Write output to file')
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbose packet tracing')
    parser.add_argument('--demo', action='store_true', help='Demo mode with synthetic RDU mesh')

    args = parser.parse_args()

    analyzer = MeshCoreTopologyAnalyzer(verbose=args.verbose)

    if args.demo:
        print("DEMO MODE: Synthetic RDU-style MeshCore network\n")
        generate_demo(analyzer)
        _output(analyzer, args)
        return 0

    if args.live:
        monitor_live(args.live, analyzer, watch=args.watch)
        return 0

    if not args.input:
        parser.print_help()
        return 1

    path = Path(args.input)
    if not path.exists():
        print(f"ERROR: File not found: {path}")
        return 1

    print(f"Loading {path} ...")
    ext = path.suffix.lower()
    if ext in ('.pcap', '.pcapng'):
        packets = load_pcap(path)
    elif ext == '.csv':
        packets = load_csv(path)
    else:
        print(f"ERROR: Unsupported format '{ext}'. Use .pcap or .csv")
        return 1

    print(f"Loaded {len(packets)} packets. Analyzing...\n")
    for data, ts in packets:
        analyzer.process_raw(data, ts)

    _output(analyzer, args)
    return 0


def _output(analyzer: MeshCoreTopologyAnalyzer, args):
    if args.format == 'ascii':
        text = analyzer.to_ascii()
    elif args.format == 'json':
        text = json.dumps(analyzer.to_json(), indent=2, default=str)
    elif args.format == 'graphviz':
        text = analyzer.to_graphviz()
    else:
        text = analyzer.to_ascii()

    if args.output:
        with open(args.output, 'w', encoding='utf-8') as f:
            f.write(text)
        print(f"Saved to {args.output}")
        if args.format == 'graphviz':
            print(f"Render: dot -Tpng -o mesh.png {args.output}")
            print(f"        neato -Tsvg -o mesh.svg {args.output}")
    else:
        print(text)


if __name__ == '__main__':
    sys.exit(main())
