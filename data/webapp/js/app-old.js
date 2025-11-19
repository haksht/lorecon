/* =================================================================
   ESP32 LoRa Recon - Simple REST-only Client
   Mirrors the serial menu commands exactly
   ================================================================= */

class ReconApp {
    constructor() {
        // DOM element references
        this.el = {
            connection: document.getElementById('connection'),
            connectionText: document.getElementById('connection-text'),
            mode: document.getElementById('mode'),
            devices: document.getElementById('devices'),
            packets: document.getElementById('packets'),
            uptime: document.getElementById('uptime'),
            results: document.getElementById('results'),
            resultsTitle: document.getElementById('results-title'),
            toast: document.getElementById('toast')
        };

        // App state
        this.lastCommand = null;
        this.statusTimer = null;
        this.isQuietMode = false;

        // Start the app
        this.init();
    }

    async init() {
        await this.updateStatus();
        this.statusTimer = setInterval(() => this.updateStatus(), 5000);
        this.setConnected(true);
    }

    // ================================================================
    // Status Updates
    // ================================================================

    async updateStatus() {
        try {
            const data = await this.get('/api/status');
            if (data.status === 'success') {
                this.el.mode.textContent = this.formatMode(data.mode);
                this.el.devices.textContent = data.devices || 0;
                this.el.packets.textContent = data.totalPackets || 0;
                this.el.uptime.textContent = this.formatDuration(data.uptime);
                this.setConnected(true);
            }
        } catch (error) {
            this.setConnected(false);
        }
    }

    setConnected(connected) {
        const dot = this.el.connection.querySelector('.dot');
        if (dot) {
            if (connected) {
                dot.classList.add('connected');
                this.el.connectionText.textContent = 'Connected';
            } else {
                dot.classList.remove('connected');
                this.el.connectionText.textContent = 'Disconnected';
            }
        }
    }

    // ================================================================
    // Menu Commands (mirrors serial menu)
    // ================================================================

    async showMenu() {
        try {
            const data = await this.get('/api/recon/summary');
            if (data.status !== 'success') {
                this.showResults('Menu', '<p>No data available yet. Run reconnaissance first.</p>');
                return;
            }

            const summary = data.summary || {};
            let html = '<h3>Reconnaissance Complete</h3>';
            html += `<p><strong>Duration:</strong> ${this.formatDuration(summary.reconDurationSeconds)}</p>`;
            html += `<p><strong>Targetable Devices:</strong> ${summary.targetableDevices || 0}</p>`;
            html += `<p><strong>Nodes Tracked:</strong> ${summary.nodesTracked || 0}</p>`;

            // Show devices
            const devices = await this.get('/api/devices');
            if (devices.status === 'success' && devices.devices && devices.devices.length > 0) {
                html += '<hr><h3>Targetable Devices</h3>';
                html += '<table class="table"><thead><tr>';
                html += '<th>Node ID</th><th>Protocol</th><th>RSSI</th><th>Packets</th><th>Action</th>';
                html += '</tr></thead><tbody>';
                
                devices.devices.forEach((dev, idx) => {
                    const nodeHex = dev.nodeId || (dev.nodeIdDecimal ? dev.nodeIdDecimal.toString(16).toUpperCase() : '');
                    html += '<tr>';
                    html += `<td>0x${nodeHex}</td>`;
                    html += `<td>${dev.protocol || 'Unknown'}</td>`;
                    html += `<td>${dev.rssi ? dev.rssi.toFixed(1) : '—'} dBm</td>`;
                    html += `<td>${dev.packetCount || 0}</td>`;
                    html += `<td><button class="btn btn-primary" onclick="app.targetDevice('${nodeHex}')">Target</button></td>`;
                    html += '</tr>';
                });
                
                html += '</tbody></table>';
            } else {
                html += '<p><em>No devices discovered yet. Wait for reconnaissance to find active nodes.</em></p>';
            }

            this.showResults('Menu', html);
        } catch (error) {
            this.showError('Failed to load menu');
        }
    }

    async targetDevice(nodeId) {
        try {
            const result = await this.post('/api/capture/start', { nodeId });
            if (result.status === 'success') {
                this.showToast('Targeted capture started on 0x' + nodeId);
                await this.updateStatus();
            } else {
                this.showToast(result.error || 'Failed to start capture', 'error');
            }
        } catch (error) {
            this.showToast('Failed to start capture', 'error');
        }
    }

    async resumeRecon() {
        try {
            const result = await this.post('/api/scan/start');
            if (result.status === 'success') {
                this.showToast('Reconnaissance resumed');
                await this.updateStatus();
            } else {
                this.showToast(result.error || 'Failed to resume', 'error');
            }
        } catch (error) {
            this.showToast('Failed to resume reconnaissance', 'error');
        }
    }

    async stopCapture() {
        try {
            const result = await this.post('/api/capture/stop');
            if (result.status === 'success') {
                this.showToast('Capture stopped');
                await this.updateStatus();
            } else {
                this.showToast(result.error || 'Failed to stop', 'error');
            }
        } catch (error) {
            this.showToast('Failed to stop capture', 'error');
        }
    }

    async showActivity() {
        try {
            const data = await this.get('/api/activity');
            if (data.status !== 'success' || !data.activities) {
                this.showResults('RF Activity', '<p>No activity data available yet.</p>');
                return;
            }

            let html = '<h3>RF Activity by Configuration</h3>';
            html += '<table class="table"><thead><tr>';
            html += '<th>#</th><th>Protocol</th><th>Frequency</th><th>Packets</th><th>Avg RSSI</th><th>Peak RSSI</th>';
            html += '</tr></thead><tbody>';

            data.activities.forEach(activity => {
                const idx = (activity.configIndex || 0) + 1;
                html += '<tr>';
                html += `<td>${idx}</td>`;
                html += `<td>${activity.protocol || 'Unknown'}</td>`;
                html += `<td>${this.formatFreq(activity.frequencyMHz)}</td>`;
                html += `<td>${activity.packets || 0}</td>`;
                html += `<td>${this.formatSignal(activity.avgRSSI)}</td>`;
                html += `<td>${this.formatSignal(activity.peakRSSI)}</td>`;
                html += '</tr>';
            });

            html += '</tbody></table>';
            this.showResults('RF Activity', html);
        } catch (error) {
            this.showError('Failed to load activity data');
        }
    }

    async showDeviceTypes() {
        try {
            const data = await this.get('/api/recon/device-types');
            if (data.status !== 'success' || !data.deviceTypes) {
                this.showResults('Device Types', '<p>No device type data available yet.</p>');
                return;
            }

            let html = '<h3>Device Type Analysis</h3>';
            html += `<p><strong>Total Device Types:</strong> ${data.summary?.totalDeviceTypes || 0}</p>`;
            html += `<p><strong>Total Devices:</strong> ${data.summary?.totalDevices || 0}</p>`;
            html += `<p><strong>Routers Detected:</strong> ${data.summary?.routersDetected || 0}</p>`;

            html += '<hr><table class="table"><thead><tr>';
            html += '<th>Type</th><th>Count</th><th>Avg RSSI</th><th>Routers</th>';
            html += '</tr></thead><tbody>';

            data.deviceTypes.forEach(type => {
                html += '<tr>';
                html += `<td>${type.type}</td>`;
                html += `<td>${type.count}</td>`;
                html += `<td>${this.formatSignal(type.avgRSSI)}</td>`;
                html += `<td>${type.routers || 0}</td>`;
                html += '</tr>';
            });

            html += '</tbody></table>';
            this.showResults('Device Types', html);
        } catch (error) {
            this.showError('Failed to load device types');
        }
    }

    async showFrequencyMenu() {
        try {
            const data = await this.get('/api/activity');
            if (data.status !== 'success' || !data.activities) {
                this.showResults('Frequency Targeting', '<p>No frequency data available yet.</p>');
                return;
            }

            let html = '<h3>Frequency Targeting</h3>';
            html += '<p>Select a configuration to target directly:</p>';
            html += '<table class="table"><thead><tr>';
            html += '<th>#</th><th>Protocol</th><th>Frequency</th><th>Activity</th><th>Action</th>';
            html += '</tr></thead><tbody>';

            data.activities.forEach(activity => {
                const idx = (activity.configIndex || 0) + 1;
                const hasActivity = activity.packets > 0 ? '🔥' : '⚪';
                html += '<tr>';
                html += `<td>${idx}</td>`;
                html += `<td>${activity.protocol || 'Unknown'}</td>`;
                html += `<td>${this.formatFreq(activity.frequencyMHz)}</td>`;
                html += `<td>${hasActivity} ${activity.packets || 0} packets</td>`;
                html += `<td><button class="btn" onclick="app.targetFrequency(${activity.configIndex})">Target</button></td>`;
                html += '</tr>';
            });

            html += '</tbody></table>';
            this.showResults('Frequency Targeting', html);
        } catch (error) {
            this.showError('Failed to load frequency data');
        }
    }

    async targetFrequency(configIndex) {
        try {
            const result = await this.post('/api/frequency/target', { configIndex });
            if (result.status === 'success') {
                this.showToast('Frequency targeting started');
                await this.updateStatus();
            } else {
                this.showToast(result.error || 'Failed to target frequency', 'error');
            }
        } catch (error) {
            this.showToast('Failed to target frequency', 'error');
        }
    }

    async showGPS() {
        try {
            const data = await this.get('/api/positions');
            if (data.status !== 'success' || !data.positions || data.positions.length === 0) {
                this.showResults('GPS Data', '<p>No GPS positions captured yet.</p>');
                return;
            }

            let html = '<h3>GPS Positions</h3>';
            html += `<p><strong>Total Positions:</strong> ${data.totalPositions || data.positions.length}</p>`;
            html += '<table class="table"><thead><tr>';
            html += '<th>Node ID</th><th>Latitude</th><th>Longitude</th><th>Altitude</th><th>Time</th>';
            html += '</tr></thead><tbody>';

            data.positions.forEach(pos => {
                const time = pos.timestamp ? new Date(pos.timestamp).toLocaleTimeString() : '—';
                html += '<tr>';
                html += `<td>${pos.nodeId || 'Unknown'}</td>`;
                html += `<td>${pos.lat ? pos.lat.toFixed(5) : '—'}</td>`;
                html += `<td>${pos.lon ? pos.lon.toFixed(5) : '—'}</td>`;
                html += `<td>${pos.alt || 0}m</td>`;
                html += `<td>${time}</td>`;
                html += '</tr>';
            });

            html += '</tbody></table>';
            this.showResults('GPS Data', html);
        } catch (error) {
            this.showError('Failed to load GPS data');
        }
    }

    async showReplayMenu() {
        try {
            const data = await this.get('/api/replay/slots');
            if (data.status !== 'success') {
                this.showResults('Replay Slots', '<p>Replay data not available.</p>');
                return;
            }

            let html = '<h3>Packet Replay Slots</h3>';
            html += `<p><strong>Captured:</strong> ${data.count || 0} / ${data.capacity || 10}</p>`;
            html += `<p><strong>Available:</strong> ${data.available || 0}</p>`;

            if (data.slots && data.slots.length > 0) {
                html += '<hr><table class="table"><thead><tr>';
                html += '<th>Slot</th><th>Protocol</th><th>Length</th><th>RSSI</th><th>Config</th>';
                html += '</tr></thead><tbody>';

                data.slots.forEach(slot => {
                    html += '<tr>';
                    html += `<td>#${slot.index}</td>`;
                    html += `<td>${slot.protocol || 'Unknown'}</td>`;
                    html += `<td>${slot.length} bytes</td>`;
                    html += `<td>${this.formatSignal(slot.rssi)}</td>`;
                    html += `<td>#${(slot.configIndex || 0) + 1}</td>`;
                    html += '</tr>';
                });

                html += '</tbody></table>';
                html += '<p><button class="btn btn-danger" onclick="app.clearReplaySlots()">Clear All Slots</button></p>';
            } else {
                html += '<p><em>No packets captured for replay yet. Press "c" during targeted capture to save packets.</em></p>';
            }

            this.showResults('Replay Slots', html);
        } catch (error) {
            this.showError('Failed to load replay slots');
        }
    }

    async clearReplaySlots() {
        if (!confirm('Clear all replay slots?')) return;
        
        try {
            const result = await this.post('/api/replay/clear');
            if (result.status === 'success') {
                this.showToast('Replay slots cleared');
                await this.showReplayMenu();
            } else {
                this.showToast(result.error || 'Failed to clear slots', 'error');
            }
        } catch (error) {
            this.showToast('Failed to clear slots', 'error');
        }
    }

    async showSecurity() {
        try {
            const data = await this.get('/api/recon/security');
            if (data.status !== 'success' || !data.summary) {
                this.showResults('Security Assessment', '<p>No security data available yet.</p>');
                return;
            }

            let html = '<h3>Security Vulnerability Assessment</h3>';
            html += `<p><strong>Status:</strong> ${data.summary.status || 'Unknown'}</p>`;
            html += `<p><strong>Vulnerable:</strong> ${data.summary.vulnerable || 0}</p>`;
            html += `<p><strong>Moderate:</strong> ${data.summary.moderate || 0}</p>`;
            html += `<p><strong>Secure:</strong> ${data.summary.secure || 0}</p>`;

            if (data.devices && data.devices.length > 0) {
                html += '<hr><table class="table"><thead><tr>';
                html += '<th>Node ID</th><th>Type</th><th>Risk Level</th><th>RSSI</th><th>Packets</th>';
                html += '</tr></thead><tbody>';

                data.devices.forEach(dev => {
                    const nodeId = dev.nodeId ? `0x${dev.nodeId}` : dev.nodeIdDecimal || 'Unknown';
                    html += '<tr>';
                    html += `<td>${nodeId}</td>`;
                    html += `<td>${dev.deviceType || 'Unknown'}</td>`;
                    html += `<td>${dev.riskLevel || 'unknown'}</td>`;
                    html += `<td>${this.formatSignal(dev.bestRSSI)}</td>`;
                    html += `<td>${dev.packetCount || 0}</td>`;
                    html += '</tr>';
                });

                html += '</tbody></table>';
            }

            this.showResults('Security Assessment', html);
        } catch (error) {
            this.showError('Failed to load security data');
        }
    }

    async toggleQuiet() {
        try {
            const data = await this.get('/api/diagnostics');
            const currentMode = data.diagnostics?.verboseMode || false;
            const newMode = !currentMode;

            const result = await this.post('/api/diagnostics/verbose', { enable: newMode ? 'true' : 'false' });
            if (result.status === 'success') {
                this.isQuietMode = !newMode;
                this.showToast(newMode ? 'Verbose mode enabled' : 'Quiet mode enabled');
            } else {
                this.showToast(result.error || 'Failed to toggle mode', 'error');
            }
        } catch (error) {
            this.showToast('Failed to toggle verbose mode', 'error');
        }
    }

    async showDiagnostics() {
        try {
            const data = await this.get('/api/diagnostics');
            if (data.status !== 'success' || !data.diagnostics) {
                this.showResults('Diagnostics', '<p>No diagnostic data available yet.</p>');
                return;
            }

            const diag = data.diagnostics;
            let html = '<h3>Diagnostic Report</h3>';
            html += `<p><strong>Verbose Mode:</strong> ${diag.verboseMode ? 'Enabled' : 'Quiet'}</p>`;
            html += `<p><strong>Gap Count:</strong> ${diag.gapCount || 0}</p>`;
            html += `<p><strong>Max Gap:</strong> ${diag.maxGapMs || 0} ms</p>`;
            html += `<p><strong>Large Gaps:</strong> ${diag.largeGapCount || 0}</p>`;

            if (diag.encryption) {
                html += '<hr><h4>Encryption Stats</h4>';
                html += `<p><strong>Encrypted:</strong> ${diag.encryption.encryptedPackets || 0}</p>`;
                html += `<p><strong>Plaintext:</strong> ${diag.encryption.plaintextPackets || 0}</p>`;
                html += `<p><strong>Unknown:</strong> ${diag.encryption.unknownPackets || 0}</p>`;
            }

            this.showResults('Diagnostics', html);
        } catch (error) {
            this.showError('Failed to load diagnostics');
        }
    }

    async exportKML() {
        try {
            const response = await fetch('/api/export/kml');
            const kml = await response.text();
            
            const blob = new Blob([kml], { type: 'application/vnd.google-earth.kml+xml' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = 'lora-positions.kml';
            a.click();
            URL.revokeObjectURL(url);
            
            this.showToast('KML file downloaded');
        } catch (error) {
            this.showToast('Failed to export KML', 'error');
        }
    }

    async exportGeoJSON() {
        try {
            const response = await fetch('/api/export/geojson');
            const geojson = await response.text();
            
            const blob = new Blob([geojson], { type: 'application/json' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = 'lora-positions.geojson';
            a.click();
            URL.revokeObjectURL(url);
            
            this.showToast('GeoJSON file downloaded');
        } catch (error) {
            this.showToast('Failed to export GeoJSON', 'error');
        }
    }

    async rebootConfirm() {
        if (!confirm('⚠️ Reboot device? This will clear ALL discovered devices, nodes, and replay slots!')) {
            return;
        }

        this.showToast('Rebooting device...', 'warning');
        
        try {
            await this.post('/api/system/reboot');
            setTimeout(() => {
                this.showToast('Device rebooted. Reconnecting...', 'success');
                setTimeout(() => window.location.reload(), 5000);
            }, 2000);
        } catch (error) {
            this.showToast('Reboot command sent (device may be restarting)', 'warning');
        }
    }

    refreshResults() {
        if (this.lastCommand) {
            this.lastCommand();
        }
    }

    // ================================================================
    // Utility Functions
    // ================================================================

    showResults(title, html) {
        this.el.resultsTitle.textContent = title;
        this.el.results.innerHTML = html;
        this.lastCommand = () => this.showResults(title, html);
    }

    showError(message) {
        this.showResults('Error', `<p class="error">${message}</p>`);
    }

    showToast(message, type = 'success') {
        this.el.toast.textContent = message;
        this.el.toast.className = `toast ${type}`;
        this.el.toast.style.display = 'block';
        
        setTimeout(() => {
            this.el.toast.style.display = 'none';
        }, 3000);
    }

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

    formatSignal(rssi) {
        if (rssi == null) return '—';
        return `${rssi.toFixed(1)} dBm`;
    }

    formatFreq(freq) {
        if (!freq) return '—';
        return `${freq.toFixed(3)} MHz`;
    }

    // ================================================================
    // API Methods
    // ================================================================

    async get(endpoint) {
        const response = await fetch(endpoint);
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

        return await response.json();
    }
}

// Initialize app when DOM is ready
let app;
document.addEventListener('DOMContentLoaded', () => {
    app = new ReconApp();
});
