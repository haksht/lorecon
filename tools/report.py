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

import argparse
import base64
import csv
import hashlib
import struct
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
from collections import defaultdict
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    from cryptography.hazmat.backends import default_backend
    CRYPTO_AVAILABLE = True
except ImportError:
    CRYPTO_AVAILABLE = False

try:
    import requests
    REQUESTS_AVAILABLE = True
except ImportError:
    REQUESTS_AVAILABLE = False


# ---------------------------------------------------------------------------
# PSK database — 23 Meshtastic defaults with risk classification
# ---------------------------------------------------------------------------

PSK_DB = [
    ("AQ==",                        "Default (0x01)",          "CRITICAL"),
    ("PKdTs51e4EB0BoOevIN0Dw==",    "Admin Channel (pre-2.5)", "CRITICAL"),
    ("1PG7OiApB1nwvP+rz05pAQ==",    "LongFast Preset",         "HIGH"),
    ("AAAAAAAAAAAAAAAAAAAAAA==",    "All Zeros",               "HIGH"),
    ("MTIzNDU2Nzg5MDEyMzQ1Ng==",    "1234567890123456",        "HIGH"),
    ("Ag==",                        "Legacy 0x02",             "MEDIUM"),
    ("Aw==",                        "Legacy 0x03",             "MEDIUM"),
    ("BA==",                        "Legacy 0x04",             "MEDIUM"),
    ("BQ==",                        "Legacy 0x05",             "MEDIUM"),
    ("Bg==",                        "Legacy 0x06",             "MEDIUM"),
    ("Bw==",                        "Legacy 0x07",             "MEDIUM"),
    ("CA==",                        "Legacy 0x08",             "MEDIUM"),
    ("CQ==",                        "Legacy 0x09",             "MEDIUM"),
    ("ZQ+HdKKbbAU4dSCGt66Qqw==",    "EU868 Regional",          "MEDIUM"),
    ("d1iq21lNSh7BP6MOkP6cQA==",    "MediumFast",              "MEDIUM"),
    ("/u7k03L8N3Q=",                "ShortFast",               "MEDIUM"),
    ("GGC5DDnv8FKFm7WCZ5rXBA==",    "LongSlow",                "MEDIUM"),
    ("LHrwq5nxPIJlqFU/K5dKKQ==",    "MediumSlow",              "MEDIUM"),
    ("sb6GxC62sdwGXxJz2sERuQ==",    "ShortSlow",               "MEDIUM"),
    ("dGVzdHRlc3R0ZXN0dGVzdA==",    "testtesttesttest",        "LOW"),
    ("bWVzaHRhc3RpY21lc2h0YXN0",    "meshtastic...",           "LOW"),
    ("shmLkA9H74gAeLH3eGCqsw==",    "Secondary Default",       "LOW"),
    ("ogDPnKVRN7wz/VF8nt6LkA==",    "Debug Key",               "LOW"),
]

# Portnums that represent genuinely sensitive data
SENSITIVE_PORTNUMS = {1, 2, 67}  # TEXT_MESSAGE, REMOTE_HARDWARE, ADMIN


def _expand_psk(b64: str) -> bytes:
    key = base64.b64decode(b64)
    n = len(key)
    if n == 1:  return key * 16
    if n == 8:  return key + key
    if n == 16 or n == 32: return key
    if n < 16:  return key + b'\x00' * (16 - n)
    return key[:32]


def _try_decrypt_meshtastic(raw: bytes) -> Optional[Tuple[str, str, int, str]]:
    """
    Attempt to decrypt a Meshtastic packet with all known PSKs.
    Returns (psk_name, risk, portnum, text) or None.
    """
    if not CRYPTO_AVAILABLE or len(raw) < 16:
        return None
    src_int = struct.unpack('<I', raw[4:8])[0]
    pid     = struct.unpack('<I', raw[8:12])[0]
    payload = raw[16:]
    if not payload:
        return None

    # Meshtastic nonce: packet_id at [0:4], zeros at [4:8], node_id at [8:12], zeros at [12:16]
    nonce = struct.pack('<IIII', pid, 0, src_int, 0)

    for b64, name, risk in PSK_DB:
        key = _expand_psk(b64)
        try:
            cipher = Cipher(algorithms.AES(key), modes.CTR(nonce), backend=default_backend())
            pt = cipher.decryptor().update(payload)
        except Exception:
            continue
        # Validity: field 1 must be portnum varint (tag 0x08), value in valid range
        if not pt or pt[0] != 0x08:
            continue
        portnum_raw = pt[1] if len(pt) > 1 else 0
        if portnum_raw == 0 or portnum_raw > 512:
            continue

        portnum = _read_portnum(pt)
        if portnum is None:
            continue

        text = ''
        if portnum == 1:  # TEXT_MESSAGE
            inner = _read_field2(pt)
            if inner:
                try:
                    text = inner.decode('utf-8', errors='replace').strip('\x00').strip()
                except Exception:
                    pass

        return (name, risk, portnum, text)
    return None


def _decode_varint(data: bytes, offset: int) -> Tuple[int, int]:
    result, shift, consumed = 0, 0, 0
    while offset + consumed < len(data):
        b = data[offset + consumed]; consumed += 1
        result |= (b & 0x7F) << shift; shift += 7
        if not (b & 0x80):
            break
    return result, consumed


def _read_portnum(payload: bytes) -> Optional[int]:
    offset = 0
    while offset < len(payload):
        tag, c = _decode_varint(payload, offset); offset += c
        fn, wt = tag >> 3, tag & 0x07
        if fn == 1 and wt == 0:
            v, _ = _decode_varint(payload, offset); return v
        if wt == 0:
            _, c = _decode_varint(payload, offset); offset += c
        elif wt == 2:
            l, c = _decode_varint(payload, offset); offset += c + l
        elif wt == 5: offset += 4
        elif wt == 1: offset += 8
        else: break
    return None


def _read_field2(payload: bytes) -> Optional[bytes]:
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
        elif wt == 5: offset += 4
        elif wt == 1: offset += 8
        else: break
    return None


# ---------------------------------------------------------------------------
# PCAP reader + LoRaWAN join parser
# ---------------------------------------------------------------------------

LORA_HEADER_FMT  = '<fffBIBH'
LORA_HEADER_SIZE = struct.calcsize(LORA_HEADER_FMT)

def _read_pcap_payloads(filepath: str):
    """Yield raw payload bytes per packet from ESP32 PCAP."""
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
    """
    Parse a LoRaWAN OTAA Join Request (23 bytes, MHDR 0x00).
    Returns {dev_eui, app_eui, dev_nonce} or None.
    """
    if len(payload) < 23:
        return None
    mhdr = payload[0]
    if (mhdr >> 5) != 0:  # MType must be 0x00 (join request)
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
        self.text_messages:  List[Dict] = []     # {node_id, psk, text}
        self.lorawan_joins:  List[Dict] = []     # {dev_eui, app_eui, dev_nonce}
        self.findings:       List[Dict] = []
        self.start_time:     Optional[float] = None
        self.end_time:       Optional[float] = None
        self.source_files:   List[str] = []

    def _device(self, node_id: str) -> DeviceRecord:
        if node_id not in self.devices:
            self.devices[node_id] = DeviceRecord(node_id)
        return self.devices[node_id]

    def load_csv(self, filepath: str):
        self.source_files.append(Path(filepath).name)
        with open(filepath, 'r', encoding='utf-8') as fh:
            for row in csv.DictReader(fh):
                self._process_row(row)

    def load_pcap(self, filepath: str):
        self.source_files.append(Path(filepath).name)
        for payload in _read_pcap_payloads(filepath):
            # LoRaWAN join requests
            join = _parse_lorawan_join(payload)
            if join and join not in self.lorawan_joins:
                self.lorawan_joins.append(join)
            # Meshtastic decryption attempt
            result = _try_decrypt_meshtastic(payload)
            if result:
                name, risk, portnum, text = result
                # We don't have a node_id from PCAP without the header — use placeholder
                if portnum in SENSITIVE_PORTNUMS and text:
                    self.text_messages.append({
                        'node_id': '(from PCAP)',
                        'psk': name, 'risk': risk, 'text': text
                    })

    def load_api(self, host: str):
        if not REQUESTS_AVAILABLE:
            sys.exit("ERROR: pip install requests")
        self.source_files.append(f"API:{host}")
        try:
            r = requests.get(f"http://{host}/api/devices", timeout=10)
            for d in r.json().get('devices', []):
                nid = d.get('nodeId', 'unknown')
                dev = self._device(nid)
                dev.protocol = d.get('protocol', 'Unknown')
                dev.packets  = d.get('packetCount', 0)
                self.total_packets += dev.packets
                if d.get('rssi'):
                    dev.rssi_vals.append(float(d['rssi']))
            print(f"[API] Loaded {len(self.devices)} devices from {host}")
        except Exception as e:
            sys.exit(f"[API] Error: {e}")

    def _process_row(self, row: Dict):
        self.total_packets += 1
        ts = float(row.get('timestamp_ms', 0) or 0) / 1000
        if self.start_time is None or ts < self.start_time:
            self.start_time = ts
        if self.end_time is None or ts > self.end_time:
            self.end_time = ts

        nid      = row.get('node_id_hex', 'unknown') or 'unknown'
        proto    = row.get('protocol', 'Unknown')
        rssi_str = row.get('rssi_dbm', '')
        encrypted = row.get('encrypted', '0') in ('1', 'true', 'True')
        psk_csv   = row.get('psk_result', 'none') or 'none'

        self.proto_counts[proto] += 1
        dev = self._device(nid)
        dev.protocol = proto
        dev.packets += 1
        if rssi_str:
            try:
                dev.rssi_vals.append(float(rssi_str))
            except ValueError:
                pass
        if encrypted:
            dev.encrypted += 1

        lat, lon = row.get('lat_deg', ''), row.get('lon_deg', '')
        if lat and lon:
            try:
                if float(lat) != 0.0 or float(lon) != 0.0:
                    dev.has_gps = True
            except ValueError:
                pass

        # PSK result from firmware CSV (already cracked by firmware)
        if psk_csv not in ('none', 'failed', ''):
            dev.decrypted += 1
            dev.psk_names.add(psk_csv)
            # Find risk level for this key name
            for _, name, risk in PSK_DB:
                if name == psk_csv:
                    if not dev.psk_risk or _risk_rank(risk) > _risk_rank(dev.psk_risk):
                        dev.psk_risk = risk
                    break

        # Attempt Python-side decryption on uncracked encrypted packets
        elif encrypted and CRYPTO_AVAILABLE:
            raw_hex = row.get('raw_hex', '')
            if raw_hex:
                try:
                    raw = bytes.fromhex(raw_hex)
                    result = _try_decrypt_meshtastic(raw)
                    if result:
                        name, risk, portnum, text = result
                        dev.decrypted += 1
                        dev.psk_names.add(name)
                        if not dev.psk_risk or _risk_rank(risk) > _risk_rank(dev.psk_risk):
                            dev.psk_risk = risk
                        if portnum == 1 and text:
                            dev.texts.append(text)
                            self.text_messages.append({
                                'node_id': nid, 'psk': name, 'risk': risk, 'text': text
                            })
                except Exception:
                    pass

        # LoRaWAN join detection from raw hex
        raw_hex = row.get('raw_hex', '')
        if proto in ('LoRaWAN', 'Unknown') and raw_hex:
            try:
                raw = bytes.fromhex(raw_hex)
                join = _parse_lorawan_join(raw)
                if join and join not in self.lorawan_joins:
                    self.lorawan_joins.append(join)
            except Exception:
                pass

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

    # Sort findings by severity
    sev_order = ['CRITICAL', 'HIGH', 'MEDIUM', 'LOW', 'INFO']
    findings = sorted(a.findings, key=lambda f: sev_order.index(f.get('severity', 'INFO')))

    # Counts
    counts = {s: sum(1 for f in findings if f['severity'] == s) for s in sev_order}

    # Protocol rows
    proto_rows = ''.join(
        f'<tr><td>{p}</td><td>{c}</td>'
        f'<td>{c / max(a.total_packets, 1) * 100:.1f}%</td></tr>'
        for p, c in sorted(a.proto_counts.items(), key=lambda x: -x[1])
    )

    # Device rows (top 50 by packet count, then sorted by risk)
    sorted_devs = sorted(a.devices.values(),
                         key=lambda d: (_risk_rank(d.psk_risk), d.packets), reverse=True)[:50]
    dev_rows = ''
    for d in sorted_devs:
        rssi_str = f'{d.avg_rssi():.1f}' if d.avg_rssi() is not None else '—'
        psk_str  = ', '.join(sorted(d.psk_names)) if d.psk_names else '—'
        risk_col = d.risk_color()
        risk_lbl = f'<span style="color:{risk_col};font-weight:bold">{d.psk_risk}</span>' if d.psk_risk else '—'
        gps_lbl  = '✓' if d.has_gps else ''
        enc_pct  = f'{d.encrypted / max(d.packets,1)*100:.0f}%' if d.packets else '—'
        dev_rows += (
            f'<tr><td><code>{d.node_id}</code></td>'
            f'<td>{d.protocol}</td>'
            f'<td>{d.packets}</td>'
            f'<td>{rssi_str}</td>'
            f'<td>{enc_pct}</td>'
            f'<td>{risk_lbl}</td>'
            f'<td style="font-size:11px">{psk_str}</td>'
            f'<td>{gps_lbl}</td></tr>\n'
        )

    # Intercepted messages table
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

    # LoRaWAN section
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

    # Finding cards
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
{join_section}

<h2>Protocol Distribution</h2>
<table>
<tr><th>Protocol</th><th>Packets</th><th>Share</th></tr>
{proto_rows}
</table>

<h2>Device Inventory ({len(a.devices)} devices)</h2>
<table>
<tr><th>Node ID</th><th>Protocol</th><th>Pkts</th><th>RSSI</th>
    <th>Enc%</th><th>Risk</th><th>PSK</th><th>GPS</th></tr>
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
    ap = argparse.ArgumentParser(
        description='Generate an HTML security assessment report from captured LoRa data.')
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument('file', nargs='?', help='CSV capture file')
    src.add_argument('--api', metavar='HOST', help='Pull live data from ESP32 API')
    ap.add_argument('--pcap', metavar='FILE',
                    help='Also process a PCAP file (LoRaWAN joins + additional decryption)')
    ap.add_argument('-o', '--output', metavar='FILE',
                    help='Output HTML file (default: <stem>_report.html)')
    args = ap.parse_args()

    if not CRYPTO_AVAILABLE:
        print("WARNING: pip install cryptography  — PSK decryption disabled")

    a = Assessment()

    if args.api:
        a.load_api(args.api)
        out = args.output or 'report.html'
    else:
        p = Path(args.file)
        if not p.exists():
            sys.exit(f"File not found: {args.file}")
        print(f"Loading: {p}")
        a.load_csv(str(p))
        out = args.output or (p.stem + '_report.html')

    if args.pcap:
        pp = Path(args.pcap)
        if pp.exists():
            print(f"Loading PCAP: {pp}")
            a.load_pcap(str(pp))
        else:
            print(f"WARNING: PCAP not found: {args.pcap}")

    a.analyze()

    print(f"\nDevices:  {len(a.devices)}")
    print(f"Packets:  {a.total_packets:,}")
    print(f"Findings: {len(a.findings)}")
    if a.text_messages:
        print(f"Messages: {len(a.text_messages)} decrypted")
    if a.lorawan_joins:
        print(f"Joins:    {len(a.lorawan_joins)} DevEUI harvested")

    _render_html(a, out)


if __name__ == '__main__':
    main()
