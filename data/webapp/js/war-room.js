/* =================================================================
   War Room Dashboard - Tactical reconnaissance overview
   Real-time operational metrics and activity monitoring
   ================================================================= */

class WarRoom {
    constructor(containerId) {
        this.container = document.getElementById(containerId);
        this.metrics = {
            activeDevices: 0,
            totalPackets: 0,
            packetRate: 0,
            avgRSSI: 0,
            scanProgress: 0,
            currentFreq: '—',
            uptime: 0
        };
        this.frequencyActivity = new Array(26).fill(0); // RSSI by config
        this.recentEvents = [];
        this.maxEvents = 5;
        this.lastPacketCount = 0;
        this.lastUpdateTime = Date.now();
        
        if (!this.container) {
            console.error('[WarRoom] Container not found:', containerId);
        } else {
            console.log('[WarRoom] Initialized');
            this.render();
        }
    }
    
    update(data) {
        console.log('[WarRoom] Updating with data:', data);
        
        // Update metrics from status data
        if (data.devices !== undefined) this.metrics.activeDevices = data.devices;
        if (data.totalPackets !== undefined) {
            const now = Date.now();
            const timeDiff = (now - this.lastUpdateTime) / 1000; // seconds
            if (timeDiff > 0 && this.lastPacketCount > 0) {
                const packetDiff = data.totalPackets - this.lastPacketCount;
                this.metrics.packetRate = Math.round((packetDiff / timeDiff) * 60); // packets per minute
            }
            this.lastPacketCount = data.totalPackets;
            this.lastUpdateTime = now;
            this.metrics.totalPackets = data.totalPackets;
        }
        if (data.uptime !== undefined) this.metrics.uptime = data.uptime;
        
        // Check if in reconnaissance or targeted mode
        const mode = (data.mode || '').toLowerCase();
        const isTargeted = mode.includes('target');
        
        if (isTargeted && data.target) {
            // In targeted mode - show target info
            this.metrics.scanProgress = 100; // Always 100% when targeting
            const freq = data.target.frequency ? `${data.target.frequency} MHz` : 'Unknown';
            this.updateCurrentFrequency(`Targeting: ${freq}`);
            
            if (this.recentEvents.length === 0) {
                this.addEvent('mode', `Targeting device ${data.target.nodeId || 'Unknown'}`);
            }
        } else {
            // In reconnaissance mode - show scan progress
            if (data.scan && data.scan.currentConfig !== undefined && data.scan.totalConfigs !== undefined) {
                this.metrics.scanProgress = Math.round((data.scan.currentConfig / data.scan.totalConfigs) * 100);
                this.updateCurrentFrequency(`Config ${data.scan.currentConfig + 1}/${data.scan.totalConfigs}`);
            }
            
            if (this.recentEvents.length === 0) {
                this.addEvent('scan', `Scanning in ${mode} mode`);
            }
        }
        
        this.render();
    }
    
    updateFrequencyActivity(configIndex, rssi) {
        if (configIndex >= 0 && configIndex < this.frequencyActivity.length) {
            // Decay old values and update current
            this.frequencyActivity = this.frequencyActivity.map((val, idx) => {
                if (idx === configIndex) return rssi;
                return val * 0.95; // Decay
            });
        }
    }
    
    updateCurrentFrequency(freq) {
        this.metrics.currentFreq = freq;
    }
    
    updateAvgRSSI(rssi) {
        // Exponential moving average
        this.metrics.avgRSSI = this.metrics.avgRSSI === 0 ? rssi : (this.metrics.avgRSSI * 0.8 + rssi * 0.2);
    }
    
    addEvent(type, message) {
        this.recentEvents.unshift({
            type: type,
            message: message,
            timestamp: new Date().toLocaleTimeString()
        });
        
        if (this.recentEvents.length > this.maxEvents) {
            this.recentEvents = this.recentEvents.slice(0, this.maxEvents);
        }
        
        this.render();
    }
    
    render() {
        if (!this.container) return;
        
        // Check if in targeting mode
        const isTargeted = this.metrics.currentFreq.includes('Targeting');
        
        const html = `
            <div class="war-room-dashboard">
                ${isTargeted ? '<div class="war-room-alert">🎯 In Targeting Mode <button class="btn btn-primary btn-sm" onclick="app.post(\'/api/scan/start\').then(() => location.reload())">↩️ Resume Recon</button></div>' : ''}
                
                <div class="war-room-metrics">
                    <div class="war-metric-card">
                        <div class="war-metric-label">Active Devices</div>
                        <div class="war-metric-value">${this.metrics.activeDevices}</div>
                    </div>
                    <div class="war-metric-card">
                        <div class="war-metric-label">Packet Rate</div>
                        <div class="war-metric-value">${this.metrics.packetRate}<span class="war-metric-unit">/min</span></div>
                    </div>
                    <div class="war-metric-card">
                        <div class="war-metric-label">Avg RSSI</div>
                        <div class="war-metric-value">${this.metrics.avgRSSI.toFixed(1)}<span class="war-metric-unit">dBm</span></div>
                    </div>
                    <div class="war-metric-card">
                        <div class="war-metric-label">${isTargeted ? 'Lock Status' : 'Scan Progress'}</div>
                        <div class="war-metric-value">${this.metrics.scanProgress}<span class="war-metric-unit">%</span></div>
                    </div>
                </div>
                
                <div class="war-room-frequency">
                    <h4>📡 ${isTargeted ? 'Target Lock' : 'Scan Progress'}</h4>
                    <div class="frequency-bars">
                        ${this.renderFrequencyBars()}
                    </div>
                    <div class="frequency-legend">
                        <span class="freq-current">Current: ${this.metrics.currentFreq}</span>
                    </div>
                </div>
                
                <div class="war-room-events">
                    <h4>⚡ Recent Activity</h4>
                    <div class="event-list">
                        ${this.renderEvents()}
                    </div>
                </div>
            </div>
        `;
        
        this.container.innerHTML = html;
    }
    
    renderFrequencyBars() {
        // Simplified: Show a progress indicator instead of per-frequency bars
        // since WebSocket doesn't provide per-config RSSI data
        const progress = this.metrics.scanProgress || 0;
        return `
            <div class="scan-progress-bar">
                <div class="scan-progress-fill" style="width: ${progress}%"></div>
            </div>
            <div class="scan-progress-text">Scan cycle: ${progress}% complete</div>
        `;
    }
    
    getRSSIColor(rssi) {
        if (rssi >= -70) return '#10b981'; // Excellent (green)
        if (rssi >= -90) return '#f59e0b'; // Good (orange)
        if (rssi >= -110) return '#ef4444'; // Fair (red)
        return '#6b7280'; // Poor (gray)
    }
    
    renderEvents() {
        if (this.recentEvents.length === 0) {
            return '<div class="event-empty">Waiting for activity...</div>';
        }
        
        return this.recentEvents.map(event => {
            const icon = this.getEventIcon(event.type);
            return `
                <div class="event-item event-${event.type}">
                    <span class="event-icon">${icon}</span>
                    <span class="event-message">${event.message}</span>
                    <span class="event-time">${event.timestamp}</span>
                </div>
            `;
        }).join('');
    }
    
    getEventIcon(type) {
        const icons = {
            'device': '🔍',
            'packet': '📦',
            'capture': '✅',
            'mode': '🔄',
            'scan': '📡'
        };
        return icons[type] || '•';
    }
    
    formatUptime(seconds) {
        const h = Math.floor(seconds / 3600);
        const m = Math.floor((seconds % 3600) / 60);
        const s = seconds % 60;
        return h > 0 ? `${h}h ${m}m` : m > 0 ? `${m}m ${s}s` : `${s}s`;
    }
}

// Export for use in app.js
window.WarRoom = WarRoom;
