#!/usr/bin/env python3
"""
Generate a decrypt reveal HTML page from a PCAP capture file.

Parses the pcap, decrypts Meshtastic packets, extracts text messages,
and generates a self-contained presentation page.

Usage:
    python tools/demo/make_reveal.py capture.pcap
    python tools/demo/make_reveal.py capture.pcap -o my_reveal.html
    python tools/demo/make_reveal.py capture.pcap --all-ports  # include position, telemetry, etc.

Requirements:
    pip install cryptography
"""

import argparse
import html
import json
import struct
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
from collections import defaultdict
from pathlib import Path

# Add parent dirs so we can import sibling modules
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "meshtastic"))
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from meshtastic_decoder import MeshtasticDecoder, PORT_NAMES


# --- PCAP parsing (matches pcap_analyzer.py pseudo-header format) -----------

LORA_HEADER_SIZE = 20  # 3 floats + u8 + u32 + u8 + u16 = 20 bytes

def parse_pcap(filepath):
    """Parse pcap, strip 20-byte LoRa pseudo-header, return raw packets + metadata."""
    packets = []

    with open(filepath, "rb") as f:
        global_header = f.read(24)
        if len(global_header) < 24:
            raise ValueError("Invalid PCAP file")

        magic = struct.unpack("<I", global_header[:4])[0]
        if magic not in (0xA1B2C3D4, 0xA1B23C4D):
            raise ValueError(f"Invalid PCAP magic: 0x{magic:08x}")

        while True:
            pkt_header = f.read(16)
            if len(pkt_header) < 16:
                break

            ts_sec, ts_usec, incl_len, orig_len = struct.unpack("<IIII", pkt_header)
            pkt_data = f.read(incl_len)
            if len(pkt_data) < incl_len:
                break

            # Extract RSSI from pseudo-header before stripping
            rssi = None
            if len(pkt_data) > LORA_HEADER_SIZE and pkt_data[:4] != b"\xff\xff\xff\xff":
                rssi = struct.unpack("<f", pkt_data[4:8])[0]
                pkt_data = pkt_data[LORA_HEADER_SIZE:]

            packets.append({
                "data": pkt_data,
                "rssi": rssi,
                "timestamp": ts_sec + ts_usec / 1e6,
            })

    return packets


# --- Decode + extract -------------------------------------------------------

def decode_packets(packets, all_ports=False):
    """Decode all packets, return stats and text messages."""
    decoder = MeshtasticDecoder(verbose=False)

    stats = {
        "total": len(packets),
        "meshtastic": 0,
        "decrypted": 0,
        "unencrypted": 0,
        "failed": 0,
        "devices": set(),
        "ports": defaultdict(int),
        "psks": defaultdict(int),
    }

    text_messages = []
    seen_ids = set()  # deduplicate retransmissions

    for pkt in packets:
        result = decoder.decode_packet(pkt["data"])
        if result is None:
            continue

        stats["meshtastic"] += 1
        node = result["from_node"]
        stats["devices"].add(node)

        if result["decrypted"]:
            stats["decrypted"] += 1
            stats["psks"][result["psk_used"]] += 1
        elif result.get("unencrypted"):
            stats["unencrypted"] += 1
        else:
            stats["failed"] += 1

        port_name = result.get("port_name", "UNKNOWN")
        if port_name:
            stats["ports"][port_name] += 1

        # Extract messages
        pkt_id = result.get("packet_id", "")
        portnum = result.get("portnum")

        # Deduplicate: same packet_id from mesh retransmits
        dedup_key = (pkt_id, node)
        if dedup_key in seen_ids:
            continue
        seen_ids.add(dedup_key)

        # Text messages always included
        if result.get("text_message"):
            encrypted_hex = pkt["data"][16:].hex()
            spaced_hex = " ".join(encrypted_hex[i:i+2] for i in range(0, len(encrypted_hex), 2))
            text_messages.append({
                "node": node,
                "text": result["text_message"],
                "hex": spaced_hex,
                "psk": result.get("psk_used", "unknown"),
                "port": "TEXT_MESSAGE_APP",
                "rssi": pkt.get("rssi"),
            })

        # Optionally include other interesting ports
        elif all_ports and result["decrypted"]:
            if result.get("position"):
                pos = result["position"]
                lat = pos.get("latitude", "?")
                lon = pos.get("longitude", "?")
                text = f"GPS: {lat}, {lon}"
                if "altitude" in pos:
                    text += f" ({pos['altitude']}m)"
                encrypted_hex = pkt["data"][16:].hex()
                spaced_hex = " ".join(encrypted_hex[i:i+2] for i in range(0, len(encrypted_hex), 2))
                text_messages.append({
                    "node": node,
                    "text": text,
                    "hex": spaced_hex,
                    "psk": result.get("psk_used", "unknown"),
                    "port": "POSITION_APP",
                    "rssi": pkt.get("rssi"),
                })

    # Compute summary
    total_mesh = stats["meshtastic"]
    if total_mesh > 0:
        stats["decrypt_pct"] = round(
            (stats["decrypted"] + stats["unencrypted"]) / total_mesh * 100, 1
        )
    else:
        stats["decrypt_pct"] = 0.0

    stats["device_count"] = len(stats["devices"])
    stats["position_count"] = stats["ports"].get("POSITION_APP", 0)

    # Find the dominant PSK
    if stats["psks"]:
        top_psk = max(stats["psks"], key=stats["psks"].get)
        stats["top_psk"] = top_psk
    else:
        stats["top_psk"] = "none"

    return stats, text_messages


# --- HTML generation --------------------------------------------------------

HTML_TEMPLATE = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Meshtastic PSK Decrypt // Live</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }

  body {
    background: #0a0a0a;
    color: #00ff41;
    font-family: 'Courier New', monospace;
    min-height: 100vh;
    overflow: hidden;
  }

  .scanline {
    position: fixed; top: 0; left: 0; right: 0; bottom: 0;
    background: repeating-linear-gradient(
      0deg, transparent, transparent 2px, rgba(0,255,65,0.03) 2px, rgba(0,255,65,0.03) 4px
    );
    pointer-events: none; z-index: 100;
  }

  header {
    padding: 30px 60px 20px;
    border-bottom: 1px solid #1a3a1a;
  }

  header h1 {
    font-size: 18px;
    color: #555;
    font-weight: normal;
    letter-spacing: 2px;
  }

  header h1 span { color: #00ff41; }

  .stats {
    position: fixed; top: 30px; right: 60px;
    text-align: right; font-size: 13px; color: #444;
  }
  .stats .val { color: #00ff41; }
  .stats .crit { color: #ff3333; }

  .container {
    padding: 40px 60px;
    max-width: 1200px;
  }

  .message-card {
    margin-bottom: 30px;
    opacity: 0;
    transform: translateY(20px);
    transition: opacity 0.4s, transform 0.4s;
  }

  .message-card.visible {
    opacity: 1;
    transform: translateY(0);
  }

  .meta {
    font-size: 12px;
    color: #444;
    margin-bottom: 6px;
    display: flex;
    gap: 20px;
  }

  .meta .node { color: #666; }
  .meta .psk { color: #ff3333; }
  .meta .port { color: #888; }

  .hex-block {
    background: #0f0f0f;
    border: 1px solid #1a1a1a;
    border-left: 3px solid #333;
    padding: 16px 20px;
    font-size: 14px;
    color: #2a5a2a;
    letter-spacing: 1px;
    line-height: 1.6;
    word-break: break-all;
    position: relative;
    overflow: hidden;
    min-height: 56px;
  }

  .hex-block.cracking {
    border-left-color: #ff3333;
  }

  .hex-block.cracking .hex-text {
    animation: glitch 0.08s infinite;
  }

  .hex-block.decrypted {
    border-left-color: #00ff41;
    background: #0a1a0a;
  }

  .plaintext {
    display: none;
    font-size: 22px;
    color: #00ff41;
    letter-spacing: 0;
    text-shadow: 0 0 10px rgba(0,255,65,0.3);
    padding: 4px 0;
  }

  .hex-block.decrypted .hex-text { display: none; }
  .hex-block.decrypted .plaintext { display: block; }

  .crack-label {
    position: absolute;
    top: 8px; right: 12px;
    font-size: 10px;
    letter-spacing: 2px;
    text-transform: uppercase;
  }

  .hex-block:not(.decrypted) .crack-label { color: #333; }
  .hex-block.cracking .crack-label { color: #ff3333; animation: blink 0.3s infinite; }
  .hex-block.decrypted .crack-label { color: #00ff41; }

  .prompt {
    position: fixed;
    bottom: 30px;
    left: 0; right: 0;
    text-align: center;
    font-size: 13px;
    color: #333;
    letter-spacing: 1px;
  }
  .prompt span { color: #555; }

  .big-reveal {
    display: none;
    position: fixed;
    top: 0; left: 0; right: 0; bottom: 0;
    background: rgba(0,0,0,0.95);
    z-index: 50;
    justify-content: center;
    align-items: center;
    flex-direction: column;
    padding: 60px;
  }

  .big-reveal.active { display: flex; }

  .big-reveal .big-msg {
    font-size: 42px;
    color: #00ff41;
    text-align: center;
    text-shadow: 0 0 30px rgba(0,255,65,0.4);
    max-width: 900px;
    line-height: 1.4;
    opacity: 0;
    transform: scale(0.95);
    transition: opacity 0.6s, transform 0.6s;
  }

  .big-reveal.active .big-msg.show {
    opacity: 1;
    transform: scale(1);
  }

  .big-reveal .big-meta {
    margin-top: 30px;
    font-size: 14px;
    color: #ff3333;
    letter-spacing: 2px;
    opacity: 0;
    transition: opacity 0.6s 0.3s;
  }

  .big-reveal.active .big-meta.show { opacity: 1; }

  .summary {
    display: none;
    position: fixed;
    top: 0; left: 0; right: 0; bottom: 0;
    background: rgba(0,0,0,0.97);
    z-index: 50;
    justify-content: center;
    align-items: center;
    flex-direction: column;
    padding: 60px;
  }

  .summary.active { display: flex; }

  .summary h2 {
    font-size: 28px;
    color: #ff3333;
    margin-bottom: 40px;
    letter-spacing: 3px;
  }

  .summary .stat-row {
    font-size: 20px;
    margin: 8px 0;
    color: #666;
  }

  .summary .stat-row .num {
    color: #00ff41;
    font-size: 28px;
    display: inline-block;
    min-width: 60px;
    text-align: right;
    margin-right: 12px;
  }

  .summary .punchline {
    margin-top: 50px;
    font-size: 18px;
    color: #ff3333;
    text-align: center;
    max-width: 700px;
    line-height: 1.6;
    letter-spacing: 1px;
  }

  @keyframes glitch {
    0%   { transform: translateX(0); }
    25%  { transform: translateX(-2px); }
    50%  { transform: translateX(1px); }
    75%  { transform: translateX(-1px); }
    100% { transform: translateX(0); }
  }

  @keyframes blink {
    0%, 100% { opacity: 1; }
    50% { opacity: 0; }
  }
</style>
</head>
<body>

<div class="scanline"></div>

<header>
  <h1>MESHTASTIC <span>PSK DECRYPT</span> // PASSIVE CAPTURE</h1>
</header>

<div class="stats">
  <div>PACKETS <span class="val">%%TOTAL_PACKETS%%</span></div>
  <div>DEVICES <span class="val">%%DEVICE_COUNT%%</span></div>
  <div>DECRYPTED <span class="crit">%%DECRYPT_PCT%%%</span></div>
  <div>PSK <span class="crit">%%TOP_PSK%%</span></div>
</div>

<div class="container" id="cards"></div>

<div class="big-reveal" id="bigReveal">
  <div class="big-msg" id="bigMsg"></div>
  <div class="big-meta" id="bigMeta"></div>
</div>

<div class="summary" id="summary">
  <h2>CAPTURE SUMMARY</h2>
  <div class="stat-row"><span class="num">%%TOTAL_PACKETS%%</span> packets captured passively</div>
  <div class="stat-row"><span class="num">%%DEVICE_COUNT%%</span> unique devices identified</div>
  <div class="stat-row"><span class="num">%%DECRYPT_PCT%%%</span> of traffic decrypted</div>
  <div class="stat-row"><span class="num">%%MSG_COUNT%%</span> private text messages read</div>
  <div class="stat-row"><span class="num">%%POSITION_COUNT%%</span> GPS positions extracted</div>
  <div class="stat-row"><span class="num">$25</span> hardware cost</div>
  <div class="punchline">
    Every message encrypted with the factory default key.<br>
    "Encrypted" does not mean "secure."
  </div>
</div>

<div class="prompt" id="prompt">
  press <span>SPACE</span> to begin decryption
</div>

<script>
const messages = %%MESSAGES_JSON%%;

let step = -1;

const container = document.getElementById('cards');
messages.forEach((m, i) => {
  const card = document.createElement('div');
  card.className = 'message-card';
  card.id = `card-${i}`;
  card.innerHTML = `
    <div class="meta">
      <span class="node">NODE ${m.node}</span>
      <span class="port">${m.port}</span>
      <span class="psk">PSK: ${m.psk}</span>
    </div>
    <div class="hex-block" id="hex-${i}">
      <span class="hex-text">${m.hex}</span>
      <span class="plaintext">${m.text}</span>
      <span class="crack-label" id="label-${i}">ENCRYPTED</span>
    </div>
  `;
  container.appendChild(card);
});

function randomHex(len) {
  let s = '';
  for (let i = 0; i < len; i++) {
    s += Math.floor(Math.random() * 256).toString(16).padStart(2, '0');
    if (i < len - 1) s += ' ';
  }
  return s;
}

function advance() {
  step++;

  const msgIndex = Math.floor(step / 2);
  const phase = step % 2;

  if (msgIndex >= messages.length) {
    document.getElementById('bigReveal').classList.remove('active');
    document.getElementById('summary').classList.add('active');
    document.getElementById('prompt').style.display = 'none';
    return;
  }

  const card = document.getElementById(`card-${msgIndex}`);
  const hexBlock = document.getElementById(`hex-${msgIndex}`);
  const label = document.getElementById(`label-${msgIndex}`);
  const hexText = hexBlock.querySelector('.hex-text');

  if (phase === 0) {
    card.classList.add('visible');
    hexBlock.classList.add('cracking');
    label.textContent = 'CRACKING...';

    let glitchCount = 0;
    const glitchInterval = setInterval(() => {
      hexText.textContent = randomHex(Math.floor(messages[msgIndex].hex.length / 3));
      glitchCount++;
      if (glitchCount > 12) {
        clearInterval(glitchInterval);
        hexText.textContent = messages[msgIndex].hex;
      }
    }, 60);

    document.getElementById('prompt').innerHTML = 'press <span>SPACE</span> to decrypt';
    document.getElementById('bigReveal').classList.remove('active');

  } else {
    hexBlock.classList.remove('cracking');
    hexBlock.classList.add('decrypted');
    label.textContent = 'DECRYPTED';

    const reveal = document.getElementById('bigReveal');
    const bigMsg = document.getElementById('bigMsg');
    const bigMeta = document.getElementById('bigMeta');

    bigMsg.textContent = messages[msgIndex].text;
    bigMeta.textContent = `NODE ${messages[msgIndex].node} // DECRYPTED WITH DEFAULT PSK`;

    reveal.classList.add('active');
    bigMsg.classList.remove('show');
    bigMeta.classList.remove('show');

    requestAnimationFrame(() => {
      requestAnimationFrame(() => {
        bigMsg.classList.add('show');
        bigMeta.classList.add('show');
      });
    });

    if (msgIndex < messages.length - 1) {
      document.getElementById('prompt').innerHTML = 'press <span>SPACE</span> for next message';
    } else {
      document.getElementById('prompt').innerHTML = 'press <span>SPACE</span> for summary';
    }
  }
}

document.addEventListener('keydown', (e) => {
  if (e.code === 'Space' || e.code === 'ArrowRight') {
    e.preventDefault();
    advance();
  }
  if (e.code === 'ArrowLeft' && step > 0) {
    const target = step - 2;
    step = -1;
    document.querySelectorAll('.message-card').forEach(c => c.classList.remove('visible'));
    document.querySelectorAll('.hex-block').forEach(h => {
      h.classList.remove('cracking', 'decrypted');
      h.querySelector('.hex-text').textContent = messages[parseInt(h.id.split('-')[1])].hex;
    });
    document.querySelectorAll('[id^=label-]').forEach(l => l.textContent = 'ENCRYPTED');
    document.getElementById('bigReveal').classList.remove('active');
    document.getElementById('summary').classList.remove('active');
    document.getElementById('prompt').innerHTML = 'press <span>SPACE</span> to begin decryption';
    for (let i = 0; i <= target; i++) advance();
  }
});

document.addEventListener('click', () => advance());
</script>
</body>
</html>"""


def generate_html(stats, messages, output_path):
    """Fill template with data and write HTML file."""
    # Prepare messages for JSON embedding — escape for safe HTML injection
    js_messages = []
    for m in messages:
        js_messages.append({
            "node": m["node"],
            "text": m["text"],
            "hex": m["hex"][:120],  # trim very long hex for display
            "psk": m["psk"],
            "port": m["port"],
        })

    content = HTML_TEMPLATE
    content = content.replace("%%TOTAL_PACKETS%%", f"{stats['total']:,}")
    content = content.replace("%%DEVICE_COUNT%%", f"{stats['device_count']:,}")
    content = content.replace("%%DECRYPT_PCT%%", f"{stats['decrypt_pct']}")
    content = content.replace("%%TOP_PSK%%", stats["top_psk"])
    content = content.replace("%%MSG_COUNT%%", str(len(messages)))
    content = content.replace("%%POSITION_COUNT%%", f"{stats['position_count']:,}")
    content = content.replace("%%MESSAGES_JSON%%", json.dumps(js_messages, ensure_ascii=False, indent=2))

    Path(output_path).write_text(content, encoding="utf-8")
    return output_path


# --- Main -------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Generate a decrypt reveal presentation from a PCAP file",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python tools/demo/make_reveal.py capture.pcap
  python tools/demo/make_reveal.py capture.pcap -o talk_demo.html
  python tools/demo/make_reveal.py capture.pcap --all-ports
        """,
    )
    parser.add_argument("pcap", help="PCAP file from ESP32 LoRa Sniffer")
    parser.add_argument("-o", "--output", help="Output HTML file (default: decrypt_reveal.html)")
    parser.add_argument(
        "--all-ports",
        action="store_true",
        help="Include position/telemetry decrypts (not just text messages)",
    )
    args = parser.parse_args()

    pcap_path = Path(args.pcap)
    if not pcap_path.exists():
        print(f"Error: {pcap_path} not found")
        sys.exit(1)

    output_path = args.output or "decrypt_reveal.html"

    print(f"Loading {pcap_path} ({pcap_path.stat().st_size:,} bytes)...")
    packets = parse_pcap(pcap_path)
    print(f"Parsed {len(packets):,} packets")

    print("Decoding and decrypting...")
    stats, messages = decode_packets(packets, all_ports=args.all_ports)

    print(f"\nResults:")
    print(f"  Meshtastic packets: {stats['meshtastic']:,}")
    print(f"  Decrypted: {stats['decrypted']:,} ({stats['decrypt_pct']}%)")
    print(f"  Devices: {stats['device_count']}")
    print(f"  Text messages: {len(messages)}")

    if not messages:
        print("\nNo text messages found in capture.")
        if not args.all_ports:
            print("Try --all-ports to include position/telemetry decrypts.")
        sys.exit(0)

    out = generate_html(stats, messages, output_path)
    print(f"\nGenerated: {out}")
    print(f"Open in browser for presentation. SPACE/click to advance.")


if __name__ == "__main__":
    main()
