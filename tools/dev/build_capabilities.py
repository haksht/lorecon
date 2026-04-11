#!/usr/bin/env python3
"""
Build a single capabilities.csv from all foedata logs.

Per-device summary aggregated across every capture session, with
Python-side PSK decryption to enrich findings the firmware missed.

Output: c:/Users/tim/foedata/capabilities.csv
"""

import csv
import glob
import base64
import struct
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
from pathlib import Path
from collections import defaultdict

try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    from cryptography.hazmat.backends import default_backend
    CRYPTO = True
except ImportError:
    CRYPTO = False
    print("WARNING: pip install cryptography — PSK decryption disabled")

# ---------------------------------------------------------------------------
# PSK key database — all 23 Meshtastic defaults
# ---------------------------------------------------------------------------

PSK_DB = [
    # Try private/weak keys first so genuine private-channel traffic isn't
    # swallowed by a public-key false positive.
    # CRITICAL: keys that were supposed to be private
    ("PKdTs51e4EB0BoOevIN0Dw==", "Admin Channel (pre-2.5)", "CRITICAL"),
    # HIGH: user intended a private channel but chose a known-weak key
    ("AAAAAAAAAAAAAAAAAAAAAA==", "All Zeros",               "HIGH"),
    ("MTIzNDU2Nzg5MDEyMzQ1Ng==", "1234567890123456",        "HIGH"),
    # MEDIUM: legacy single-byte keys and named-preset channels
    ("Ag==",                     "Legacy 0x02",             "MEDIUM"),
    ("Aw==",                     "Legacy 0x03",             "MEDIUM"),
    ("BA==",                     "Legacy 0x04",             "MEDIUM"),
    ("BQ==",                     "Legacy 0x05",             "MEDIUM"),
    ("Bg==",                     "Legacy 0x06",             "MEDIUM"),
    ("Bw==",                     "Legacy 0x07",             "MEDIUM"),
    ("CA==",                     "Legacy 0x08",             "MEDIUM"),
    ("CQ==",                     "Legacy 0x09",             "MEDIUM"),
    ("d1iq21lNSh7BP6MOkP6cQA==", "MediumFast",              "MEDIUM"),
    ("/u7k03L8N3Q=",             "ShortFast",               "MEDIUM"),
    ("GGC5DDnv8FKFm7WCZ5rXBA==", "LongSlow",                "MEDIUM"),
    ("LHrwq5nxPIJlqFU/K5dKKQ==", "MediumSlow",              "MEDIUM"),
    ("sb6GxC62sdwGXxJz2sERuQ==", "ShortSlow",               "MEDIUM"),
    ("ZQ+HdKKbbAU4dSCGt66Qqw==", "EU868 Regional",          "MEDIUM"),
    # LOW: test/debug keys
    ("dGVzdHRlc3R0ZXN0dGVzdA==", "testtesttesttest",        "LOW"),
    ("bWVzaHRhc3RpY21lc2h0YXN0", "meshtastic...",           "LOW"),
    ("shmLkA9H74gAeLH3eGCqsw==", "Secondary Default",       "LOW"),
    ("ogDPnKVRN7wz/VF8nt6LkA==", "Debug Key",               "LOW"),
    # PUBLIC: well-known open community channels — no expectation of privacy.
    # Tried last so they don't mask traffic on genuinely private keys.
    ("AQ==",                     "Default (0x01)",          "PUBLIC"),
    ("1PG7OiApB1nwvP+rz05pAQ==", "LongFast Preset",         "PUBLIC"),
]

RISK_RANK = {"CRITICAL": 5, "HIGH": 4, "MEDIUM": 3, "LOW": 2, "PUBLIC": 1, "": 0}


def expand_psk(b64: str) -> bytes:
    k = base64.b64decode(b64)
    n = len(k)
    if n == 1:  return k * 16
    if n == 8:  return k + k
    if n in (16, 32): return k
    return (k + b"\x00" * (16 - n)) if n < 16 else k[:32]


COMPILED_KEYS = [(expand_psk(b64), name, risk) for b64, name, risk in PSK_DB]


def try_decrypt(raw: bytes):
    """
    Try all 23 PSKs. Returns (psk_name, risk, portnum, text) or None.
    text is only populated for portnum=1 (TEXT_MESSAGE).
    """
    if not CRYPTO or len(raw) < 20:
        return None
    src_int = struct.unpack("<I", raw[4:8])[0]
    pid     = struct.unpack("<I", raw[8:12])[0]
    payload = raw[16:]
    if not payload:
        return None
    nonce = struct.pack("<IIII", pid, 0, src_int, 0)
    for key, name, risk in COMPILED_KEYS:
        try:
            cipher = Cipher(algorithms.AES(key), modes.CTR(nonce), backend=default_backend())
            pt = cipher.decryptor().update(payload)
            if not pt or pt[0] != 0x08:
                continue
            portnum_raw = pt[1] if len(pt) > 1 else 0
            if portnum_raw == 0 or portnum_raw > 127:
                continue
            text = ""
            if portnum_raw == 1 and len(pt) > 3 and pt[2] == 0x12:
                tlen = pt[3]
                text = pt[4:4 + tlen].decode("utf-8", errors="replace").strip("\x00").strip()
            return (name, risk, portnum_raw, text)
        except Exception:
            pass
    return None


def parse_lorawan_join(raw: bytes):
    """Return True if this looks like a LoRaWAN OTAA Join Request."""
    return len(raw) >= 23 and (raw[0] >> 5) == 0


# ---------------------------------------------------------------------------
# Per-device accumulator
# ---------------------------------------------------------------------------

def new_device(nid: str) -> dict:
    return {
        "node_id":        nid,
        "packets":        0,
        "protocols":      set(),
        "sessions":       set(),
        "capture_dates":  set(),
        "rssi_vals":      [],
        "encrypted":      0,
        "cleartext":      0,
        "psk_cracked":    0,
        "psk_keys":       set(),
        "worst_psk_risk": "",
        "messages":       0,
        "sample_message": "",
        "lorawan_joins":  0,
        "has_gps":        False,
        "is_router":      False,
        "hop_counts":     [],
    }


def risk_label(d: dict) -> str:
    # Only surface PUBLIC if no higher-severity key was also cracked
    risk = d["worst_psk_risk"]
    if risk and risk != "PUBLIC":
        return risk
    if d["cleartext"] > 0 and d["encrypted"] == 0 and d["packets"] > 2:
        return "MEDIUM"
    if risk == "PUBLIC":
        return "PUBLIC"   # open channel — visible but no privacy expectation violated
    if d["packets"] < 2:
        return "INFO"
    return "NONE"


# ---------------------------------------------------------------------------
# Main processing
# ---------------------------------------------------------------------------

FOEDATA = Path("c:/Users/tim/foedata")
files = sorted(FOEDATA.glob("**/*.csv"))

devices: dict = {}
skipped = 0

for f in files:
    capture_date = f.parent.name
    try:
        with open(f, encoding="utf-8", errors="replace") as fh:
            reader = csv.DictReader(fh)
            for row in reader:
                nid = row.get("node_id_hex", "") or "unknown"
                if nid not in devices:
                    devices[nid] = new_device(nid)
                d = devices[nid]

                d["packets"] += 1
                proto = row.get("protocol", "Unknown")
                d["protocols"].add(proto)
                d["sessions"].add(row.get("session_id", "?"))
                d["capture_dates"].add(capture_date)

                try:
                    d["rssi_vals"].append(float(row.get("rssi_dbm", "") or 0))
                except (ValueError, TypeError):
                    pass

                enc = row.get("encrypted", "0") in ("1", "true", "True")
                if enc:
                    d["encrypted"] += 1
                else:
                    d["cleartext"] += 1

                if row.get("is_router", "0") == "1":
                    d["is_router"] = True

                hc = row.get("hop_count", "")
                if hc and hc not in ("", "-1"):
                    try:
                        d["hop_counts"].append(int(hc))
                    except ValueError:
                        pass

                lat = row.get("lat_deg", "")
                lon = row.get("lon_deg", "")
                if lat and lon:
                    try:
                        if float(lat) != 0.0 or float(lon) != 0.0:
                            d["has_gps"] = True
                    except (ValueError, TypeError):
                        pass

                # LoRaWAN join detection
                if proto in ("LoRaWAN", "Unknown"):
                    raw_hex = row.get("raw_hex", "")
                    if raw_hex:
                        try:
                            if parse_lorawan_join(bytes.fromhex(raw_hex)):
                                d["lorawan_joins"] += 1
                        except ValueError:
                            pass

                # PSK decryption for encrypted Meshtastic
                if enc and proto == "Meshtastic":
                    psk_csv = row.get("psk_result", "none") or "none"
                    if psk_csv not in ("none", "failed", ""):
                        # Firmware already identified the key
                        d["psk_cracked"] += 1
                        d["psk_keys"].add(psk_csv)
                    else:
                        raw_hex = row.get("raw_hex", "")
                        if raw_hex:
                            try:
                                result = try_decrypt(bytes.fromhex(raw_hex))
                                if result:
                                    name, risk, portnum, text = result
                                    d["psk_cracked"] += 1
                                    d["psk_keys"].add(name)
                                    if RISK_RANK.get(risk, 0) > RISK_RANK.get(d["worst_psk_risk"], 0):
                                        d["worst_psk_risk"] = risk
                                    if portnum == 1 and text:
                                        d["messages"] += 1
                                        if not d["sample_message"]:
                                            d["sample_message"] = text[:100]
                            except (ValueError, Exception):
                                pass

    except Exception as e:
        print(f"SKIP {f.name}: {e}", file=sys.stderr)
        skipped += 1

# ---------------------------------------------------------------------------
# Write capabilities CSV
# ---------------------------------------------------------------------------

COLS = [
    "node_id",
    "risk_level",
    "protocols",
    "capture_dates",
    "sessions_count",
    "total_packets",
    "encrypted_pct",
    "cleartext_packets",
    "avg_rssi_dbm",
    "best_rssi_dbm",
    "psk_cracked_packets",
    "psk_keys",
    "messages_intercepted",
    "sample_message",
    "lorawan_joins",
    "has_gps",
    "is_router",
    "avg_hop_count",
]

out_path = FOEDATA / "capabilities.csv"

# Sort: CRITICAL first, then by psk_cracked count, then packets
sorted_devices = sorted(
    devices.values(),
    key=lambda d: (
        -RISK_RANK.get(risk_label(d), 0),
        -d["psk_cracked"],
        -d["packets"],
    ),
)

with open(out_path, "w", newline="", encoding="utf-8") as fh:
    w = csv.DictWriter(fh, fieldnames=COLS)
    w.writeheader()
    for d in sorted_devices:
        rssi = d["rssi_vals"]
        avg_rssi  = round(sum(rssi) / len(rssi), 1) if rssi else ""
        best_rssi = round(max(rssi), 1) if rssi else ""  # max = least negative = strongest
        enc_pct   = round(d["encrypted"] / max(d["packets"], 1) * 100, 1)
        hop_avg   = round(sum(d["hop_counts"]) / len(d["hop_counts"]), 1) if d["hop_counts"] else ""
        w.writerow({
            "node_id":             d["node_id"],
            "risk_level":          risk_label(d),
            "protocols":           "|".join(sorted(d["protocols"])),
            "capture_dates":       "|".join(sorted(d["capture_dates"])),
            "sessions_count":      len(d["sessions"]),
            "total_packets":       d["packets"],
            "encrypted_pct":       enc_pct,
            "cleartext_packets":   d["cleartext"],
            "avg_rssi_dbm":        avg_rssi,
            "best_rssi_dbm":       best_rssi,
            "psk_cracked_packets": d["psk_cracked"],
            "psk_keys":            "|".join(sorted(d["psk_keys"])),
            "messages_intercepted":d["messages"],
            "sample_message":      d["sample_message"],
            "lorawan_joins":       d["lorawan_joins"],
            "has_gps":             "yes" if d["has_gps"] else "no",
            "is_router":           "yes" if d["is_router"] else "no",
            "avg_hop_count":       hop_avg,
        })

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

total_pkts    = sum(d["packets"]  for d in devices.values())
cracked_devs  = sum(1 for d in devices.values() if d["psk_cracked"] > 0)
total_msgs    = sum(d["messages"] for d in devices.values())
total_joins   = sum(d["lorawan_joins"] for d in devices.values())
gps_devs      = sum(1 for d in devices.values() if d["has_gps"])
router_devs   = sum(1 for d in devices.values() if d["is_router"])
by_risk = {r: sum(1 for d in devices.values() if risk_label(d) == r)
           for r in ("CRITICAL", "HIGH", "MEDIUM", "LOW", "PUBLIC", "NONE", "INFO")}

print(f"\nOutput: {out_path}")
print(f"  Source files:          {len(files):>5}  ({skipped} skipped)")
print(f"  Unique nodes:          {len(devices):>5}")
print(f"  Total packets:         {total_pkts:>5,}")
print()
print(f"  PSK-cracked nodes:     {cracked_devs:>5}")
print(f"  Text messages:         {total_msgs:>5}")
print(f"  LoRaWAN join requests: {total_joins:>5}")
print(f"  Nodes with GPS:        {gps_devs:>5}")
print(f"  Router nodes:          {router_devs:>5}")
print()
print("  Risk distribution:")
for r in ("CRITICAL", "HIGH", "MEDIUM", "LOW", "PUBLIC", "NONE", "INFO"):
    note = "  (open channel, no privacy expectation)" if r == "PUBLIC" else ""
    print(f"    {r:<10} {by_risk[r]:>5}{note}")
