"""Typed capture models shared by analysis tools.

Mirrors the 24-column CSV written by firmware/src/packet_logger.cpp
and the /api/replay/slots JSON shape from firmware/src/json_builders.cpp.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional


@dataclass
class RelaySighting:
    """A relay's copy of a previously-seen packet (relay sidecar, 2026-04-12)."""
    relay_byte: int          # byte 15 of raw packet (relay node's low byte)
    rssi: float
    snr: float
    hop_count: Optional[int] = None


@dataclass
class CapturedPacket:
    # Identity
    timestamp_ms: int
    session_id: str
    node_id_hex: str                         # origin node (e.g. "0xA8923B66")
    dest_id_hex: Optional[str] = None
    packet_id: Optional[int] = None          # 32-bit packet ID (Meshtastic)

    # Radio
    frequency_mhz: float = 0.0
    config_index: int = 0
    rssi_dbm: float = 0.0
    snr_db: float = 0.0
    length_bytes: int = 0

    # Protocol
    protocol: str = "Unknown"                # "Meshtastic" | "MeshCore" | "LoRaWAN" | ...
    packet_type: str = "unknown"
    channel: Optional[str] = None            # numeric (Meshtastic) or name (MeshCore)
    encrypted: bool = False
    psk_result: str = "none"                 # "none" or PSK name that decrypted it
    psk_id: Optional[str] = None

    # Mesh
    hop_count: Optional[int] = None
    is_router: bool = False
    power_class: int = 0
    relay_byte: Optional[int] = None         # byte 15 from raw packet
    relay_sightings: list[RelaySighting] = field(default_factory=list)

    # Position at RX time (GPS-equipped boards only). position_source is
    # "node" when decoded from the packet payload (true node location) or
    # "sniffer" when it's our GPS at RX (where WE were, not the node).
    # None for legacy CSVs written before the column existed.
    lat_deg: Optional[float] = None
    lon_deg: Optional[float] = None
    alt_m: Optional[float] = None
    position_source: Optional[str] = None

    # Raw
    raw_hex: str = ""

    @property
    def raw_bytes(self) -> bytes:
        return bytes.fromhex(self.raw_hex) if self.raw_hex else b""

    @property
    def is_broadcast(self) -> bool:
        return self.dest_id_hex is None or self.dest_id_hex.upper() in ("0xFFFFFFFF", "FFFFFFFF")


@dataclass
class Device:
    """A node as reported by /api/devices — aggregated, not per-packet.

    This is a different shape than CapturedPacket: the firmware has already
    collapsed many packets into a per-node summary. Tools that want
    per-packet data should use Capture instead.
    """
    node_id: str
    protocol: str = "Unknown"
    packet_count: int = 0
    rssi_dbm: Optional[float] = None
    lat_deg: Optional[float] = None
    lon_deg: Optional[float] = None
    alt_m: Optional[float] = None
    raw: dict = field(default_factory=dict)  # full JSON for tool-specific fields


@dataclass
class Capture:
    """A loaded capture: list of packets plus source metadata."""
    packets: list[CapturedPacket]
    source: str                              # filename or "api:host"
    source_kind: str = "csv"                 # "csv" | "pcap" | "api"

    def __len__(self) -> int:
        return len(self.packets)

    def __iter__(self):
        return iter(self.packets)

    def time_range_ms(self) -> tuple[int, int]:
        """(min, max) timestamp_ms across packets. (0, 0) if empty."""
        if not self.packets:
            return (0, 0)
        ts = [p.timestamp_ms for p in self.packets if p.timestamp_ms]
        return (min(ts), max(ts)) if ts else (0, 0)
