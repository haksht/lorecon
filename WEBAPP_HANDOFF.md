# ESP32 LoRa Sniffer - Webapp UI/UX Handoff Document

## Project Context
ESP32-based LoRa packet sniffer with web interface. Device scans LoRa frequencies, discovers Meshtastic/LoRaWAN devices, captures packets, and provides real-time visualization through a Progressive Web App.

## Current Status
✅ **Working:** Core functionality operational, recently simplified from 2,870 → 1,300 lines of JavaScript
⚠️ **Needs UI/UX Expert:** Layout issues, design inconsistencies, missing responsive behavior

## Architecture

### Backend (ESP32)
- **Web Server:** AsyncWebServer on port 80, LittleFS filesystem
- **API:** REST endpoints + WebSocket for real-time updates
- **Language:** C++ (Arduino framework)
- **Key Files:**
  - `firmware/src/web_server.cpp` - HTTP server & WebSocket
  - `firmware/src/api_controller.cpp` - REST API handlers
  - `firmware/src/recon_service.cpp` - Data formatting

### Frontend (Progressive Web App)
- **Tech Stack:** Vanilla JavaScript (no frameworks), WebSocket + REST
- **Location:** `data/webapp/`
- **Files:**
  - `index.html` (127 lines) - Main structure
  - `js/app.js` (486 lines) - Core application logic
  - `js/network-map.js` (413 lines) - Canvas-based network visualization
  - `js/war-room.js` (216 lines) - Statistics dashboard
  - `js/toast.js` (146 lines) - Notifications
  - `css/style.css` (1,283 lines) - All styling

### Deployment
```bash
~/.platformio/penv/Scripts/platformio.exe run -t uploadfs
```
Compresses and uploads to ESP32 LittleFS partition at 0x00670000

## API Endpoints

### Status & Config
- `GET /api/status` - System status (mode, uptime, devices, packets, heap)
- `GET /api/dashboard` - Combined initial load data
- `GET /api/config` - Current configuration
- `POST /api/scan/start` - Resume reconnaissance
- `POST /api/scan/stop` - Stop scanning

### Device Management
- `GET /api/devices` - List all discovered devices
  - Returns: `{status, devices: [{nodeId, deviceType, packetCount, bestRSSI, lastSeen, frequency, protocol}]}`
- `GET /api/device/:nodeId` - Single device details
- `POST /api/capture/start` - Target device or frequency
  - Body: `{nodeId: "ABC123"}` OR `{configIndex: 5}`
- `POST /api/capture/stop` - Stop targeting, resume recon

### Packet Data
- `GET /api/replay/slots` - Captured packets in memory (NOT /api/packets which requires SD card)
  - Returns: `{slots: [{valid, from, to, payloadSize, rssi, snr}]}`
- `POST /api/replay` - Replay packet
  - Body: `{slot: 0, repeatCount: 1, delayMs: 0}`

### Analysis
- `GET /api/activity` - RF activity by frequency
  - Returns: `{activities: [{frequency, protocol, packets, avgRSSI, configIndex}]}`
- `GET /api/gps` - GPS coordinates from devices
  - Returns: `{locations: [{nodeId, latitude, longitude, altitude}]}`
- `GET /api/security` - Encryption analysis
  - Returns: `{summary: {encryptedPackets, unencryptedPackets, defaultKeys}}`
- `GET /api/statistics` - Protocol distribution, packet stats
- `GET /api/kml` - Export as KML file

### WebSocket (`ws://192.168.4.1/ws`)
- Real-time status updates: `{type: "status", mode, devices, capturedPackets, uptime, freeHeap}`
- Connects automatically, reconnects on disconnect

## Current UI Issues

### Critical Layout Problems
1. **Devices Tab:**
   - Table doesn't render properly without `<div class="table-wrapper">` wrapper
   - Must use `.table` class (not `.data-table`)
   - Example structure:
   ```html
   <div class="table-wrapper">
     <table class="table">
       <thead><tr><th>...</th></tr></thead>
       <tbody><tr><td>...</td></tr></tbody>
     </table>
   </div>
   ```

2. **Packets Tab:**
   - NOW FIXED: Uses `/api/replay/slots` (not `/api/packets`)
   - Filters out `!pkt.valid` slots
   - Uses correct field names: `payloadSize`, `from`, `to`, `rssi`, `snr`

3. **Responsive Design:**
   - Mobile hamburger menu uses `.active` class on `.actions-section`
   - Overlay uses `.active` class (not `.show`)
   - Sidebar stats stay visible, action buttons slide in via hamburger

4. **Info Tab:**
   - Combines GPS, Frequency Analysis, Security into single tab
   - Frequency table now has "Target" buttons with `data-action="target-frequency"`

### Design Inconsistencies
- CSS is 1,283 lines with redundant rules
- Mobile breakpoints: 768px, 1024px, 1280px
- Color scheme defined but inconsistently applied
- Tables scroll horizontally on mobile but could be card-based
- Status cards clickable but no visual feedback

### Missing Features
- No loading states (just "Loading..." placeholders)
- No error states beyond console.error
- Toast notifications exist but underutilized
- No pull-to-refresh
- No visual feedback on button clicks
- Network map and War Room components work but styling is inconsistent

## Code Patterns

### Adding New Tab Content
```javascript
async showNewTab() {
    try {
        const data = await this.get('/api/endpoint');
        if (!data || !data.items) {
            this.el.contentDiv.innerHTML = '<p class="placeholder">No data</p>';
            return;
        }
        
        let html = '<div class="table-wrapper"><table class="table">';
        // ... build table
        html += '</table></div>';
        this.el.contentDiv.innerHTML = html;
    } catch (error) {
        console.error('Failed:', error);
        this.el.contentDiv.innerHTML = '<p class="error">Failed to load</p>';
    }
}
```

### Adding Button Actions
```javascript
// In handleAction() method:
case 'new-action':
    await this.post('/api/endpoint', { param: value });
    showToast('Action completed', 'success');
    break;
```

## What We Need

### Immediate UI/UX Improvements
1. **Visual Polish:**
   - Consistent button styles and hover states
   - Loading spinners instead of text placeholders
   - Better error states with retry buttons
   - Visual feedback on interactions
   - Smooth transitions/animations

2. **Responsive Refinement:**
   - Mobile-first cards for tables on small screens
   - Better touch targets (44px minimum)
   - Improved hamburger menu animation
   - Tablet layout optimization

3. **Information Architecture:**
   - Review 5-tab structure (Devices, Packets, Network, Stats, Info)
   - Consider merging/splitting tabs
   - Prioritize most-used features
   - Reduce cognitive load

4. **CSS Optimization:**
   - Consolidate redundant rules
   - Use CSS custom properties consistently
   - Minimize specificity issues
   - Consider utility classes

### API Improvements Needed
1. **Pagination:** Large device lists crash browser
2. **Filtering:** Need to filter by protocol, RSSI, time
3. **Sorting:** Tables should be sortable
4. **Search:** Device search by node ID
5. **Real-time efficiency:** WebSocket broadcasts could be optimized

## Design System

### Colors (CSS Variables)
```css
--primary: #4a90e2;
--secondary: #50c878;
--accent: #ff6b6b;
--bg-primary: #0f0f23;
--bg-secondary: #16213e;
--bg-card: #1a1a2e;
--text-primary: #e0e0e0;
--text-secondary: #a0a0a0;
--border: rgba(255, 255, 255, 0.1);
--success: #50c878;
--warning: #ffd93d;
--error: #ff6b6b;
```

### Typography
- Base: system font stack
- Headers: 1.5rem - 2rem
- Body: 0.9rem
- Compact: 0.85rem
- Tiny: 0.7rem

### Spacing
- Cards: 1rem padding
- Sections: 1.5rem margins
- Grid gaps: 0.75rem - 1rem

### Components
- `.stat-card` - 4 sidebar status cards
- `.btn`, `.btn-primary`, `.btn-secondary`, `.btn-small`
- `.table`, `.table-wrapper` - Responsive tables
- `.tab-btn`, `.tab-content` - Tab navigation
- `.status-badge` - Connection indicator
- `.target-info` - Targeting banner

## Testing Environment
- **Hardware:** ESP32-S3 @ 192.168.4.1 (WiFi AP mode)
- **Browsers:** iPhone Safari (primary), laptop Chrome
- **Test Devices:** Meshtastic T-Deck, iPhone, MacBook
- **No external dependencies** - all assets served from ESP32

## Backup Files
- `index-old.html.bak` - Previous version before simplification
- `app-old.js.bak` - Previous 1,454 line version
- Git repo: `tiarno/esp32-sniffer` branch `main`

## Success Criteria
✅ Tables render perfectly on all screen sizes
✅ Mobile hamburger menu is intuitive and smooth
✅ Loading/error states are clear and actionable
✅ Design feels modern, consistent, professional
✅ No layout shifts or visual bugs
✅ All interactions have visual feedback
✅ CSS reduced by at least 30%
✅ Accessible (keyboard nav, ARIA labels)

## Questions for UI/UX Expert
1. Should tables become cards on mobile? NO
2. Is 5 tabs too many? Optimal IA?
3. Should we add a settings/config UI (currently serial-only)?
4. Toast position/duration/styling optimal?
5. Network canvas map vs. list view - which is more useful?
6. Should we add dark/light mode toggle? NO
7. PWA install prompt - when/how to show?

## Current Webapp Size
- **Total:** ~35KB compressed
- **HTML:** 127 lines
- **JS:** ~1,300 lines (app.js + components)
- **CSS:** 1,283 lines ⚠️ optimization opportunity

## Contact Context
User (Tim) is testing on iPhone + laptop, knows C++/embedded well, less familiar with modern web UI/UX patterns. Prefers simple, functional design over flashy. Device is for field reconnaissance/wardriving. use full path for pio commands.
