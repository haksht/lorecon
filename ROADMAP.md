# ESP32 LoRa Sniffer - Development Roadmap

## Phase 1: Core Architecture ✅ (COMPLETE - Nov 12, 2025)
**Status:** Complete - All refactoring phases finished

### Completed:
- ✅ Architecture refactoring (988 → 662 lines, 33% reduction)
- ✅ RadioController extraction (hardware abstraction)
- ✅ PacketProcessor extraction (queue + analysis)
- ✅ IReconTool interface (dependency inversion)
- ✅ CommandHandler using dispatch table
- ✅ Hardware validation on ESP32-S3 + SX1262
- ✅ Bug fixes (node tracking, serial input)
- ✅ Security assessment per-device breakdown
- ✅ Context-aware UI commands
- ✅ GPS parsing (wire type 0 and 5 support) - Phase 4 complete
- ✅ Documentation updated - all docs reflect v2.0
- ✅ Debug output cleaned - production ready
- ✅ Dead code removed (~290 lines)
- ✅ Merged to refactor/architecture-simplification branch

**Key Commits:**
- `b640c7f` - RadioController class
- `da1b615` - PacketProcessor class
- `1aac7f0` - IReconTool interface
- `194c343` - Duplicate code removal
- `0806ab6` - GPS parsing, UI enhancements, debug cleanup

---

## Phase 2: Web Server & REST API 🎯 (Next)
**Goal:** Enable wireless operation and remote control

### ESP32 Backend:
```cpp
// REST API endpoints
GET  /api/positions        // JSON array of GPS points
GET  /api/devices          // Targetable devices list
GET  /api/packets          // Recent packet statistics
GET  /api/activity         // RF activity per frequency
GET  /api/security         // Vulnerability assessment
POST /api/capture/start    // Start targeted capture
POST /api/capture/stop     // Stop capture
GET  /api/export/geojson   // GeoJSON download
GET  /api/export/kml       // KML download
```

### Implementation:
- **Libraries:**
  - `ESPAsyncWebServer` - Non-blocking HTTP server
  - `AsyncTCP` - Async networking
  - `ArduinoJson` - JSON serialization (already using)
  - `WebSocket` - Real-time packet feed

- **Features:**
  - WiFi AP mode (ESP32 creates network)
  - WiFi Station mode (join existing network)
  - WebSocket for real-time updates
  - CORS support for development
  - Basic authentication (optional)

### Benefits:
- No serial cable required
- Multiple simultaneous clients
- Mobile operation ready
- Scriptable via curl/Python

**Estimated effort:** 2-3 days

---

## Phase 3: Progressive Web App (PWA) 📱 (Future)
**Goal:** Rich web interface for analysis and visualization

### Technology Stack:
**Frontend:**
- Vanilla JavaScript (no framework required)
- Modern Browser APIs:
  - `IndexedDB` - Local storage (100MB+)
  - `Web Workers` - Background processing
  - `Service Workers` - Offline support
  - `WebSocket API` - Real-time updates

**Visualization:**
- `Leaflet.js` - Interactive mapping
- `Chart.js` or `Apache ECharts` - Data visualization
- `D3.js` - Advanced graphs (optional)

**Storage:**
- `idb` library - IndexedDB wrapper
- Persistent packet storage
- Historical analysis

### Features:

#### 1. Interactive Dashboard
```
┌────────────────────────────────────┐
│  [Devices: 4] [Packets: 127] [...] │
├────────────────────────────────────┤
│  ┌──────────┐  ┌──────────────┐   │
│  │ Device   │  │  Map View    │   │
│  │ List     │  │  (Leaflet)   │   │
│  │          │  │              │   │
│  └──────────┘  └──────────────┘   │
├────────────────────────────────────┤
│  📊 Traffic Patterns               │
│  [Chart showing activity over time]│
└────────────────────────────────────┘
```

#### 2. Real-time Packet Feed
- WebSocket connection to ESP32
- Live updates as packets arrive
- Filtering by device/protocol/RSSI

#### 3. Geographic Intelligence
- Plot devices on map
- Movement tracking
- Coverage heatmaps
- Export KML/GeoJSON

#### 4. Security Dashboard
- Per-device vulnerability scores
- Recommendations
- Attack surface analysis
- Encryption status

#### 5. Traffic Analysis
```javascript
// Analyze patterns in vanilla JS
function analyzeTrafficPattern(packets) {
  const intervals = packets.map((p, i) => 
    i > 0 ? p.timestamp - packets[i-1].timestamp : 0
  );
  return {
    avgInterval: mean(intervals),
    stdDev: standardDeviation(intervals),
    pattern: detectPattern(intervals) // periodic, random, burst
  };
}

// Device fingerprinting
function fingerprintDevice(packets) {
  return {
    rssiProfile: packets.map(p => p.rssi),
    txIntervals: calculateIntervals(packets),
    powerClass: estimatePowerClass(packets),
    routerBehavior: detectRoutingPattern(packets)
  };
}
```

#### 6. Historical Analysis
- Store packets in IndexedDB
- Cross-session correlation
- Device tracking over time
- Pattern detection

#### 7. Offline Support
- Service Worker caching
- Work without internet
- Sync when connection restored

### PWA Installation:
- Add to Home Screen (iOS/Android)
- Looks like native app
- No App Store approval needed
- Instant deployment

**Estimated effort:** 1-2 weeks

---

## Phase 4: Advanced Analysis 🔬 (Long-term)

### 1. Enhanced Pattern Detection
- Machine learning for device classification
- Anomaly detection in traffic
- Behavioral profiling
- Predictive analytics

### 2. Network Topology Mapping
- Mesh network graph visualization
- Router identification
- Message routing analysis
- Network bottleneck detection

### 3. Geospatial Intelligence
- Movement prediction
- Route analysis
- Coverage optimization
- Heatmap generation

### 4. Cross-Device Correlation
- Multi-device tracking
- Network relationship mapping
- Communication pattern analysis
- Community detection

### 5. Python Tools Integration
```python
# Enhanced tools/live_visualizer.py
- WebSocket client to ESP32
- Real-time map updates
- Advanced visualization
- Export to formats (PCAP, CSV, etc.)
```

---

## Phase 5: Hardware Enhancements 🔧 (Optional)

### SD Card Integration
- Packet logging to SD
- Offline operation
- Large capture sessions
- Data export via file

### GPS Module
- Add external GPS (ESP32 doesn't have it)
- Tag sniffer location with packets
- Mobile recon tracking
- Coverage mapping

### Battery Optimization
- Deep sleep between scans
- Low-power modes
- Solar charging support
- Battery monitoring

### Display Upgrade
- Larger OLED
- Touch screen (optional)
- More detailed stats
- Standalone operation

---

## Architecture Evolution

### Current (Phase 1):
```
Serial Terminal ←→ ESP32 (All processing)
```

### Phase 2-3:
```
┌─────────────┐  WiFi/WebSocket  ┌──────────────┐
│   ESP32     │ ←──────────────→ │ PWA (Phone)  │
│ (Capture +  │   REST API       │ (Display +   │
│  Analysis)  │                  │  Storage)    │
└─────────────┘                  └──────────────┘
                                       │
                                  IndexedDB
```

### Future (Phase 4):
```
┌─────────────┐                  ┌──────────────┐
│   ESP32     │                  │ PWA (Phone)  │
│ (Capture +  │ ←──────────────→ │ (Analysis +  │
│  Basic      │   WebSocket      │  ML +        │
│  Filtering) │                  │  Storage)    │
└─────────────┘                  └──────────────┘
                                       │
                                 Heavy Processing
```

---

## Technology Decisions

### Why PWA over Native App?
1. **Single codebase** - iOS, Android, Desktop
2. **No app store** - Instant deployment
3. **Easier development** - Web technologies
4. **Sufficient capabilities** - Modern browser APIs handle our needs
5. **Offline support** - Service Workers

### Why Vanilla JS?
1. **No build step** - Deploy directly
2. **Smaller footprint** - Faster loading
3. **No framework lock-in** - Future flexibility
4. **Educational value** - Understand fundamentals
5. **Modern APIs sufficient** - Don't need React/Vue for this scale

### Can upgrade later if needed:
- Add Alpine.js (lightweight reactivity)
- Add Petite-Vue (Vue subset)
- Full framework (React/Svelte) if complexity grows

---

## Success Metrics

### Phase 1: ✅
- Clean architecture (< 700 lines per module)
- All hardware features working
- Maintainable codebase

### Phase 2: 🎯
- Wireless operation working
- REST API functional
- Multiple clients supported

### Phase 3: 📱
- PWA installable on phone
- Real-time updates < 100ms latency
- IndexedDB storing 1000+ packets
- Maps rendering smoothly

### Phase 4: 🔬
- Pattern detection accuracy > 80%
- Historical analysis working
- ML models running on-device

---

## Notes from Nov 12, 2024 Discussion

**Key insights:**
- ESP32 handles known PSK decryption fine (no need for phone crypto)
- Phone excels at: pattern analysis, storage, visualization
- Hybrid approach: ESP32 captures, Phone analyzes
- PWA better than native for this use case
- Start simple (serial), add web server, then PWA
- Refactored architecture makes API layer easy to add

**Design philosophy:**
- Build incrementally
- Each phase is usable standalone
- Don't over-engineer early
- Clean interfaces enable evolution

---

## Getting Started (After Phase 1 Complete)

### Phase 2 First Steps:
1. Add `ESPAsyncWebServer` to platformio.ini
2. Create `web_server.cpp/h` modules
3. Define REST endpoint structure
4. Implement JSON serialization for existing data structures
5. Test with curl/Postman
6. Add WebSocket for real-time feed

### Phase 3 First Steps:
1. Create `webapp/` directory
2. Basic HTML/CSS/JS structure
3. Connect to ESP32 via fetch API
4. Add Leaflet.js map
5. Implement IndexedDB storage
6. Add Service Worker for PWA
7. Test installation on phone

**Current focus:** Complete Phase 1, update docs, commit to main. Then Phase 2 planning.
