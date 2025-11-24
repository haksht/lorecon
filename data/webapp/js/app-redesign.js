/* LoRa Recon - Redesigned UI */

class LoRaRecon {
    constructor() {
        // DOM Cache
        this.dom = {
            // Header
            connection: document.getElementById('connection'),
            connectionText: document.getElementById('connection-text'),
            devices: document.getElementById('devices'),
            packets: document.getElementById('packets'),
            modeDisplay: document.getElementById('mode-display'),
            modeDetail: document.getElementById('mode-detail'),
            
            // Sidebar
            sidebar: document.getElementById('sidebar'),
            sidebarOverlay: document.getElementById('sidebar-overlay'),
            menuToggle: document.getElementById('menu-toggle'),
            scanToggle: document.getElementById('scan-toggle'),
            targetSection: document.getElementById('target-section'),
            targetDetails: document.getElementById('target-details'),
            uptime: document.getElementById('uptime'),
            heap: document.getElementById('heap'),
            configCount: document.getElementById('config-count'),
            
            // Content Areas
            liveActivity: document.getElementById('live-activity'),
            recentDevices: document.getElementById('recent-devices'),
            activeFrequencies: document.getElementById('active-frequencies'),
            protocolStats: document.getElementById('protocol-stats'),
            devicesContent: document.getElementById('devices-content'),
            packetsContent: document.getElementById('packets-content'),
            frequencyContent: document.getElementById('frequency-content'),
            gpsContent: document.getElementById('gps-content'),
            networkCanvas: document.getElementById('network-canvas'),
            nodeDetails: document.getElementById('node-details')
        };

        // State
        this.currentTab = 'dashboard';
        this.scanning = false;
        this.targetMode = false;
        this.ws = null;
        this.networkMap = null;
        
        this.init();
    }

    async init() {
        this.setupWebSocket();
        this.setupNavigation();
        this.setupSidebar();
        this.setupActions();
        
        // Load initial dashboard
        await this.loadDashboard();
        
        // Start polling
        this.startPolling();
    }

    // ==================== WebSocket ====================
    
    setupWebSocket() {
        const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
        this.ws = new WebSocket(`${protocol}//${location.host}/ws`);
        
        this.ws.onopen = () => this.setConnected(true);
        this.ws.onclose = () => {
            this.setConnected(false);
            setTimeout(() => this.setupWebSocket(), 3000);
        };
        this.ws.onmessage = (e) => {
            try {
                const data = JSON.parse(e.data);
                if (data.type === 'status') this.updateStatus(data);
            } catch (err) {
                console.error('WS error:', err);
            }
        };
    }

    setConnected(connected) {
        this.dom.connection.classList.toggle('connected', connected);
        this.dom.connection.classList.toggle('disconnected', !connected);
        this.dom.connectionText.textContent = connected ? 'Connected' : 'Disconnected';
    }

    // ==================== Status Updates ====================
    
    async updateStatus(data) {
        // Update header stats
        this.dom.devices.textContent = data.devices || 0;
        this.dom.packets.textContent = data.capturedPackets || 0;
        
        // Update mode display
        const mode = data.mode || 'idle';
        const isScanning = mode.includes('reconnaissance');
        const isTargeted = mode.includes('target');
        
        this.scanning = isScanning;
        this.targetMode = isTargeted;
        
        // Update mode indicator
        if (isScanning) {
            this.dom.modeDisplay.textContent = '🔄';
            this.dom.modeDetail.textContent = 'Scanning';
        } else if (isTargeted) {
            this.dom.modeDisplay.textContent = '🎯';
            this.dom.modeDetail.textContent = 'Targeted';
        } else {
            this.dom.modeDisplay.textContent = '⏸️';
            this.dom.modeDetail.textContent = 'Idle';
        }
        
        // Update sidebar
        this.dom.uptime.textContent = this.formatDuration(data.uptime || 0);
        this.dom.heap.textContent = this.formatBytes(data.freeHeap || 0);
        
        // Update scan button
        if (this.dom.scanToggle) {
            this.dom.scanToggle.innerHTML = isScanning
                ? '<span class=\"btn-icon\">⏸</span><span class=\"btn-text\">Pause Scanning</span>'
                : '<span class=\"btn-icon\">▶</span><span class=\"btn-text\">Start Scanning</span>';
            this.dom.scanToggle.classList.toggle('scanning', isScanning);
        }
        
        // Show/hide target section
        if (isTargeted && data.target) {
            this.dom.targetSection.style.display = 'block';
            this.updateTargetDisplay(data.target);
        } else {
            this.dom.targetSection.style.display = 'none';
        }
        
        // Refresh current view if needed
        if (this.currentTab === 'dashboard') {
            this.refreshDashboard();
        }
    }

    updateTargetDisplay(target) {
        let html = `
            <div class=\"target-item\">
                <span class=\"target-label\">Frequency:</span>
                <span class=\"target-value\">${target.frequency?.toFixed(3) || '—'} MHz</span>
            </div>
        `;
        
        if (target.nodeId) {
            html += `
                <div class=\"target-item\">
                    <span class=\"target-label\">Node ID:</span>
                    <span class=\"target-value\">0x${target.nodeId}</span>
                </div>
            `;
        }
        
        if (target.protocol) {
            html += `
                <div class=\"target-item\">
                    <span class=\"target-label\">Protocol:</span>
                    <span class=\"target-value\">${target.protocol}</span>
                </div>
            `;
        }
        
        this.dom.targetDetails.innerHTML = html;
    }

    // ==================== Navigation ====================
    
    setupNavigation() {
        document.querySelectorAll('[data-tab]').forEach(btn => {
            btn.addEventListener('click', () => {
                const tab = btn.dataset.tab;
                this.switchTab(tab);
            });
        });
        
        // Quick stat navigation
        document.querySelectorAll('[data-action=\"view-devices\"]').forEach(el => {
            el.addEventListener('click', () => this.switchTab('devices'));
        });
        
        document.querySelectorAll('[data-action=\"view-activity\"]').forEach(el => {
            el.addEventListener('click', () => this.switchTab('activity'));
        });
        
        document.querySelectorAll('[data-action=\"view-all-devices\"]').forEach(el => {
            el.addEventListener('click', () => this.switchTab('devices'));
        });
    }

    async switchTab(tabName) {
        // Update nav buttons
        document.querySelectorAll('.nav-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.tab === tabName);
        });
        
        // Update content
        document.querySelectorAll('.tab-content').forEach(content => {
            content.classList.toggle('active', content.id === `tab-${tabName}`);
        });
        
        this.currentTab = tabName;
        
        // Load tab content
        switch(tabName) {
            case 'dashboard':
                await this.loadDashboard();
                break;
            case 'devices':
                await this.loadDevices();
                break;
            case 'activity':
                await this.loadActivity();
                break;
            case 'map':
                await this.loadMap();
                break;
        }
        
        // Close sidebar on mobile
        if (window.innerWidth < 1024) {
            this.closeSidebar();
        }
    }

    // ==================== Sidebar ====================
    
    setupSidebar() {
        this.dom.menuToggle?.addEventListener('click', () => this.toggleSidebar());
        this.dom.sidebarOverlay?.addEventListener('click', () => this.closeSidebar());
        
        // Close on ESC
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape' && this.dom.sidebar.classList.contains('open')) {
                this.closeSidebar();
            }
        });
    }

    toggleSidebar() {
        const isOpen = this.dom.sidebar.classList.toggle('open');
        this.dom.sidebarOverlay.classList.toggle('visible', isOpen);
        document.body.classList.toggle('sidebar-open', isOpen);
    }

    closeSidebar() {
        this.dom.sidebar.classList.remove('open');
        this.dom.sidebarOverlay.classList.remove('visible');
        document.body.classList.remove('sidebar-open');
    }

    // ==================== Actions ====================
    
    setupActions() {
        document.addEventListener('click', async (e) => {
            const action = e.target.closest('[data-action]')?.dataset.action;
            if (!action) return;
            
            e.preventDefault();
            
            switch(action) {
                case 'toggle-scan':
                    await this.toggleScanning();
                    break;
                case 'stop-capture':
                    await this.stopCapture();
                    break;
                case 'export-kml':
                    window.open('/api/kml', '_blank');
                    break;
                case 'clear-data':
                    if (confirm('Clear all captured data?')) {
                        await this.clearData();
                    }
                    break;
                case 'target-device':
                    const nodeId = e.target.closest('[data-action]').dataset.value;
                    await this.targetDevice(nodeId);
                    break;
                case 'target-frequency':
                    const configIndex = e.target.closest('[data-action]').dataset.value;
                    await this.targetFrequency(configIndex);
                    break;
                case 'replay-packet':
                    const slotIndex = e.target.closest('[data-action]').dataset.value;
                    await this.replayPacket(slotIndex);
                    break;
            }
        });
    }

    async toggleScanning() {
        try {
            const endpoint = this.scanning ? '/api/scan/stop' : '/api/scan/start';
            const response = await fetch(endpoint, { method: 'POST' });
            const data = await response.json();
            
            if (data.status === 'success') {
                showToast(this.scanning ? 'Scan paused' : 'Scanning started', 'success');
            }
        } catch (err) {
            showToast('Failed to toggle scan', 'error');
        }
    }

    async stopCapture() {
        try {
            await fetch('/api/capture/stop', { method: 'POST' });
            showToast('Capture stopped', 'success');
        } catch (err) {
            showToast('Failed to stop capture', 'error');
        }
    }

    async targetDevice(nodeId) {
        try {
            const response = await fetch('/api/capture/start', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: `nodeId=${nodeId}`
            });
            const data = await response.json();
            
            if (data.status === 'success') {
                showToast(`Targeting device 0x${nodeId}`, 'success');
                this.switchTab('activity');
            }
        } catch (err) {
            showToast('Failed to target device', 'error');
        }
    }

    async targetFrequency(configIndex) {
        try {
            const response = await fetch('/api/frequency/target', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: `configIndex=${configIndex}`
            });
            const data = await response.json();
            
            if (data.status === 'success') {
                showToast(`Targeting frequency config #${configIndex}`, 'success');
            }
        } catch (err) {
            showToast('Failed to target frequency', 'error');
        }
    }

    async replayPacket(slotIndex) {
        try {
            const response = await fetch('/api/replay/packet', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: `slot=${slotIndex}&repeat=1&delay=100`
            });
            const data = await response.json();
            
            if (data.status === 'success') {
                showToast('Packet replayed', 'success');
            }
        } catch (err) {
            showToast('Failed to replay packet', 'error');
        }
    }

    async clearData() {
        try {
            await fetch('/api/replay/clear', { method: 'POST' });
            showToast('Data cleared', 'success');
            this.loadDashboard();
        } catch (err) {
            showToast('Failed to clear data', 'error');
        }
    }

    // ==================== Dashboard ====================
    
    async loadDashboard() {
        await Promise.all([
            this.updateLiveActivity(),
            this.updateRecentDevices(),
            this.updateActiveFrequencies(),
            this.updateProtocolStats()
        ]);
    }

    async refreshDashboard() {
        // Only refresh if we're on dashboard
        if (this.currentTab === 'dashboard') {
            await this.loadDashboard();
        }
    }

    async updateLiveActivity() {
        try {
            const data = await this.fetchJSON('/api/status');
            
            let html = '<div class=\"activity-summary\">';
            html += `<div class=\"activity-stat\">`;
            html += `<div class=\"stat-number\">${data.totalPackets || 0}</div>`;
            html += `<div class=\"stat-label\">Total Packets</div>`;
            html += `</div>`;
            html += `<div class=\"activity-stat\">`;
            html += `<div class=\"stat-number\">${data.devices || 0}</div>`;
            html += `<div class=\"stat-label\">Devices</div>`;
            html += `</div>`;
            html += `<div class=\"activity-stat\">`;
            html += `<div class=\"stat-number\">${data.capturedPackets || 0}</div>`;
            html += `<div class=\"stat-label\">Captured</div>`;
            html += `</div>`;
            html += '</div>';
            
            this.dom.liveActivity.innerHTML = html;
        } catch (err) {
            this.dom.liveActivity.innerHTML = '<p class=\"error\">Failed to load activity</p>';
        }
    }

    async updateRecentDevices() {
        try {
            const data = await this.fetchJSON('/api/devices');
            
            if (!data.devices || data.devices.length === 0) {
                this.dom.recentDevices.innerHTML = '<p class=\"placeholder\">No devices detected yet</p>';
                return;
            }
            
            // Show top 5 most recent
            const recent = data.devices.slice(0, 5);
            
            let html = '<div class=\"device-list-compact\">';
            recent.forEach(device => {
                const rssiClass = (device.rssi || -100) > -70 ? 'rssi-strong' : (device.rssi || -100) > -90 ? 'rssi-medium' : 'rssi-weak';
                html += `
                    <button class=\"device-item-compact\" data-action=\"target-device\" data-value=\"${device.nodeId}\">
                        <div class=\"device-info\">
                            <strong>0x${device.nodeId}</strong>
                            <span class=\"device-type\">${device.deviceType || 'Unknown'}</span>
                        </div>
                        <div class=\"device-stats\">
                            <span class=\"${rssiClass}\">${device.rssi || '—'} dBm</span>
                            <span>${device.packetCount || 0} pkts</span>
                        </div>
                    </button>
                `;
            });
            html += '</div>';
            
            this.dom.recentDevices.innerHTML = html;
        } catch (err) {
            this.dom.recentDevices.innerHTML = '<p class=\"error\">Failed to load devices</p>';
        }
    }

    async updateActiveFrequencies() {
        try {
            const data = await this.fetchJSON('/api/activity');
            
            if (!data.activities) {
                this.dom.activeFrequencies.innerHTML = '<p class=\"placeholder\">No activity yet</p>';
                return;
            }
            
            // Show only active frequencies
            const active = data.activities.filter(a => a.packets > 0).slice(0, 5);
            
            if (active.length === 0) {
                this.dom.activeFrequencies.innerHTML = '<p class=\"placeholder\">No active frequencies</p>';
                return;
            }
            
            let html = '<div class=\"frequency-list-compact\">';
            active.forEach(freq => {
                const rssiClass = freq.avgRSSI > -70 ? 'rssi-strong' : freq.avgRSSI > -90 ? 'rssi-medium' : 'rssi-weak';
                html += `
                    <div class=\"frequency-item-compact\">
                        <div class=\"freq-info\">
                            <strong>${freq.protocol}</strong>
                            <span>${freq.frequencyMHz.toFixed(3)} MHz</span>
                        </div>
                        <div class=\"freq-stats\">
                            <span class=\"${rssiClass}\">${freq.avgRSSI} dBm</span>
                            <span>${freq.packets} pkts</span>
                        </div>
                    </div>
                `;
            });
            html += '</div>';
            
            this.dom.activeFrequencies.innerHTML = html;
        } catch (err) {
            this.dom.activeFrequencies.innerHTML = '<p class=\"error\">Failed to load frequencies</p>';
        }
    }

    async updateProtocolStats() {
        try {
            const data = await this.fetchJSON('/api/statistics');
            
            if (!data.statistics || !data.statistics.protocolDistribution) {
                this.dom.protocolStats.innerHTML = '<p class=\"placeholder\">No data yet</p>';
                return;
            }
            
            const protocols = data.statistics.protocolDistribution;
            const total = Object.values(protocols).reduce((sum, val) => sum + val, 0);
            
            if (total === 0) {
                this.dom.protocolStats.innerHTML = '<p class=\"placeholder\">No protocols detected</p>';
                return;
            }
            
            let html = '<div class=\"protocol-breakdown\">';
            Object.entries(protocols).forEach(([name, count]) => {
                if (count > 0) {
                    const percent = ((count / total) * 100).toFixed(0);
                    html += `
                        <div class=\"protocol-bar\">
                            <div class=\"protocol-label\">
                                <span class=\"protocol-name\">${name}</span>
                                <span class=\"protocol-count\">${count}</span>
                            </div>
                            <div class=\"protocol-progress\">
                                <div class=\"protocol-fill\" style=\"width: ${percent}%\"></div>
                            </div>
                        </div>
                    `;
                }
            });
            html += '</div>';
            
            this.dom.protocolStats.innerHTML = html;
        } catch (err) {
            this.dom.protocolStats.innerHTML = '<p class=\"error\">Failed to load stats</p>';
        }
    }

    // ==================== Devices View ====================
    
    async loadDevices() {
        try {
            const data = await this.fetchJSON('/api/devices');
            
            if (!data.devices || data.devices.length === 0) {
                this.dom.devicesContent.innerHTML = '<p class=\"placeholder\">No devices discovered yet. Start scanning to detect nearby LoRa devices.</p>';
                return;
            }
            
            let html = '<div class=\"table-wrapper\">';
            html += '<table class=\"data-table\"><thead><tr>';
            html += '<th>Node ID</th><th>Type</th><th>Protocol</th><th>Packets</th><th>RSSI</th><th>Frequency</th><th>Last Seen</th><th>Actions</th>';
            html += '</tr></thead><tbody>';
            
            data.devices.forEach(device => {
                const rssiClass = (device.rssi || -100) > -70 ? 'rssi-strong' : (device.rssi || -100) > -90 ? 'rssi-medium' : 'rssi-weak';
                html += '<tr>';
                html += `<td><code>0x${device.nodeId}</code></td>`;
                html += `<td>${device.deviceType || 'Unknown'}</td>`;
                html += `<td><code>${device.protocol || '—'}</code></td>`;
                html += `<td>${device.packetCount || 0}</td>`;
                html += `<td><span class=\"${rssiClass}\">${device.rssi || '—'} dBm</span></td>`;
                html += `<td>${(device.frequency || 0).toFixed(3)} MHz</td>`;
                html += `<td>${this.formatDuration((Date.now() - device.lastSeen) / 1000)} ago</td>`;
                html += `<td><button data-action=\"target-device\" data-value=\"${device.nodeId}\" class=\"btn btn-primary btn-small\">🎯 Target</button></td>`;
                html += '</tr>';
            });
            
            html += '</tbody></table></div>';
            this.dom.devicesContent.innerHTML = html;
        } catch (err) {
            this.dom.devicesContent.innerHTML = '<p class=\"error\">Failed to load devices</p>';
        }
    }

    // ==================== Activity View ====================
    
    async loadActivity() {
        await Promise.all([
            this.loadPackets(),
            this.loadFrequencies()
        ]);
    }

    async loadPackets() {
        try {
            const data = await this.fetchJSON('/api/replay/slots');
            
            if (!data.slots || data.slots.length === 0) {
                this.dom.packetsContent.innerHTML = '<p class=\"placeholder\">No packets captured yet. Target a device or frequency to capture packets.</p>';
                return;
            }
            
            let html = '<div class=\"table-wrapper\">';
            html += '<table class=\"data-table\"><thead><tr>';
            html += '<th>Slot</th><th>Protocol</th><th>Node ID</th><th>Size</th><th>RSSI</th><th>Frequency</th><th>Message</th><th>Actions</th>';
            html += '</tr></thead><tbody>';
            
            data.slots.forEach((pkt, idx) => {
                const rssiClass = (pkt.rssi || -100) > -70 ? 'rssi-strong' : (pkt.rssi || -100) > -90 ? 'rssi-medium' : 'rssi-weak';
                html += '<tr>';
                html += `<td><strong>${pkt.index || idx + 1}</strong></td>`;
                html += `<td><code>${pkt.protocol || 'Unknown'}</code></td>`;
                html += `<td>${pkt.nodeId ? '<code>0x' + pkt.nodeId + '</code>' : '—'}</td>`;
                html += `<td>${pkt.length || 0} B</td>`;
                html += `<td><span class=\"${rssiClass}\">${pkt.rssi || '—'} dBm</span></td>`;
                html += `<td>${(pkt.frequencyMHz || 0).toFixed(3)} MHz</td>`;
                html += `<td class=\"msg-cell\">${pkt.decryptedText || '—'}</td>`;
                html += `<td><button data-action=\"replay-packet\" data-value=\"${idx}\" class=\"btn btn-primary btn-small\">🔁 Replay</button></td>`;
                html += '</tr>';
            });
            
            html += '</tbody></table></div>';
            this.dom.packetsContent.innerHTML = html;
            
            // Update packet count
            const countEl = document.getElementById('packet-count');
            if (countEl) countEl.textContent = data.slots.length;
        } catch (err) {
            this.dom.packetsContent.innerHTML = '<p class=\"error\">Failed to load packets</p>';
        }
    }

    async loadFrequencies() {
        try {
            const data = await this.fetchJSON('/api/activity');
            
            if (!data.activities) {
                this.dom.frequencyContent.innerHTML = '<p class=\"placeholder\">Loading frequency configurations...</p>';
                return;
            }
            
            let html = '<div class=\"table-wrapper\">';
            html += '<table class=\"data-table\"><thead><tr>';
            html += '<th>Protocol</th><th>Frequency</th><th>SF</th><th>BW</th><th>Packets</th><th>RSSI</th><th>Actions</th>';
            html += '</tr></thead><tbody>';
            
            data.activities.forEach(act => {
                const isActive = act.packets > 0;
                const rssiClass = act.avgRSSI > -70 ? 'rssi-strong' : act.avgRSSI > -90 ? 'rssi-medium' : 'rssi-weak';
                const rowClass = isActive ? '' : 'inactive-row';
                html += `<tr class=\"${rowClass}\">`;
                html += `<td><strong>${act.protocol}</strong> <span class=\"badge config-badge\">#${act.configIndex}</span></td>`;
                html += `<td>${act.frequencyMHz.toFixed(3)} MHz</td>`;
                html += `<td>SF${act.spreadingFactor}</td>`;
                html += `<td>${act.bandwidthKHz} kHz</td>`;
                if (isActive) {
                    html += `<td>${act.packets}</td>`;
                    html += `<td><span class=\"${rssiClass}\">${act.avgRSSI} dBm</span></td>`;
                } else {
                    html += '<td class=\"text-muted\">—</td>';
                    html += '<td class=\"text-muted\">—</td>';
                }
                html += `<td><button data-action=\"target-frequency\" data-value=\"${act.configIndex}\" class=\"btn btn-primary btn-small\">🎯 Target</button></td>`;
                html += '</tr>';
            });
            
            html += '</tbody></table></div>';
            this.dom.frequencyContent.innerHTML = html;
        } catch (err) {
            this.dom.frequencyContent.innerHTML = '<p class=\"error\">Failed to load frequencies</p>';
        }
    }

    // ==================== Map View ====================
    
    async loadMap() {
        // Initialize network map if needed
        if (!this.networkMap && typeof NetworkMap !== 'undefined') {
            this.networkMap = new NetworkMap('network-canvas', 'node-details');
        }
        
        // Load devices for map
        try {
            const data = await this.fetchJSON('/api/devices');
            if (data && data.devices && this.networkMap) {
                this.networkMap.updateDevices(data.devices);
            }
        } catch (err) {
            console.error('Failed to update map:', err);
        }
        
        // Load GPS data
        try {
            const gpsData = await this.fetchJSON('/api/positions');
            
            if (gpsData && gpsData.positions && gpsData.positions.length > 0) {
                let html = '<div class=\"table-wrapper\"><table class=\"data-table\"><thead><tr>';
                html += '<th>Node ID</th><th>Latitude</th><th>Longitude</th><th>Altitude</th>';
                html += '</tr></thead><tbody>';
                
                gpsData.positions.forEach(pos => {
                    html += '<tr>';
                    html += `<td><code>0x${pos.nodeId}</code></td>`;
                    html += `<td>${pos.lat.toFixed(6)}</td>`;
                    html += `<td>${pos.lon.toFixed(6)}</td>`;
                    html += `<td>${pos.alt || '—'} m</td>`;
                    html += '</tr>';
                });
                
                html += '</tbody></table></div>';
                this.dom.gpsContent.innerHTML = html;
            } else {
                this.dom.gpsContent.innerHTML = '<p class=\"placeholder\">No GPS locations captured yet</p>';
            }
        } catch (err) {
            this.dom.gpsContent.innerHTML = '<p class=\"placeholder\">No GPS data available</p>';
        }
    }

    // ==================== Polling ====================
    
    startPolling() {
        // Poll status every 5 seconds
        setInterval(async () => {
            try {
                const data = await this.fetchJSON('/api/status');
                this.updateStatus(data);
            } catch (err) {
                console.error('Polling error:', err);
            }
        }, 5000);
        
        // Refresh current view every 3 seconds
        setInterval(() => {
            if (this.currentTab === 'dashboard') {
                this.refreshDashboard();
            }
        }, 3000);
    }

    // ==================== Utilities ====================
    
    async fetchJSON(url) {
        const response = await fetch(url);
        return response.json();
    }

    formatDuration(seconds) {
        if (seconds < 60) return `${Math.floor(seconds)}s`;
        if (seconds < 3600) return `${Math.floor(seconds / 60)}m`;
        const hours = Math.floor(seconds / 3600);
        const mins = Math.floor((seconds % 3600) / 60);
        return `${hours}h ${mins}m`;
    }

    formatBytes(bytes) {
        if (bytes < 1024) return `${bytes} B`;
        if (bytes < 1048576) return `${(bytes / 1024).toFixed(1)} KB`;
        return `${(bytes / 1048576).toFixed(1)} MB`;
    }
}

// Initialize when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => new LoRaRecon());
} else {
    new LoRaRecon();
}
