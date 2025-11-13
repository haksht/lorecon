/**
 * ESP32 LoRa Sniffer - Main Application Logic
 */

class LoRaSnifferApp {
    constructor() {
        this.updateInterval = null;
        this.packetFeedLimit = 50;
        this.startTime = Date.now();
    }

    /**
     * Initialize application
     */
    async init() {
        console.log('Initializing ESP32 LoRa Sniffer App...');
        
        // Setup event listeners
        this.setupEventListeners();
        
        // Connect to WebSocket
        apiClient.connectWebSocket();
        
        // Setup WebSocket event handlers
        this.setupWebSocketHandlers();
        
        // Initial data load
        await this.loadInitialData();
        
        // Hide loading screen, show app
        document.getElementById('loading-screen').classList.add('hidden');
        document.getElementById('app').classList.remove('hidden');
        
        // Start periodic updates
        this.startPeriodicUpdates();
        
        console.log('✓ Application initialized');
    }

    /**
     * Setup UI event listeners
     */
    setupEventListeners() {
        // Tab switching
        document.querySelectorAll('.tab').forEach(tab => {
            tab.addEventListener('click', (e) => {
                this.switchTab(e.target.dataset.tab);
            });
        });
        
        console.log('✓ Event listeners setup');
    }

    /**
     * Setup WebSocket event handlers
     */
    setupWebSocketHandlers() {
        // Connection status
        apiClient.on('wsconnect', () => {
            this.updateConnectionStatus(true);
            this.showToast('Connected to ESP32', 'success');
        });
        
        apiClient.on('wsdisconnect', () => {
            this.updateConnectionStatus(false);
            this.showToast('Disconnected from ESP32', 'warning');
        });
        
        // Real-time packet updates
        apiClient.on('packet', (data) => {
            this.handlePacketUpdate(data);
        });
        
        // Status updates
        apiClient.on('status', (data) => {
            this.handleStatusUpdate(data.data);
        });
        
        // Device updates
        apiClient.on('deviceUpdate', (data) => {
            this.refreshDevices();
        });
        
        console.log('✓ WebSocket handlers setup');
    }

    /**
     * Load initial data
     */
    async loadInitialData() {
        try {
            // Load status
            await this.updateStatus();
            
            // Load devices
            await this.refreshDevices();
            
            // Load statistics
            await this.updateStatistics();
            
            // Load activity
            await this.updateActivity();
            
            console.log('✓ Initial data loaded');
        } catch (error) {
            console.error('Failed to load initial data:', error);
            this.showToast('Failed to load data from ESP32', 'error');
        }
    }

    /**
     * Start periodic updates (when not using WebSocket)
     */
    startPeriodicUpdates() {
        this.updateInterval = setInterval(async () => {
            await this.updateStatus();
            this.updateUptime();
        }, 5000); // Update every 5 seconds
    }

    /**
     * Switch between tabs
     */
    switchTab(tabName) {
        // Update tab buttons
        document.querySelectorAll('.tab').forEach(tab => {
            tab.classList.remove('active');
            if (tab.dataset.tab === tabName) {
                tab.classList.add('active');
            }
        });
        
        // Update tab content
        document.querySelectorAll('.tab-content').forEach(content => {
            content.classList.remove('active');
        });
        document.getElementById(`tab-${tabName}`).classList.add('active');
        
        // Initialize map if switching to map tab
        if (tabName === 'map') {
            setTimeout(() => {
                mapHandler.init();
                this.refreshMap();
            }, 100);
        }
    }

    /**
     * Update connection status indicator
     */
    updateConnectionStatus(connected) {
        const indicator = document.getElementById('connection-status');
        if (connected) {
            indicator.classList.add('connected');
            indicator.classList.remove('disconnected');
        } else {
            indicator.classList.remove('connected');
            indicator.classList.add('disconnected');
        }
    }

    /**
     * Handle real-time packet update
     */
    handlePacketUpdate(packet) {
        // Update packet counter
        const packetCount = document.getElementById('stat-packets');
        if (packetCount) {
            const current = parseInt(packetCount.textContent) || 0;
            packetCount.textContent = current + 1;
        }
        
        // Add to packet feed
        this.addPacketToFeed(packet);
    }

    /**
     * Add packet to live feed
     */
    addPacketToFeed(packet) {
        const feed = document.getElementById('packet-feed');
        if (!feed) return;
        
        // Remove "no data" message if present
        const noData = feed.querySelector('.no-data');
        if (noData) {
            noData.remove();
        }
        
        // Create packet item
        const item = document.createElement('div');
        item.className = 'packet-item';
        item.innerHTML = `
            <div class="packet-header">
                <span class="packet-node">📱 ${packet.nodeId}</span>
                <span class="packet-time">${new Date(packet.timestamp).toLocaleTimeString()}</span>
            </div>
            <div class="packet-details">
                <span>Protocol: ${packet.protocol}</span>
                <span>RSSI: ${packet.rssi} dBm</span>
                <span>SNR: ${packet.snr} dB</span>
                <span>Length: ${packet.length} bytes</span>
                ${packet.message ? `<div class="packet-message">📧 ${packet.message}</div>` : ''}
            </div>
        `;
        
        // Add to top of feed
        feed.insertBefore(item, feed.firstChild);
        
        // Limit feed size
        while (feed.children.length > this.packetFeedLimit) {
            feed.removeChild(feed.lastChild);
        }
    }

    /**
     * Handle status update
     */
    handleStatusUpdate(status) {
        if (status.devices !== undefined) {
            document.getElementById('stat-devices').textContent = status.devices;
        }
        if (status.totalPackets !== undefined) {
            document.getElementById('stat-packets').textContent = status.totalPackets;
        }
        
        // Update system status
        if (status.mode) {
            document.getElementById('status-mode').textContent = status.mode;
        }
        if (status.freeHeap !== undefined) {
            const heapKB = Math.round(status.freeHeap / 1024);
            document.getElementById('status-heap').textContent = `${heapKB} KB`;
        }
    }

    /**
     * Update status from API
     */
    async updateStatus() {
        try {
            const response = await apiClient.getStatus();
            if (response.status === 'success') {
                this.handleStatusUpdate(response);
                
                // Update WebSocket status
                const wsStatus = apiClient.ws && apiClient.ws.readyState === WebSocket.OPEN;
                document.getElementById('status-ws').textContent = wsStatus ? 'Connected' : 'Disconnected';
            }
        } catch (error) {
            console.error('Failed to update status:', error);
        }
    }

    /**
     * Update uptime display
     */
    updateUptime() {
        const uptimeSec = Math.floor((Date.now() - this.startTime) / 1000);
        const hours = Math.floor(uptimeSec / 3600);
        const minutes = Math.floor((uptimeSec % 3600) / 60);
        const seconds = uptimeSec % 60;
        
        let uptimeStr = '';
        if (hours > 0) uptimeStr += `${hours}h `;
        if (minutes > 0) uptimeStr += `${minutes}m `;
        uptimeStr += `${seconds}s`;
        
        document.getElementById('stat-uptime').textContent = uptimeStr;
    }

    /**
     * Refresh device list
     */
    async refreshDevices() {
        try {
            const response = await apiClient.getDevices();
            
            if (response.status === 'success') {
                this.renderDevices(response.devices || []);
                
                // Update device counter
                document.getElementById('stat-devices').textContent = response.totalDevices || 0;
            }
        } catch (error) {
            console.error('Failed to refresh devices:', error);
            this.showToast('Failed to load devices', 'error');
        }
    }

    /**
     * Render device list
     */
    renderDevices(devices) {
        const container = document.getElementById('device-list');
        if (!container) return;
        
        if (devices.length === 0) {
            container.innerHTML = '<p class="no-data">No devices discovered yet...</p>';
            return;
        }
        
        container.innerHTML = devices.map(device => `
            <div class="device-card">
                <div class="device-header">
                    <span class="device-node-id">📱 ${device.nodeId}</span>
                    <span class="device-protocol">${device.protocol}</span>
                </div>
                <div class="device-stats">
                    <div class="device-stat">
                        <span class="device-stat-label">Frequency</span>
                        <span class="device-stat-value">${device.frequency.toFixed(3)} MHz</span>
                    </div>
                    <div class="device-stat">
                        <span class="device-stat-label">RSSI</span>
                        <span class="device-stat-value">${device.rssi} dBm</span>
                    </div>
                    <div class="device-stat">
                        <span class="device-stat-label">SNR</span>
                        <span class="device-stat-value">${device.snr} dB</span>
                    </div>
                    <div class="device-stat">
                        <span class="device-stat-label">Packets</span>
                        <span class="device-stat-value">${device.packetCount}</span>
                    </div>
                </div>
                <div class="device-actions">
                    <button class="btn btn-primary" onclick="app.targetDevice('${device.nodeId}')">
                        🎯 Target
                    </button>
                    <button class="btn btn-secondary" onclick="app.showDeviceDetails('${device.nodeIdDecimal}')">
                        ℹ️ Details
                    </button>
                </div>
            </div>
        `).join('');
    }

    /**
     * Target specific device
     */
    async targetDevice(nodeId) {
        try {
            const response = await apiClient.startCapture(nodeId);
            if (response.status === 'success') {
                this.showToast(response.message, 'success');
            } else {
                this.showToast(response.error || 'Failed to target device', 'error');
            }
        } catch (error) {
            console.error('Failed to target device:', error);
            this.showToast('Failed to target device', 'error');
        }
    }

    /**
     * Show device details (placeholder)
     */
    showDeviceDetails(nodeId) {
        this.showToast(`Device details for ${nodeId} (feature coming soon)`, 'info');
    }

    /**
     * Refresh map
     */
    async refreshMap() {
        await mapHandler.loadPositions();
    }

    /**
     * Update statistics
     */
    async updateStatistics() {
        try {
            const response = await apiClient.getStatistics();
            
            if (response.status === 'success') {
                const container = document.getElementById('statistics');
                if (!container) return;
                
                container.innerHTML = `
                    <div class="stat-row">
                        <span class="status-label">Total Packets:</span>
                        <span>${response.totalPackets || 0}</span>
                    </div>
                    <div class="stat-row">
                        <span class="status-label">Total Devices:</span>
                        <span>${response.totalDevices || 0}</span>
                    </div>
                    <div class="stat-row">
                        <span class="status-label">RF Activities:</span>
                        <span>${response.rfActivities || 0}</span>
                    </div>
                    <div class="stat-row">
                        <span class="status-label">Meshtastic Devices:</span>
                        <span>${response.protocols?.meshtastic || 0}</span>
                    </div>
                    <div class="stat-row">
                        <span class="status-label">LoRaWAN Devices:</span>
                        <span>${response.protocols?.lorawan || 0}</span>
                    </div>
                    <div class="stat-row">
                        <span class="status-label">Generic Devices:</span>
                        <span>${response.protocols?.generic || 0}</span>
                    </div>
                `;
            }
        } catch (error) {
            console.error('Failed to update statistics:', error);
        }
    }

    /**
     * Update RF activity
     */
    async updateActivity() {
        try {
            const response = await apiClient.getActivity();
            
            if (response.status === 'success') {
                const container = document.getElementById('activity-list');
                if (!container) return;
                
                const activities = response.activities || [];
                
                if (activities.length === 0) {
                    container.innerHTML = '<p class="no-data">No RF activity data yet...</p>';
                    return;
                }
                
                container.innerHTML = activities.map(activity => `
                    <div class="activity-item">
                        <div class="activity-frequency">📡 ${activity.frequency.toFixed(3)} MHz</div>
                        <div class="activity-details">
                            <span>Packets: ${activity.packetCount}</span>
                            <span>Peak RSSI: ${activity.peakRSSI} dBm</span>
                            <span>Avg RSSI: ${activity.avgRSSI.toFixed(1)} dBm</span>
                            <span>Last: ${new Date(activity.lastSeen).toLocaleTimeString()}</span>
                        </div>
                    </div>
                `).join('');
            }
        } catch (error) {
            console.error('Failed to update activity:', error);
        }
    }

    /**
     * Export GeoJSON
     */
    async exportGeoJSON() {
        try {
            const data = await apiClient.exportGeoJSON();
            this.downloadFile(JSON.stringify(data, null, 2), 'lora-positions.geojson', 'application/json');
            this.showToast('GeoJSON exported successfully', 'success');
        } catch (error) {
            console.error('Failed to export GeoJSON:', error);
            this.showToast('Failed to export GeoJSON', 'error');
        }
    }

    /**
     * Export KML
     */
    async exportKML() {
        try {
            const data = await apiClient.exportKML();
            this.downloadFile(data, 'lora-positions.kml', 'application/vnd.google-earth.kml+xml');
            this.showToast('KML exported successfully', 'success');
        } catch (error) {
            console.error('Failed to export KML:', error);
            this.showToast('Failed to export KML', 'error');
        }
    }

    /**
     * Download file
     */
    downloadFile(content, filename, mimeType) {
        const blob = new Blob([content], { type: mimeType });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
    }

    /**
     * Show toast notification
     */
    showToast(message, type = 'info') {
        const container = document.getElementById('toast-container');
        if (!container) return;
        
        const toast = document.createElement('div');
        toast.className = `toast ${type}`;
        toast.textContent = message;
        
        container.appendChild(toast);
        
        // Auto-remove after 3 seconds
        setTimeout(() => {
            toast.style.animation = 'slideOut 0.3s';
            setTimeout(() => {
                if (toast.parentNode) {
                    container.removeChild(toast);
                }
            }, 300);
        }, 3000);
    }
}

// Initialize app when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    window.app = new LoRaSnifferApp();
    app.init();
});
