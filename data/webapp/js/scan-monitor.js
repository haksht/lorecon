/* =================================================================
   Scan Monitor - Real-time frequency scanning visualization
   Shows current scan progress and signal strength heatmap
   ================================================================= */

class ScanMonitor {
    constructor(containerId) {
        this.container = document.getElementById(containerId);
        this.configs = [];
        this.signalData = {}; // configIndex -> {rssi, count, lastSeen}
        
        if (!this.container) {
            console.error('[ScanMonitor] Container not found:', containerId);
            return;
        }
        
        this.render();
    }
    
    update(statusData) {
        if (!statusData || !this.container) return;
        
        const currentConfig = statusData.scan?.currentConfig || 0;
        const totalConfigs = statusData.scan?.totalConfigs || 26;
        const frequency = statusData.scan?.frequency || 0;
        const protocol = statusData.scan?.protocol || 'Unknown';
        const mode = statusData.mode || 'reconnaissance';
        
        // Update signal data if we have RSSI info
        if (statusData.lastRSSI && statusData.lastRSSI !== 0) {
            if (!this.signalData[currentConfig]) {
                this.signalData[currentConfig] = { rssi: statusData.lastRSSI, count: 1, lastSeen: Date.now() };
            } else {
                const data = this.signalData[currentConfig];
                data.rssi = ((data.rssi * data.count) + statusData.lastRSSI) / (data.count + 1);
                data.count++;
                data.lastSeen = Date.now();
            }
        }
        
        this.render(currentConfig, totalConfigs, frequency, protocol, mode);
    }
    
    render(currentConfig = 0, totalConfigs = 26, frequency = 0, protocol = 'Unknown', mode = 'reconnaissance') {
        if (!this.container) return;
        
        const isScanning = mode === 'reconnaissance';
        const progress = totalConfigs > 0 ? ((currentConfig + 1) / totalConfigs * 100) : 0;
        
        let html = `
            <div class="scan-monitor">
                <div class="scan-status-card">
                    <h4>Current Scan</h4>
                    <div class="scan-current-info">
                        <div class="scan-info-row">
                            <span class="label">Mode:</span>
                            <span class="value ${isScanning ? 'scanning' : 'paused'}">${isScanning ? '🔄 Scanning' : '⏸️ Paused'}</span>
                        </div>
                        <div class="scan-info-row">
                            <span class="label">Config:</span>
                            <span class="value">${currentConfig + 1} / ${totalConfigs}</span>
                        </div>
                        <div class="scan-info-row">
                            <span class="label">Frequency:</span>
                            <span class="value">${frequency.toFixed(3)} MHz</span>
                        </div>
                        <div class="scan-info-row">
                            <span class="label">Protocol:</span>
                            <span class="value">${protocol}</span>
                        </div>
                    </div>
                    <div class="scan-progress-bar">
                        <div class="scan-progress-fill" style="width: ${progress}%"></div>
                    </div>
                    <div class="scan-progress-text">${progress.toFixed(1)}% complete</div>
                </div>
                
                <div class="scan-heatmap-card">
                    <h4>Signal Strength Heatmap</h4>
                    <div class="scan-heatmap">
                        ${this.renderHeatmap(totalConfigs, currentConfig)}
                    </div>
                    <div class="scan-legend">
                        <span>🟢 Strong</span>
                        <span>🟡 Medium</span>
                        <span>🟠 Weak</span>
                        <span>⚪ No Data</span>
                    </div>
                </div>
                
                <div class="scan-stats-card">
                    <h4>Scan Statistics</h4>
                    <div class="scan-stats-grid">
                        <div class="stat-item">
                            <div class="stat-value">${Object.keys(this.signalData).length}</div>
                            <div class="stat-label">Active Frequencies</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-value">${this.getTotalPackets()}</div>
                            <div class="stat-label">Total Packets</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-value">${this.getAvgRSSI()}</div>
                            <div class="stat-label">Avg RSSI</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-value">${this.getStrongestFreq()}</div>
                            <div class="stat-label">Strongest</div>
                        </div>
                    </div>
                </div>
            </div>
        `;
        
        this.container.innerHTML = html;
    }
    
    renderHeatmap(totalConfigs, currentConfig) {
        let html = '<div class="heatmap-grid">';
        
        for (let i = 0; i < totalConfigs; i++) {
            const data = this.signalData[i];
            let className = 'heatmap-cell';
            let title = `Config ${i + 1}`;
            
            if (i === currentConfig) {
                className += ' current';
                title += ' (Current)';
            }
            
            if (data) {
                const rssi = data.rssi;
                if (rssi > -80) {
                    className += ' strong';
                    title += `\nRSSI: ${rssi.toFixed(1)} dBm (Strong)`;
                } else if (rssi > -100) {
                    className += ' medium';
                    title += `\nRSSI: ${rssi.toFixed(1)} dBm (Medium)`;
                } else {
                    className += ' weak';
                    title += `\nRSSI: ${rssi.toFixed(1)} dBm (Weak)`;
                }
                title += `\nPackets: ${data.count}`;
            } else {
                className += ' no-data';
                title += '\nNo data yet';
            }
            
            html += `<div class="${className}" title="${title}">${i + 1}</div>`;
        }
        
        html += '</div>';
        return html;
    }
    
    getTotalPackets() {
        return Object.values(this.signalData).reduce((sum, data) => sum + data.count, 0);
    }
    
    getAvgRSSI() {
        const values = Object.values(this.signalData);
        if (values.length === 0) return 'N/A';
        const avg = values.reduce((sum, data) => sum + data.rssi, 0) / values.length;
        return `${avg.toFixed(1)} dBm`;
    }
    
    getStrongestFreq() {
        const values = Object.entries(this.signalData);
        if (values.length === 0) return 'N/A';
        const strongest = values.reduce((max, [idx, data]) => 
            data.rssi > max.rssi ? { idx: parseInt(idx), rssi: data.rssi } : max,
            { idx: 0, rssi: -999 }
        );
        return `#${strongest.idx + 1}`;
    }
    
    clear() {
        this.signalData = {};
        this.render();
    }
}
