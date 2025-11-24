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
            
            // Load first tab
            this.loadTabContent('devices');
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
            if (!data || !data.devices) {
                this.el.devicesContent.innerHTML = '<p class="placeholder">No devices found</p>';
                return;
            }
            
            let html = '<table class="data-table"><thead><tr>';
            html += '<th>Node ID</th><th>Type</th><th>Packets</th><th>RSSI</th><th>Last Seen</th><th>Actions</th>';
            html += '</tr></thead><tbody>';
            
            data.devices.forEach(device => {
                html += '<tr>';
                html += `<td><code>0x${device.nodeId}</code></td>`;
                html += `<td>${device.deviceType || 'Unknown'}</td>`;
                html += `<td>${device.packetCount || 0}</td>`;
                html += `<td>${device.bestRSSI || '—'} dBm</td>`;
                html += `<td>${this.formatDuration((Date.now() - device.lastSeen) / 1000)} ago</td>`;
                html += `<td><button data-action="target-device" data-value="${device.nodeId}" class="btn-small">Target</button></td>`;
                html += '</tr>';
            });
            
            html += '</tbody></table>';
            this.el.devicesContent.innerHTML = html;
        } catch (error) {
            console.error('Failed to load devices:', error);
            this.el.devicesContent.innerHTML = '<p class="error">Failed to load devices</p>';
        }
    }

    async showPackets() {
        try {
            const data = await this.get('/api/packets');
            if (!data || !data.packets || data.packets.length === 0) {
                this.el.packetsContent.innerHTML = '<p class="placeholder">No packets captured</p>';
                return;
            }
            
            let html = '<table class="data-table"><thead><tr>';
            html += '<th>#</th><th>From</th><th>To</th><th>Size</th><th>RSSI</th><th>SNR</th><th>Actions</th>';
            html += '</tr></thead><tbody>';
            
            data.packets.forEach((pkt, idx) => {
                html += '<tr>';
                html += `<td>${idx + 1}</td>`;
                html += `<td><code>0x${pkt.from || '?'}</code></td>`;
                html += `<td><code>0x${pkt.to || '?'}</code></td>`;
                html += `<td>${pkt.size || 0} B</td>`;
                html += `<td>${pkt.rssi || '—'} dBm</td>`;
                html += `<td>${pkt.snr || '—'}</td>`;
                html += `<td><button data-action="replay-packet" data-value="${idx}" class="btn-small">Replay</button></td>`;
                html += '</tr>';
            });
            
            html += '</tbody></table>';
            this.el.packetsContent.innerHTML = html;
        } catch (error) {
            console.error('Failed to load packets:', error);
            this.el.packetsContent.innerHTML = '<p class="error">Failed to load packets</p>';
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
        
        // Load GPS data
        try {
            const gpsData = await this.get('/api/gps');
            if (gpsData && gpsData.locations && gpsData.locations.length > 0) {
                let html = '<table class="data-table"><thead><tr>';
                html += '<th>Node ID</th><th>Latitude</th><th>Longitude</th><th>Altitude</th>';
                html += '</tr></thead><tbody>';
                
                gpsData.locations.forEach(loc => {
                    html += '<tr>';
                    html += `<td><code>0x${loc.nodeId}</code></td>`;
                    html += `<td>${loc.latitude.toFixed(6)}</td>`;
                    html += `<td>${loc.longitude.toFixed(6)}</td>`;
                    html += `<td>${loc.altitude || '—'} m</td>`;
                    html += '</tr>';
                });
                
                html += '</tbody></table>';
                this.el.gpsContent.innerHTML = html;
            } else {
                this.el.gpsContent.innerHTML = '<p class="placeholder">No GPS data available</p>';
            }
        } catch (error) {
            console.error('Failed to load GPS:', error);
        }
        
        // Load frequency analysis
        try {
            const freqData = await this.get('/api/activity');
            if (freqData && freqData.activities && freqData.activities.length > 0) {
                let html = '<table class="data-table"><thead><tr>';
                html += '<th>Frequency</th><th>Protocol</th><th>Packets</th><th>Avg RSSI</th>';
                html += '</tr></thead><tbody>';
                
                freqData.activities.forEach(act => {
                    html += '<tr>';
                    html += `<td>${act.frequency.toFixed(3)} MHz</td>`;
                    html += `<td>${act.protocol}</td>`;
                    html += `<td>${act.packets}</td>`;
                    html += `<td>${act.avgRSSI} dBm</td>`;
                    html += '</tr>';
                });
                
                html += '</tbody></table>';
                this.el.frequencyContent.innerHTML = html;
            }
        } catch (error) {
            console.error('Failed to load frequency data:', error);
        }
        
        // Load security analysis
        try {
            const secData = await this.get('/api/security');
            if (secData && secData.summary) {
                let html = '<div class="security-summary">';
                html += `<p><strong>Encrypted Packets:</strong> ${secData.summary.encryptedPackets || 0}</p>`;
                html += `<p><strong>Unencrypted Packets:</strong> ${secData.summary.unencryptedPackets || 0}</p>`;
                html += `<p><strong>Default Keys Detected:</strong> ${secData.summary.defaultKeys || 0}</p>`;
                html += '</div>';
                this.el.securityContent.innerHTML = html;
            }
        } catch (error) {
            console.error('Failed to load security data:', error);
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
                    break;
                case 'replay-packet':
                    await this.post('/api/replay', { slot: value });
                    showToast(`Replaying packet ${value}`, 'success');
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
                document.querySelector('.sidebar').classList.toggle('show');
                this.el.mobileMenuOverlay.classList.toggle('show');
            });
        }
        
        if (this.el.mobileMenuOverlay) {
            this.el.mobileMenuOverlay.addEventListener('click', () => {
                document.querySelector('.sidebar').classList.remove('show');
                this.el.mobileMenuOverlay.classList.remove('show');
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
