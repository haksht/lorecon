#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - Security Assessment Report

Analyzes a captured CSV (and optional PCAP) for security findings:
  - Default PSK decryption (23 known Meshtastic keys + MeshCore public key)
  - Intercepted plaintext message content
  - Unencrypted traffic
  - LoRaWAN DevEUI / AppEUI exposure (OTAA join requests sent in plaintext)
  - Legacy admin key vulnerability
  - Device inventory with risk scoring

Outputs a self-contained HTML report that opens in any browser.

Usage:
    python report.py capture.csv
    python report.py capture.csv -o report.html
    python report.py capture.csv --pcap capture.pcap
    python report.py --api 192.168.4.1

Requirements:
    pip install cryptography   (PSK decryption)
    pip install requests       (--api mode)
"""

import struct
import sys
from collections import defaultdict
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

from tools.core import capture as capture_loader
from tools.core import cli, decode
from tools.core.models import Capture, CapturedPacket


CRYPTO_AVAILABLE = decode.CRYPTO_AVAILABLE

# PSK_DB preserved as (b64, name, risk) tuples so existing report code iterates
# the same way. Built from the canonical core.decode.PSK_DB.
PSK_DB: List[Tuple[str, str, str]] = [(e.b64, e.name, e.risk) for e in decode.PSK_DB]

SENSITIVE_PORTNUMS = {1, 2, 67}


def _try_decrypt_meshtastic(raw: bytes) -> Optional[Tuple[str, str, int, str]]:
    """Decrypt a Meshtastic packet. Returns (psk_name, risk, portnum, text) or None."""
    result = decode.try_decrypt(raw)
    if not result:
        return None
    text = ''
    if result.portnum == decode.PORT_TEXT_MESSAGE:
        inner = decode.extract_inner(result.plaintext)
        if inner:
            try:
                text = inner.decode('utf-8', errors='replace').strip('\x00').strip()
            except Exception:
                pass
    return (result.entry.name, result.entry.risk, result.portnum, text)


# ---------------------------------------------------------------------------
# ESP32 PCAP reader + LoRaWAN join parser
# (ESP32 pcaps prepend a custom LoRa header per packet, so we keep a local
# reader here rather than using core.capture._load_pcap.)
# ---------------------------------------------------------------------------

LORA_HEADER_FMT  = '<fffBIBH'
LORA_HEADER_SIZE = struct.calcsize(LORA_HEADER_FMT)


def _read_pcap_payloads(filepath: str):
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
                _, _, cap_len, _ = struct.unpack('<IIII', rec)
                pkt = fh.read(cap_len)
                if len(pkt) > LORA_HEADER_SIZE:
                    yield pkt[LORA_HEADER_SIZE:]
    except Exception:
        return


def _parse_lorawan_join(payload: bytes) -> Optional[Dict]:
    if len(payload) < 23:
        return None
    mhdr = payload[0]
    if (mhdr >> 5) != 0:
        return None
    app_eui   = payload[1:9][::-1].hex(':').upper()
    dev_eui   = payload[9:17][::-1].hex(':').upper()
    dev_nonce = struct.unpack('<H', payload[17:19])[0]
    return {'dev_eui': dev_eui, 'app_eui': app_eui, 'dev_nonce': dev_nonce}


# ---------------------------------------------------------------------------
# Assessment data model
# ---------------------------------------------------------------------------

class DeviceRecord:
    def __init__(self, node_id: str):
        self.node_id       = node_id
        self.protocol      = 'Unknown'
        self.packets       = 0
        self.rssi_vals:    List[float] = []
        self.encrypted     = 0
        self.decrypted     = 0
        self.psk_names:    Set[str]   = set()
        self.psk_risk:     str        = ''
        self.texts:        List[str]  = []
        self.has_gps       = False
        self.positions:    List[Tuple[float, float]] = []  # (lat, lon)
        self.timestamps:   List[float] = []                 # packet unix ts (s)
        # NodeInfo identity (populated when a NODEINFO packet decrypts)
        self.short_name:   Optional[str] = None
        self.long_name:    Optional[str] = None
        self.hw_model:     Optional[str] = None
        self.is_licensed:  bool = False

    def risk_color(self) -> str:
        return {'CRITICAL': '#e74c3c', 'HIGH': '#e67e22',
                'MEDIUM': '#f1c40f', 'LOW': '#3498db'}.get(self.psk_risk, '#888')

    def avg_rssi(self) -> Optional[float]:
        return sum(self.rssi_vals) / len(self.rssi_vals) if self.rssi_vals else None


class Assessment:
    def __init__(self):
        self.devices:        Dict[str, DeviceRecord] = {}
        self.total_packets:  int = 0
        self.proto_counts:   Dict[str, int] = defaultdict(int)
        self.text_messages:  List[Dict] = []
        self.lorawan_joins:  List[Dict] = []
        self.findings:       List[Dict] = []
        self.start_time:     Optional[float] = None
        self.end_time:       Optional[float] = None
        self.source_files:   List[str] = []
        # (node_id, packet_id) -> {ciphertext_hash: count}. Used to detect
        # AES-CTR nonce reuse: same (src, pid) with *different* ciphertexts
        # means the same keystream was applied to different plaintexts.
        self._pid_seen:      Dict[Tuple[str, int], Dict[int, int]] = defaultdict(lambda: defaultdict(int))
        self.nonce_reuses:   List[Dict] = []
        # Baseline diff: populated by compute_baseline_diff() when --baseline is given
        self.baseline_source: Optional[str] = None
        self.diff_new:        List[DeviceRecord]  = []
        self.diff_vanished:   List[Dict]          = []
        self.diff_risk_up:    List[Dict]          = []
        self.diff_new_psk:    List[Dict]          = []

    def _device(self, node_id: str) -> DeviceRecord:
        if node_id not in self.devices:
            self.devices[node_id] = DeviceRecord(node_id)
        return self.devices[node_id]

    def ingest_capture(self, cap: Capture) -> None:
        self.source_files.append(cap.source)
        for p in cap:
            self._process_packet(p)

    def ingest_devices_api(self, host: str) -> None:
        devices = capture_loader.load_devices(host)
        self.source_files.append(f"API:{host}")
        for d in devices:
            dev = self._device(d.node_id)
            dev.protocol = d.protocol
            dev.packets  = d.packet_count
            self.total_packets += dev.packets
            if d.rssi_dbm is not None:
                dev.rssi_vals.append(d.rssi_dbm)
            if d.lat_deg is not None and d.lon_deg is not None:
                if d.lat_deg != 0.0 or d.lon_deg != 0.0:
                    dev.has_gps = True
        print(f"[API] Loaded {len(self.devices)} devices from {host}")

    def load_pcap(self, filepath: str) -> None:
        self.source_files.append(Path(filepath).name)
        for payload in _read_pcap_payloads(filepath):
            join = _parse_lorawan_join(payload)
            if join and join not in self.lorawan_joins:
                self.lorawan_joins.append(join)
            result = _try_decrypt_meshtastic(payload)
            if result:
                name, risk, portnum, text = result
                if portnum in SENSITIVE_PORTNUMS and text:
                    self.text_messages.append({
                        'node_id': '(from PCAP)',
                        'psk': name, 'risk': risk, 'text': text,
                    })

    def _process_packet(self, p: CapturedPacket) -> None:
        self.total_packets += 1
        ts = p.timestamp_ms / 1000.0 if p.timestamp_ms else 0.0
        if ts:
            if self.start_time is None or ts < self.start_time:
                self.start_time = ts
            if self.end_time is None or ts > self.end_time:
                self.end_time = ts

        self.proto_counts[p.protocol] += 1
        dev = self._device(p.node_id_hex)
        dev.protocol = p.protocol
        dev.packets += 1
        if p.rssi_dbm:
            dev.rssi_vals.append(p.rssi_dbm)
        if p.encrypted:
            dev.encrypted += 1

        if ts:
            dev.timestamps.append(ts)

        # Only count as a node location when firmware tagged it as such.
        # `sniffer`-sourced positions are where WE were, not the node — they
        # would otherwise pin every heard node onto our own track.
        if (p.lat_deg is not None and p.lon_deg is not None
                and (p.lat_deg != 0.0 or p.lon_deg != 0.0)
                and p.position_source != 'sniffer'):
            dev.has_gps = True
            dev.positions.append((p.lat_deg, p.lon_deg))

        # PSK result from firmware CSV (already cracked by firmware)
        if p.psk_result and p.psk_result not in ('none', 'failed'):
            dev.decrypted += 1
            dev.psk_names.add(p.psk_result)
            for _, name, risk in PSK_DB:
                if name == p.psk_result:
                    if not dev.psk_risk or _risk_rank(risk) > _risk_rank(dev.psk_risk):
                        dev.psk_risk = risk
                    break

        # Python-side decryption on still-encrypted packets
        elif p.encrypted and CRYPTO_AVAILABLE and p.raw_hex:
            try:
                raw = p.raw_bytes
                dec_result = decode.try_decrypt(raw)
                if dec_result:
                    name     = dec_result.entry.name
                    risk     = dec_result.entry.risk
                    portnum  = dec_result.portnum
                    pt       = dec_result.plaintext
                    dev.decrypted += 1
                    dev.psk_names.add(name)
                    if not dev.psk_risk or _risk_rank(risk) > _risk_rank(dev.psk_risk):
                        dev.psk_risk = risk
                    # NodeInfo identity harvest
                    if portnum == decode.PORT_NODEINFO:
                        inner = decode.extract_inner(pt)
                        if inner:
                            user = decode.parse_user(inner)
                            if user:
                                if user.get('short_name'): dev.short_name = user['short_name']
                                if user.get('long_name'):  dev.long_name  = user['long_name']
                                if user.get('hw_model_name'): dev.hw_model = user['hw_model_name']
                                if user.get('is_licensed'): dev.is_licensed = True
                    # Position disclosure (decrypted lat/lon on public PSK)
                    if portnum == decode.PORT_POSITION:
                        inner = decode.extract_inner(pt)
                        if inner:
                            pos = decode.parse_position(inner)
                            if pos:
                                dev.has_gps = True
                                dev.positions.append((pos[0], pos[1]))
                    # Text intercepts
                    if portnum == decode.PORT_TEXT_MESSAGE:
                        inner = decode.extract_inner(pt)
                        if inner:
                            try:
                                text = inner.decode('utf-8', errors='replace').strip('\x00').strip()
                            except Exception:
                                text = ''
                            if text:
                                dev.texts.append(text)
                                self.text_messages.append({
                                    'node_id': p.node_id_hex, 'psk': name, 'risk': risk, 'text': text,
                                })
                    # Nonce-reuse tracking: (src, pid) with different ciphertexts
                    # means same AES-CTR keystream applied to different plaintexts.
                    if p.packet_id is not None and len(raw) >= 20:
                        ct = raw[16:]
                        ct_hash = hash(bytes(ct))
                        self._pid_seen[(p.node_id_hex, p.packet_id)][ct_hash] += 1
            except Exception:
                pass

        # LoRaWAN join detection from raw hex
        if p.protocol in ('LoRaWAN', 'Unknown') and p.raw_hex:
            try:
                join = _parse_lorawan_join(p.raw_bytes)
                if join and join not in self.lorawan_joins:
                    self.lorawan_joins.append(join)
            except Exception:
                pass

    def compute_baseline_diff(self, baseline_path: str) -> None:
        """Compare current devices against a prior capture. Populates
        diff_new / diff_vanished / diff_risk_up / diff_new_psk."""
        bp = Path(baseline_path)
        if not bp.exists():
            print(f"WARNING: baseline not found: {baseline_path}")
            return
        print(f"Loading baseline: {bp}")
        base = Assessment()
        base.ingest_capture(capture_loader.load(str(bp)))
        self.baseline_source = bp.name

        base_devs = base.devices
        for nid, d in self.devices.items():
            b = base_devs.get(nid)
            if b is None:
                self.diff_new.append(d)
                continue
            if _risk_rank(d.psk_risk) > _risk_rank(b.psk_risk):
                self.diff_risk_up.append({
                    'node_id': nid, 'from': b.psk_risk or '—', 'to': d.psk_risk,
                    'short_name': d.short_name or '',
                })
            added_psk = d.psk_names - b.psk_names
            if added_psk:
                self.diff_new_psk.append({
                    'node_id': nid, 'added': sorted(added_psk),
                    'short_name': d.short_name or '',
                })
        for nid, b in base_devs.items():
            if nid not in self.devices:
                self.diff_vanished.append({
                    'node_id': nid, 'protocol': b.protocol, 'packets': b.packets,
                    'short_name': b.short_name or '',
                })

    def analyze(self):
        """Generate security findings."""
        self.findings = []

        # --- Default PSK usage ---
        decrypted_devs = {nid: d for nid, d in self.devices.items() if d.psk_names}
        if decrypted_devs:
            all_keys: Set[str] = set()
            for d in decrypted_devs.values():
                all_keys.update(d.psk_names)
            worst_risk = max((_risk_rank(d.psk_risk) for d in decrypted_devs.values()), default=0)
            severity   = ['LOW', 'LOW', 'MEDIUM', 'HIGH', 'CRITICAL'][min(worst_risk, 4)]
            total_dec  = sum(d.decrypted for d in decrypted_devs.values())
            node_ids   = sorted(decrypted_devs)[:10]
            node_str   = ', '.join(f'<code>{n}</code>' for n in node_ids)
            if len(decrypted_devs) > 10:
                node_str += f' (+{len(decrypted_devs) - 10} more)'
            self.findings.append({
                'severity': severity,
                'title': f'Default PSK Encryption Cracked ({len(decrypted_devs)} devices)',
                'desc': (
                    f'{total_dec} packets decrypted using known default keys: '
                    f'<b>{", ".join(sorted(all_keys)[:5])}</b>. '
                    f'Affected nodes: {node_str}.'
                ),
                'rec': 'Generate unique channel PSKs. Do not ship factory defaults.',
                'cwe': 'CWE-1392',
            })

        # --- Intercepted text messages ---
        if self.text_messages:
            preview = self.text_messages[:3]
            samples = ''.join(
                f'<li><code>{m["node_id"][-8:]}</code>: "{_trunc(m["text"], 80)}"</li>'
                for m in preview
            )
            self.findings.append({
                'severity': 'CRITICAL',
                'title': f'{len(self.text_messages)} Private Messages Intercepted',
                'desc': (
                    f'Decrypted with default PSKs — plaintext message content recovered:<ul>'
                    f'{samples}</ul>'
                    + (f'({len(self.text_messages) - 3} more not shown)' if len(self.text_messages) > 3 else '')
                ),
                'rec': 'Deploy unique PSKs. Treat all default-key traffic as compromised.',
                'cwe': 'CWE-311',
            })

        # --- Legacy admin key ---
        admin_devs = [nid for nid, d in self.devices.items()
                      if any('admin' in k.lower() or 'Admin' in k for k in d.psk_names)]
        if admin_devs:
            self.findings.append({
                'severity': 'CRITICAL',
                'title': 'Legacy Admin Key — Remote Config Attack Possible',
                'desc': (
                    f'Devices using pre-2.5 admin channel default key: '
                    f'{", ".join(f"<code>{n}</code>" for n in admin_devs[:6])}. '
                    f'An attacker can send admin commands to these devices.'
                ),
                'rec': 'Update firmware ≥2.5 and regenerate admin channel key.',
                'cwe': 'CWE-798',
            })

        # --- Cross-channel operator bridging ---
        # Nodes whose traffic decrypts with 2+ distinct PSKs = same device
        # listens on multiple channels. Critical if one is the Admin key
        # (device accepts remote-config AND is on public mesh).
        bridgers = [d for d in self.devices.values() if len(d.psk_names) >= 2]
        if bridgers:
            admin_bridgers = [d for d in bridgers
                              if any('admin' in k.lower() for k in d.psk_names)]
            sample = sorted(bridgers, key=lambda d: -d.packets)[:8]
            rows = ''.join(
                f'<li><code>{d.node_id}</code>'
                + (f' <b>{_html_esc(d.short_name)}</b>' if d.short_name else '')
                + f' — {", ".join(sorted(d.psk_names))}</li>'
                for d in sample
            )
            sev = 'CRITICAL' if admin_bridgers else 'HIGH'
            self.findings.append({
                'severity': sev,
                'title': f'Cross-Channel Bridging ({len(bridgers)} devices on 2+ keyed channels)',
                'desc': (
                    f'{len(bridgers)} devices decrypt with multiple distinct PSKs — '
                    f'they sit on more than one channel simultaneously. '
                    + (f'{len(admin_bridgers)} of these also carry the Admin channel key, '
                       f'meaning remote-admin commands can be pivoted through public mesh traffic. '
                       if admin_bridgers else '')
                    + f'<ul>{rows}</ul>'
                ),
                'rec': 'Segregate admin channel to dedicated hardware; do not co-locate admin + public keys.',
                'cwe': 'CWE-653',
            })

        # --- DevNonce reuse (LoRaWAN OTAA replay) ---
        # Same (DevEUI, DevNonce) seen twice = replay-vulnerable join.
        # LoRaWAN 1.0.x servers MUST reject reused DevNonces; some don't.
        nonce_seen: Dict[Tuple[str, int], int] = defaultdict(int)
        for j in self.lorawan_joins:
            nonce_seen[(j['dev_eui'], j['dev_nonce'])] += 1
        reused = {k: v for k, v in nonce_seen.items() if v > 1}
        if reused:
            rows = ''.join(
                f'<li><code>{eui}</code> DevNonce <code>0x{nonce:04x}</code> — seen {count}×</li>'
                for (eui, nonce), count in list(reused.items())[:10]
            )
            # Separately flag DevEUIs that appear with multiple distinct nonces
            # (normal) vs with the SAME nonce repeated (replay or server misconfig).
            self.findings.append({
                'severity': 'HIGH',
                'title': f'LoRaWAN DevNonce Reuse ({len(reused)} collisions)',
                'desc': (
                    f'Same DevEUI + DevNonce pair transmitted more than once. Under LoRaWAN 1.0.x '
                    f'a compliant network server rejects duplicate DevNonces (anti-replay). '
                    f'If the server accepts, an attacker who recorded the corresponding JoinAccept '
                    f'can bring the node online on a rogue server.<ul>{rows}</ul>'
                ),
                'rec': 'Verify the network server rejects duplicate DevNonces. Consider LoRaWAN 1.1 JoinEUI + monotonic DevNonce.',
                'cwe': 'CWE-294',
            })

        # --- LoRaWAN DevEUI exposure ---
        if self.lorawan_joins:
            sample = self.lorawan_joins[:3]
            rows = ''.join(
                f'<li>DevEUI <code>{j["dev_eui"]}</code> AppEUI <code>{j["app_eui"]}</code></li>'
                for j in sample
            )
            extra = f'({len(self.lorawan_joins) - 3} more)' if len(self.lorawan_joins) > 3 else ''
            self.findings.append({
                'severity': 'MEDIUM',
                'title': f'LoRaWAN DevEUI Harvested ({len(self.lorawan_joins)} join requests)',
                'desc': (
                    f'OTAA join requests transmit DevEUI + AppEUI in plaintext by design:<ul>'
                    f'{rows}</ul>{extra}'
                ),
                'rec': 'Use ABP activation only where join-request tracking is a concern. '
                       'Treat DevEUI as a persistent identifier (correlates physical device).',
                'cwe': 'CWE-359',
            })

        # --- Unencrypted traffic ---
        cleartext = [nid for nid, d in self.devices.items()
                     if d.encrypted == 0 and d.packets > 2]
        if cleartext:
            node_str = ', '.join(f'<code>{n}</code>' for n in sorted(cleartext)[:8])
            self.findings.append({
                'severity': 'MEDIUM',
                'title': f'Unencrypted Traffic ({len(cleartext)} devices)',
                'desc': f'{len(cleartext)} devices transmitting without encryption: {node_str}',
                'rec': 'Enable channel encryption on all endpoints.',
                'cwe': 'CWE-319',
            })

        # --- AES-CTR nonce reuse ---
        self.nonce_reuses = []
        for (nid, pid), ct_hashes in self._pid_seen.items():
            if len(ct_hashes) > 1:  # same (src,pid), different ciphertexts
                self.nonce_reuses.append({
                    'node_id': nid, 'packet_id': pid,
                    'distinct_ciphertexts': len(ct_hashes),
                    'total_packets': sum(ct_hashes.values()),
                })
        if self.nonce_reuses:
            sample = self.nonce_reuses[:5]
            rows = ''.join(
                f'<li><code>{r["node_id"]}</code> / pid <code>0x{r["packet_id"]:08x}</code> — '
                f'{r["distinct_ciphertexts"]} distinct ciphertexts</li>'
                for r in sample
            )
            self.findings.append({
                'severity': 'HIGH',
                'title': f'AES-CTR Nonce Reuse ({len(self.nonce_reuses)} collisions)',
                'desc': (
                    f'Same (node_id, packet_id) pair observed with different ciphertexts. '
                    f'Meshtastic derives the AES-CTR nonce from these two fields, so reuse means '
                    f'the same keystream encrypted different plaintexts — XOR of the ciphertexts '
                    f'leaks XOR of the plaintexts.<ul>{rows}</ul>'
                ),
                'rec': 'Use a larger packet_id space or include a counter/timestamp in the nonce.',
                'cwe': 'CWE-323',
            })

        # --- Identified devices (NodeInfo harvest) ---
        named_devs = [d for d in self.devices.values() if d.short_name or d.long_name]
        if named_devs:
            sample = sorted(named_devs, key=lambda d: -d.packets)[:6]
            rows = ''.join(
                f'<li><code>{d.node_id}</code> → '
                f'<b>{_html_esc(d.short_name or "?")}</b> / '
                f'{_html_esc(d.long_name or "—")}'
                + (f' ({d.hw_model})' if d.hw_model else '')
                + '</li>'
                for d in sample
            )
            self.findings.append({
                'severity': 'MEDIUM',
                'title': f'{len(named_devs)} Operators Identified by NodeInfo Broadcast',
                'desc': (
                    f'NODEINFO packets broadcast the operator-chosen name and hardware model '
                    f'on the default-keyed channel, attributing node IDs to real people:<ul>{rows}</ul>'
                ),
                'rec': 'Operators should avoid real names / call signs on public channels.',
                'cwe': 'CWE-359',
            })

        # --- Baseline diff ---
        if self.baseline_source and (self.diff_new or self.diff_vanished
                                     or self.diff_risk_up or self.diff_new_psk):
            bits = []
            if self.diff_new:      bits.append(f'{len(self.diff_new)} new')
            if self.diff_vanished: bits.append(f'{len(self.diff_vanished)} vanished')
            if self.diff_risk_up:  bits.append(f'{len(self.diff_risk_up)} risk↑')
            if self.diff_new_psk:  bits.append(f'{len(self.diff_new_psk)} new PSK')
            sev = 'HIGH' if (self.diff_risk_up or self.diff_new_psk) else 'MEDIUM'
            self.findings.append({
                'severity': sev,
                'title': f'Changes since baseline ({self.baseline_source}): ' + ', '.join(bits),
                'desc': ('Devices added, removed, or whose key posture changed since the '
                         'baseline capture. See table below for details.'),
                'rec': 'Investigate new devices and nodes that gained access to additional PSKs.',
                'cwe': None,
            })

        # --- Mobility + GPS privacy ---
        # Classify each GPS-emitting device as stationary (<100 m bounding box)
        # or mobile. Flag devices disclosing real-time position on a known-weak PSK.
        import math
        def _bbox_diag_m(pts: List[Tuple[float, float]]) -> float:
            if len(pts) < 2:
                return 0.0
            lats = [p[0] for p in pts]; lons = [p[1] for p in pts]
            lat_m = (max(lats) - min(lats)) * 111000.0
            lon_m = (max(lons) - min(lons)) * 111000.0 * math.cos(math.radians(sum(lats) / len(lats)))
            return math.hypot(lat_m, lon_m)

        mobile_devs: List[DeviceRecord] = []
        for d in self.devices.values():
            if len(d.positions) >= 2:
                d.bbox_m = _bbox_diag_m(d.positions)
                d.is_mobile = d.bbox_m > 100.0
                if d.is_mobile:
                    mobile_devs.append(d)
            elif d.positions:
                d.bbox_m = 0.0
                d.is_mobile = False

        leak_devs = [d for d in self.devices.values()
                     if d.positions and d.psk_risk in ('CRITICAL', 'HIGH')]
        if leak_devs:
            sample = sorted(leak_devs, key=lambda d: -len(d.positions))[:8]
            rows = ''.join(
                f'<li><code>{d.node_id}</code>'
                + (f' <b>{_html_esc(d.short_name)}</b>' if d.short_name else '')
                + f' — {len(d.positions)} fixes, '
                + ('mobile' if getattr(d, "is_mobile", False) else 'stationary')
                + f' ({d.psk_risk})</li>'
                for d in sample
            )
            self.findings.append({
                'severity': 'HIGH',
                'title': f'Plaintext GPS Position Disclosure ({len(leak_devs)} devices)',
                'desc': (
                    f'Devices are broadcasting lat/lon on channels encrypted with a known-weak '
                    f'PSK. Anyone with the default key can track their location in real time:<ul>{rows}</ul>'
                ),
                'rec': 'Disable position broadcast on public channels, or move to a unique PSK.',
                'cwe': 'CWE-200',
            })

        if mobile_devs:
            self.findings.append({
                'severity': 'INFO',
                'title': f'{len(mobile_devs)} mobile devices tracked',
                'desc': (f'Devices whose observed positions span more than 100 m — likely vehicles, '
                         f'handhelds, or roving operators. Stationary nodes ({sum(1 for d in self.devices.values() if d.positions and not getattr(d, "is_mobile", False))}) '
                         f'are likely fixed infrastructure.'),
                'rec': 'Correlate mobile tracks with physical presence for OSINT enrichment.',
                'cwe': None,
            })

        # --- Close-range infrastructure ---
        close = [nid for nid, d in self.devices.items()
                 if d.rssi_vals and max(d.rssi_vals) > -50]
        if close:
            self.findings.append({
                'severity': 'INFO',
                'title': f'High-Power Fixed Infrastructure ({len(close)} nodes, RSSI > −50 dBm)',
                'desc': f'Strong signal suggests base stations or fixed installs within ~100 m.',
                'rec': 'Audit physical security of these installations.',
                'cwe': None,
            })


def _risk_rank(r: str) -> int:
    return {'LOW': 1, 'MEDIUM': 2, 'HIGH': 3, 'CRITICAL': 4}.get(r, 0)


def _trunc(s: str, n: int) -> str:
    return s[:n] + '…' if len(s) > n else s


# ---------------------------------------------------------------------------
# HTML report generator
# ---------------------------------------------------------------------------

SEVERITY_STYLE = {
    'CRITICAL': ('background:#7b1313;color:#fff;', '⬛ CRITICAL'),
    'HIGH':     ('background:#7a3800;color:#fff;', '🟠 HIGH'),
    'MEDIUM':   ('background:#6b5a00;color:#fff;', '🟡 MEDIUM'),
    'LOW':      ('background:#1a3a5c;color:#fff;', '🔵 LOW'),
    'INFO':     ('background:#1e3a1e;color:#ddd;', 'ℹ INFO'),
}


def _render_html(a: Assessment, output: str):
    now = datetime.now().strftime('%Y-%m-%d %H:%M')
    duration_str = ''
    if a.start_time and a.end_time:
        mins = (a.end_time - a.start_time) / 60
        duration_str = f'{mins:.1f} min'

    sev_order = ['CRITICAL', 'HIGH', 'MEDIUM', 'LOW', 'INFO']
    findings = sorted(a.findings, key=lambda f: sev_order.index(f.get('severity', 'INFO')))

    counts = {s: sum(1 for f in findings if f['severity'] == s) for s in sev_order}

    proto_rows = ''.join(
        f'<tr><td>{p}</td><td>{c}</td>'
        f'<td>{c / max(a.total_packets, 1) * 100:.1f}%</td></tr>'
        for p, c in sorted(a.proto_counts.items(), key=lambda x: -x[1])
    )

    # Capture time window for per-device sparklines
    _spark_start = a.start_time or 0.0
    _spark_end   = a.end_time   or 0.0
    _spark_span  = max(_spark_end - _spark_start, 1.0)
    _SPARK = ' ▁▂▃▄▅▆▇█'
    def _sparkline(tss: List[float], buckets: int = 24) -> str:
        if not tss or _spark_span <= 1.0:
            return ''
        bins = [0] * buckets
        for t in tss:
            i = int((t - _spark_start) / _spark_span * buckets)
            if 0 <= i < buckets:
                bins[i] += 1
        mx = max(bins) or 1
        return ''.join(_SPARK[min(8, (v * 8 + mx - 1) // mx)] for v in bins)

    sorted_devs = sorted(a.devices.values(),
                         key=lambda d: (_risk_rank(d.psk_risk), d.packets), reverse=True)[:50]
    dev_rows = ''
    for d in sorted_devs:
        rssi_str = f'{d.avg_rssi():.1f}' if d.avg_rssi() is not None else '—'
        psk_str  = ', '.join(sorted(d.psk_names)) if d.psk_names else '—'
        risk_col = d.risk_color()
        risk_lbl = f'<span style="color:{risk_col};font-weight:bold">{d.psk_risk}</span>' if d.psk_risk else '—'
        gps_lbl  = '✓' if d.has_gps else ''
        if d.positions and getattr(d, 'is_mobile', False):
            gps_lbl = f'✓ <span title="bbox {getattr(d,"bbox_m",0):.0f} m">mob</span>'
        elif d.positions:
            gps_lbl = '✓ fix'
        spark = _sparkline(d.timestamps)
        enc_pct  = f'{d.encrypted / max(d.packets,1)*100:.0f}%' if d.packets else '—'
        name_lbl = _html_esc(d.short_name or '') if d.short_name else '—'
        if d.long_name and d.long_name != d.short_name:
            name_lbl = f'<b>{_html_esc(d.short_name or "")}</b> <small>{_html_esc(d.long_name)}</small>' \
                       if d.short_name else f'<small>{_html_esc(d.long_name)}</small>'
        dev_rows += (
            f'<tr><td><code>{d.node_id}</code></td>'
            f'<td>{name_lbl}</td>'
            f'<td>{d.protocol}</td>'
            f'<td>{d.packets}</td>'
            f'<td>{rssi_str}</td>'
            f'<td>{enc_pct}</td>'
            f'<td>{risk_lbl}</td>'
            f'<td style="font-size:11px">{psk_str}</td>'
            f'<td>{gps_lbl}</td>'
            f'<td style="font-family:monospace;letter-spacing:-1px">{spark}</td></tr>\n'
        )

    ident_section = ''
    named_devs = sorted(
        [d for d in a.devices.values() if d.short_name or d.long_name],
        key=lambda d: -d.packets,
    )
    if named_devs:
        ident_rows = ''.join(
            f'<tr><td><code>{d.node_id}</code></td>'
            f'<td><b>{_html_esc(d.short_name or "—")}</b></td>'
            f'<td>{_html_esc(d.long_name or "—")}</td>'
            f'<td>{_html_esc(d.hw_model or "—")}</td>'
            f'<td>{"✓" if d.is_licensed else ""}</td>'
            f'<td>{d.packets}</td></tr>\n'
            for d in named_devs
        )
        ident_section = f'''
<h2>Device Identities <span class="badge" style="background:#6b5a00">{len(named_devs)}</span></h2>
<p>Operator-chosen names and hardware models extracted from decrypted NODEINFO broadcasts.</p>
<table>
<tr><th>Node ID</th><th>Short</th><th>Long Name</th><th>Hardware</th><th>Licensed</th><th>Pkts</th></tr>
{ident_rows}
</table>
'''

    hist_section = ''
    all_ts: List[float] = []
    for d in a.devices.values():
        all_ts.extend(d.timestamps)
    if all_ts and _spark_span > 1.0:
        buckets = 48
        bins = [0] * buckets
        for t in all_ts:
            i = int((t - _spark_start) / _spark_span * buckets)
            if 0 <= i < buckets:
                bins[i] += 1
        mx = max(bins) or 1
        bars = ''.join(
            f'<div style="display:inline-block;width:14px;margin:0 1px;height:{max(2,int(v/mx*100))}px;'
            f'background:#3498db;vertical-align:bottom" title="{v} pkts"></div>'
            for v in bins
        )
        start_s = datetime.fromtimestamp(_spark_start).strftime('%H:%M')
        end_s   = datetime.fromtimestamp(_spark_end  ).strftime('%H:%M')
        hist_section = (f'<h2>Capture Activity Timeline</h2>'
                        f'<p>Packets per time bucket across the capture window '
                        f'({start_s} → {end_s}, {buckets} buckets).</p>'
                        f'<div style="height:110px;padding:4px;background:#1a1a1a;border:1px solid #333">{bars}</div>')

    diff_section = ''
    if a.baseline_source and (a.diff_new or a.diff_vanished
                              or a.diff_risk_up or a.diff_new_psk):
        def _r(rows, cols):
            return ('<table><tr>' + ''.join(f'<th>{c}</th>' for c in cols) + '</tr>'
                    + ''.join('<tr>' + ''.join(f'<td>{c}</td>' for c in row) + '</tr>'
                              for row in rows) + '</table>')
        blocks = []
        if a.diff_new:
            rows = [(f'<code>{d.node_id}</code>', _html_esc(d.short_name or '—'),
                     d.protocol, d.packets, d.psk_risk or '—')
                    for d in sorted(a.diff_new, key=lambda x: -x.packets)[:50]]
            blocks.append(f'<h3>New devices ({len(a.diff_new)})</h3>'
                          + _r(rows, ['Node', 'Name', 'Protocol', 'Pkts', 'Risk']))
        if a.diff_vanished:
            rows = [(f'<code>{v["node_id"]}</code>', _html_esc(v["short_name"] or '—'),
                     v['protocol'], v['packets'])
                    for v in sorted(a.diff_vanished, key=lambda x: -x['packets'])[:50]]
            blocks.append(f'<h3>Vanished devices ({len(a.diff_vanished)})</h3>'
                          + _r(rows, ['Node', 'Name', 'Protocol', 'Pkts (baseline)']))
        if a.diff_risk_up:
            rows = [(f'<code>{v["node_id"]}</code>', _html_esc(v["short_name"] or '—'),
                     v['from'], v['to']) for v in a.diff_risk_up[:50]]
            blocks.append(f'<h3>Risk increased ({len(a.diff_risk_up)})</h3>'
                          + _r(rows, ['Node', 'Name', 'From', 'To']))
        if a.diff_new_psk:
            rows = [(f'<code>{v["node_id"]}</code>', _html_esc(v["short_name"] or '—'),
                     ', '.join(v['added'])) for v in a.diff_new_psk[:50]]
            blocks.append(f'<h3>Newly observed PSKs ({len(a.diff_new_psk)})</h3>'
                          + _r(rows, ['Node', 'Name', 'Added PSK(s)']))
        diff_section = (f'<h2>Changes Since Baseline '
                        f'<span class="badge" style="background:#2c3e50">'
                        f'{_html_esc(a.baseline_source)}</span></h2>' + ''.join(blocks))

    reuse_section = ''
    if a.nonce_reuses:
        reuse_rows = ''.join(
            f'<tr><td><code>{r["node_id"]}</code></td>'
            f'<td><code>0x{r["packet_id"]:08x}</code></td>'
            f'<td>{r["distinct_ciphertexts"]}</td>'
            f'<td>{r["total_packets"]}</td></tr>\n'
            for r in a.nonce_reuses[:50]
        )
        reuse_section = f'''
<h2>AES-CTR Nonce Reuse <span class="badge" style="background:#7a3800">{len(a.nonce_reuses)}</span></h2>
<p>Same (node_id, packet_id) → same nonce. Distinct ciphertexts at the same nonce means
   the same keystream encrypted different plaintexts: XOR of the two ciphertexts equals
   XOR of the two plaintexts (the key cancels out).</p>
<table>
<tr><th>Node</th><th>Packet ID</th><th>Distinct Ciphertexts</th><th>Total Packets</th></tr>
{reuse_rows}
</table>
'''

    msg_section = ''
    if a.text_messages:
        msg_rows = ''.join(
            f'<tr><td><code>{m["node_id"][-8:]}</code></td>'
            f'<td>{m["psk"]}</td>'
            f'<td style="color:{_risk_col(m["risk"])}">{m["risk"]}</td>'
            f'<td>{_html_esc(m["text"])}</td></tr>\n'
            for m in a.text_messages[:100]
        )
        more = f'<p>({len(a.text_messages) - 100} more messages not shown)</p>' if len(a.text_messages) > 100 else ''
        msg_section = f'''
<h2>Intercepted Messages <span class="badge" style="background:#7b1313">{len(a.text_messages)}</span></h2>
<table>
<tr><th>Node</th><th>PSK Used</th><th>Risk</th><th>Content</th></tr>
{msg_rows}
</table>{more}
'''

    join_section = ''
    if a.lorawan_joins:
        join_rows = ''.join(
            f'<tr><td><code>{j["dev_eui"]}</code></td>'
            f'<td><code>{j["app_eui"]}</code></td>'
            f'<td>{j["dev_nonce"]}</td></tr>\n'
            for j in a.lorawan_joins[:50]
        )
        join_section = f'''
<h2>LoRaWAN DevEUI Harvest <span class="badge" style="background:#6b5a00">{len(a.lorawan_joins)}</span></h2>
<p>OTAA join requests broadcast device identity in plaintext:</p>
<table>
<tr><th>DevEUI</th><th>AppEUI (JoinEUI)</th><th>DevNonce</th></tr>
{join_rows}
</table>
'''

    finding_cards = ''
    for f in findings:
        style, label = SEVERITY_STYLE.get(f['severity'], ('', f['severity']))
        cwe = f'<small>Ref: {f["cwe"]}</small>' if f.get('cwe') else ''
        finding_cards += f'''
<div class="finding">
  <div class="finding-header" style="{style}">{label}: {f["title"]}</div>
  <div class="finding-body">
    <p>{f["desc"]}</p>
    <p><b>Recommendation:</b> {f["rec"]}</p>
    {cwe}
  </div>
</div>
'''

    html = f'''<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>LoRa Sniffer Security Report — {now}</title>
<style>
  body {{ font-family: 'Segoe UI', sans-serif; background:#0d1117; color:#c9d1d9;
         margin:0; padding:20px; }}
  h1   {{ color:#58a6ff; border-bottom:1px solid #30363d; padding-bottom:8px; }}
  h2   {{ color:#79c0ff; margin-top:32px; }}
  .badge {{ display:inline-block; padding:2px 8px; border-radius:4px;
            font-size:13px; color:#fff; margin-left:8px; }}
  .summary-grid {{ display:grid; grid-template-columns:repeat(auto-fit,minmax(130px,1fr));
                   gap:12px; margin:16px 0; }}
  .metric {{ background:#161b22; border:1px solid #30363d; border-radius:8px;
             padding:14px; text-align:center; }}
  .metric-val {{ font-size:2em; font-weight:bold; color:#58a6ff; }}
  .metric-lbl {{ font-size:12px; color:#8b949e; margin-top:4px; }}
  .finding {{ margin:16px 0; border-radius:8px; overflow:hidden;
              border:1px solid #30363d; }}
  .finding-header {{ padding:10px 16px; font-weight:bold; font-size:15px; }}
  .finding-body  {{ padding:12px 16px; background:#161b22; }}
  table {{ border-collapse:collapse; width:100%; margin:12px 0; font-size:13px; }}
  th {{ background:#1c2128; color:#79c0ff; padding:8px 12px; text-align:left;
        border-bottom:1px solid #30363d; }}
  td {{ padding:6px 12px; border-bottom:1px solid #21262d; }}
  tr:hover td {{ background:#161b22; }}
  code {{ background:#161b22; padding:1px 5px; border-radius:3px;
          font-family:monospace; font-size:12px; }}
  small {{ color:#8b949e; }}
  .critical {{ color:#e74c3c; }} .high {{ color:#e67e22; }}
  .medium   {{ color:#f1c40f; }} .low  {{ color:#3498db; }}
</style>
</head>
<body>
<h1>LoRa Network Security Assessment</h1>
<p>Generated: <b>{now}</b> &nbsp;|&nbsp;
   Source: <b>{", ".join(a.source_files)}</b> &nbsp;|&nbsp;
   Duration: <b>{duration_str or "—"}</b></p>

<h2>Executive Summary</h2>
<div class="summary-grid">
  <div class="metric"><div class="metric-val">{len(a.devices)}</div>
    <div class="metric-lbl">Devices Discovered</div></div>
  <div class="metric"><div class="metric-val">{a.total_packets:,}</div>
    <div class="metric-lbl">Total Packets</div></div>
  <div class="metric"><div class="metric-val" style="color:#e74c3c">{counts["CRITICAL"]}</div>
    <div class="metric-lbl">Critical Findings</div></div>
  <div class="metric"><div class="metric-val" style="color:#e67e22">{counts["HIGH"]}</div>
    <div class="metric-lbl">High Findings</div></div>
  <div class="metric"><div class="metric-val" style="color:#f1c40f">{counts["MEDIUM"]}</div>
    <div class="metric-lbl">Medium Findings</div></div>
  <div class="metric"><div class="metric-val">{len(a.text_messages)}</div>
    <div class="metric-lbl">Messages Decrypted</div></div>
  <div class="metric"><div class="metric-val">{len(a.lorawan_joins)}</div>
    <div class="metric-lbl">DevEUI Harvested</div></div>
</div>

<h2>Security Findings</h2>
{finding_cards}

{msg_section}
{ident_section}
{hist_section}
{diff_section}
{reuse_section}
{join_section}

<h2>Protocol Distribution</h2>
<table>
<tr><th>Protocol</th><th>Packets</th><th>Share</th></tr>
{proto_rows}
</table>

<h2>Device Inventory ({len(a.devices)} devices)</h2>
<table>
<tr><th>Node ID</th><th>Name</th><th>Protocol</th><th>Pkts</th><th>RSSI</th>
    <th>Enc%</th><th>Risk</th><th>PSK</th><th>GPS</th><th title="Packet activity over capture window">Activity</th></tr>
{dev_rows}
</table>
{"<p><i>(showing top 50 by risk/packet count)</i></p>" if len(a.devices) > 50 else ""}

<hr style="border-color:#30363d;margin-top:40px">
<p style="color:#8b949e;font-size:12px">
  ESP32 LoRa Sniffer &nbsp;|&nbsp; {now}
</p>
</body>
</html>
'''

    with open(output, 'w', encoding='utf-8') as fh:
        fh.write(html)
    print(f"Report saved: {output}")
    print(f"Open: {Path(output).resolve().as_uri()}")


def _risk_col(r: str) -> str:
    return {'CRITICAL': '#e74c3c', 'HIGH': '#e67e22',
            'MEDIUM': '#f1c40f', 'LOW': '#3498db'}.get(r, '#888')


def _html_esc(s: str) -> str:
    return s.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    ap = cli.base_parser('Generate an HTML security assessment report from captured LoRa data.')
    ap.add_argument('--pcap', metavar='FILE',
                    help='Also process a PCAP file (LoRaWAN joins + additional decryption)')
    ap.add_argument('--baseline', metavar='FILE',
                    help='Compare against a prior capture — report new / vanished / risk-changed nodes')
    args = ap.parse_args()

    if not CRYPTO_AVAILABLE:
        print("WARNING: pip install cryptography  — PSK decryption disabled")

    a = Assessment()

    auto_open = not args.output

    if args.api:
        a.ingest_devices_api(args.api)
        out = args.output or cli.temp_output('.html', stem='report')
    else:
        if not args.input:
            sys.exit("ERROR: provide a capture file or --api HOST")
        p = Path(args.input)
        if not p.exists():
            sys.exit(f"File not found: {args.input}")
        print(f"Loading: {p}")
        cap = capture_loader.load(str(p))
        a.ingest_capture(cap)
        out = args.output or cli.temp_output('.html', stem=f'report_{p.stem}')

    if args.pcap:
        pp = Path(args.pcap)
        if pp.exists():
            print(f"Loading PCAP: {pp}")
            a.load_pcap(str(pp))
        else:
            print(f"WARNING: PCAP not found: {args.pcap}")

    if args.baseline:
        a.compute_baseline_diff(args.baseline)

    a.analyze()

    print(f"\nDevices:  {len(a.devices)}")
    print(f"Packets:  {a.total_packets:,}")
    print(f"Findings: {len(a.findings)}")
    if a.text_messages:
        print(f"Messages: {len(a.text_messages)} decrypted")
    if a.lorawan_joins:
        print(f"Joins:    {len(a.lorawan_joins)} DevEUI harvested")
    if a.baseline_source:
        print(f"Diff vs {a.baseline_source}: "
              f"{len(a.diff_new)} new, {len(a.diff_vanished)} vanished, "
              f"{len(a.diff_risk_up)} risk↑, {len(a.diff_new_psk)} new PSK")

    _render_html(a, out)

    if auto_open:
        print(f"Opening {out} in browser...")
        cli.open_in_browser(out)


if __name__ == '__main__':
    main()
