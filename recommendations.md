# ESP32 LoRa Sniffer – Recommendations

Last updated: 2025-11-20

This document summarizes the current state of the project (as reflected in the docs) and captures concrete recommendations for next steps, with a focus on **demo wow factor** and SD-card-driven analysis tooling.

---

## 1. Big-Picture Read of the Project

Based on the existing documentation (`README.md`, `QUICKSTART.md`, `FEATURES.md`, `BUILD_GUIDE.md`, `PHONE_APP_GUIDE.md`, `API_REFERENCE.md`, `UNDERSTANDING_v2.md`, `ENCRYPTION_REALITY.md`, troubleshooting and API deep dives):

### 1.1 Tool Identity

- **Core mission:** Passive LoRa reconnaissance – *scan → discover → capture → replay → analyze*.
- The firmware is explicitly **receive-focused** for security research, RF experimentation, and network assessment.
- Legacy offensive/stress-testing paths and docs have been removed; the focus is on visibility, default-PSK exposure, and intelligence, not on active attack tooling.

### 1.2 Architecture & Code Quality

- Architecture is **v2.0 refactored** around clean separation of concerns:
  - `RadioController` – SX1262 hardware abstraction, ISR + atomic flags, cached RSSI/SNR.
  - `PacketProcessor` – packet queue, protocol analysis, PSK decryption coordination, geo extraction, logging.
  - `ReconState` – scan configs, RF activity, device records, replay slots, modes.
  - `LoRaReconTool` implementing `IReconTool` – orchestrator, modes, initialization.
  - `CommandHandler` – dispatch-table-based command routing (serial and mirrored in web API).
  - `OLEDDisplay` / `UserInterface` / `ui_components` – standalone visual UI on supported hardware.
  - `GeoIntelligence`, `ProtocolAnalyzer`, PSK decryption module, diagnostics, error handler, watchdog.
- `UNDERSTANDING_v2.md` provides a very complete teaching document for the architecture – this is already at “internal book” level.
- Reliability is treated seriously (watchdog, error codes, graceful degradation when OLED/SD are missing).

### 1.3 User Surfaces

- **Serial UI**: Full feature set via commands (`m`, `f`, `a`, `d`, `v`, `g/k/j`, `c`, `p`, `q`, `x`, etc.).
- **Web / Phone UI**:
  - ESP32 runs a WiFi AP and serves a minimal PWA from LittleFS.
  - REST API + WebSocket defined in `API_REFERENCE.md` and `docs/api/recon_service.md`.
  - `PHONE_APP_GUIDE.md` documents parity between serial and web for recon, capture, replay, analysis, and geo.
- **PC Tools**:
  - `tools/live_visualizer.py` for real-time serial-based visualization (RSSI graphs, device stats, histograms, summary).

### 1.4 Encryption Reality

- `ENCRYPTION_REALITY.md` correctly scopes capabilities against Meshtastic 2.5.0+:
  - **Decryptable with known PSKs:** broadcasts (position, telemetry, node info, traceroutes), routing packets, channel/group messages.
  - **Not decryptable:** direct messages and certain admin/control traffic using Curve25519 PKC.
- The message to users: this is a **reconnaissance and analysis tool**, not a magic “decrypt everything” device.

### 1.5 Maturity

- QUICKSTART and BUILD docs position the firmware as **“2.0 production ready”**.
- The combination of architecture, docs, and tooling supports that claim.
- Some older docs (e.g. older roadmap bits) reference removed/deferred features, but overall the narrative is coherent.

---

## 2. Strategy: Demo Wow Factor Track

Given that core functionality and docs are strong, the most leverage for **demos and conference talks** comes from:

1. **A polished, repeatable story**: “We captured a session in the field → now let’s visualize and dissect it on a big screen.”
2. **Visuals that are independent of live RF conditions**: you always have a compelling fallback, even in a noisy or dead RF environment.

The recommended path:

- Standardize **CSV logging to SD** for packets with enough fields to support rich offline analysis.
- Build a single **Python `session_analyzer.py` tool** that loads the CSV and presents a 2×2 dashboard:
  - Packets over time.
  - Top devices and basic stats.
  - RF-frequency / config distribution.
  - A simple map-like scatter of node positions.

The live serial visualizer remains valuable, but the SD + analyzer workflow gives you a clean narrative:

> “Here’s what we saw in the field, preserved as data. Now watch us explore it visually and pull out insights.”

---

## 3. SD Card CSV Schema (Packets)

Goal: a **simple, robust, analyzer-friendly** CSV that the ESP32 can write line-by-line, and Python can parse into higher-level stats and plots.

### 3.1 File Naming

- One file per session, e.g.: `SNF_2025-11-20_1530.csv`.
- Session = typically one power-on or one deliberate “start logging” event.

### 3.2 Columns

**1. Core timing / identity**

- `timestamp_ms` – `uint64` milliseconds since boot, or Unix epoch ms if available.
- `session_id` – short string; can be constant per boot (e.g. ISO datetime) or left empty initially.
- `node_id_hex` – e.g. `9EA3D744` (empty if unknown).
- `protocol` – e.g. `Meshtastic`, `LoRaWAN`, `Helium`, `Unknown`.

**2. RF / configuration**

- `frequency_mhz` – float, e.g. `906.875`.
- `config_index` – integer (0–15), scan configuration index.
- `rssi_dbm` – float.
- `snr_db` – float.

**3. Packet structure / classification**

- `length_bytes` – integer.
- `packet_type` – string enum, one of: `routing`, `position`, `telemetry`, `text`, `admin`, `other`, `unknown`.
- `encrypted` – `0` or `1`.
- `psk_result` – `none`, `hit`, or `miss`.
- `psk_id` – short label/index for which PSK hit (e.g. `default_AQ==`, `idx_0`), or empty.

**4. Geo + device hints (optional, sparse)**

- `lat_deg` – float, empty when not a position.
- `lon_deg` – float, empty when not a position.
- `alt_m` – float, optional.
- `hop_count` – integer, if available.
- `is_router` – `0` / `1`, or empty.
- `power_class` – small integer, or empty.

**5. Raw payload**

- `raw_hex` – hex string (uppercase, no spaces), placed as the **last column** so that any commas in earlier columns do not affect parsing.

### 3.3 Example Header and Rows

```text
timestamp_ms,session_id,node_id_hex,protocol,frequency_mhz,config_index,rssi_dbm,snr_db,length_bytes,packet_type,encrypted,psk_result,psk_id,lat_deg,lon_deg,alt_m,hop_count,is_router,power_class,raw_hex
123456,2025-11-20T15:30Z,9EA3D744,Meshtastic,906.875,3,-68.5,8.2,64,position,1,hit,default_AQ==,37.77490,-122.41940,15,2,0,1,FFAABBCC...
123890,2025-11-20T15:30Z,9EA3D744,Meshtastic,906.875,3,-70.1,7.9,58,text,1,hit,default_AQ==,,,,,,,"A1B2C3D4..."
124200,2025-11-20T15:30Z,,LoRaWAN,915.000,7,-72.0,6.5,32,routing,1,none,,,"",,,,"01020304..."
```

Notes:

- Empty fields are acceptable; the analyzer treats them as unknown/`None`.
- On-device logging can be implemented as a fixed header written once, followed by simple `printf`-style row formatting.

---

## 4. Python `session_analyzer.py` Design

Location: `tools/session_analyzer.py`

Dependencies:

- Already in `tools/requirements.txt`: `matplotlib`.
- Standard library: `csv`, `argparse`, `collections`, `dataclasses`, `datetime`, `typing`.

### 4.1. Purpose and Entry Point

Command-line usage examples:

```bash
# Basic usage
python tools/session_analyzer.py SNF_2025-11-20_1530.csv

# With custom bin size and top-N devices
python tools/session_analyzer.py SNF_2025-11-20_1530.csv --bin-seconds 30 --top-n 12
```

Responsibilities:

1. Load packets from a CSV created by the SD logger.
2. Compute basic aggregates:
   - Packets per time bin.
   - Per-device stats.
   - Per-frequency/config stats.
   - Position set (lat/lon/alt).
3. Render a **2×2 matplotlib dashboard**:
   - Timeline.
   - Top devices.
   - RF distribution.
   - Position scatter.

### 4.2. Data Model

A lightweight dataclass for each packet:

```python
from dataclasses import dataclass
from typing import Optional

@dataclass
class Packet:
    timestamp_ms: int
    node_id_hex: Optional[str]
    protocol: str
    frequency_mhz: float
    config_index: int
    rssi_dbm: float
    snr_db: float
    length_bytes: int
    packet_type: str
    encrypted: bool
    psk_result: str
    psk_id: Optional[str]
    lat_deg: Optional[float]
    lon_deg: Optional[float]
    alt_m: Optional[float]
```

### 4.3. CSV Loading

- Use `csv.DictReader`.
- Convert numeric fields with `int()` / `float()`.
- Treat empty strings as `None` where appropriate.
- Skip rows that fail to parse, optionally counting/logging how many were skipped.

Pseudo-code sketch:

```python
def load_packets(csv_path: str) -> list[Packet]:
    packets: list[Packet] = []
    with open(csv_path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                packets.append(Packet(
                    timestamp_ms=int(row["timestamp_ms"]),
                    node_id_hex=row["node_id_hex"] or None,
                    protocol=row["protocol"] or "Unknown",
                    frequency_mhz=float(row["frequency_mhz"]),
                    config_index=int(row["config_index"]),
                    rssi_dbm=float(row["rssi_dbm"]),
                    snr_db=float(row["snr_db"]),
                    length_bytes=int(row["length_bytes"]),
                    packet_type=row["packet_type"] or "unknown",
                    encrypted=row["encrypted"] == "1",
                    psk_result=row["psk_result"] or "none",
                    psk_id=row["psk_id"] or None,
                    lat_deg=float(row["lat_deg"]) if row["lat_deg"] else None,
                    lon_deg=float(row["lon_deg"]) if row["lon_deg"] else None,
                    alt_m=float(row["alt_m"]) if row["alt_m"] else None,
                ))
            except Exception:
                # Ignore malformed lines for now; could log in future.
                continue
    return packets
```

### 4.4. Statistics Computation

Aggregate into simple structures suited for plotting.

Core ideas:

- **Timeline** – bin by `bin_seconds` (e.g. 60 s):
  - `bin_index = int((t_s - t0) // bin_seconds)`.
- **Device stats** – keyed by `node_id_hex`:
  - Count, best RSSI, average RSSI, first/last seen, protocols.
- **RF distribution** – keyed by frequency (rounded to e.g. 3 decimals):
  - Count and average RSSI.
- **Positions** – list of packets with non-`None` lat/lon.

This can live in a function like `compute_stats(packets, bin_seconds=60)` returning a dict with:

- `t0` – session start time in seconds.
- `timeline` – dict of `bin_index → count`.
- `devices` – dict of `node_id → stats`.
- `rf_by_freq` – dict of `frequency → stats`.
- `positions` – list of `Packet` objects that carry coordinates.

### 4.5. Plots / Dashboard Layout

Use `matplotlib` with `GridSpec(2, 2)` to create four subplots:

1. **Packets Over Time (top-left)**
   - X-axis: minutes since session start.
   - Y-axis: packets per bin.
   - Simple line plot with markers.

2. **Top Devices (top-right)**
   - Horizontal bar chart of packet counts by `node_id_hex`.
   - Show top N devices by packet count (`--top-n`, default 8).

3. **RF Distribution (bottom-left)**
   - Bar chart of packets per frequency.
   - X-axis: discrete frequencies (tick labels may be rotated).

4. **Positions (bottom-right)**
   - Scatter plot of lon/lat.
   - Basic axis labels: “Longitude” / “Latitude”.
   - Later could be replaced with a real map-backed view, but even a simple scatter is demo-worthy.

### 4.6. CLI Wrapper

- Parse arguments (`csv_path`, `--bin-seconds`, `--top-n`).
- Load packets; if empty, print a short message and exit.
- Compute stats and pass them to the plotting function.

The result is a single command that opens a window ready for projection.

---

## 5. Demo Flow Using SD + Session Analyzer

Recommended narrative for a talk or demo:

1. **Field capture**
   - Run the ESP32 sniffer in recon mode with SD logging enabled.
   - Walk around / collect data for a few minutes.

2. **Data handoff**
   - Remove SD card, copy `SNF_*.csv` to a laptop.

3. **Analysis on the big screen**
   - Run `python tools/session_analyzer.py SNF_2025-11-20_1530.csv`.
   - Walk through each quadrant:
     - “Here’s traffic over time – when things got busy.”
     - “Here are the top devices, who’s loudest and closest.”
     - “Here’s how the RF spectrum/configs were used.”
     - “Here’s where the devices physically were (approximate map).”

4. **Tie back to capabilities**
   - Connect visualized data to recon abilities already documented (PSK hits, geolocation, device types, security assessment).

This gives you a **repeatable demo story** that is decoupled from live RF conditions but still grounded in real captures from the same firmware and hardware.

---

## 6. Possible Future Enhancements (Nice to Have)

Once the basics above are in place and working smoothly, some optional next steps:

- **Session recording & playback** in the analyzer:
  - Save derived stats to JSON; reload quickly without re-parsing CSV.
- **Filtering UI**:
  - Checkboxes or CLI flags to restrict to certain protocols, device IDs, or time ranges.
- **Map integration**:
  - Use a mapping library (or export to a browser-based map) for nicer visuals.
- **Integration with live WebSocket API**:
  - Allow the analyzer to attach directly via WiFi for “semi-live” dashboards, in addition to SD CSV.

These are intentionally deferred until after the basic SD + analyzer pipeline is working and demo-tested.
