"""Unified capture loader.

Dispatches on source:
    - path ending in .csv  → _load_csv (firmware-written session log)
    - path ending in .pcap → _load_pcap (ESP32 PCAP format; payloads only)
    - "api:HOST"           → _load_api (live sniffer, replay-slot view only)

The API path returns at most Config::Replay::MAX_SLOTS packets — it mirrors
the firmware's in-RAM replay buffer, not the full capture history. For
complete data, use the SD-card CSV.
"""
from __future__ import annotations

import csv
from pathlib import Path
from typing import Optional

from .models import Capture, CapturedPacket, Device, RelaySighting


def load(source: str) -> Capture:
    if source.startswith("api:"):
        return _load_api(source[4:])
    p = Path(source)
    suffix = p.suffix.lower()
    if suffix == ".csv":
        return _load_csv(p)
    if suffix == ".pcap":
        return _load_pcap(p)
    raise ValueError(f"Unknown capture source: {source!r} (expected .csv, .pcap, or 'api:HOST')")


# ---------- CSV ----------

def _load_csv(path: Path) -> Capture:
    packets: list[CapturedPacket] = []
    with path.open(encoding="utf-8", newline="") as fh:
        for row in csv.DictReader(fh):
            packets.append(_row_to_packet(row))
    return Capture(packets=packets, source=path.name, source_kind="csv")


def _row_to_packet(row: dict) -> CapturedPacket:
    return CapturedPacket(
        timestamp_ms = _int(row.get("timestamp_ms"), 0),
        session_id   = row.get("session_id", "") or "",
        node_id_hex  = (row.get("node_id_hex") or "unknown"),
        dest_id_hex  = row.get("dest_id_hex") or None,
        packet_id    = _int_or_none(row.get("packet_id"), base=0),

        frequency_mhz= _float(row.get("frequency_mhz"), 0.0),
        config_index = _int(row.get("config_index"), 0),
        rssi_dbm     = _float(row.get("rssi_dbm"), 0.0),
        snr_db       = _float(row.get("snr_db"), 0.0),
        length_bytes = _int(row.get("length_bytes"), 0),

        protocol     = row.get("protocol", "Unknown") or "Unknown",
        packet_type  = row.get("packet_type", "unknown") or "unknown",
        channel      = row.get("channel") or None,
        encrypted    = _bool(row.get("encrypted")),
        psk_result   = row.get("psk_result") or "none",
        psk_id       = row.get("psk_id") or None,

        hop_count    = _int_or_none(row.get("hop_count")),
        is_router    = _bool(row.get("is_router")),
        power_class  = _int(row.get("power_class"), 0),
        relay_byte   = _int_or_none(row.get("relay_byte"), base=16),

        lat_deg      = _float_or_none(row.get("lat_deg")),
        lon_deg      = _float_or_none(row.get("lon_deg")),
        alt_m        = _float_or_none(row.get("alt_m")),
        position_source = (row.get("position_source") or None),

        raw_hex      = row.get("raw_hex", "") or "",
    )


# ---------- PCAP ----------

def _load_pcap(path: Path) -> Capture:
    # PCAP carries raw payloads only — no protocol/node_id/timestamps with
    # the same fidelity as the CSV. Tools that want richer data should use CSV.
    # We intentionally keep this minimal; callers that need decoded metadata
    # can decode from raw_bytes in their own pass.
    packets: list[CapturedPacket] = []
    for ts_ms, payload in _iter_pcap_records(path):
        packets.append(CapturedPacket(
            timestamp_ms=ts_ms,
            session_id="",
            node_id_hex="unknown",
            length_bytes=len(payload),
            raw_hex=payload.hex().upper(),
        ))
    return Capture(packets=packets, source=path.name, source_kind="pcap")


def _iter_pcap_records(path: Path):
    """Yield (timestamp_ms, payload_bytes). Supports classic PCAP only."""
    import struct
    with path.open("rb") as fh:
        hdr = fh.read(24)
        if len(hdr) < 24:
            return
        magic = struct.unpack("<I", hdr[:4])[0]
        if magic == 0xA1B2C3D4:
            endian = "<"
        elif magic == 0xD4C3B2A1:
            endian = ">"
        else:
            return  # Not a classic PCAP we understand
        while True:
            rec = fh.read(16)
            if len(rec) < 16:
                return
            ts_sec, ts_usec, incl_len, _orig_len = struct.unpack(endian + "IIII", rec)
            data = fh.read(incl_len)
            if len(data) < incl_len:
                return
            yield (ts_sec * 1000 + ts_usec // 1000), data


# ---------- API ----------

def _load_api(host: str) -> Capture:
    try:
        import requests
    except ImportError:
        raise RuntimeError("pip install requests to use --api mode")
    url = f"http://{host}/api/replay/slots"
    r = requests.get(url, timeout=10)
    r.raise_for_status()
    body = r.json()
    packets = [_slot_to_packet(s) for s in body.get("slots", [])]
    return Capture(packets=packets, source=f"api:{host}", source_kind="api")


def _slot_to_packet(s: dict) -> CapturedPacket:
    channel = s.get("channelName")
    if channel is None and "channel" in s:
        channel = str(s["channel"])

    sightings = [
        RelaySighting(
            relay_byte=_int_or_none(rs.get("relayByte"), base=16) or 0,
            rssi=float(rs.get("rssi", 0) or 0),
            snr=float(rs.get("snr", 0) or 0),
            hop_count=_int_or_none(rs.get("hopCount")),
        )
        for rs in s.get("relaySightings", [])
    ]

    return CapturedPacket(
        timestamp_ms = 0,  # API doesn't expose an absolute ts; capturedSecondsAgo is relative
        session_id   = "",
        node_id_hex  = s.get("nodeId", "unknown") or "unknown",
        dest_id_hex  = s.get("destId") or None,
        packet_id    = _int_or_none(s.get("packetId"), base=16),
        frequency_mhz= float(s.get("frequencyMHz", 0) or 0),
        config_index = int(s.get("configIndex", 0) or 0),
        rssi_dbm     = float(s.get("rssi", 0) or 0),
        snr_db       = float(s.get("snr", 0) or 0),
        length_bytes = int(s.get("length", 0) or 0),
        protocol     = s.get("protocol", "Unknown") or "Unknown",
        channel      = channel,
        hop_count    = _int_or_none(s.get("hopCount")),
        relay_sightings=sightings,
    )


# ---------- Device loader (separate from Capture) ----------

def load_devices(host: str) -> list[Device]:
    """Fetch aggregated per-device summary from /api/devices.

    This is a distinct model from Capture (per-packet). The firmware has
    already collapsed packets into device-level summaries; there's no way
    to recover per-packet data from this endpoint.
    """
    try:
        import requests
    except ImportError:
        raise RuntimeError("pip install requests to use --api mode")
    r = requests.get(f"http://{host}/api/devices", timeout=10)
    r.raise_for_status()
    out: list[Device] = []
    for d in r.json().get("devices", []):
        out.append(Device(
            node_id      = d.get("nodeId", "unknown") or "unknown",
            protocol     = d.get("protocol", "Unknown") or "Unknown",
            packet_count = int(d.get("packetCount", 0) or 0),
            rssi_dbm     = _float_or_none(d.get("rssi")),
            lat_deg      = _float_or_none(d.get("lat") or d.get("latitude")),
            lon_deg      = _float_or_none(d.get("lon") or d.get("longitude")),
            alt_m        = _float_or_none(d.get("alt") or d.get("altitude")),
            raw          = d,
        ))
    return out


# ---------- coercion helpers ----------

def _int(v, default: int) -> int:
    try:
        if v in (None, ""):
            return default
        return int(float(v))
    except (TypeError, ValueError):
        return default


def _float(v, default: float) -> float:
    try:
        if v in (None, ""):
            return default
        return float(v)
    except (TypeError, ValueError):
        return default


def _int_or_none(v, base: int = 10) -> Optional[int]:
    if v in (None, ""):
        return None
    s = str(v).strip()
    if not s:
        return None
    try:
        if base == 16:
            return int(s, 16)
        if base == 0:
            return int(s, 0)
        return int(s, base)
    except ValueError:
        try:
            return int(float(s))
        except ValueError:
            return None


def _float_or_none(v) -> Optional[float]:
    if v in (None, ""):
        return None
    try:
        return float(v)
    except (TypeError, ValueError):
        return None


def _bool(v) -> bool:
    return str(v).strip().lower() in ("1", "true", "yes")
