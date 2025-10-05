# ESP32 LoRa Sniffer - Tools

## Live Visualizer

Real-time visualization tool for LoRa reconnaissance data from the ESP32 sniffer.

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
