/* ESP32 LoRa Recon - Simplified Client */

class ReconApp {
    constructor() {
        // DOM references
        this.el = {
            connection: document.getElementById('connection'),
            connectionText: document.getElementById('connection-text'),
            mode: document.getElementById('mode'),
            devices: document.getElementById('devices'),
            packets: document.getElementById('packets'),
            uptime: document.getElementById('uptime'),
            devicesContent: document.getElementById('devices-content'),
            packetsContent: document.getElementById('packets-content'),
            frequencyContent: document.getElementById('frequency-content'),
            gpsContent: document.getElementById('gps-content'),
            securityContent: document.getElementById('security-content'),
            infoMode: document.getElementById('info-mode'),
            infoUptime: document.getElementById('info-uptime'),
            infoPackets: document.getElementById('info-packets'),
            infoDevices: document.getElementById('info-devices'),
            infoHeap: document.getElementById('info-heap'),
            mobileMenuToggle: document.getElementById('mobile-menu-toggle'),
            mobileMenuOverlay: document.getElementById('mobile-menu-overlay'),
            targetInfo: document.getElementById('target-info'),
            targetDetails: document.getElementById('target-details')
        };

        // State
        this.statusTimer = null;
        this.currentTab = 'devices';
        this.isMobile = window.innerWidth < 768;
        this.networkMap = null;
        this.warRoom = null;
        this.ws = null;

        this.init();
    }

    async init() {
        try {
            // Setup WebSocket
            this.connectWebSocket();
            
            // Setup event handlers
            this.setupTabs();
            this.setupButtons();
            this.setupMobileMenu();
            
            // Initial data load
            await this.updateStatus();
            this.statusTimer = setInterval(() => this.updateStatus(), 10000);
            
            // Load first tab (Info)
            this.loadTabContent('info');
        } catch (error) {
            console.error('Init error:', error);
        }
    }

    // ============ WebSocket ============
    
    connectWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        this.ws = new WebSocket(`${protocol}//${window.location.host}/ws`);
        
        this.ws.onopen = () => {
            this.setConnected(true);
        };
        
        this.ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                if (data.type === 'status') {
                    this.handleStatusUpdate(data);
                }
            } catch (error) {
                console.error('WebSocket message error:', error);
            }
        };
        
        this.ws.onclose = () => {
            this.setConnected(false);
            setTimeout(() => this.connectWebSocket(), 3000);
        };
    }

    handleStatusUpdate(data) {
        // Update sidebar stats
        if (this.el.mode) this.el.mode.textContent = this.formatMode(data.mode);
        if (this.el.devices) this.el.devices.textContent = data.devices || 0;
        if (this.el.packets) this.el.packets.textContent = data.capturedPackets || 0;
        if (this.el.uptime) this.el.uptime.textContent = this.formatDuration(data.uptime || 0);
        
        // Update info tab if visible
        if (this.currentTab === 'info') {
            if (this.el.infoMode) this.el.infoMode.textContent = this.formatMode(data.mode);
            if (this.el.infoUptime) this.el.infoUptime.textContent = this.formatDuration(data.uptime || 0);
            if (this.el.infoPackets) this.el.infoPackets.textContent = data.totalPackets || 0;
            if (this.el.infoDevices) this.el.infoDevices.textContent = data.devices || 0;
            if (this.el.infoHeap) this.el.infoHeap.textContent = this.formatBytes(data.freeHeap || 0);
        }
        
        // Update target banner
        this.updateTargetBanner(data);
    }

    updateTargetBanner(data) {
        if (!this.el.targetInfo || !data.target) return;
        
        const mode = (data.mode || '').toLowerCase();
        if (mode.includes('target') && data.target.frequency) {
            this.el.targetInfo.style.display = 'block';
            let html = `<strong>Frequency:</strong> ${data.target.frequency.toFixed(3)} MHz`;
            if (data.target.nodeId) {
                html += `<br><strong>Node:</strong> 0x${data.target.nodeId}`;
            }
            this.el.targetDetails.innerHTML = html;
        } else {
            this.el.targetInfo.style.display = 'none';
        }
    }

    // ============ Status API ============
    
    async updateStatus() {
        try {
            const data = await this.get('/api/status');
            if (data) this.handleStatusUpdate(data);
            this.setConnected(true);
        } catch (error) {
            console.error('Status update failed:', error);
            this.setConnected(false);
        }
    }

    setConnected(connected) {
        if (this.el.connection) {
            this.el.connection.classList.toggle('connected', connected);
            this.el.connection.classList.toggle('disconnected', !connected);
        }
        if (this.el.connectionText) {
            this.el.connectionText.textContent = connected ? 'Connected' : 'Disconnected';
        }
    }

    // ============ Tabs ============
    
    setupTabs() {
        document.querySelectorAll('.tab-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                const tab = btn.dataset.tab;
                this.switchTab(tab);
            });
        });
    }

    switchTab(tabName) {
        // Update active tab button
        document.querySelectorAll('.tab-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.tab === tabName);
        });
        
        // Update active tab content
        document.querySelectorAll('.tab-content').forEach(content => {
            content.classList.toggle('active', content.id === `tab-${tabName}`);
        });
        
        this.currentTab = tabName;
        this.loadTabContent(tabName);
    }

    async loadTabContent(tabName) {
        switch(tabName) {
            case 'devices':
                await this.showDevices();
                break;
            case 'packets':
                await this.showPackets();
                break;
            case 'frequency':
                await this.showFrequency();
                break;
            case 'network':
                await this.showNetwork();
                break;
            case 'stats':
                await this.showStats();
                break;
            case 'info':
                await this.showInfo();
                break;
        }
    }

    // ============ Tab Content Loaders ============
    
    async showDevices() {
        try {
            const data = await this.get('/api/devices');
            if (!data || !data.devices || data.devices.length === 0) {
                this.el.devicesContent.innerHTML = '<p class="placeholder">No devices found. Devices will appear as they are discovered during scanning.</p>';
                return;
            }
            
            let html = '<div class="table-wrapper">';
            html += '<table class="table"><thead><tr>';
            html += '<th>Node ID</th><th>Type</th><th>Protocol</th><th>Packets</th><th>RSSI</th><th>Frequency</th><th>Last Seen</th><th>Actions</th>';
            html += '</tr></thead><tbody>';
            
            data.devices.forEach(device => {
                const rssiClass = (device.rssi || -100) > -70 ? 'rssi-strong' : (device.rssi || -100) > -90 ? 'rssi-medium' : 'rssi-weak';
                html += '<tr>';
                html += `<td><code>0x${device.nodeId}</code></td>`;
                html += `<td>${device.deviceType || 'Unknown'}</td>`;
                html += `<td><code>${device.protocol || '—'}</code></td>`;
                html += `<td>${device.packetCount || 0}</td>`;
                html += `<td><span class="${rssiClass}">${device.rssi || '—'} dBm</span></td>`;
                html += `<td>${(device.frequency || 0).toFixed(3)} MHz</td>`;
                html += `<td>${this.formatDuration((Date.now() - device.lastSeen) / 1000)} ago</td>`;
                html += `<td><button data-action="target-device" data-value="${device.nodeId}" class="btn btn-primary btn-small">🎯 Target</button></td>`;
                html += '</tr>';
            });
            
            html += '</tbody></table></div>';
            this.el.devicesContent.innerHTML = html;
        } catch (error) {
            console.error('Failed to load devices:', error);
            this.el.devicesContent.innerHTML = '<div class="error-state"><p class="error">Failed to load devices</p><button data-action="retry-devices" class="btn btn-primary">Retry</button></div>';
        }
    }

    async showPackets() {
        try {
            const data = await this.get('/api/replay/slots');
            if (!data || !data.slots || data.slots.length === 0) {
                this.el.packetsContent.innerHTML = '<p class="placeholder">No packets captured yet. Packets will appear here when targeting a device.</p>';
                return;
            }
            
            let html = '<div class="table-wrapper">';
            html += '<table class="table"><thead><tr>';
            html += '<th>Slot</th><th>Protocol</th><th>Node ID</th><th>Size</th><th>RSSI</th><th>Frequency</th><th>Message</th><th>Actions</th>';
            html += '</tr></thead><tbody>';
            
            data.slots.forEach((pkt, idx) => {
                const rssiClass = (pkt.rssi || -100) > -70 ? 'rssi-strong' : (pkt.rssi || -100) > -90 ? 'rssi-medium' : 'rssi-weak';
                html += '<tr>';
                html += `<td><strong>${pkt.index || idx + 1}</strong></td>`;
                html += `<td><code>${pkt.protocol || 'Unknown'}</code></td>`;
                html += `<td>${pkt.nodeId ? '<code>0x' + pkt.nodeId + '</code>' : '—'}</td>`;
                html += `<td>${pkt.length || 0} B</td>`;
                html += `<td><span class="${rssiClass}">${pkt.rssi || '—'} dBm</span></td>`;
                html += `<td>${(pkt.frequencyMHz || 0).toFixed(3)} MHz</td>`;
                html += `<td>${pkt.decryptedText || '—'}</td>`;
                html += `<td><button data-action="replay-packet" data-value="${idx}" class="btn btn-primary btn-small">🔁 Replay</button></td>`;
                html += '</tr>';
            });
            
            html += '</tbody></table></div>';
            this.el.packetsContent.innerHTML = html;
        } catch (error) {
            console.error('Failed to load packets:', error);
            this.el.packetsContent.innerHTML = '<div class="error-state"><p class="error">Failed to load packets</p><button data-action="retry-packets" class="btn btn-primary">Retry</button></div>';
        }
    }

    async showFrequency() {
        try {
            // Get activity data - now includes ALL configs with their names
            const activityData = await this.get('/api/activity');
            
            if (!activityData || !activityData.activities || activityData.activities.length === 0) {
                this.el.frequencyContent.innerHTML = '<p class="placeholder">Frequency configurations loading...</p>';
                return;
            }
            
            const allConfigs = activityData.activities;
            const totalConfigs = activityData.totalConfigs || allConfigs.length;
            
            let html = '<div class="frequency-intro">';
            html += `<p><strong>${totalConfigs} frequency configurations available.</strong> Target any config to focus packet capture on that frequency.</p>`;
            html += '</div>';
            
            html += '<div class="table-wrapper">';
            html += '<table class="table freq-table"><thead><tr>';
            html += '<th>Protocol</th><th>Frequency</th><th>SF</th><th>BW</th><th>Packets</th><th>RSSI</th><th>Actions</th>';
            html += '</tr></thead><tbody>';
            
            allConfigs.forEach(act => {
                const isActive = act.packets > 0;
                const rssiClass = act.avgRSSI > -70 ? 'rssi-strong' : act.avgRSSI > -90 ? 'rssi-medium' : 'rssi-weak';
                const rowClass = isActive ? '' : 'inactive-row';
                html += `<tr class="${rowClass}">`;
                html += `<td><strong>${act.protocol}</strong> <span class="badge config-badge">#${act.configIndex}</span></td>`;
                html += `<td>${act.frequencyMHz.toFixed(3)} MHz</td>`;
                html += `<td>SF${act.spreadingFactor}</td>`;
                html += `<td>${act.bandwidthKHz} kHz</td>`;
                if (isActive) {
                    html += `<td>${act.packets}</td>`;
                    html += `<td><span class="${rssiClass}">${act.avgRSSI} dBm</span></td>`;
                } else {
                    html += '<td class="text-muted">—</td>';
                    html += '<td class="text-muted">—</td>';
                }
                html += `<td><button data-action="target-frequency" data-value="${act.configIndex}" class="btn btn-primary btn-small">🎯 Target</button></td>`;
                html += '</tr>';
            });
            
            html += '</tbody></table></div>';
            this.el.frequencyContent.innerHTML = html;
        } catch (error) {
            console.error('Failed to load frequency data:', error);
            this.el.frequencyContent.innerHTML = '<div class="error-state"><p class="error">Failed to load frequency data</p><button data-action="retry-frequency" class="btn btn-primary">Retry</button></div>';
        }
    }

    async showNetwork() {
        if (!this.networkMap && typeof NetworkMap !== 'undefined') {
            this.networkMap = new NetworkMap('network-canvas', 'node-details');
        }
        
        try {
            const data = await this.get('/api/devices');
            if (data && data.devices && this.networkMap) {
                this.networkMap.updateDevices(data.devices);
            }
        } catch (error) {
            console.error('Failed to update network map:', error);
        }
    }

    async showStats() {
        if (!this.warRoom && typeof WarRoom !== 'undefined') {
            this.warRoom = new WarRoom('war-room-container');
        }
        
        try {
            const data = await this.get('/api/status');
            if (data && this.warRoom) {
                this.warRoom.update(data);
            }
        } catch (error) {
            console.error('Failed to update stats:', error);
        }
    }

    async showInfo() {
        // Status already updated via handleStatusUpdate
        
        // Load GPS data from /api/positions
        try {
            const gpsData = await this.get('/api/positions');
            if (gpsData && gpsData.positions && gpsData.positions.length > 0) {
                let html = '<div class="table-wrapper"><table class="table"><thead><tr>';
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
                this.el.gpsContent.innerHTML = html;
            } else {
                this.el.gpsContent.innerHTML = '<p class="placeholder">No GPS data captured yet. Device positions will appear here once discovered.</p>';
            }
        } catch (error) {
            console.error('Failed to load GPS:', error);
            this.el.gpsContent.innerHTML = '<p class="placeholder">No GPS data available</p>';
        }
        
        // Load security analysis from /api/statistics
        try {
            const statsData = await this.get('/api/statistics');
            if (statsData && statsData.statistics) {
                const stats = statsData.statistics;
                let html = '<div class="security-summary">';
                html += `<p><strong>Total Packets Received:</strong> ${stats.totalPackets || 0}</p>`;
                html += `<p><strong>Devices Discovered:</strong> ${stats.totalDevices || 0}</p>`;
                
                // Protocol distribution
                if (stats.protocolDistribution) {
                    html += '<p><strong>Protocol Distribution:</strong></p><ul>';
                    const protocols = stats.protocolDistribution;
                    if (protocols.Meshtastic) html += `<li>Meshtastic: ${protocols.Meshtastic}</li>`;
                    if (protocols.LoRaWAN) html += `<li>LoRaWAN: ${protocols.LoRaWAN}</li>`;
                    if (protocols.Helium) html += `<li>Helium: ${protocols.Helium}</li>`;
                    if (protocols.Other) html += `<li>Other: ${protocols.Other}</li>`;
                    html += '</ul>';
                }
                
                html += '</div>';
                this.el.securityContent.innerHTML = html;
            } else {
                this.el.securityContent.innerHTML = '<p class="placeholder">No analysis available yet. Statistics will appear as packets are captured.</p>';
            }
        } catch (error) {
            console.error('Failed to load security data:', error);
            this.el.securityContent.innerHTML = '<p class="placeholder">Analysis will appear once packets are captured</p>';
        }
    }

    // ============ Button Handlers ============
    
    setupButtons() {
        document.addEventListener('click', (e) => {
            const btn = e.target.closest('button[data-action]');
            if (!btn) return;
            
            e.preventDefault();
            const action = btn.dataset.action;
            const value = btn.dataset.value;
            this.handleAction(action, value);
        });
    }

    async handleAction(action, value) {
        try {
            switch(action) {
                case 'resume-recon':
                    await this.post('/api/scan/start', {});
                    showToast('Reconnaissance resumed', 'success');
                    break;
                case 'stop-capture':
                    await this.post('/api/capture/stop', {});
                    showToast('Capture stopped', 'success');
                    break;
                case 'export-kml':
                    window.open('/api/kml', '_blank');
                    break;
                case 'target-device':
                    await this.post('/api/capture/start', { nodeId: value });
                    showToast(`Targeting device 0x${value}`, 'success');
                    await this.updateStatus();
                    break;
                case 'replay-packet':
                    await this.post('/api/replay', { slot: value });
                    showToast(`Replaying packet ${parseInt(value) + 1}`, 'success');
                    break;
                case 'target-frequency':
                    await this.post('/api/frequency/target', { configIndex: value });
                    showToast(`Targeting frequency config ${value}`, 'success');
                    await this.updateStatus();
                    break;
                case 'retry-devices':
                    await this.showDevices();
                    break;
                case 'retry-packets':
                    await this.showPackets();
                    break;
                case 'retry-frequency':
                    await this.showFrequency();
                    break;
                case 'retry-gps':
                case 'retry-security':
                    await this.showInfo();
                    break;
            }
        } catch (error) {
            console.error('Action failed:', error);
            showToast('Action failed: ' + error.message, 'error');
        }
    }

    // ============ Mobile Menu ============
    
    setupMobileMenu() {
        if (this.el.mobileMenuToggle) {
            this.el.mobileMenuToggle.addEventListener('click', () => {
                const actionsSection = document.querySelector('.actions-section');
                if (actionsSection) {
                    actionsSection.classList.toggle('active');
                    this.el.mobileMenuOverlay.classList.toggle('active');
                }
            });
        }
        
        if (this.el.mobileMenuOverlay) {
            this.el.mobileMenuOverlay.addEventListener('click', () => {
                const actionsSection = document.querySelector('.actions-section');
                if (actionsSection) {
                    actionsSection.classList.remove('active');
                    this.el.mobileMenuOverlay.classList.remove('active');
                }
            });
        }
    }

    // ============ API Helpers ============
    
    async get(endpoint) {
        const response = await fetch(endpoint);
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        return await response.json();
    }

    async post(endpoint, body = {}) {
        const formData = new URLSearchParams();
        Object.entries(body).forEach(([key, value]) => {
            formData.append(key, value);
        });

        const response = await fetch(endpoint, {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: formData.toString()
        });

        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        return await response.json();
    }

    // ============ Formatters ============
    
    formatMode(mode) {
        if (!mode) return 'Unknown';
        return mode.toString()
            .replace(/_/g, ' ')
            .split(' ')
            .map(w => w.charAt(0).toUpperCase() + w.slice(1).toLowerCase())
            .join(' ');
    }

    formatDuration(seconds) {
        if (!seconds) return '0s';
        const h = Math.floor(seconds / 3600);
        const m = Math.floor((seconds % 3600) / 60);
        const s = Math.floor(seconds % 60);
        const parts = [];
        if (h > 0) parts.push(`${h}h`);
        if (m > 0) parts.push(`${m}m`);
        parts.push(`${s}s`);
        return parts.join(' ');
    }

    formatBytes(bytes) {
        if (bytes < 1024) return bytes + ' B';
        if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
        return (bytes / 1048576).toFixed(1) + ' MB';
    }
}

// Initialize app
let app;
document.addEventListener('DOMContentLoaded', () => {
    app = new ReconApp();
    window.app = app;
});
