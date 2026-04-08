#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - Mesh Topology Analyzer

Parses Meshtastic traceroute and neighborinfo packets to build mesh structure.
Identifies gateway nodes, edge nodes, and calculates mesh density metrics.

NEW: Integrated PSK decryption - automatically tries 23 default keys on
encrypted packets to extract topology data.

Output formats:
- JSON graph (nodes + edges with hop counts)
- ASCII visualization for terminal
- Optional networkx/graphviz export

Usage:
    python mesh_topology_analyzer.py capture.csv
    python mesh_topology_analyzer.py capture.pcap --format graphviz
    python mesh_topology_analyzer.py --live 192.168.4.1 --watch
    python mesh_topology_analyzer.py capture.csv --ascii
    python mesh_topology_analyzer.py capture.csv --no-decrypt  # Skip PSK testing

Requirements:
    pip install requests (for live monitoring)
    pip install networkx (optional, for advanced analysis)
    pip install graphviz (optional, for DOT export)
    pip install cryptography (for PSK decryption)
"""

import argparse
import csv
import json
import struct
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
from collections import defaultdict
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

try:
    import requests
    REQUESTS_AVAILABLE = True
except ImportError:
    REQUESTS_AVAILABLE = False

try:
    import networkx as nx
    NETWORKX_AVAILABLE = True
except ImportError:
    NETWORKX_AVAILABLE = False

# PSK decryption support
try:
    from psk_decrypt import MeshtasticDecryptor
    PSK_DECRYPT_AVAILABLE = True
except ImportError:
    PSK_DECRYPT_AVAILABLE = False


# Meshtastic portnum values
PORT_TRACEROUTE = 70      # 0x46
PORT_NEIGHBORINFO = 71    # 0x47
PORT_POSITION = 3
PORT_NODEINFO = 4


class MeshNode:
    """Represents a node in the mesh network"""
    
    def __init__(self, node_id: str):
        self.node_id = node_id
        self.short_name: Optional[str] = None
        self.long_name: Optional[str] = None
        self.first_seen: Optional[int] = None
        self.last_seen: Optional[int] = None
        self.packet_count = 0
        
        # Position data
        self.latitude: Optional[float] = None
        self.longitude: Optional[float] = None
        self.altitude: Optional[float] = None
        
        # Connectivity
        self.neighbors: Dict[str, NeighborInfo] = {}  # node_id -> NeighborInfo
        self.routes: List[List[str]] = []  # Traceroute paths through this node
        
        # Metrics
        self.is_gateway = False
        self.is_edge = False
        self.hop_count_from_us: Optional[int] = None
        self.avg_snr: Optional[float] = None
        
    def update_seen(self, timestamp_ms: int):
        """Update first/last seen timestamps"""
        if self.first_seen is None:
            self.first_seen = timestamp_ms
        self.last_seen = timestamp_ms
        self.packet_count += 1


class NeighborInfo:
    """Information about a neighbor relationship"""
    
    def __init__(self, neighbor_id: str):
        self.neighbor_id = neighbor_id
        self.snr: Optional[float] = None
        self.last_heard: Optional[int] = None
        self.bidirectional = False
        self.hop_count = 1


class MeshEdge:
    """Represents an edge (link) between two nodes"""
    
    def __init__(self, node_a: str, node_b: str):
        # Normalize edge direction for consistent lookup
        self.node_a, self.node_b = sorted([node_a, node_b])
        self.first_seen: Optional[int] = None
        self.last_seen: Optional[int] = None
        self.packet_count = 0
        self.snr_readings: List[float] = []
        self.hop_count = 1
        self.bidirectional = False
    
    @property
    def key(self) -> Tuple[str, str]:
        return (self.node_a, self.node_b)
    
    @property
    def avg_snr(self) -> Optional[float]:
        if not self.snr_readings:
            return None
        return sum(self.snr_readings) / len(self.snr_readings)


class MeshTopologyAnalyzer:
    """Build and analyze mesh network topology from packet captures"""
    
    def __init__(self, verbose=False):
        self.verbose = verbose
        self.nodes: Dict[str, MeshNode] = {}
        self.edges: Dict[Tuple[str, str], MeshEdge] = {}
        self.traceroutes: List[Dict] = []
        self.stats = {
            'total_packets': 0,
            'traceroute_packets': 0,
            'neighborinfo_packets': 0,
            'position_packets': 0,
            'nodeinfo_packets': 0,
        }
    
    def get_or_create_node(self, node_id: str) -> MeshNode:
        """Get existing node or create new one"""
        if node_id not in self.nodes:
            self.nodes[node_id] = MeshNode(node_id)
        return self.nodes[node_id]
    
    def get_or_create_edge(self, node_a: str, node_b: str) -> MeshEdge:
        """Get existing edge or create new one"""
        edge = MeshEdge(node_a, node_b)
        key = edge.key
        if key not in self.edges:
            self.edges[key] = edge
        return self.edges[key]
    
    def process_traceroute(self, timestamp_ms: int, source: str, dest: str, 
                          route: List[str], snr_list: Optional[List[float]] = None):
        """Process a traceroute packet"""
        self.stats['traceroute_packets'] += 1
        
        # Record full path
        full_path = [source] + route + [dest]
        self.traceroutes.append({
            'timestamp': timestamp_ms,
            'source': source,
            'dest': dest,
            'path': full_path,
            'hop_count': len(route) + 1,
        })
        
        if self.verbose:
            print(f"[TRACEROUTE] {source} -> {' -> '.join(route)} -> {dest}")
        
        # Create nodes and edges for path
        for i, node_id in enumerate(full_path):
            node = self.get_or_create_node(node_id)
            node.update_seen(timestamp_ms)
            node.routes.append(full_path)
            
            # Set hop count from source
            if node.hop_count_from_us is None or i < node.hop_count_from_us:
                node.hop_count_from_us = i
        
        # Create edges between consecutive nodes
        for i in range(len(full_path) - 1):
            edge = self.get_or_create_edge(full_path[i], full_path[i + 1])
            edge.packet_count += 1
            if edge.first_seen is None:
                edge.first_seen = timestamp_ms
            edge.last_seen = timestamp_ms
            
            if snr_list and i < len(snr_list):
                edge.snr_readings.append(snr_list[i])
    
    def process_neighborinfo(self, timestamp_ms: int, reporting_node: str,
                            neighbors: List[Dict]):
        """Process a neighborinfo packet"""
        self.stats['neighborinfo_packets'] += 1
        
        node = self.get_or_create_node(reporting_node)
        node.update_seen(timestamp_ms)
        
        if self.verbose:
            print(f"[NEIGHBORINFO] {reporting_node} has {len(neighbors)} neighbors")
        
        for neighbor_data in neighbors:
            neighbor_id = neighbor_data.get('node_id')
            if not neighbor_id:
                continue
            
            # Update neighbor node
            neighbor_node = self.get_or_create_node(neighbor_id)
            neighbor_node.update_seen(timestamp_ms)
            
            # Record neighbor relationship
            info = NeighborInfo(neighbor_id)
            info.snr = neighbor_data.get('snr')
            info.last_heard = neighbor_data.get('last_heard')
            node.neighbors[neighbor_id] = info
            
            # Create edge
            edge = self.get_or_create_edge(reporting_node, neighbor_id)
            edge.packet_count += 1
            if edge.first_seen is None:
                edge.first_seen = timestamp_ms
            edge.last_seen = timestamp_ms
            
            if info.snr is not None:
                edge.snr_readings.append(info.snr)
            
            # Check for bidirectional link
            if neighbor_id in self.nodes:
                if reporting_node in self.nodes[neighbor_id].neighbors:
                    edge.bidirectional = True
                    info.bidirectional = True
    
    def process_packet(self, packet: Dict):
        """Process a single packet from capture data"""
        self.stats['total_packets'] += 1
        
        timestamp = packet.get('timestamp_ms', 0)
        protocol = packet.get('protocol', '').lower()
        port = packet.get('portnum', packet.get('port', -1))
        
        # Extract source node
        source = packet.get('from', packet.get('source', packet.get('node_id')))
        if not source:
            return
        
        # Normalize node ID format
        if isinstance(source, int):
            source = f"!{source:08x}"
        elif not source.startswith('!'):
            source = f"!{source}"
        
        # Update source node
        node = self.get_or_create_node(source)
        node.update_seen(timestamp)
        
        # Process by port type
        if port == PORT_TRACEROUTE or 'traceroute' in str(packet.get('app', '')).lower():
            route = packet.get('route', packet.get('route_list', []))
            dest = packet.get('to', packet.get('dest'))
            if dest:
                if isinstance(dest, int):
                    dest = f"!{dest:08x}"
                elif not dest.startswith('!'):
                    dest = f"!{dest}"
                self.process_traceroute(timestamp, source, dest, route)
        
        elif port == PORT_NEIGHBORINFO or 'neighborinfo' in str(packet.get('app', '')).lower():
            neighbors = packet.get('neighbors', packet.get('neighbor_list', []))
            self.process_neighborinfo(timestamp, source, neighbors)
        
        elif port == PORT_POSITION:
            self.stats['position_packets'] += 1
            lat = packet.get('latitude')
            lon = packet.get('longitude')
            if lat is not None and lon is not None:
                node.latitude = lat
                node.longitude = lon
                node.altitude = packet.get('altitude')
        
        elif port == PORT_NODEINFO:
            self.stats['nodeinfo_packets'] += 1
            node.short_name = packet.get('short_name')
            node.long_name = packet.get('long_name')
    
    def classify_nodes(self):
        """Classify nodes as gateway, relay, or edge"""
        for node_id, node in self.nodes.items():
            neighbor_count = len(node.neighbors)
            route_count = len(node.routes)
            
            # Gateway: many neighbors OR appears in many routes as relay
            relay_appearances = sum(
                1 for route in self.traceroutes
                if node_id in route['path'][1:-1]  # Not source or dest
            )
            
            if neighbor_count >= 4 or relay_appearances >= 3:
                node.is_gateway = True
            elif neighbor_count <= 1 and relay_appearances == 0:
                node.is_edge = True
    
    def calculate_metrics(self) -> Dict:
        """Calculate mesh topology metrics"""
        self.classify_nodes()
        
        total_nodes = len(self.nodes)
        total_edges = len(self.edges)
        
        if total_nodes == 0:
            return {
                'error': 'No nodes found in capture',
                'total_packets': self.stats['total_packets'],
            }
        
        gateway_count = sum(1 for n in self.nodes.values() if n.is_gateway)
        edge_count = sum(1 for n in self.nodes.values() if n.is_edge)
        bidirectional_edges = sum(1 for e in self.edges.values() if e.bidirectional)
        
        # Average neighbors per node
        avg_neighbors = sum(len(n.neighbors) for n in self.nodes.values()) / total_nodes
        
        # Mesh density: actual edges / possible edges
        max_edges = total_nodes * (total_nodes - 1) / 2
        density = total_edges / max_edges if max_edges > 0 else 0
        
        # Average hop count
        hop_counts = [
            t['hop_count'] for t in self.traceroutes if t['hop_count'] > 0
        ]
        avg_hops = sum(hop_counts) / len(hop_counts) if hop_counts else 0
        
        return {
            'total_nodes': total_nodes,
            'total_edges': total_edges,
            'gateway_nodes': gateway_count,
            'edge_nodes': edge_count,
            'relay_nodes': total_nodes - gateway_count - edge_count,
            'bidirectional_links': bidirectional_edges,
            'unidirectional_links': total_edges - bidirectional_edges,
            'avg_neighbors': round(avg_neighbors, 2),
            'mesh_density': round(density, 4),
            'total_traceroutes': len(self.traceroutes),
            'avg_hop_count': round(avg_hops, 2),
            'packets_analyzed': self.stats,
        }
    
    def to_json(self) -> Dict:
        """Export topology as JSON"""
        metrics = self.calculate_metrics()
        
        nodes_json = []
        for node_id, node in self.nodes.items():
            nodes_json.append({
                'id': node_id,
                'short_name': node.short_name,
                'long_name': node.long_name,
                'first_seen': node.first_seen,
                'last_seen': node.last_seen,
                'packet_count': node.packet_count,
                'latitude': node.latitude,
                'longitude': node.longitude,
                'neighbor_count': len(node.neighbors),
                'is_gateway': node.is_gateway,
                'is_edge': node.is_edge,
                'hop_count': node.hop_count_from_us,
            })
        
        edges_json = []
        for edge in self.edges.values():
            edges_json.append({
                'source': edge.node_a,
                'target': edge.node_b,
                'packet_count': edge.packet_count,
                'first_seen': edge.first_seen,
                'last_seen': edge.last_seen,
                'avg_snr': edge.avg_snr,
                'bidirectional': edge.bidirectional,
                'hop_count': edge.hop_count,
            })
        
        return {
            'metrics': metrics,
            'nodes': nodes_json,
            'edges': edges_json,
            'traceroutes': self.traceroutes,
            'generated_at': datetime.now().isoformat(),
        }
    
    def to_graphviz(self) -> str:
        """Export topology as Graphviz DOT format"""
        lines = ['digraph MeshNetwork {']
        lines.append('    rankdir=LR;')
        lines.append('    node [shape=ellipse];')
        lines.append('')
        
        # Node definitions
        for node_id, node in self.nodes.items():
            label = node.short_name or node_id[-6:]
            
            if node.is_gateway:
                color = 'green'
                shape = 'box'
            elif node.is_edge:
                color = 'gray'
                shape = 'ellipse'
            else:
                color = 'blue'
                shape = 'ellipse'
            
            lines.append(f'    "{node_id}" [label="{label}", color={color}, shape={shape}];')
        
        lines.append('')
        
        # Edge definitions
        for edge in self.edges.values():
            style = 'solid' if edge.bidirectional else 'dashed'
            label = f'{edge.packet_count}p'
            if edge.avg_snr is not None:
                label += f' ({edge.avg_snr:.1f}dB)'
            
            if edge.bidirectional:
                lines.append(f'    "{edge.node_a}" -> "{edge.node_b}" '
                           f'[label="{label}", style={style}, dir=both];')
            else:
                lines.append(f'    "{edge.node_a}" -> "{edge.node_b}" '
                           f'[label="{label}", style={style}];')
        
        lines.append('}')
        return '\n'.join(lines)
    
    def to_ascii(self) -> str:
        """Generate ASCII visualization of topology"""
        lines = []
        lines.append('=' * 60)
        lines.append('MESH TOPOLOGY ANALYSIS')
        lines.append('=' * 60)
        lines.append('')
        
        metrics = self.calculate_metrics()

        if 'error' in metrics:
            lines.append(f"No topology data: {metrics['error']}")
            lines.append(f"Packets analyzed: {metrics.get('total_packets', 0)}")
            return '\n'.join(lines)

        # Summary
        lines.append(f"Total Nodes: {metrics['total_nodes']}")
        lines.append(f"Total Links: {metrics['total_edges']}")
        lines.append(f"Mesh Density: {metrics['mesh_density']:.2%}")
        lines.append(f"Avg Neighbors: {metrics['avg_neighbors']:.1f}")
        lines.append('')
        
        # Node classification
        lines.append('NODE CLASSIFICATION:')
        lines.append('-' * 40)
        lines.append(f"  Gateway Nodes:  {metrics['gateway_nodes']}")
        lines.append(f"  Relay Nodes:    {metrics['relay_nodes']}")
        lines.append(f"  Edge Nodes:     {metrics['edge_nodes']}")
        lines.append('')
        
        # Link quality
        lines.append('LINK QUALITY:')
        lines.append('-' * 40)
        lines.append(f"  Bidirectional:  {metrics['bidirectional_links']}")
        lines.append(f"  Unidirectional: {metrics['unidirectional_links']}")
        lines.append('')
        
        # Node details
        lines.append('NODE DETAILS:')
        lines.append('-' * 40)
        
        # Sort by neighbor count (gateways first)
        sorted_nodes = sorted(
            self.nodes.values(),
            key=lambda n: (n.is_gateway, len(n.neighbors)),
            reverse=True
        )
        
        for node in sorted_nodes:
            role = '[GW]' if node.is_gateway else '[EDGE]' if node.is_edge else '[RELAY]'
            name = node.short_name or node.node_id[-8:]
            neighbors = len(node.neighbors)
            lines.append(f"  {role:8} {name:12} - {neighbors} neighbors")
            
            # Show neighbor connections
            for neighbor_id in list(node.neighbors.keys())[:5]:
                neighbor = self.nodes.get(neighbor_id)
                neighbor_name = neighbor.short_name if neighbor else neighbor_id[-8:]
                info = node.neighbors[neighbor_id]
                bidir = '↔' if info.bidirectional else '→'
                snr = f' ({info.snr:.1f}dB)' if info.snr else ''
                lines.append(f"           {bidir} {neighbor_name}{snr}")
            
            if len(node.neighbors) > 5:
                lines.append(f"           ... and {len(node.neighbors) - 5} more")
        
        lines.append('')
        
        # Traceroute paths
        if self.traceroutes:
            lines.append('TRACEROUTE PATHS:')
            lines.append('-' * 40)
            for tr in self.traceroutes[:10]:
                path_str = ' → '.join(p[-6:] for p in tr['path'])
                lines.append(f"  {path_str} ({tr['hop_count']} hops)")
            
            if len(self.traceroutes) > 10:
                lines.append(f"  ... and {len(self.traceroutes) - 10} more paths")
        
        lines.append('')
        lines.append('=' * 60)
        
        return '\n'.join(lines)
    
    def to_networkx(self):
        """Export to NetworkX graph (requires networkx package)"""
        if not NETWORKX_AVAILABLE:
            raise ImportError("NetworkX not installed. Run: pip install networkx")
        
        G = nx.Graph()
        
        # Add nodes
        for node_id, node in self.nodes.items():
            G.add_node(
                node_id,
                short_name=node.short_name,
                is_gateway=node.is_gateway,
                is_edge=node.is_edge,
                latitude=node.latitude,
                longitude=node.longitude,
            )
        
        # Add edges
        for edge in self.edges.values():
            G.add_edge(
                edge.node_a,
                edge.node_b,
                weight=edge.packet_count,
                snr=edge.avg_snr,
                bidirectional=edge.bidirectional,
            )
        
        return G

    def to_matplotlib(self, output_file=None) -> None:
        """Render topology graph using networkx + matplotlib. No Graphviz needed."""
        try:
            import networkx as nx
            import matplotlib.pyplot as plt
            import matplotlib.patches as mpatches
        except ImportError:
            print("ERROR: networkx and matplotlib required. Install: pip install networkx matplotlib")
            return

        G = nx.DiGraph()

        for node_id, node in self.nodes.items():
            label = node.short_name or node_id[-6:]
            if node.is_gateway:
                color, shape = '#2ecc71', 's'
            elif node.is_edge:
                color, shape = '#555555', 'o'
            else:
                color, shape = '#4682b4', 'o'
            G.add_node(node_id, label=label, color=color, shape=shape,
                       is_gateway=node.is_gateway)

        for edge in self.edges.values():
            G.add_edge(edge.node_a, edge.node_b,
                       weight=edge.packet_count,
                       snr=edge.avg_snr,
                       bidirectional=edge.bidirectional)
            if edge.bidirectional:
                G.add_edge(edge.node_b, edge.node_a,
                           weight=edge.packet_count,
                           snr=edge.avg_snr,
                           bidirectional=True)

        if len(G.nodes) == 0:
            print("No nodes to render.")
            return

        fig, ax = plt.subplots(figsize=(13, 9))
        ax.set_facecolor('#1a1a2e')
        fig.patch.set_facecolor('#1a1a2e')

        try:
            pos = nx.spring_layout(G, seed=42, k=2.5)
        except Exception:
            pos = nx.circular_layout(G)

        for shape in ('s', 'o'):
            nodes_in_group = [n for n, d in G.nodes(data=True) if d.get('shape') == shape]
            if not nodes_in_group:
                continue
            colors = [G.nodes[n]['color'] for n in nodes_in_group]
            nx.draw_networkx_nodes(G, pos, nodelist=nodes_in_group,
                                   node_color=colors, node_shape=shape,
                                   node_size=900, ax=ax, alpha=0.9)

        edge_widths = [1 + G.edges[e].get('weight', 1) * 0.3 for e in G.edges()]
        nx.draw_networkx_edges(G, pos, width=edge_widths,
                               edge_color='#88aacc', arrows=True,
                               arrowsize=18, ax=ax,
                               connectionstyle='arc3,rad=0.08')

        labels = {n: d.get('label', n[-6:]) for n, d in G.nodes(data=True)}
        nx.draw_networkx_labels(G, pos, labels, font_size=8,
                                font_color='white', ax=ax)

        edge_labels = {}
        seen = set()
        for e in G.edges(data=True):
            key = tuple(sorted([e[0], e[1]]))
            if key not in seen:
                seen.add(key)
                snr = e[2].get('snr')
                w = e[2].get('weight', 1)
                lbl = f"{w}p"
                if snr is not None:
                    lbl += f"\n{snr:.1f}dB"
                edge_labels[(e[0], e[1])] = lbl
        nx.draw_networkx_edge_labels(G, pos, edge_labels, font_size=7,
                                     font_color='#ccddff', ax=ax)

        legend_items = [
            mpatches.Patch(color='#2ecc71', label='Gateway'),
            mpatches.Patch(color='#4682b4', label='Router'),
            mpatches.Patch(color='#555555', label='Edge node'),
        ]
        ax.legend(handles=legend_items, loc='upper left',
                  facecolor='#2a2a4a', labelcolor='white', fontsize=9)

        ax.set_title('Meshtastic Network Topology', color='white', fontsize=14, pad=12)
        ax.axis('off')
        plt.tight_layout()

        if output_file:
            plt.savefig(output_file, dpi=150, bbox_inches='tight',
                        facecolor=fig.get_facecolor())
            print(f"Saved to {output_file}")
        else:
            plt.show()


# ============================================================================
# Meshtastic Protobuf Parsing (lightweight, no protobuf dependency)
# ============================================================================

def decode_varint(data: bytes, offset: int = 0) -> Tuple[int, int]:
    """Decode a protobuf varint, return (value, bytes_consumed)"""
    result = 0
    shift = 0
    consumed = 0
    
    while offset + consumed < len(data):
        byte = data[offset + consumed]
        consumed += 1
        result |= (byte & 0x7F) << shift
        shift += 7
        if (byte & 0x80) == 0:
            break
    
    return result, consumed


def parse_meshtastic_header(raw_data: bytes) -> Optional[Dict]:
    """
    Parse Meshtastic packet header (unencrypted part).

    Header format (16 bytes):
    - 4 bytes: Destination node ID (0xFFFFFFFF = broadcast, otherwise specific node)
    - 4 bytes: Source node ID (sender, little-endian)
    - 4 bytes: Packet ID (little-endian)
    - 1 byte: Flags (hop_limit[2:0] | want_ack[3] | via_mqtt[4])
    - 1 byte: Channel hash
    - 2 bytes: Reserved/padding
    """
    if len(raw_data) < 16:
        return None

    dest_raw = struct.unpack('<I', raw_data[0:4])[0]
    node_id = struct.unpack('<I', raw_data[4:8])[0]

    # Sanity check: source node ID must be non-zero
    if node_id == 0:
        return None

    packet_id = struct.unpack('<I', raw_data[8:12])[0]
    flags = raw_data[12]
    channel_hash = raw_data[13]

    return {
        'node_id': f"!{node_id:08x}",
        'dest': f"!{dest_raw:08x}" if dest_raw != 0xFFFFFFFF else '!ffffffff',
        'packet_id': packet_id,
        'hop_limit': flags & 0x07,
        'want_ack': bool(flags & 0x08),
        'via_mqtt': bool(flags & 0x10),
        'channel_hash': channel_hash,
        'payload': raw_data[16:],  # Encrypted or unencrypted payload
    }


def parse_meshtastic_data(payload: bytes) -> Optional[Dict]:
    """
    Parse decrypted Meshtastic Data protobuf.
    
    Looks for:
    - Field 1 (portnum): varint
    - Field 2 (payload): bytes
    - Field 3 (want_response): bool
    - Field 4 (dest): fixed32
    - Field 5 (source): fixed32
    """
    result = {'portnum': None, 'inner_payload': None, 'dest': None, 'source': None}
    offset = 0
    
    while offset < len(payload):
        if offset >= len(payload):
            break
            
        # Read field tag
        tag, consumed = decode_varint(payload, offset)
        offset += consumed
        
        field_num = tag >> 3
        wire_type = tag & 0x07
        
        if field_num == 1 and wire_type == 0:  # portnum (varint)
            result['portnum'], consumed = decode_varint(payload, offset)
            offset += consumed
        elif field_num == 2 and wire_type == 2:  # payload (length-delimited)
            length, consumed = decode_varint(payload, offset)
            offset += consumed
            result['inner_payload'] = payload[offset:offset + length]
            offset += length
        elif field_num == 4 and wire_type == 5:  # dest (fixed32)
            if offset + 4 <= len(payload):
                result['dest'] = struct.unpack('<I', payload[offset:offset+4])[0]
                offset += 4
        elif field_num == 5 and wire_type == 5:  # source (fixed32)
            if offset + 4 <= len(payload):
                result['source'] = struct.unpack('<I', payload[offset:offset+4])[0]
                offset += 4
        elif wire_type == 0:  # varint - skip
            _, consumed = decode_varint(payload, offset)
            offset += consumed
        elif wire_type == 2:  # length-delimited - skip
            length, consumed = decode_varint(payload, offset)
            offset += consumed + length
        elif wire_type == 5:  # fixed32 - skip
            offset += 4
        elif wire_type == 1:  # fixed64 - skip
            offset += 8
        else:
            break  # Unknown wire type
    
    return result if result['portnum'] is not None else None


def parse_traceroute_response(payload: bytes) -> Optional[List[str]]:
    """
    Parse RouteDiscovery protobuf (traceroute response).
    
    Field 1 (route): repeated fixed32
    """
    route = []
    offset = 0
    
    while offset < len(payload):
        tag, consumed = decode_varint(payload, offset)
        offset += consumed
        
        field_num = tag >> 3
        wire_type = tag & 0x07
        
        if field_num == 1 and wire_type == 5:  # fixed32
            if offset + 4 <= len(payload):
                node_id = struct.unpack('<I', payload[offset:offset+4])[0]
                route.append(f"!{node_id:08x}")
                offset += 4
        elif wire_type == 0:
            _, consumed = decode_varint(payload, offset)
            offset += consumed
        elif wire_type == 2:
            length, consumed = decode_varint(payload, offset)
            offset += consumed + length
        elif wire_type == 5:
            offset += 4
        else:
            break
    
    return route if route else None


def parse_neighborinfo(payload: bytes) -> Optional[List[Dict]]:
    """
    Parse NeighborInfo protobuf.
    
    Field 1 (node_id): fixed32
    Field 2 (last_sent_by_id): fixed32  
    Field 3 (node_broadcast_interval_secs): uint32
    Field 4 (neighbors): repeated Neighbor message
    
    Neighbor message:
    - Field 1 (node_id): fixed32
    - Field 2 (snr): int32 (as varint, can be negative)
    """
    neighbors = []
    offset = 0
    
    while offset < len(payload):
        tag, consumed = decode_varint(payload, offset)
        offset += consumed
        
        field_num = tag >> 3
        wire_type = tag & 0x07
        
        if field_num == 4 and wire_type == 2:  # neighbors (repeated message)
            length, consumed = decode_varint(payload, offset)
            offset += consumed
            neighbor_data = payload[offset:offset + length]
            offset += length
            
            # Parse Neighbor submessage
            neighbor = {'node_id': None, 'snr': None}
            sub_offset = 0
            while sub_offset < len(neighbor_data):
                sub_tag, sub_consumed = decode_varint(neighbor_data, sub_offset)
                sub_offset += sub_consumed
                
                sub_field = sub_tag >> 3
                sub_wire = sub_tag & 0x07
                
                if sub_field == 1 and sub_wire == 5:  # node_id (fixed32)
                    if sub_offset + 4 <= len(neighbor_data):
                        nid = struct.unpack('<I', neighbor_data[sub_offset:sub_offset+4])[0]
                        neighbor['node_id'] = f"!{nid:08x}"
                        sub_offset += 4
                elif sub_field == 2 and sub_wire == 0:  # snr (varint)
                    snr_raw, sub_consumed = decode_varint(neighbor_data, sub_offset)
                    # Handle zigzag encoding for signed int
                    neighbor['snr'] = (snr_raw >> 1) ^ -(snr_raw & 1)
                    sub_offset += sub_consumed
                elif sub_wire == 0:
                    _, sub_consumed = decode_varint(neighbor_data, sub_offset)
                    sub_offset += sub_consumed
                elif sub_wire == 5:
                    sub_offset += 4
                else:
                    break
            
            if neighbor['node_id']:
                neighbors.append(neighbor)
        
        elif wire_type == 0:
            _, consumed = decode_varint(payload, offset)
            offset += consumed
        elif wire_type == 2:
            length, consumed = decode_varint(payload, offset)
            offset += consumed + length
        elif wire_type == 5:
            offset += 4
        else:
            break
    
    return neighbors if neighbors else None


def load_csv(filepath: Path, try_decrypt: bool = True) -> List[Dict]:
    """
    Load and parse packets from CSV file with raw hex data.
    
    Args:
        filepath: Path to CSV file
        try_decrypt: If True, attempt PSK decryption on encrypted packets
        
    Returns:
        List of parsed packet dicts with topology data
    """
    packets = []
    decryption_stats = {'attempted': 0, 'successful': 0}
    
    # Initialize decryptor if available
    decryptor = None
    if try_decrypt and PSK_DECRYPT_AVAILABLE:
        try:
            decryptor = MeshtasticDecryptor(verbose=False)
            print("[*] PSK decryption enabled - will try 23 default keys")
        except Exception as e:
            print(f"[!] PSK decryption unavailable: {e}")
    
    has_raw_column = None  # determined on first row

    with open(filepath, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            # Process Meshtastic and Unknown rows (Unknown = directed Meshtastic
            # packets that pcap_analyzer couldn't identify as broadcast)
            protocol = row.get('protocol', '').lower()
            if protocol not in ('meshtastic', 'unknown'):
                continue

            # Detect whether this CSV has raw packet bytes
            if has_raw_column is None:
                has_raw_column = bool(
                    row.get('raw_hex') or row.get('raw') or row.get('data') or row.get('dataHex')
                )
                if not has_raw_column:
                    print("[!] CSV has no raw packet column (raw_hex/data) — "
                          "showing device inventory only (no routing topology)")

            # Get raw hex data
            raw_hex = row.get('raw_hex', row.get('raw', row.get('data', row.get('dataHex', ''))))
            if not raw_hex:
                # Fallback: build a minimal node-seen record from CSV columns
                node_id = row.get('node_id_hex', row.get('nodeId', row.get('node_id', '')))
                if not node_id:
                    continue
                # Normalize to !xxxxxxxx format
                nid = node_id[2:].lower() if node_id.lower().startswith('0x') else node_id.lower()
                packet = {
                    'timestamp_ms': int(row['timestamp_ms']) if row.get('timestamp_ms') else 0,
                    'protocol': 'Meshtastic',
                    'from': f"!{nid}",
                    'rssi': float(row.get('rssi_dbm', row.get('rssi', -100)) or -100),
                }
                packets.append(packet)
                continue
            
            try:
                raw_bytes = bytes.fromhex(raw_hex.replace(' ', ''))
            except ValueError:
                continue
            
            # Parse header
            header = parse_meshtastic_header(raw_bytes)
            if not header:
                continue
            
            # Build packet dict
            packet = {
                'timestamp_ms': int(row.get('timestamp_ms', 0)) if row.get('timestamp_ms') else 0,
                'protocol': 'Meshtastic',
                'from': header['node_id'],
                'rssi': float(row.get('rssi_dbm', row.get('rssi', -100))),
                'raw_payload': header['payload'],
            }
            
            # Try to parse payload (may be encrypted)
            payload = header['payload']
            data = None
            
            # Check if unencrypted (starts with 0x08 = field 1 varint)
            if len(payload) >= 2 and payload[0] == 0x08:
                data = parse_meshtastic_data(payload)
            
            # If encrypted (doesn't start with valid protobuf), try decryption
            elif decryptor and len(payload) >= 6:
                decryption_stats['attempted'] += 1
                result = decryptor.try_decrypt(raw_bytes)
                if result and result.plaintext:
                    decryption_stats['successful'] += 1
                    # Parse the decrypted payload
                    data = parse_meshtastic_data(result.plaintext)
                    if data:
                        packet['decrypted'] = True
                        packet['psk_key'] = result.key_name
            
            if data:
                packet['portnum'] = data['portnum']
                packet['to'] = f"!{data['dest']:08x}" if data['dest'] else None
                
                # Check for traceroute/neighborinfo
                inner = data.get('inner_payload')
                if inner:
                    if data['portnum'] == PORT_TRACEROUTE:
                        route = parse_traceroute_response(inner)
                        if route:
                            packet['route'] = route
                    
                    elif data['portnum'] == PORT_NEIGHBORINFO:
                        neighbors = parse_neighborinfo(inner)
                        if neighbors:
                            packet['neighbors'] = neighbors
            
            packets.append(packet)
    
    # Report decryption stats
    if decryptor and decryption_stats['attempted'] > 0:
        print(f"[*] Decryption: {decryption_stats['successful']}/{decryption_stats['attempted']} "
              f"packets decrypted ({decryption_stats['successful']/decryption_stats['attempted']*100:.1f}%)")
        stats = decryptor.get_stats()
        if stats['key_hits']:
            print("[*] Keys that worked:")
            for key_name, count in stats['key_hits'].items():
                print(f"      {key_name}: {count} packets")
    
    return packets


def load_pcap(filepath: Path, try_decrypt: bool = True) -> List[Dict]:
    """Load packets from PCAP file using our custom format with Meshtastic parsing"""
    # Import from parent tools/ directory
    import sys
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
    from pcap_analyzer import parse_pcap_native
    
    raw_packets = parse_pcap_native(str(filepath))
    
    # Initialize decryptor if available
    decryptor = None
    if try_decrypt and PSK_DECRYPT_AVAILABLE:
        try:
            decryptor = MeshtasticDecryptor(verbose=False)
        except Exception:
            pass
    
    packets = []
    for pkt in raw_packets:
        # Only process Meshtastic packets
        if pkt.protocol != 'Meshtastic':
            continue
        
        # Parse header from raw data
        header = parse_meshtastic_header(pkt.data)
        if not header:
            continue
        
        packet = {
            'timestamp_ms': pkt.timestamp_ms,
            'protocol': 'Meshtastic',
            'from': header['node_id'],
            'rssi': pkt.rssi,
            'raw_payload': header['payload'],
        }
        
        # Try to parse payload (may be encrypted)
        payload = header['payload']
        data = None
        
        # Check if unencrypted (starts with 0x08 = field 1 varint)
        if len(payload) >= 2 and payload[0] == 0x08:
            data = parse_meshtastic_data(payload)
        # Try decryption on encrypted packets
        elif decryptor and len(payload) >= 6:
            result = decryptor.try_decrypt(pkt.data)
            if result and result.plaintext:
                data = parse_meshtastic_data(result.plaintext)
                if data:
                    packet['decrypted'] = True
                    packet['psk_key'] = result.key_name
        
        if data:
            packet['portnum'] = data['portnum']
            packet['to'] = f"!{data['dest']:08x}" if data['dest'] else None
            
            # Check for traceroute/neighborinfo
            inner = data.get('inner_payload')
            if inner:
                if data['portnum'] == PORT_TRACEROUTE:
                    route = parse_traceroute_response(inner)
                    if route:
                        packet['route'] = route
                
                elif data['portnum'] == PORT_NEIGHBORINFO:
                    neighbors = parse_neighborinfo(inner)
                    if neighbors:
                        packet['neighbors'] = neighbors
        
        packets.append(packet)
    
    return packets


def monitor_live(host: str, analyzer: MeshTopologyAnalyzer, watch: bool = False):
    """Monitor live packets from ESP32 WebSocket"""
    if not REQUESTS_AVAILABLE:
        print("ERROR: requests package required for live monitoring")
        print("Install with: pip install requests")
        return
    
    import websocket
    import time
    
    ws_url = f"ws://{host}/ws"
    print(f"Connecting to {ws_url}...")
    
    def on_message(ws, message):
        try:
            data = json.loads(message)
            if data.get('type') == 'packet':
                analyzer.process_packet(data.get('data', data))
                
                if watch:
                    # Clear and redraw
                    print('\033[2J\033[H', end='')
                    print(analyzer.to_ascii())
        except json.JSONDecodeError:
            pass
    
    def on_error(ws, error):
        print(f"WebSocket error: {error}")
    
    def on_close(ws, close_status_code, close_msg):
        print("WebSocket closed")
    
    def on_open(ws):
        print("Connected! Monitoring for topology packets...")
    
    ws = websocket.WebSocketApp(
        ws_url,
        on_open=on_open,
        on_message=on_message,
        on_error=on_error,
        on_close=on_close
    )
    
    try:
        ws.run_forever()
    except KeyboardInterrupt:
        print("\nStopped monitoring")


def generate_demo_topology(analyzer):
    """Generate a realistic demo mesh network for presentations.

    Uses the same five canonical node IDs as DemoDataGenerator in
    enhanced_live_visualizer.py so the audience recognises the same
    devices across every demo step of the talk.

    Network layout:
      !a42b8c56  ROUTER   — gateway, highest connectivity
      !401acd4e  PRESENTER— relay, presenter's own node
      !598b29ce  ATTENDEE — relay, nearby attendee
      !b3f42a10  MOBILE   — edge node (mobile)
      !7c891def  HIDDEN   — edge node (low signal)
    """
    import random
    rng = random.Random(42)

    print("  Generating demo mesh topology...\n")

    # Canonical nodes matching enhanced_live_visualizer.py DEMO_NODES
    # (node_id, short_name, role, neighbors)
    nodes = [
        ("!a42b8c56", "ROUTER",    "gateway", ["!401acd4e", "!598b29ce", "!b3f42a10", "!7c891def"]),
        ("!401acd4e", "PRESENTER", "relay",   ["!a42b8c56", "!598b29ce", "!b3f42a10"]),
        ("!598b29ce", "ATTENDEE",  "relay",   ["!a42b8c56", "!401acd4e", "!7c891def"]),
        ("!b3f42a10", "MOBILE",    "edge",    ["!401acd4e", "!a42b8c56"]),
        ("!7c891def", "HIDDEN",    "edge",    ["!598b29ce"]),
    ]

    base_time = int(datetime.now().timestamp() * 1000)

    for node_id, short_name, role, neighbors in nodes:
        node = analyzer.get_or_create_node(node_id)
        node.short_name = short_name
        node.update_seen(base_time + rng.randint(0, 60000))

        if role == "gateway":
            node.is_gateway = True
        elif role == "edge":
            node.is_edge = True

        for neighbor_id in neighbors:
            snr = rng.uniform(-3, 9)
            analyzer.process_neighborinfo(
                base_time + rng.randint(0, 60000),
                node_id,
                [{'node_id': neighbor_id, 'snr': snr}]
            )

    # Traceroutes demonstrating multi-hop paths through the mesh
    traceroutes = [
        ("!7c891def", "!b3f42a10", ["!598b29ce", "!a42b8c56", "!401acd4e"]),  # 4 hops
        ("!b3f42a10", "!7c891def", ["!401acd4e", "!a42b8c56", "!598b29ce"]),  # 4 hops
        ("!b3f42a10", "!598b29ce", ["!401acd4e"]),                             # 2 hops
        ("!401acd4e", "!7c891def", ["!598b29ce"]),                             # 2 hops
        ("!401acd4e", "!a42b8c56", []),                                        # 1 hop (direct)
        ("!598b29ce", "!a42b8c56", []),                                        # 1 hop (direct)
    ]

    for source, dest, route in traceroutes:
        analyzer.process_traceroute(
            base_time + rng.randint(0, 120000),
            source, dest, route
        )

    print(f"  Created {len(analyzer.nodes)} nodes, {len(analyzer.edges)} links")
    print(f"  {len(traceroutes)} traceroute paths simulated\n")


def main():
    parser = argparse.ArgumentParser(
        description='Analyze Meshtastic mesh topology from captured packets',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
    %(prog)s capture.csv                    # Analyze CSV capture
    %(prog)s capture.csv --ascii            # ASCII visualization
    %(prog)s capture.csv --format graphviz  # Export Graphviz DOT
    %(prog)s capture.csv --format json -o topology.json
    %(prog)s --live 192.168.4.1 --watch     # Live monitoring
    %(prog)s capture.csv --no-decrypt       # Skip PSK testing
'''
    )
    
    parser.add_argument('input', nargs='?', help='Input file (CSV or PCAP)')
    parser.add_argument('--live', metavar='HOST',
                       help='Connect to ESP32 WebSocket and build topology from live packets (replaces file input)')
    parser.add_argument('--watch', action='store_true',
                       help='Auto-refresh the topology display after each packet (use with --live; without this, results print only on Ctrl+C)')
    parser.add_argument('--format', choices=['json', 'graphviz', 'ascii', 'networkx', 'png'],
                       default='ascii', help='Output format (default: ascii)')
    parser.add_argument('--ascii', action='store_true',
                       help='ASCII visualization (shorthand for --format ascii)')
    parser.add_argument('-o', '--output', help='Output file path')
    parser.add_argument('-v', '--verbose', action='store_true',
                       help='Verbose output')
    parser.add_argument('--no-decrypt', action='store_true',
                       help='Skip PSK decryption (faster, but misses encrypted packets)')
    parser.add_argument('--demo', action='store_true',
                       help='Demo mode: generate simulated mesh topology (no input needed)')
    
    args = parser.parse_args()
    if args.ascii:
        args.format = 'ascii'

    analyzer = MeshTopologyAnalyzer(verbose=args.verbose)
    
    # Demo mode - generate synthetic mesh topology
    if args.demo:
        print("\n🎮 DEMO MODE: Mesh Topology Analysis\n")
        generate_demo_topology(analyzer)
        
        # Output in requested format
        if args.format == 'ascii':
            print(analyzer.to_ascii())
        elif args.format == 'json':
            output = json.dumps(analyzer.to_json(), indent=2)
            if args.output:
                with open(args.output, 'w', encoding='utf-8') as f:
                    f.write(output)
                print(f"Saved to {args.output}")
            else:
                print(output)
        elif args.format == 'graphviz':
            output = analyzer.to_graphviz()
            if args.output:
                with open(args.output, 'w', encoding='utf-8') as f:
                    f.write(output)
                print(f"Saved to {args.output}")
                print("Render with: dot -Tpng -o mesh.png", args.output)
            else:
                print(output)
        elif args.format == 'png':
            analyzer.to_matplotlib(output_file=args.output)
        return
    
    # Live monitoring mode
    if args.live:
        try:
            import websocket
        except ImportError:
            print("ERROR: websocket-client package required for live monitoring")
            print("Install with: pip install websocket-client")
            sys.exit(1)
        
        monitor_live(args.live, analyzer, watch=args.watch)
        return
    
    # File mode
    if not args.input:
        parser.print_help()
        sys.exit(1)
    
    input_path = Path(args.input)
    if not input_path.exists():
        print(f"ERROR: File not found: {input_path}")
        sys.exit(1)
    
    # Load data
    print(f"Loading {input_path}...")
    try_decrypt = not args.no_decrypt
    if input_path.suffix.lower() == '.csv':
        packets = load_csv(input_path, try_decrypt=try_decrypt)
    elif input_path.suffix.lower() == '.pcap':
        packets = load_pcap(input_path, try_decrypt=try_decrypt)
    else:
        print(f"ERROR: Unsupported file format: {input_path.suffix}")
        sys.exit(1)
    
    # Process packets
    for packet in packets:
        analyzer.process_packet(packet)
    
    # Generate output
    if args.format == 'ascii':
        output = analyzer.to_ascii()
        if args.output:
            with open(args.output, 'w') as f:
                f.write(output)
            print(f"Saved to {args.output}")
        else:
            print(output)
    
    elif args.format == 'json':
        output = json.dumps(analyzer.to_json(), indent=2)
        if args.output:
            with open(args.output, 'w') as f:
                f.write(output)
            print(f"Saved to {args.output}")
        else:
            print(output)
    
    elif args.format == 'graphviz':
        output = analyzer.to_graphviz()
        if args.output:
            with open(args.output, 'w') as f:
                f.write(output)
            print(f"Saved to {args.output}")
            print("Render with: dot -Tpng -o mesh.png", args.output)
        else:
            print(output)
    
    elif args.format == 'networkx':
        if not NETWORKX_AVAILABLE:
            print("ERROR: NetworkX not installed. Run: pip install networkx")
            sys.exit(1)

        G = analyzer.to_networkx()
        print(f"NetworkX graph created with {G.number_of_nodes()} nodes, "
              f"{G.number_of_edges()} edges")

        if args.output:
            import pickle
            with open(args.output, 'wb') as f:
                pickle.dump(G, f)
            print(f"Saved pickled graph to {args.output}")

    elif args.format == 'png':
        analyzer.to_matplotlib(output_file=args.output)


if __name__ == '__main__':
    main()
