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
            statusMode: document.getElementById('status-mode'),
            statusUptime: document.getElementById('status-uptime'),
            statusPackets: document.getElementById('status-packets'),
            statusDevices: document.getElementById('status-devices'),
            devicesContent: document.getElementById('devices-content'),
            packetsContent: document.getElementById('packets-content'),
            frequencyContent: document.getElementById('frequency-content'),
            gpsContent: document.getElementById('gps-content'),
            mobileMenuToggle: document.getElementById('mobile-menu-toggle'),
            mobileMenuOverlay: document.getElementById('mobile-menu-overlay')
        };

        // App state
        this.lastCommand = null;
        this.statusTimer = null;
        this.isQuietMode = false;
        this.currentTab = 'status';
        this.isMobile = window.innerWidth < 768;
        this.connectionManager = null;

        // Start the app
        this.init();
    }

    async init() {
        // Initialize connection manager
        this.connectionManager = new ConnectionManager(this);
        await this.connectionManager.init();
        
        // Initial status update
        await this.updateStatus();
        
        // Periodic status refresh (less frequent with WebSocket updates)
        this.statusTimer = setInterval(() => this.updateStatus(), 10000);
        
        this.setupEventDelegation();
        this.setupTabs();
        this.setupMobileMenu();
        this.setupResponsive();
        this.loadTabContent('status');
    }

    setupMobileMenu() {
        const sidebar = document.querySelector('.actions-section');
        
        // Toggle menu on button click
        this.el.mobileMenuToggle?.addEventListener('click', () => {
            sidebar?.classList.toggle('active');
            this.el.mobileMenuOverlay?.classList.toggle('active');
        });
        
        // Close menu on overlay click
        this.el.mobileMenuOverlay?.addEventListener('click', () => {
            sidebar?.classList.remove('active');
            this.el.mobileMenuOverlay?.classList.remove('active');
        });
        
        // Close menu after action button click
        document.querySelectorAll('.actions-section .btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                // Don't close immediately - let onclick handler execute first
                // Use requestAnimationFrame to defer menu closing until after current event loop
                if (this.isMobile) {
                    requestAnimationFrame(() => {
                        sidebar?.classList.remove('active');
                        this.el.mobileMenuOverlay?.classList.remove('active');
                    });
                }
            });
        });
    }

    setupResponsive() {
        // Track window resize
        let resizeTimer;
        window.addEventListener('resize', () => {
            clearTimeout(resizeTimer);
            resizeTimer = setTimeout(() => {
                const wasMobile = this.isMobile;
                this.isMobile = window.innerWidth < 768;
                
                // Re-render if switching between mobile/desktop
                if (wasMobile !== this.isMobile && this.currentTab === 'devices') {
                    this.showMenu();
                }
            }, 250);
        });
    }

    setupTabs() {
        const tabs = document.querySelectorAll('.tab-btn');
        tabs.forEach(tab => {
            tab.addEventListener('click', () => {
                const tabName = tab.dataset.tab;
                this.switchTab(tabName);
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
                await this.showMenu();
                break;
            case 'packets':
                await this.showReplayMenu();
                break;
            case 'frequency':
                await this.showFrequencyMenu();
                break;
            case 'gps':
                await this.showGPS();
                break;
            case 'status':
                // Status updates automatically
                break;
        }
    }

    // ================================================================
    // Status Updates
    // ================================================================

    async updateStatus() {
        try {
            const data = await this.get('/api/status');
            if (data.status === 'success') {
                const mode = this.formatMode(data.mode);
                const uptime = this.formatDuration(data.uptime);
                const devices = data.devices || 0;
                const packets = data.totalPackets || 0;
                
                // Update sidebar stats with null checks
                if (this.el.mode) this.el.mode.textContent = mode;
                if (this.el.devices) this.el.devices.textContent = devices;
                if (this.el.packets) this.el.packets.textContent = packets;
                if (this.el.uptime) this.el.uptime.textContent = uptime;
                
                // Update status tab if it exists
                if (this.el.statusMode) this.el.statusMode.textContent = mode;
                if (this.el.statusUptime) this.el.statusUptime.textContent = uptime;
                if (this.el.statusPackets) this.el.statusPackets.textContent = packets;
                if (this.el.statusDevices) this.el.statusDevices.textContent = devices;
                
                this.setConnected(true);
            }
        } catch (error) {
            console.error('updateStatus failed:', error);
            this.setConnected(false);
        }
    }

    setConnected(connected) {
        if (!this.el.connection) return;
        
        const dot = this.el.connection.querySelector('.dot');
        if (dot) {
            if (connected) {
                dot.classList.add('connected');
                if (this.el.connectionText) {
                    this.el.connectionText.textContent = 'Connected';
                }
            } else {
                dot.classList.remove('connected');
                if (this.el.connectionText) {
                    this.el.connectionText.textContent = 'Disconnected';
                }
            }
        }
    }

    setupEventDelegation() {
        // Handle clicks on dynamically created buttons and table rows
        document.addEventListener('click', (e) => {
            // Check for buttons first
            const btn = e.target.closest('button[data-action]');
            if (btn) {
                e.preventDefault();
                const action = btn.dataset.action;
                const value = btn.dataset.value;
                try {
                    this.handleDynamicAction(action, value);
                } catch (error) {
                    console.error('Action handler error:', error);
                    this.showToast('Action failed: ' + error.message, 'error');
                }
                return;
            }
            
            // Check for clickable table rows
            const row = e.target.closest('tr[data-action]');
            if (row) {
                e.preventDefault();
                const action = row.dataset.action;
                const value = row.dataset.value;
                try {
                    this.handleDynamicAction(action, value);
                } catch (error) {
                    console.error('Action handler error:', error);
                    this.showToast('Action failed: ' + error.message, 'error');
                }
            }
        });
    }

    handleDynamicAction(action, value) {
        switch (action) {
            case 'target-device':
                this.targetDevice(value);
                break;
            case 'target-frequency':
                this.targetFrequency(parseInt(value));
                break;
            case 'view-packet':
                this.viewPacket(parseInt(value));
                break;
            case 'replay-packet':
                this.promptReplayPacket(parseInt(value));
                break;
            case 'replay-menu':
            case 'show-replay':
                this.showReplayMenu();
                break;
            case 'show-menu':
                this.showMenu();
                break;
            case 'show-activity':
                this.showActivity();
                break;
            case 'show-device-types':
                this.showDeviceTypes();
                break;
            case 'show-frequency':
                this.showFrequencyMenu();
                break;
            case 'show-gps':
                this.showGPS();
                break;
            case 'show-security':
                this.showSecurity();
                break;
            case 'export-kml':
                this.exportKML();
                break;
            case 'resume-recon':
                this.resumeRecon();
                break;
            case 'stop-capture':
                this.stopCapture();
                break;
            case 'clear-replay-slots':
            case 'clear-replay':
                this.clearReplaySlots();
                break;
            default:
                console.warn('Unknown action:', action);
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
                html += '<h3>Targetable Devices</h3>';
                
                // Render devices responsively - cards on mobile, table on desktop
                if (this.isMobile) {
                    html += this.renderDeviceCards(devices.devices);
                } else {
                    html += this.renderDeviceTable(devices.devices);
                }
                
                html += '<div style="margin-top: 1.5rem; padding: 1rem; background: rgba(74, 144, 226, 0.1); border-left: 4px solid #4a90e2; border-radius: 4px;">';
                html += '<p><strong>💡 How to capture packets:</strong></p>';
                html += '<p>1. Click <strong>Target</strong> on a device above</p>';
                html += '<p>2. Wait for packets to be captured</p>';
                html += '<p>3. View them in <strong>Packets</strong> menu (left sidebar)</p>';
                html += '<p><em>Note: Decrypted packet content is only visible via serial monitor (USB connection)</em></p>';
                html += '</div>';
            } else {
                html += '<p><em>No devices discovered yet. Reconnaissance is running...</em></p>';
            }

            this.showResults('Discovered Devices', html);
        } catch (error) {
            this.showError('Failed to load menu');
        }
    }

    renderDeviceTable(devices) {
        let html = '<div style="overflow-x: auto;"><table class="table"><thead><tr>';
        html += '<th>Node ID</th><th>Protocol</th><th>RSSI</th><th>Packets</th><th>Action</th>';
        html += '</tr></thead><tbody>';
        
        devices.forEach((dev, idx) => {
            const nodeHex = dev.nodeId || (dev.nodeIdDecimal ? dev.nodeIdDecimal.toString(16).toUpperCase() : '');
            html += '<tr>';
            html += `<td>0x${nodeHex}</td>`;
            html += `<td>${dev.protocol || 'Unknown'}</td>`;
            html += `<td>${dev.rssi ? dev.rssi.toFixed(1) : '—'} dBm</td>`;
            html += `<td>${dev.packetCount || 0}</td>`;
            html += `<td><button class="btn btn-primary" data-action="target-device" data-value="${nodeHex}">Target</button></td>`;
            html += '</tr>';
        });
        
        html += '</tbody></table></div>';
        return html;
    }

    renderDeviceCards(devices) {
        let html = '<div class="device-list-mobile">';
        
        devices.forEach((dev, idx) => {
            const nodeHex = dev.nodeId || (dev.nodeIdDecimal ? dev.nodeIdDecimal.toString(16).toUpperCase() : '');
            const rssi = dev.rssi || 0;
            const signalQuality = this.getSignalQuality(rssi);
            
            html += `
                <div class="device-card">
                    <div class="card-header">
                        <h3 class="node-id">0x${nodeHex}</h3>
                        <span class="badge">${dev.protocol || 'Unknown'}</span>
                    </div>
                    <div class="card-body">
                        <div class="info-grid">
                            <div class="info-item">
                                <span class="label">Signal</span>
                                <span class="value">${rssi.toFixed(1)} dBm ${signalQuality.icon}</span>
                            </div>
                            <div class="info-item">
                                <span class="label">Packets</span>
                                <span class="value">${dev.packetCount || 0}</span>
                            </div>
                            <div class="info-item">
                                <span class="label">Device Type</span>
                                <span class="value">${dev.deviceType || 'Unknown'}</span>
                            </div>
                            <div class="info-item">
                                <span class="label">Frequency</span>
                                <span class="value">${dev.frequency ? dev.frequency.toFixed(3) + ' MHz' : '—'}</span>
                            </div>
                        </div>
                    </div>
                    <div class="card-actions">
                        <button class="btn btn-primary btn-block" data-action="target-device" data-value="${nodeHex}">
                            🎯 Target Device
                        </button>
                    </div>
                </div>
            `;
        });
        
        html += '</div>';
        return html;
    }

    getSignalQuality(rssi) {
        if (rssi > -60) return { strength: 'excellent', icon: '📶' };
        if (rssi > -75) return { strength: 'good', icon: '📡' };
        if (rssi > -90) return { strength: 'fair', icon: '📉' };
        return { strength: 'poor', icon: '⚠️' };
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
            html += '<div style="overflow-x: auto;"><table class="table"><thead><tr>';
            html += '<th>#</th><th>Protocol</th><th>Frequency</th><th>Packets</th><th>Avg RSSI</th><th>Peak RSSI</th>';
            html += '</tr></thead><tbody>';

            data.activities.forEach((activity, idx) => {
                html += '<tr>';
                html += `<td>${idx + 1}</td>`;
                html += `<td>${activity.protocol || 'Unknown'}</td>`;
                html += `<td>${this.formatFreq(activity.frequency)}</td>`;
                html += `<td>${activity.packets || 0}</td>`;
                html += `<td>${this.formatSignal(activity.avgRSSI)}</td>`;
                html += `<td>${this.formatSignal(activity.peakRSSI)}</td>`;
                html += '</tr>';
            });

            html += '</tbody></table></div>';
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

            html += '<div style="overflow-x: auto;"><table class="table"><thead><tr>';
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

            html += '</tbody></table></div>';
            this.showResults('Device Types', html);
        } catch (error) {
            this.showError('Failed to load device types');
        }
    }

    async showReplayMenu() {
        try {
            const data = await this.get('/api/replay/slots');
            console.log('Replay slots data:', data);
            
            if (data.status !== 'success') {
                this.showError('Failed to load replay slots');
                return;
            }

            let html = '<h3>Packet Replay</h3>';
            
            if (!data.slots || data.slots.length === 0) {
                html += '<p>No packets captured yet.</p>';
                html += '<p><strong>To capture packets:</strong></p>';
                html += '<ol>';
                html += '<li>Target a device or frequency</li>';
                html += '<li>Wait for packet reception</li>';
                html += '<li>Packet will be automatically captured</li>';
                html += '</ol>';
                this.showResults('Packet Replay', html);
                return;
            }

            html += `<p>Captured Packets: ${data.count}/${data.capacity} slots used</p>`;
            html += '<div style="overflow-x: auto;"><table class="table"><thead><tr>';
            html += '<th style="width: 40px;">#</th>';
            html += '<th style="width: 100px;">Node ID</th>';
            html += '<th style="width: 120px;">Protocol</th>';
            html += '<th style="width: 60px;">Bytes</th>';
            html += '<th style="width: 75px;">Freq MHz</th>';
            html += '<th style="width: 65px;">RSSI</th>';
            html += '<th style="width: 55px;">Age</th>';
            html += '<th>Decrypted Text</th>';
            html += '</tr></thead><tbody>';

            data.slots.forEach((slot, idx) => {
                const age = this.formatAge(slot.capturedSecondsAgo || 0);
                const decryptedText = slot.decryptedText ? `"${slot.decryptedText}"` : '<em style="color: var(--text-muted);">encrypted</em>';
                const nodeId = slot.nodeId ? `0x${slot.nodeId}` : '<span style="color: var(--text-muted);">—</span>';
                html += '<tr style="cursor: pointer;" data-action="replay-packet" data-value="' + (slot.index || idx) + '">';
                html += `<td>${slot.index || (idx + 1)}</td>`;
                html += `<td style="font-size: 0.8em;">${nodeId}</td>`;
                html += `<td style="font-size: 0.8em;">${slot.protocol || 'Unknown'}</td>`;
                html += `<td>${slot.length}</td>`;
                html += `<td>${this.formatFreq(slot.frequencyMHz)}</td>`;
                html += `<td>${slot.rssi ? slot.rssi.toFixed(0) : 'N/A'}</td>`;
                html += `<td style="font-size: 0.8em;">${age}</td>`;
                html += `<td style="max-width: 200px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-size: 0.85em;">${decryptedText}</td>`;
                html += '</tr>';
            });

            html += '</tbody></table></div>';
            html += '<div style="margin-top: 1rem;">';
            html += '<button class="btn" data-action="clear-replay-slots" style="background: var(--danger); color: white;">Clear All Slots</button>';
            html += '</div>';
            html += '<div style="margin-top: 1.5rem; padding: 1rem; background: rgba(74, 144, 226, 0.1); border-left: 4px solid #4a90e2; border-radius: 4px;">';
            html += '<p><strong>💡 How to replay packets:</strong></p>';
            html += '<p>Click any packet row above to replay it. You can configure repeat count and delay between transmissions.</p>';
            html += '</div>';
            this.showResults('Captured Packets', html);
        } catch (error) {
            console.error('Error loading replay slots:', error);
            this.showError('Failed to load replay slots: ' + error.message);
        }
    }

    async showFrequencyMenu() {
        try {
            // Get recon summary which includes rfActivity
            const data = await this.get('/api/recon/summary');
            console.log('Recon summary data:', data);
            
            if (data.status !== 'success') {
                this.showError('Failed to load frequency data');
                return;
            }

            // Build a list of all scan configurations (hardcoded like in the firmware)
            // This matches the serial menu which shows ALL configs, not just ones with activity
            const scanConfigs = [
                { protocol: 'Meshtastic_ShortFast', frequencyMHz: 906.875, sf: 7, bw: 250 },
                { protocol: 'Meshtastic_ShortSlow', frequencyMHz: 906.875, sf: 8, bw: 250 },
                { protocol: 'Meshtastic_MedFast', frequencyMHz: 906.875, sf: 9, bw: 250 },
                { protocol: 'Meshtastic_MedSlow', frequencyMHz: 906.875, sf: 10, bw: 250 },
                { protocol: 'Meshtastic_LongFast', frequencyMHz: 906.875, sf: 11, bw: 250 },
                { protocol: 'Meshtastic_LongSlow', frequencyMHz: 906.875, sf: 12, bw: 250 },
                { protocol: 'Meshtastic_VLongSlow', frequencyMHz: 906.875, sf: 12, bw: 125 }
            ];

            // Create a map of activity by config index
            const activityMap = {};
            if (data.rfActivity && Array.isArray(data.rfActivity)) {
                data.rfActivity.forEach(activity => {
                    activityMap[activity.configIndex] = activity;
                });
            }

            let html = '<h3>Frequency Targeting</h3>';
            html += '<p>Select a configuration to target directly (🔥 = activity detected, ⚪ = no activity):</p>';
            html += '<div style="overflow-x: auto;"><table class="table"><thead><tr>';
            html += '<th>#</th><th>Protocol</th><th>Frequency</th><th>Activity</th><th>Action</th>';
            html += '</tr></thead><tbody>';

            scanConfigs.forEach((config, idx) => {
                const activity = activityMap[idx];
                const hasActivity = activity && activity.packets > 0;
                const activityIcon = hasActivity ? '🔥' : '⚪';
                const packets = activity ? activity.packets : 0;
                
                html += '<tr>';
                html += `<td>${idx + 1}</td>`;
                html += `<td>${config.protocol}</td>`;
                html += `<td>${this.formatFreq(config.frequencyMHz)} (SF${config.sf}, BW${config.bw})</td>`;
                html += `<td>${activityIcon} ${packets} packets</td>`;
                html += `<td><button class="btn" data-action="target-frequency" data-value="${idx}">Target</button></td>`;
                html += '</tr>';
            });

            html += '</tbody></table></div>';
            this.showResults('Frequency Targeting', html);
        } catch (error) {
            console.error('Error loading frequency data:', error);
            this.showError('Failed to load frequency data: ' + error.message);
        }
    }

    async viewPacket(slotIndex) {
        try {
            const data = await this.get('/api/replay/slots');
            if (data.status !== 'success' || !data.slots || slotIndex >= data.slots.length) {
                this.showError('Invalid packet slot');
                return;
            }

            const slot = data.slots[slotIndex];
            let html = `<h3>Packet #${slot.index || (slotIndex + 1)} Details</h3>`;
            html += '<div style="background: #2a2a2a; padding: 1rem; border-radius: 4px; margin-bottom: 1rem;">';
            html += `<p><strong>Protocol:</strong> ${slot.protocol || 'Unknown'}</p>`;
            html += `<p><strong>Frequency:</strong> ${this.formatFreq(slot.frequencyMHz)}</p>`;
            html += `<p><strong>Config Index:</strong> ${slot.configIndex}</p>`;
            html += `<p><strong>Length:</strong> ${slot.length} bytes</p>`;
            html += `<p><strong>RSSI:</strong> ${slot.rssi ? slot.rssi.toFixed(1) : 'N/A'} dBm</p>`;
            html += `<p><strong>Captured:</strong> ${this.formatAge(slot.capturedSecondsAgo || 0)} ago</p>`;
            html += '</div>';
            html += '<p><em>Note: Raw packet data and decryption details not yet available via API. Use serial monitor for detailed packet inspection.</em></p>';

            html += '<button class="btn" data-action="replay-menu" style="margin-top: 1rem;">Back to Replay Menu</button>';
            this.showResults('Packet Details', html);
        } catch (error) {
            console.error('Error viewing packet:', error);
            this.showError('Failed to load packet details: ' + error.message);
        }
    }

    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    async clearReplaySlots() {
        if (!confirm('Clear all captured packets? This cannot be undone.')) {
            return;
        }

        try {
            const data = await this.post('/api/replay/clear', {});
            if (data.status === 'success') {
                this.showToast('✅ All replay slots cleared');
                this.showReplayMenu(); // Refresh the view
            } else {
                this.showError(data.message || 'Failed to clear replay slots');
            }
        } catch (error) {
            console.error('Error clearing replay slots:', error);
            this.showError('Failed to clear replay slots: ' + error.message);
        }
    }

    async promptReplayPacket(slotIndex) {
        const repeatCount = prompt('Repeat count (1-100):', '1');
        if (!repeatCount || repeatCount === '0') {
            return;
        }

        const repeat = parseInt(repeatCount);
        if (isNaN(repeat) || repeat < 1 || repeat > 100) {
            this.showToast('Invalid repeat count (must be 1-100)', 'error');
            return;
        }

        let delayMs = 1000;
        if (repeat > 1) {
            const delayInput = prompt('Delay between packets in ms (100-10000):', '1000');
            if (delayInput) {
                delayMs = parseInt(delayInput);
                if (isNaN(delayMs) || delayMs < 100 || delayMs > 10000) {
                    this.showToast('Invalid delay (using default 1000ms)', 'warning');
                    delayMs = 1000;
                }
            }
        }

        await this.replayPacket(slotIndex, repeat, delayMs);
    }

    async replayPacket(slotIndex, repeatCount, delayMs) {
        try {
            this.showToast(`📡 Transmitting packet ${slotIndex + 1}...`, 'info');
            
            const data = await this.post('/api/replay/transmit', {
                slotIndex: slotIndex.toString(),
                repeatCount: repeatCount.toString(),
                delayMs: delayMs.toString()
            });

            if (data.status === 'success') {
                this.showToast(`✅ ${data.message || 'Replay complete'}`, 'success');
            } else {
                this.showToast(`❌ ${data.error || 'Replay failed'}`, 'error');
            }
        } catch (error) {
            console.error('Error replaying packet:', error);
            this.showToast('Failed to replay packet: ' + error.message, 'error');
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
            html += '<div style="overflow-x: auto;"><table class="table"><thead><tr>';
            html += '<th style="width: 110px;">Node ID</th><th>Latitude</th><th>Longitude</th><th style="width: 85px;">Altitude</th><th style="width: 95px;">Time</th>';
            html += '</tr></thead><tbody>';

            data.positions.forEach(pos => {
                const time = pos.timestamp ? new Date(pos.timestamp).toLocaleTimeString() : '—';
                const nodeId = pos.nodeId ? (pos.nodeId.startsWith('0x') ? pos.nodeId : `0x${pos.nodeId}`) : 'Unknown';
                html += '<tr>';
                html += `<td style="font-family: monospace; font-size: 0.85em;">${nodeId}</td>`;
                html += `<td>${pos.lat ? pos.lat.toFixed(5) : '—'}</td>`;
                html += `<td>${pos.lon ? pos.lon.toFixed(5) : '—'}</td>`;
                html += `<td>${pos.alt || 0}m</td>`;
                html += `<td style="font-size: 0.85em;">${time}</td>`;
                html += '</tr>';
            });

            html += '</tbody></table></div>';
            this.showResults('GPS Data', html);
        } catch (error) {
            this.showError('Failed to load GPS data');
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
                html += '<div style="overflow-x: auto;"><table class="table"><thead><tr>';
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

                html += '</tbody></table></div>';
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

    showTabContent(tabName, html) {
        const contentEl = this.el[`${tabName}Content`];
        if (contentEl) {
            contentEl.innerHTML = html;
        }
    }

    showResults(title, html) {
        // Route content to appropriate tab and activate it
        const tabMap = {
            'Discovered Devices': 'devices',
            'Menu': 'devices',
            'RF Activity': 'devices',           // Activity view goes to devices tab
            'Device Types': 'devices',          // Device types view goes to devices tab
            'Security Assessment': 'devices',   // Security view goes to devices tab
            'Diagnostics': 'devices',           // Diagnostics goes to devices tab
            'Captured Packets': 'packets',
            'Packet Replay': 'packets',
            'Packet Details': 'packets',
            'Frequency Targeting': 'frequency',
            'GPS Data': 'gps'
        };
        
        const tab = tabMap[title];
        if (tab) {
            // Update content
            this.showTabContent(tab, html);
            // Activate the tab so user sees the result
            this.switchTab(tab);
        }
    }

    showError(message) {
        const html = `<p class="error" style="color: var(--danger); padding: 2rem; text-align: center;">${message}</p>`;
        this.showTabContent(this.currentTab, html);
    }

    showToast(message, type = 'info') {
        // Use global toast manager if available
        if (window.toast) {
            window.toast.show(message, type);
        } else {
            // Fallback to console if toast not loaded yet
            console.log(`[${type.toUpperCase()}] ${message}`);
        }
    }

    handleRealtimeUpdate(data) {
        // Handle real-time updates from WebSocket or polling
        if (!data) return;
        
        // Update status if present
        if (data.mode !== undefined) {
            const mode = this.formatMode(data.mode);
            if (this.el.mode) this.el.mode.textContent = mode;
            if (this.el.statusMode) this.el.statusMode.textContent = mode;
        }
        
        if (data.totalPackets !== undefined) {
            if (this.el.packets) this.el.packets.textContent = data.totalPackets;
            if (this.el.statusPackets) this.el.statusPackets.textContent = data.totalPackets;
        }
        
        if (data.deviceCount !== undefined || data.devices !== undefined) {
            const count = data.deviceCount || (data.devices ? data.devices.length : 0);
            if (this.el.devices) this.el.devices.textContent = count;
            if (this.el.statusDevices) this.el.statusDevices.textContent = count;
        }
        
        if (data.uptime !== undefined) {
            const uptime = this.formatDuration(data.uptime);
            if (this.el.uptime) this.el.uptime.textContent = uptime;
            if (this.el.statusUptime) this.el.statusUptime.textContent = uptime;
        }
        
        // Handle packet events
        if (data.type === 'packet' && data.summary) {
            console.log('[Packet]', data.summary);
            // Could show toast for important packets if desired
        }
        
        // Handle device discovery events
        if (data.type === 'device_discovered') {
            this.showToast(`📡 New device discovered! (${data.count} total)`, 'info', 2000);
            // Refresh devices tab if active
            if (this.currentTab === 'devices') {
                this.showMenu();
            }
        }
        
        // Handle capture complete events
        if (data.type === 'capture_complete' && data.slot !== undefined) {
            this.showToast(`✓ Packet captured to slot ${data.slot + 1}`, 'success');
        }
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

    formatAge(seconds) {
        if (seconds < 60) return `${seconds}s`;
        if (seconds < 3600) return `${Math.floor(seconds / 60)}m`;
        return `${Math.floor(seconds / 3600)}h`;
    }

    showLoading(title = 'Loading') {
        this.resultsTitle.textContent = title;
        this.resultsContent.innerHTML = '';
        this.resultsContent.classList.add('loading');
        this.resultsPanel.style.display = 'block';
        
        // Scroll to show loading state
        setTimeout(() => {
            window.scrollTo({ 
                top: 0, 
                behavior: 'smooth' 
            });
        }, 50);
    }

    // ================================================================
    // API Methods
    // ================================================================

    async get(endpoint) {
        try {
            const response = await fetch(endpoint);
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}`);
            }
            return await response.json();
        } catch (error) {
            console.error(`GET ${endpoint} failed:`, error);
            throw error;
        }
    }

    async post(endpoint, body = {}) {
        try {
            const formData = new URLSearchParams();
            Object.entries(body).forEach(([key, value]) => {
                formData.append(key, value);
            });

            const response = await fetch(endpoint, {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: formData.toString()
            });

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}`);
            }
            return await response.json();
        } catch (error) {
            console.error(`POST ${endpoint} failed:`, error);
            throw error;
        }
    }
}

// Initialize app when DOM is ready
window.app = null;
document.addEventListener('DOMContentLoaded', () => {
    window.app = new ReconApp();
});
