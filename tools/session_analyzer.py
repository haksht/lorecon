#!/usr/bin/env python3
"""Session Analyzer for ESP32 LoRa Sniffer logs.

Loads the enhanced SD-card CSV output (packet logger) and renders a
2x2 matplotlib dashboard showing timeline activity, top devices,
frequency distribution, and geographic hits.
"""
from __future__ import annotations

import argparse
import csv
import math
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple

import matplotlib.pyplot as plt


@dataclass
class Packet:
    """Structure representing a single CSV row."""

    timestamp_ms: int
    node_id: Optional[str]
    protocol: str
    frequency_mhz: float
    config_index: int
    rssi_dbm: float
    snr_db: float
    length_bytes: int
    packet_type: str
    encrypted: bool
    psk_result: str
    lat_deg: Optional[float]
    lon_deg: Optional[float]
    alt_m: Optional[float]


def _parse_optional_float(value: str) -> Optional[float]:
    if value is None:
        return None
    value = value.strip()
    if not value:
        return None
    try:
        return float(value)
    except ValueError:
        return None


def _parse_optional_int(value: str) -> Optional[int]:
    if value is None:
        return None
    value = value.strip()
    if not value:
        return None
    try:
        return int(value)
    except ValueError:
        return None


def load_packets(csv_path: Path) -> List[Packet]:
    packets: List[Packet] = []
    with csv_path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            try:
                timestamp_ms = int(row.get("timestamp_ms", row.get("timestamp", "0")))
                node_id_raw = row.get("node_id_hex") or row.get("nodeId") or ""
                node_id = node_id_raw.strip() or None
                protocol = (row.get("protocol") or "Unknown").strip()
                frequency = float(row.get("frequency_mhz", row.get("frequency", 0.0)) or 0.0)
                config_index = int(row.get("config_index", row.get("configIndex", 0)) or 0)
                rssi = float(row.get("rssi_dbm", row.get("rssi", 0.0)) or 0.0)
                snr = float(row.get("snr_db", row.get("snr", 0.0)) or 0.0)
                length_bytes = int(row.get("length_bytes", row.get("length", 0)) or 0)
                packet_type = (row.get("packet_type", "unknown") or "unknown").strip().lower()
                encrypted_flag = (row.get("encrypted", "0").strip() in {"1", "true", "True"})
                psk_result = (row.get("psk_result", "none") or "none").strip()
                lat_deg = _parse_optional_float(row.get("lat_deg"))
                lon_deg = _parse_optional_float(row.get("lon_deg"))
                alt_m = _parse_optional_float(row.get("alt_m"))
            except ValueError:
                continue

            packets.append(
                Packet(
                    timestamp_ms=timestamp_ms,
                    node_id=node_id,
                    protocol=protocol,
                    frequency_mhz=frequency,
                    config_index=config_index,
                    rssi_dbm=rssi,
                    snr_db=snr,
                    length_bytes=length_bytes,
                    packet_type=packet_type,
                    encrypted=encrypted_flag,
                    psk_result=psk_result,
                    lat_deg=lat_deg,
                    lon_deg=lon_deg,
                    alt_m=alt_m,
                )
            )
    return packets


def compute_stats(packets: Iterable[Packet], bin_seconds: int) -> Dict[str, object]:
    packets_list = list(packets)
    if not packets_list:
        return {}

    t0_ms = min(pkt.timestamp_ms for pkt in packets_list)
    t0_s = t0_ms / 1000.0

    timeline: Dict[int, int] = defaultdict(int)
    for pkt in packets_list:
        bin_index = int(((pkt.timestamp_ms / 1000.0) - t0_s) // bin_seconds)
        timeline[bin_index] += 1

    device_stats: Dict[str, Dict[str, object]] = defaultdict(
        lambda: {"count": 0, "best_rssi": -999.0, "avg_rssi_sum": 0.0, "protocols": set()}
    )
    for pkt in packets_list:
        if not pkt.node_id:
            continue
        stats = device_stats[pkt.node_id]
        stats["count"] += 1
        stats["avg_rssi_sum"] += pkt.rssi_dbm
        stats["best_rssi"] = max(stats["best_rssi"], pkt.rssi_dbm)
        stats["protocols"].add(pkt.protocol)

    for node_id, stats in device_stats.items():
        if stats["count"]:
            stats["avg_rssi"] = stats["avg_rssi_sum"] / stats["count"]
        else:
            stats["avg_rssi"] = float("nan")

    freq_stats: Dict[float, Dict[str, float]] = defaultdict(lambda: {"count": 0, "rssi_sum": 0.0})
    for pkt in packets_list:
        freq = round(pkt.frequency_mhz, 3)
        stats = freq_stats[freq]
        stats["count"] += 1
        stats["rssi_sum"] += pkt.rssi_dbm

    for freq, stats in freq_stats.items():
        stats["avg_rssi"] = stats["rssi_sum"] / max(stats["count"], 1)

    positions = [pkt for pkt in packets_list if pkt.lat_deg is not None and pkt.lon_deg is not None]

    return {
        "t0_s": t0_s,
        "timeline": timeline,
        "bin_seconds": bin_seconds,
        "devices": device_stats,
        "freqs": freq_stats,
        "positions": positions,
    }


def plot_dashboard(stats: Dict[str, object], top_n: int) -> None:
    fig = plt.figure(figsize=(12, 8))
    grid = fig.add_gridspec(2, 2)

    # Timeline
    ax_timeline = fig.add_subplot(grid[0, 0])
    bins = sorted(stats["timeline"].items())
    x_vals = [bin_idx * stats["bin_seconds"] / 60.0 for bin_idx, _ in bins]
    y_vals = [count for _, count in bins]
    ax_timeline.plot(x_vals, y_vals, marker="o")
    ax_timeline.set_title("Packets Over Time")
    ax_timeline.set_xlabel("Minutes since start")
    ax_timeline.set_ylabel("Packets/bin")

    # Top devices
    ax_devices = fig.add_subplot(grid[0, 1])
    sorted_devices = sorted(
        stats["devices"].items(), key=lambda item: item[1]["count"], reverse=True
    )[:top_n]
    labels = [dev if dev else "Unknown" for dev, _ in sorted_devices]
    counts = [meta["count"] for _, meta in sorted_devices]
    ax_devices.barh(labels, counts)
    ax_devices.set_title(f"Top {len(labels)} Devices")
    ax_devices.set_xlabel("Packet count")
    ax_devices.invert_yaxis()

    # Frequency distribution
    ax_freq = fig.add_subplot(grid[1, 0])
    freq_entries = sorted(stats["freqs"].items())
    freq_labels = [f"{freq:.3f}" for freq, _ in freq_entries]
    freq_counts = [meta["count"] for _, meta in freq_entries]
    ax_freq.bar(freq_labels, freq_counts)
    ax_freq.set_title("Frequency Usage")
    ax_freq.set_xlabel("Frequency (MHz)")
    ax_freq.set_ylabel("Packets")
    ax_freq.tick_params(axis="x", rotation=45)

    # Positions
    ax_pos = fig.add_subplot(grid[1, 1])
    if stats["positions"]:
        lons = [pkt.lon_deg for pkt in stats["positions"] if pkt.lon_deg is not None]
        lats = [pkt.lat_deg for pkt in stats["positions"] if pkt.lat_deg is not None]
        ax_pos.scatter(lons, lats, c="tab:red", alpha=0.7)
        ax_pos.set_title("Position Fixes")
        ax_pos.set_xlabel("Longitude")
        ax_pos.set_ylabel("Latitude")
    else:
        ax_pos.text(0.5, 0.5, "No GPS points", ha="center", va="center")
        ax_pos.set_title("Position Fixes")
        ax_pos.set_xticks([])
        ax_pos.set_yticks([])

    fig.suptitle("ESP32 LoRa Session Dashboard", fontsize=16)
    fig.tight_layout(rect=(0, 0, 1, 0.97))
    plt.show()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Analyze SD-card session CSV logs.")
    parser.add_argument("csv", type=Path, help="Path to CSV exported by the ESP32 sniffer")
    parser.add_argument(
        "--bin-seconds", type=int, default=60, help="Timeline bin width in seconds (default: 60)"
    )
    parser.add_argument(
        "--top-n", type=int, default=8, help="Number of devices to show in the top-device panel"
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if not args.csv.exists():
        raise SystemExit(f"CSV file not found: {args.csv}")

    packets = load_packets(args.csv)
    if not packets:
        raise SystemExit("CSV contained no packets to analyze.")

    stats = compute_stats(packets, bin_seconds=max(1, args.bin_seconds))
    if not stats:
        raise SystemExit("Unable to compute statistics from the provided CSV.")

    plot_dashboard(stats, top_n=max(1, args.top_n))


if __name__ == "__main__":
    main()
