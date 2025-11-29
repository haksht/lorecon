# Feature Branch: PCAP Export & Web UI Fixes

**Branch:** `feature/pcap-export`  
**Created:** November 29, 2025  
**Status:** In Progress

---

## Overview

This branch implements PCAP export functionality and fixes critical web UI issues on iPhone/mobile devices.

## Issues Addressed

### 1. **PCAP Export** (New Feature)
- Standard packet capture format compatible with Wireshark, LoRa_Craft, Scapy
- Preserves RSSI/SNR/frequency metadata in pseudo-header
- Enables offline analysis with powerful desktop tools
- Future-proof format for long-term archival

### 2. **Mobile Web UI Issues** (Bug Fixes)
- ✗ RF Activity button does nothing on iPhone
- ✗ Security button does nothing on iPhone  
- ✗ Network map drawn incorrectly
- ✗ Device popups don't appear when clicking map nodes
- ✗ Device list not sorted by vulnerability

---

## Implementation Plan

### Phase 1: PCAP Export (Days 1-2) ✓ DONE

**Files Created:**
- `firmware/src/pcap_logger.h` - PCAP format writer header
- `firmware/src/pcap_logger.cpp` - PCAP implementation

**Features:**
- Standard PCAP global header (magic 0xa1b2c3d4)
- Custom LoRa pseudo-header with metadata:
  - Frequency (MHz)
  - RSSI (dBm)
  - SNR (dB)
  - Spreading Factor
  - Bandwidth
  - Coding Rate
- Auto-generated timestamped filenames
- Session management
- Periodic flushing (every 10 packets)

**Integration Points:**
- [ ] Update `packet_logger.cpp` to optionally enable PCAP
- [ ] Add PCAP toggle to `config.h`
- [ ] Web UI: Add "Export PCAP" button
- [ ] API endpoint: `GET /api/export/pcap` to download

---

### Phase 2: Web UI Fixes (Days 3-4)

#### Fix 1: Button Event Handlers (iPhone Issue)

**Problem:** `show-activity` and `show-security` buttons call `switchTab()` but don't work on iOS Safari

**Root Cause:** Event delegation issue or iOS touch event handling

**Fix:**
```javascript
// In app.js line 677-683, change from:
case 'show-activity':
    this.switchTab('frequency');
    break;
case 'show-security':
    this.switchTab('info');  // Wrong tab!
    break;

// To:
case 'show-activity':
    this.switchTab('frequency');
    this.closeMobileMenu(); // For mobile
    break;
case 'show-security':
    // Security data should go to a dedicated security tab or modal
    await this.showSecurityAssessment();
    this.closeMobileMenu();
    break;
```

**Files to Modify:**
- `data/webapp/js/app.js` - Fix event handlers
- Add iOS-specific touch event handling
- Ensure `switchTab()` works on mobile

---

#### Fix 2: Network Map Issues

**Problem:** 
- Map drawn incorrectly
- Popups don't appear when clicking devices
- No vulnerability sorting

**Root Cause Analysis:**
```javascript
// network-map.js line 91-100
handleClick(e) {
    const rect = this.canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;  // Doesn't account for scroll/zoom on mobile
    const y = e.clientY - rect.top;
    
    const node = this.findNodeAtPosition(x, y);
    if (node) {
        this.selectedNode = node;
        this.showNodeDetails(node);  // This might be failing silently
    }
}
```

**Fixes Needed:**

1. **Touch Event Support:**
```javascript
setupEventListeners() {
    // Add touch support for mobile
    this.canvas.addEventListener('touchstart', (e) => this.handleTouch(e));
    this.canvas.addEventListener('mousemove', (e) => this.handleMouseMove(e));
    this.canvas.addEventListener('click', (e) => this.handleClick(e));
}

handleTouch(e) {
    e.preventDefault();
    const touch = e.touches[0];
    const rect = this.canvas.getBoundingClientRect();
    const x = touch.clientX - rect.left;
    const y = touch.clientY - rect.top;
    
    const node = this.findNodeAtPosition(x, y);
    if (node) {
        this.selectedNode = node;
        this.showNodeDetails(node);
    }
}
```

2. **Fix Node Details Display:**
```javascript
showNodeDetails(node) {
    if (!this.detailsPanel) {
        console.error('Details panel not found');
        return;
    }
    
    // Ensure panel is visible
    this.detailsPanel.style.display = 'block';
    
    // Build HTML
    let html = '<div class="node-details-card">';
    html += `<h3>Device 0x${node.nodeId || 'Unknown'}</h3>`;
    html += `<p><strong>Protocol:</strong> ${node.protocol || 'Unknown'}</p>`;
    html += `<p><strong>RSSI:</strong> ${node.rssi || node.avgRSSI || '—'} dBm</p>`;
    html += `<p><strong>Packets:</strong> ${node.packetCount || 0}</p>`;
    
    // Add vulnerability info if available
    if (node.vulnerabilityScore !== undefined) {
        const vulnClass = node.vulnerabilityScore > 7 ? 'danger' : 
                         node.vulnerabilityScore > 4 ? 'warning' : 'success';
        html += `<p><strong>Vulnerability:</strong> <span class="${vulnClass}">${node.vulnerabilityScore}/10</span></p>`;
    }
    
    html += '</div>';
    this.detailsPanel.innerHTML = html;
}
```

3. **Sort Devices by Vulnerability:**
```javascript
updateDevices(devices) {
    // Sort by vulnerability score (highest first)
    devices.sort((a, b) => {
        const scoreA = this.calculateVulnerabilityScore(a);
        const scoreB = this.calculateVulnerabilityScore(b);
        return scoreB - scoreA; // Descending order
    });
    
    // Calculate positions...
    this.devices = devices.map((device, index) => {
        // ... existing position calculation
    });
}

calculateVulnerabilityScore(device) {
    let score = 0;
    
    // High RSSI = device is close = more vulnerable to attack
    if (device.rssi > -50) score += 3;
    else if (device.rssi > -70) score += 2;
    else if (device.rssi > -90) score += 1;
    
    // Unencrypted or default PSK = vulnerable
    if (device.hasDefaultPSK) score += 4;
    if (!device.encrypted) score += 3;
    
    // Router devices are high-value targets
    if (device.isRouter) score += 2;
    
    // High packet count = active device = valuable target
    if (device.packetCount > 100) score += 1;
    
    return score;
}
```

**Files to Modify:**
- `data/webapp/js/network-map.js` - Add touch support, fix popups, add vulnerability sorting
- `data/webapp/css/style.css` - Ensure details panel is styled for mobile

---

#### Fix 3: Real Map vs Diagram

**Question:** How much bloat to use a real map (e.g., Leaflet.js, Mapbox)?

**Analysis:**

**Option A: Keep Current Canvas Diagram**
- ✅ Minimal bloat (~2KB network-map.js)
- ✅ Works offline
- ✅ Fast rendering
- ✅ Stylized "hacker" aesthetic
- ❌ Not geographically accurate
- ❌ Can't overlay on real map

**Option B: Use Leaflet.js + OpenStreetMap**
- ❌ ~140KB minified (Leaflet.js)
- ❌ Requires internet for map tiles
- ❌ ESP32 would need to serve large JS library
- ✅ Real geographic positioning
- ✅ Familiar map interface
- ✅ Can show GPS tracks

**Option C: Hybrid Approach**
- Keep canvas diagram as default (offline, fast)
- Add optional "Geographic View" tab for users with GPS data
- Only load Leaflet.js if GPS positions exist
- Best of both worlds

**Recommendation:** Option C (Hybrid)

**Implementation:**
```javascript
// In app.js
async showNetwork() {
    const devices = await this.get('/api/devices');
    const hasGPS = devices.devices.some(d => d.hasPosition);
    
    if (hasGPS) {
        // Show tab switcher: [ Topology ] [ Geographic ]
        this.showNetworkViewSwitcher();
    }
    
    // Default to canvas topology view
    this.networkMap.updateDevices(devices.devices);
}
```

**Bloat Analysis:**
- Current: ~2KB (canvas only)
- Hybrid: ~2KB normally, ~140KB if GPS data present
- Full map: ~140KB always

**Decision:** Implement hybrid approach only if GPS data is common. Otherwise, keep canvas diagram and fix the bugs.

---

## Testing Checklist

### PCAP Export
- [ ] PCAP file created on SD card
- [ ] File has correct global header (magic number)
- [ ] Packets written with pseudo-header
- [ ] RSSI/SNR/frequency preserved
- [ ] Can open in Wireshark
- [ ] Can import into LoRa_Craft Python decoder
- [ ] Web UI download works
- [ ] API endpoint `/api/export/pcap` returns file

### Web UI - iOS Safari
- [ ] RF Activity button switches to Frequency tab
- [ ] Security button shows assessment (or switches to Info)
- [ ] Network map responds to touch events
- [ ] Device popup appears when tapping node
- [ ] Popup displays all device info
- [ ] Devices sorted by vulnerability (high to low)
- [ ] No console errors on iPhone

### Web UI - Desktop
- [ ] All buttons still work on desktop
- [ ] Network map still works with mouse
- [ ] No regressions in existing functionality

---

## Files Modified

### New Files:
- `firmware/src/pcap_logger.h`
- `firmware/src/pcap_logger.cpp`
- `docs/PCAP_EXPORT_GUIDE.md` (usage documentation)

### Modified Files:
- `firmware/src/packet_logger.cpp` - Integrate PCAP export
- `firmware/src/packet_logger.h` - Add PCAP option
- `firmware/src/config.h` - Add `ENABLE_PCAP_EXPORT` flag
- `firmware/src/web_server.cpp` - Add `/api/export/pcap` endpoint
- `firmware/src/api_controller.cpp` - PCAP download handler
- `data/webapp/index.html` - Add PCAP export button
- `data/webapp/js/app.js` - Fix iOS button handlers, add PCAP export
- `data/webapp/js/network-map.js` - Touch support, popup fixes, vulnerability sorting
- `data/webapp/css/style.css` - Mobile-friendly details panel

---

## Merge Criteria

Before merging to `main`:

1. ✓ All code compiles without errors
2. ✓ PCAP files validate in Wireshark
3. ✓ Web UI works on iPhone Safari
4. ✓ Network map popups work on mobile
5. ✓ Devices sorted by vulnerability
6. ✓ No regressions in existing features
7. ✓ Documentation updated (README.md, API_REFERENCE.md)
8. ✓ Tested on actual hardware (ESP32 + SD card)

---

## Next Steps

1. **Integrate PCAP into packet_logger.cpp**
2. **Add web API endpoint for PCAP download**
3. **Fix iOS button event handlers**
4. **Add touch support to network map**
5. **Implement vulnerability scoring**
6. **Test on iPhone**
7. **Write documentation**
8. **Merge to main**

---

**Last Updated:** November 29, 2025  
**Status:** Phase 1 Complete, Starting Phase 2
