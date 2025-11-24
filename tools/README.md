# ESP32 LoRa Sniffer - Analysis Tools

Professional-grade visualization and analysis tools for ESP32 LoRa reconnaissance data.

## 🎯 Quick Start for Conference Demos

**Best demo setup (5-panel dashboard with audio):**
```bash
# Install dependencies
pip install -r requirements.txt

# One-command demo launcher (auto-detects ESP32)
python demo.py --auto-detect --audio --record

# Or specify port manually
python demo.py COM3 --audio --record --web

# List available ports
python demo.py --list-ports
```

### Demo Launcher Features

The `demo.py` script provides one-command setup:
- 🔍 Auto-detects ESP32 port
- 📊 Launches enhanced visualizer  
- 🌐 Opens web UI in browser (optional)
- 🔊 Enables audio feedback (optional)
- 📸 Records screenshots at milestones (optional)
- 🗺️ **Exports interactive HTML map on exit** (with GPS data)
- ⏱️ Auto-exits after duration for scripted demos

**Example: 5-minute recorded demo with all features**
```bash
python demo.py COM3 --web --audio --record --duration 300

# Outputs:
# - screenshots/milestone_*.png
# - screenshots/lora_map.html (interactive map)
```

---

## Enhanced Live Visualizer ⭐ NEW

**Conference demo edition** with 5-panel dashboard and audio feedback.

### Features

- **5-Panel Dashboard**:
  1. 📡 **Signal Strength** - RSSI over time per device
  2. 🎯 **Device List** - Top active devices with stats
  3. 📊 **Protocol Pie Chart** - Visual protocol distribution
  4. 📦 **Packet Histogram** - Capture rates by device
  5. 🗺️ **GPS Map** - Geographic positions and movement trails
  
- **Audio Feedback** - "Geiger counter" effect with protocol-specific tones:
  - Meshtastic: 800 Hz
  - LoRaWAN: 600 Hz
  - Helium: 500 Hz
  
- **Auto-Screenshot** - Saves images at milestones (10, 50, 100, 500 packets)

- **Interactive Map Export** ⭐ NEW
  - Automatically exports `lora_map.html` on exit (when GPS data available)
  - OpenStreetMap basemap with device markers
  - Color-coded by protocol (Meshtastic=Blue, LoRaWAN=Orange, Helium=Green)
  - Popup details: Node ID, protocol, packet count, RSSI, coordinates
  - Movement trails for devices that changed position
  - Auto-opens in default browser

- **Protocol Colors** - Matches web UI color scheme:
  - Meshtastic: Blue
  - LoRaWAN: Orange
  - Helium: Green

### Usage

```bash
# Full demo mode
python enhanced_live_visualizer.py COM3 --audio --record

# JSON mode (if firmware supports structured output)
python enhanced_live_visualizer.py COM3 --json --audio

# Quiet mode (no audio)
python enhanced_live_visualizer.py COM3 --record
```

**Outputs:**
- Live matplotlib dashboard (5 panels)
- `screenshots/milestone_*.png` - Auto-captured screenshots
- `screenshots/lora_map.html` - Interactive map (on exit, if GPS data)

**Interactive Map Features:**
- 🗺️ OpenStreetMap basemap
- 📍 Device markers with popups (node ID, stats, coordinates)
- 🎨 Color-coded by protocol
- 🛤️ Movement trails for devices
- 🌐 Auto-opens in browser

### Requirements

```bash
pip install pyserial matplotlib folium sounddevice numpy
```

**Optional:** 
- `sounddevice` and `numpy` for audio feedback (gracefully degrades if missing)
- `folium` for interactive map export (gracefully degrades if missing)

---

## Live Visualizer (Classic)

Original real-time visualization tool for LoRa reconnaissance data from the ESP32 sniffer.

### Features

- **Real-time RSSI graphing** - Track signal strength of all discovered devices over time
- **Device statistics** - Live display of packet counts, device types, and last-seen timestamps
- **Packet histogram** - Visual comparison of packet capture rates across devices
- **Summary dashboard** - Overall reconnaissance statistics and top device information

### Installation

```bash
# Install Python dependencies
pip install -r requirements.txt

# Or install individually
pip install pyserial matplotlib
```

### Usage

#### Windows
```bash
# Auto-detect serial port
python live_visualizer.py

# Specify COM port
python live_visualizer.py COM3
```

#### Linux/Mac
```bash
# Specify serial port
python live_visualizer.py /dev/ttyUSB0
```

### Workflow

1. **Flash and connect ESP32**
   ```bash
   pio run --target upload
   ```

2. **Start live visualizer** (in separate terminal)
   ```bash
   python tools/live_visualizer.py COM3
   ```

3. **ESP32 begins reconnaissance automatically**
   - Visualizer parses serial output
   - Graphs update in real-time
   - All 4 panels refresh every 200ms

### Display Panels

```
┌─────────────────────────────────┬─────────────────────────────────┐
│ 📡 Signal Strength Over Time   │ 🎯 Discovered Devices           │
│                                 │                                 │
│ Line graph of RSSI vs time     │ List with node IDs, types,      │
│ One line per device             │ RSSI bars, packet counts        │
│                                 │                                 │
├─────────────────────────────────┼─────────────────────────────────┤
│ 📊 Packet Capture Statistics   │ 📈 Reconnaissance Summary       │
│                                 │                                 │
│ Bar chart of packets per device │ Runtime, total packets,         │
│ Color-coded by device           │ packet rate, top device info    │
│                                 │                                 │
└─────────────────────────────────┴─────────────────────────────────┘
```

### Conference Demo Usage

Perfect for projecting visualizations during live security talks:

1. Connect ESP32 to laptop
2. Run visualizer on laptop
3. Project laptop screen
4. Perform live reconnaissance
5. Audience sees real-time device discovery

### Troubleshooting

**No serial port found:**
```bash
python live_visualizer.py
# Lists available ports, choose one manually
```

**Permission denied (Linux):**
```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

**Matplotlib not displaying:**
```bash
# Install backend
pip install PyQt5
```

### Advanced Configuration

Edit `live_visualizer.py` to customize:

```python
SERIAL_BAUDRATE = 115200      # Match ESP32 baud rate
UPDATE_INTERVAL = 200         # ms between graph updates
MAX_HISTORY_POINTS = 200      # Data points to keep in memory
WINDOW_SIZE = (14, 8)         # Figure size in inches
```

### Data Format

Visualizer parses these serial output patterns:

```
[PACKET] Meshtastic, 14 bytes, -45.0 dBm, 8.2 dB SNR
[RECON] Packet #42: Meshtastic, 0x401ACD4E, -68.5 dBm
[NODE] New: 0x401ACD4E (Meshtastic)
```

### Export Data

Currently displays only. Future enhancements:
- CSV export button
- Screenshot capture
- Session recording/playback

---

**Perfect companion tool for ESP32 LoRa security research and conference demonstrations.**

## Session Analyzer (SD Log Dashboard)

Offline dashboard for SD-card CSV captures produced by the firmware's packet logger.

### Highlights

- **Portable story** – replay any field capture without needing live RF traffic.
- **2×2 dashboard** – timeline, top devices, frequency usage, and GPS scatter in one view.
- **Analyzer-friendly CSV** – consumes the enhanced schema written by `PacketLogger`.
- **Interactive map export** ⭐ NEW – Generate HTML map with `--export-map`

### Usage

```bash
# Standard analysis
python session_analyzer.py logs/snf_123456.csv --bin-seconds 60 --top-n 8

# With interactive map export
python session_analyzer.py logs/snf_123456.csv --export-map

# Custom map filename
python session_analyzer.py logs/snf_123456.csv --export-map --map-output mymap.html
```

### Interactive Map Features

When using `--export-map`:
- 🗺️ OpenStreetMap basemap
- 📍 Device markers with detailed popups
- 🎨 Color-coded by protocol (Meshtastic/LoRaWAN/Helium)
- 🛤️ Movement trails showing device paths
- 📊 Legend with session stats
- 🌐 Auto-opens in default browser

Perfect for:
- **Conference presentations** - Embed interactive map in slides
- **Security reports** - Professional geographic visualization
- **Team collaboration** - Share HTML file (self-contained, no server needed)

### Workflow

1. Insert SD card after a field session and copy the generated `snf_*.csv` file.
2. Run `session_analyzer.py` with the CSV path (adjust `--bin-seconds` for different timelines).
3. Add `--export-map` to generate interactive HTML map.
4. Project the matplotlib window or save a screenshot for slides.
5. Open the generated HTML map in browser or embed in presentation.

> Tip: pair the analyzer with live captures for a two-part conference demo—live RF when available, SD replay when the spectrum is quiet.
