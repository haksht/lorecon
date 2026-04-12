# Python Analysis Tools

Reference for the `tools/` Python toolkit — what each tool does, how to
run it, and what it produces.

See [USAGE.md](USAGE.md) for install steps and a quick-start command list.

---

## Unified entry point

`sniffer.py` dispatches to every tool. Extra arguments pass through.

```bash
python sniffer.py help
python sniffer.py report capture.csv -o report.html
python sniffer.py monitor --host 192.168.4.1 --tui
```

| Command     | Purpose                                                     |
| ----------- | ----------------------------------------------------------- |
| `report`    | Security assessment HTML report                             |
| `map`       | GPS positions → interactive HTML map                        |
| `topology`  | Mesh graph → PNG (Meshtastic traceroute + MeshCore)         |
| `merge`     | Cross-capture identity linker (2+ CSVs)                     |
| `monitor`   | Live WebSocket packet stream (headless or `--tui`)          |
| `wireshark` | Convert ESP32 PCAP → LoRaTap for Wireshark                  |
| `api`       | REST API client (~30 subcommands)                           |

---

## SD card vs live connection

| Scenario                         | Connection needed | Tools                      |
| -------------------------------- | ----------------- | -------------------------- |
| No SD card, device running       | WiFi or USB       | `monitor`, `api`           |
| SD card, device running          | WiFi or USB       | Any tool                   |
| SD card, device not with you     | None (card on PC) | `report`, `map`, `topology`, `merge`, `wireshark` |

No SD card means no persistence — export before power-cycling.

---

## `report` — security assessment

The main offline analysis tool. Produces a standalone HTML report with
findings prioritized CRITICAL → INFO.

```bash
python -m tools.report capture.csv
python -m tools.report capture.csv --pcap capture.pcap           # also parse LoRaWAN joins
python -m tools.report capture.csv -o report.html
python -m tools.report --api 192.168.4.1                         # live device as input
python -m tools.report new.csv --baseline old.csv                # diff mode
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

---

## `monitor` — live stream

Headless (default) or rich-based dashboard. Requires the device reachable
over WiFi.

```bash
python -m tools.monitor --host 192.168.4.1                  # scrolling lines
python -m tools.monitor --host 192.168.4.1 --tui            # rich dashboard
python -m tools.monitor --host 192.168.4.1 --decrypt        # try PSK decrypt live
python -m tools.monitor --host 192.168.4.1 --messages       # decrypted text only
python -m tools.monitor --host 192.168.4.1 --json           # raw JSON per line
python -m tools.monitor --host 192.168.4.1 --filter meshtastic
python -m tools.monitor --demo                              # simulated, no hardware
```

The `--tui` dashboard has four panels:

- **Header** — connection state, mode, packets/s, decrypt rate
- **Top talkers** — 15 most active nodes (node, name, protocol, pkts, RSSI, PSK)
- **New-node alerts** — most recently first-seen nodes
- **Decrypted messages** — live feed of intercepted text

`--messages` and `--decrypt` require `pip install cryptography`.

---

## `map` — GPS positions

```bash
python -m tools.map capture.csv -o map.html
python -m tools.map capture.csv --min-packets 10 -o map.html
```

Produces an interactive [folium](https://python-visualization.github.io/folium/)
HTML map. `--min-packets` filters out stragglers to keep the map readable.

---

## `topology` — mesh graph

```bash
python -m tools.topology capture.csv -o topology.png
```

PNG visualization of the mesh. Most informative when the capture contains
Meshtastic traceroute (portnum 70) or NeighborInfo (portnum 71) packets;
otherwise falls back to relay-byte inference. Decrypts NodeInfo on the fly
to label nodes with operator-chosen names. MeshCore captures render by
channel membership.

---

## `merge` — cross-capture linker

```bash
python -m tools.merge hotel.csv conference.csv airport.csv
python -m tools.merge a.csv b.csv --json linked.json --min-captures 2
```

Finds nodes appearing across multiple captures — useful for tracking
operators who travel between sessions. Sorts by ubiquity, then by total
packet count. Re-decrypts NodeInfo from raw bytes so CSVs with a missing
`node_id` column still link correctly.

---

## `wireshark` — LoRaTap conversion

```bash
python -m tools.wireshark capture.pcap
python -m tools.wireshark capture.pcap --open
```

Converts the ESP32's custom PCAP pseudo-header to standard LoRaTap (DLT
270). Output saved alongside the input with `_loratap` appended.

---

## `api` — device REST client

```bash
python -m tools.dev.api_client --host 192.168.4.1 status
python -m tools.dev.api_client --host 192.168.4.1 devices
python -m tools.dev.api_client --host 192.168.4.1 replay-slots
```

Developer tool — exposes every endpoint on the firmware API.

---

## Directory layout

```
tools/
  report.py        security assessment (main analysis tool)
  map.py           GPS → folium HTML
  topology.py      mesh graph → PNG
  merge.py         cross-capture identity linker
  monitor.py       live WebSocket stream (headless + --tui)
  wireshark.py     PCAP → LoRaTap
  sniffer.py       unified CLI dispatcher
  requirements.txt

  core/            shared code (one source of truth)
    capture.py       CSV + PCAP loaders → Capture / CapturedPacket
    decode.py        PSK_DB + AES-CTR decrypt + protobuf primitives
    models.py        shared data classes
    cli.py           argparse helpers (--api, --output, ...)

  meshtastic/      Meshtastic-specific standalone decoders
  meshcore/        MeshCore AES-128-ECB decoder
  dev/             api_client, build_capabilities, wireshark_exporter
```

PSK decryption, the PSK risk database, and Meshtastic protobuf parsing
all live in `core/decode.py` — `report`, `map`, `topology`, `merge`,
and `monitor --tui` all share it.

---

## Requirements

Install with `pip install -r tools/requirements.txt`.

- Python 3.10+
- `cryptography` — PSK decryption (highly recommended)
- `websocket-client` — live monitor
- `rich` — `monitor --tui` dashboard
- `folium` — `map` HTML output
- `matplotlib`, `networkx` — `topology` PNG
- `requests` — REST API client

Without `cryptography` the tools still run, but skip all decryption-dependent
findings (messages, nonce reuse, identity harvest, plaintext GPS).
