# Python Analysis Tools

Reference for the `tools/` Python toolkit — what each tool does, how to
run it, and what it produces.

---

## Install

```bash
python -m venv venv
.\venv\Scripts\activate.ps1   # Windows (or ./venv/bin/activate on Linux/macOS)
pip install -e .              # installs the `lorecon` console script + deps
```

Dependencies installed by `pip install -e .`:

- Python 3.10+
- `cryptography` — PSK decryption (highly recommended; without it, all decryption-dependent findings are skipped)
- `websocket-client` — live monitor
- `rich` — `monitor --tui` dashboard
- `folium` — `map` HTML output
- `matplotlib`, `networkx` — `topology` graph rendering
- `requests` — REST API client

---

## Analysis tools

These are the three tools most users will reach for. They work offline from
a CSV (or PCAP) exported from the device — no live connection needed.

```bash
lorecon report   capture.csv -o report.html
lorecon map      capture.csv -o map.html
lorecon topology capture.csv -o topology.svg
```

### `report` — security assessment

The main analysis tool. Produces a standalone HTML report with findings
prioritized CRITICAL → INFO.

```bash
lorecon report capture.csv
lorecon report capture.csv --pcap capture.pcap           # also parse LoRaWAN joins
lorecon report capture.csv -o report.html
lorecon report --api 192.168.4.1                         # live device as input
lorecon report new.csv --baseline old.csv                # diff mode
```

Findings currently emitted (when supporting data is present):

- **Default PSK cracked** — packets decrypted with known keys
- **Private messages intercepted** — plaintext TEXT_MESSAGE recovered
- **Legacy admin key** — pre-2.5 admin channel → remote-config attack surface
- **Cross-channel bridging** — one node uses 2+ PSKs (critical if admin is one)
- **AES-CTR nonce reuse** — same (node, packet_id) with different ciphertexts
- **Operators identified by NodeInfo** — names / hardware leaked on public PSK
- **LoRaWAN DevNonce reuse** — OTAA replay vulnerability (CWE-294)
- **Plaintext GPS disclosure** — positions broadcast on known-weak PSKs
- **Mobile vs stationary** — bounding box > 100 m classifies mobile
- **High-power infrastructure** — RSSI > −50 dBm (base stations)
- **Changes since baseline** — new / vanished nodes, risk↑, new PSKs

Report sections:

- Findings list (with CWE references where applicable)
- Device table — per-device packet count, risk, PSK, GPS, activity sparkline
- Capture-wide activity histogram
- Device identities (short name, long name, hardware, licensed flag)
- Intercepted message log
- Changes-since-baseline tables (when `--baseline` is given)
- AES-CTR nonce reuse table

### `map` — GPS positions

```bash
lorecon map capture.csv -o map.html
lorecon map capture.csv --min-packets 10 -o map.html
```

Produces an interactive [folium](https://python-visualization.github.io/folium/)
HTML map. `--min-packets` filters out stragglers to keep the map readable.

### `topology` — mesh graph

```bash
lorecon topology capture.csv -o topology.svg
```

Zoomable SVG visualization of the mesh (pass `-o file.png` for a raster
image instead). Most informative when the capture contains
Meshtastic traceroute (portnum 70) or NeighborInfo (portnum 71) packets;
otherwise falls back to relay-byte inference. Decrypts NodeInfo on the fly
to label nodes with operator-chosen names. MeshCore captures render by
channel membership.

---

## Dev utilities

Specialized tools for live monitoring, format conversion, and API access.
Invoked via `lorecon dev <cmd>`.

| Command     | Purpose                                                     |
| ----------- | ----------------------------------------------------------- |
| `monitor`   | Live WebSocket packet stream (headless or `--tui`)          |
| `wireshark` | Convert ESP32 PCAP → LoRaTap for Wireshark                  |
| `merge`     | Cross-capture identity linker (2+ CSVs)                     |
| `api`       | REST API client (~30 subcommands)                           |

### `monitor` — live stream

Headless (default) or rich-based dashboard. Requires the device reachable
over WiFi.

```bash
lorecon dev monitor --host 192.168.4.1                  # scrolling lines
lorecon dev monitor --host 192.168.4.1 --tui            # rich dashboard
lorecon dev monitor --host 192.168.4.1 --decrypt        # try PSK decrypt live
lorecon dev monitor --host 192.168.4.1 --messages       # decrypted text only
lorecon dev monitor --host 192.168.4.1 --json           # raw JSON per line
lorecon dev monitor --host 192.168.4.1 --filter meshtastic
lorecon dev monitor --demo                              # simulated, no hardware
```

The `--tui` dashboard has four panels:

- **Header** — connection state, mode, packets/s, decrypt rate
- **Top talkers** — 15 most active nodes (node, name, protocol, pkts, RSSI, PSK)
- **New-node alerts** — most recently first-seen nodes
- **Decrypted messages** — live feed of intercepted text

### `merge` — cross-capture linker

```bash
lorecon dev merge hotel.csv conference.csv airport.csv
lorecon dev merge a.csv b.csv --json linked.json --min-captures 2
```

Finds nodes appearing across multiple captures — useful for tracking
operators who travel between sessions. Sorts by ubiquity, then by total
packet count. Re-decrypts NodeInfo from raw bytes so CSVs with a missing
`node_id` column still link correctly.

### `wireshark` — LoRaTap conversion

```bash
lorecon dev wireshark capture.pcap
lorecon dev wireshark capture.pcap --open
```

Converts the ESP32's custom PCAP pseudo-header to standard LoRaTap (DLT
270). Output saved alongside the input with `_loratap` appended.

### `api` — device REST client

```bash
lorecon dev api --host 192.168.4.1 status
lorecon dev api --host 192.168.4.1 devices
lorecon dev api --host 192.168.4.1 replay-slots
```

Developer tool — exposes every endpoint on the firmware API.

---

## SD card vs live connection

| Scenario                         | Connection needed | Tools                      |
| -------------------------------- | ----------------- | -------------------------- |
| No SD card, device running       | WiFi or USB       | `monitor`, `api`           |
| SD card, device running          | WiFi or USB       | Any tool                   |
| SD card, device not with you     | None (card on PC) | `report`, `map`, `topology`, `merge`, `wireshark` |

No SD card means no persistence — export before power-cycling.

---

## Directory layout

```
tools/
  report.py        security assessment (main analysis tool)
  map.py           GPS → folium HTML
  topology.py      mesh graph → SVG (or PNG via -o)
  merge.py         cross-capture identity linker
  monitor.py       live WebSocket stream (headless + --tui)
  wireshark.py     PCAP → LoRaTap
  sniffer.py       unified CLI dispatcher

  core/            shared code (one source of truth)
    capture.py       CSV + PCAP loaders → Capture / CapturedPacket
    decode.py        PSK_DB + AES-CTR decrypt + protobuf primitives
    models.py        shared data classes
    cli.py           argparse helpers (--api, --output, ...)

  meshtastic/      Meshtastic-specific standalone decoders
  meshcore/        MeshCore AES-128-ECB decoder
  dev/             api_client, wireshark_exporter
```

PSK decryption, the PSK risk database, and Meshtastic protobuf parsing
all live in `core/decode.py` — `report`, `map`, `topology`, `merge`,
and `monitor --tui` all share it.
