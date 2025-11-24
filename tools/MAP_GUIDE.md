# Interactive Map Features

## What You Get

When GPS data is captured, the tools automatically generate interactive HTML maps using OpenStreetMap.

### Map Features

**Device Markers:**
- 📍 Each discovered device gets a marker at its GPS location
- 🎨 Color-coded by protocol:
  - 🔵 Blue = Meshtastic
  - 🟠 Orange = LoRaWAN
  - 🟢 Green = Helium
  - ⚪ Gray = Unknown

**Interactive Popups:**
Click any marker to see:
- Device ID (node ID)
- Protocol type
- Number of packets captured
- Average RSSI (signal strength)
- Exact coordinates (latitude/longitude)
- Altitude (if available)

**Movement Trails:**
- If a device moved during capture, a colored line shows its path
- Trail color matches the device's protocol color
- Useful for tracking mobile devices or understanding mesh topology

**Legend:**
- Fixed legend in top-right corner
- Shows protocol colors
- Displays session statistics:
  - Total devices
  - Total position fixes

### How to Generate

**Live Capture (automatic):**
```bash
python enhanced_live_visualizer.py COM3 --record

# On exit (Ctrl+C), automatically generates:
# - screenshots/lora_map.html
```

**Post-Capture (manual):**
```bash
python session_analyzer.py logs/capture.csv --export-map

# Generates:
# - lora_map.html (or custom filename with --map-output)
```

### Use Cases

**Conference Presentations:**
1. Run live demo with `--record` flag
2. On exit, HTML map is generated
3. Embed map in PowerPoint/Google Slides:
   - PowerPoint: Insert → Object → Browse → Select HTML file
   - Google Slides: Insert → Video → Upload map (convert to MP4 first)
   - Or: Take screenshot and use static image

**Security Reports:**
1. Capture session data in field
2. Generate map with `session_analyzer.py --export-map`
3. Include in report:
   - Professional geographic visualization
   - Shows device locations and movements
   - Interactive - clients can explore

**Team Collaboration:**
1. Share HTML file via email or cloud storage
2. No server needed - self-contained HTML file
3. Team members can:
   - Click markers to see details
   - Zoom in/out
   - Pan around map
   - Export to image if needed

**Publications:**
1. Generate map from captured data
2. Take screenshot at desired zoom level
3. Include in research paper
4. Or: Host HTML file and provide link

### Example Workflow

**Field Capture:**
```bash
# 2-hour capture at conference
python enhanced_live_visualizer.py COM3 --audio --record --duration 7200

# Automatically creates:
# - screenshots/milestone_10pkts_*.png
# - screenshots/milestone_50pkts_*.png
# - screenshots/milestone_100pkts_*.png
# - screenshots/milestone_500pkts_*.png
# - screenshots/final_*.png
# - screenshots/lora_map.html ← Interactive map
```

**Presentation:**
1. Open `lora_map.html` in browser
2. Use browser's full-screen mode (F11)
3. Demo interactivity:
   - Click markers: "Here's device 0x4FA2E..."
   - Show trail: "This device moved 2 km during capture"
   - Zoom in: "Multiple devices in this building"
4. Screenshot for slides if needed

### Map Quality

**Zoom Levels:**
- City view: Zoom 13 (default)
- Neighborhood: Zoom 15
- Street: Zoom 17
- Building: Zoom 19

**Coordinate Accuracy:**
- GPS precision: ±5-10 meters (typical Meshtastic)
- Displayed to 6 decimal places (~0.1 meter precision)

**Basemap:**
- OpenStreetMap (free, no API key needed)
- Shows roads, buildings, landmarks
- Works offline once cached
- Alternative tiles available (edit code):
  - `CartoDB Positron` (minimal, clean)
  - `CartoDB Dark_Matter` (dark theme)
  - `Stamen Terrain` (topographic)

### Tips

**Better Screenshots:**
- Use browser's print-to-PDF (Ctrl+P)
- Adjust zoom before printing
- Hide legend if needed (edit HTML)

**Embed in PowerPoint:**
1. Save map as `map.html`
2. In PowerPoint: Developer → More Controls → Microsoft Web Browser
3. Navigate to file
4. OR: Use screenshots for simpler embedding

**Share Securely:**
- Map contains GPS coordinates - be aware of privacy
- Consider blurring exact coordinates in public presentations
- Option: Manually edit HTML to remove sensitive details

**Customize:**
The map is a single HTML file - you can:
- Edit legend text (search for "ESP32 LoRa")
- Change colors (search for color hex codes)
- Adjust zoom level (search for "zoom_start")
- Change basemap (search for "tiles=")

### Troubleshooting

**"No GPS data to export":**
- Ensure devices have GPS enabled
- Wait longer (position broadcasts every 30-900 seconds)
- Check serial output for `[GEO]` messages

**Map doesn't open in browser:**
- Manually open the HTML file
- File location printed in terminal: `[🗺️] Interactive map saved: ...`

**Markers not showing:**
- Check browser console (F12) for errors
- Ensure folium installed: `pip install folium`
- Try different browser (Chrome, Firefox, Edge)

**Performance with many devices:**
- Map can handle 100+ devices smoothly
- For 1000+ devices, consider filtering in code
- Or: Create separate maps for different time periods

---

**Example Maps Available:**
See `examples/maps/` directory for sample outputs (if you create this directory and add examples later)
