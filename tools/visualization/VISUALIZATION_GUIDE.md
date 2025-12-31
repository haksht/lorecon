# Visualization Tools - Feature Comparison

## Tool Matrix

| Feature | enhanced_live_visualizer.py | session_analyzer.py | api_client.py | ws_monitor.py | pcap_analyzer.py |
|---------|----------------------------|-------------------|---------------|---------------|------------------|
| **Purpose** | Real-time 5-panel | Offline CSV analysis | REST API CLI | WebSocket monitor | PCAP analysis |
| **GUI** | matplotlib | matplotlib | Terminal | Terminal | Terminal |
| **GPS Mapping** | ✅ Live | ✅ Scatter plot | - | - | - |
| **Audio Feedback** | ✅ Geiger counter | ❌ | ❌ | ❌ | ❌ |
| **Protocol Colors** | ✅ Web UI matching | ⚠️ Generic | - | ✅ ANSI colors | - |
| **Auto-Screenshot** | ✅ Milestones | Manual save | - | - | - |
| **Demo Mode** | ✅ `--demo` | N/A | N/A | N/A | N/A |
| **Data Source** | Serial/WebSocket | CSV file | REST API | WebSocket | PCAP file |
| **Requires ESP32** | ✅ (or `--demo`) | ❌ | ✅ (WiFi) | ✅ (WiFi) | ❌ |
| **Best For** | Live demo (full) | Post-capture analysis | Scripting/automation | SSH monitoring | Wireshark-style |

## Recommendation by Use Case

### 🎤 **Conference Presentation**
**Use:** `python enhanced_live_visualizer.py --auto-detect --web --audio`

**Why:**
- 5-panel dashboard shows all data
- Auto-screenshots for slides
- Audio keeps audience engaged
- Works with `--demo` for no-hardware demos

### 🔬 **Security Research Session**
**Use:** `enhanced_live_visualizer.py COM3 --json --record`

**Why:**
- 5-panel dashboard shows all data
- GPS map for geographic intelligence
- Screenshots document findings
- JSON mode for machine parsing

### 💻 **SSH/Headless Monitoring**
**Use:** `ws_monitor.py --filter meshtastic`

**Why:**
- No GUI dependencies
- Works over SSH
- Color-coded terminal output
- JSON output for piping

### 🤖 **Scripting & Automation**
**Use:** `api_client.py`

**Why:**
- Full REST API access from CLI
- Download PCAP/GeoJSON/KML
- Control capture remotely
- Easy to script with bash/Python

### 📊 **Quick Field Check**
**Use:** `ws_monitor.py` or `api_client.py status`

**Why:**
- Lightweight (no matplotlib)
- Fast startup
- Good enough for "is it working?" checks

### 📈 **Post-Capture Analysis**
**Use:** `session_analyzer.py logs/capture.csv --bin-seconds 60`

**Why:**
- Works offline from SD card data
- Timeline analysis
- Frequency distribution
- GPS scatter plot
- Reproducible for reports

### 📦 **PCAP Deep Dive**
**Use:** `pcap_analyzer.py capture.pcap`

**Why:**
- Native PCAP parsing
- Extracts LoRa metadata (RSSI, SNR, freq)
- Export to CSV for other tools
- Open in Wireshark

## Dependency Comparison

### Real-Time Visualization (enhanced_live_visualizer.py)
```
pyserial
matplotlib
sounddevice  # Optional for audio
numpy        # Optional for audio
```

### Post-Processing (session_analyzer.py)
```
pandas
matplotlib
```

### CLI Tools (api_client.py, ws_monitor.py)
```
requests
websocket-client
```

### PCAP Analysis (pcap_analyzer.py)
```
# No external dependencies - uses native struct/dpkt parsing
```

### All-in-One Install
```bash
pip install -r requirements.txt  # Installs all dependencies
```

## Demo Workflow Examples

### Example 1: CackalackyCon Talk (30 minutes)

**Prep:**
```bash
# Test setup (day before)
python enhanced_live_visualizer.py --demo  # No hardware needed

# With hardware:
python enhanced_live_visualizer.py --auto-detect --audio
```

**Live Demo:**
```bash
# Start 5 minutes before talk
python enhanced_live_visualizer.py --auto-detect --web --audio

# As devices are discovered:
# - Point out audio feedback
# - Show protocol pie chart
# - Highlight GPS map
# - Explain signal strength
```

**Post-Talk:**
```bash
# Show captured screenshots in slides
# Export session data for attendees
```

### Example 2: Security Assessment (Field Work)

**Field Capture:**
```bash
# Bring USB power bank + laptop
python enhanced_live_visualizer.py COM3 --record --json

# Let run for 2-4 hours
# Screenshots auto-saved at milestones
```

**Office Analysis:**
```bash
# SD card → laptop
python session_analyzer.py captures/field_20250124.csv

# Generate timeline
# Export GPS to KML for Google Earth
```

### Example 3: Workshop Demo (Teaching)

**Simple Start:**
```bash
# First, show terminal-based monitor
python ws_monitor.py --filter meshtastic
```

**Then Upgrade:**
```bash
# Show full-featured visualizer
python enhanced_live_visualizer.py COM3 --audio
```

**Finally, Replay:**
```bash
# Use captured CSV for reproducible demo
python session_analyzer.py workshop_capture.csv
```

## Tips for Impressive Demos

### Visual Impact
1. **Large screen** - Visualizer looks best on projector/big monitor
2. **Dark background** - Use dark-themed matplotlib (already default)
3. **Protocol colors** - Blue/Orange/Green instantly recognizable
4. **Auto-screenshots** - Build slide deck while demoing

### Audio Impact
1. **Volume up** - Geiger counter effect grabs attention
2. **Explain tones** - "Each protocol has different frequency"
3. **Activity correlation** - Point out burst patterns

### Narrative Flow
1. **Start quiet** - Show empty dashboard
2. **Discovery moment** - First packet audio + visual pop
3. **Build momentum** - Show devices accumulating
4. **GPS reveal** - "And we know where they are..."
5. **Protocol analysis** - Pie chart shows network composition

### Backup Plans
1. **Pre-recorded session** - Use `session_analyzer.py` if live RF fails
2. **Web UI fallback** - If visualizer crashes, show web interface
3. **Screenshot deck** - Have slides from previous captures
4. **Test device** - Bring own Meshtastic node for guaranteed traffic

## Common Issues & Solutions

### "No audio output"
**Solution 1:** Check system volume
**Solution 2:** Install sounddevice: `pip install sounddevice numpy`
**Solution 3:** Run without `--audio` flag

### "Port access denied" (Linux)
**Solution:**
```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

### "Matplotlib window frozen"
**Solution:** Use `--json` mode for faster serial parsing

### "No GPS data showing"
**Solution:** 
- Ensure devices have GPS enabled
- Wait longer (position broadcasts every 30-900 seconds)
- Check `[GEO]` messages in serial output

### "Screenshots not saving"
**Solution:**
- Check write permissions on `screenshots/` directory
- Verify `--record` flag is set
- Watch terminal for `[📸]` messages

## Performance Tuning

### For Low-End Laptops
- Use `ws_monitor.py` (terminal-only, no GUI)
- Disable audio (`--audio` off) in visualizer
- Disable recording (`--record` off)
- Use `api_client.py status` for quick checks

### For High-Traffic Scenarios
- Enable `--json` mode for faster parsing
- Reduce `MAX_HISTORY_POINTS` (300 → 100)
- Clear old screenshots regularly

### For Publication-Quality Figures
- Run `session_analyzer.py` offline (cleaner rendering)
- Increase DPI in code: `plt.savefig(..., dpi=300)`
- Export to vector format: `.pdf` or `.svg`

## Future Enhancements (TODO)

### Planned Features
- [ ] **Real map tiles** - Use folium/plotly for proper basemap
- [ ] **Export to video** - Record matplotlib animation as MP4
- [ ] **Session replay** - Playback CSV at original speed
- [ ] **Network graph** - Show mesh topology (node connections)
- [ ] **Spectrogram** - Waterfall display of frequency activity
- [ ] **AI summary** - GPT-based analysis of captured traffic

### Community Contributions Welcome
- Multi-ESP32 aggregation (combine multiple sniffers)
- Integration with TheThingsNetwork APIs
- Helium blockchain explorer integration
- Real-time alerting (webhook on interesting packets)

---

**Questions? Open an issue or PR on GitHub!**
