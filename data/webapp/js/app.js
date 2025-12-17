/* ESP32 LoRa Recon - Simplified Client */

// ===== Security Utilities =====
// HTML escape function to prevent XSS attacks
function escapeHtml(text) {
    if (text === null || text === undefined) return '';
    const str = String(text);
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
}

// Dependency checks - ensure scripts loaded in correct order
if (typeof showToast === 'undefined') {
    console.error('FATAL: toast.js must be loaded before app.js');
    alert('Script loading error - please refresh the page');
}
if (typeof WarRoom === 'undefined') {
    console.warn('WARNING: war-room.js not loaded - War Room tab will be disabled');
}
if (typeof NetworkMap === 'undefined') {
    console.warn('WARNING: network-map.js not loaded - Network Map will be disabled');
}

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
            settingsContent: document.getElementById('settings-content'),
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
        this.lastDropWarning = null;  // Track queue warning timestamp
        this.currentTab = 'info';
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
            
            // Check WiFi setup mode and show banner if needed
            await this.checkSetupMode();
            
            // Load first tab (Info)
            this.loadTabContent('info');
        } catch (error) {
            console.error('Init error:', error);
        }
    }
    
    async checkSetupMode() {
        try {
            const wifi = await this.get('/api/wifi/status');
            if (wifi && wifi.mode === 'setup') {
                this.updateSetupBanner(true);
                // Auto-show settings tab to configure WiFi
                showToast('First run! Configure your phone hotspot to get started.', 'info');
            }
        } catch (error) {
            console.log('WiFi status check failed (may be normal on first load)');
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
        // Check for queue drops and show warning if significant
        if (data.droppedPackets > 0 && data.totalPackets > 0) {
            const totalReceived = data.totalPackets + data.droppedPackets;
            const dropRate = ((data.droppedPackets / totalReceived) * 100).toFixed(1);
            if (dropRate > 5 && (!this.lastDropWarning || Date.now() - this.lastDropWarning > 60000)) {
                showToast(`⚠️ Queue overload: ${data.droppedPackets} packets dropped (${dropRate}%)`, 'warning');
                this.lastDropWarning = Date.now();
            }
        }
        
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
            this.consecutiveErrors = 0; // Reset error counter
        } catch (error) {
            this.consecutiveErrors = (this.consecutiveErrors || 0) + 1;
            // Only log first 3 errors to avoid spam
            if (this.consecutiveErrors <= 3) {
                console.error('Status update failed:', error);
                if (this.consecutiveErrors === 3) {
                    console.warn('[Status] Suppressing further connection errors...');
                }
            }
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
            case 'settings':
                await this.showSettings();
                break;
            case 'info':
                await this.showInfo();
                break;
        }
    }

    // ============ Tab Content Loaders ============
    
    async showDevices() {
        // Show loading state
        this.el.devicesContent.innerHTML = '<div class="loading-container"><div class="loading-spinner"></div><p>Loading devices...</p></div>';
        
        try {
            const data = await this.get('/api/devices');
            if (!data || !data.devices || data.devices.length === 0) {
                this.el.devicesContent.innerHTML = '<p class="placeholder">No devices found. Devices will appear as they are discovered during scanning.</p>';
                return;
            }
            
            // Fetch security assessment to get vulnerability scores
            let securityData = null;
            try {
                securityData = await this.get('/api/recon/security');
            } catch (err) {
                console.warn('Security data not available, sorting by discovery order');
            }
            
            // Create a map of nodeId to vulnerability score
            const scoreMap = new Map();
            if (securityData && securityData.devices) {
                securityData.devices.forEach(secDev => {
                    scoreMap.set(secDev.nodeIdDecimal, {
                        score: secDev.score,
                        riskLevel: secDev.riskLevel
                    });
                });
            }
            
            // Enrich devices with security scores and sort by vulnerability (lowest score first)
            const enrichedDevices = data.devices.map(device => {
                const secInfo = scoreMap.get(device.nodeIdDecimal) || { score: 100, riskLevel: 'unknown' };
                return {
                    ...device,
                    vulnerabilityScore: secInfo.score,
                    riskLevel: secInfo.riskLevel
                };
            });
            
            // Sort by vulnerability score (lowest first = most vulnerable first)
            enrichedDevices.sort((a, b) => a.vulnerabilityScore - b.vulnerabilityScore);
            
            let html = '<div class="table-wrapper">';
            html += '<table class="table"><thead><tr>';
            html += '<th>Node ID</th><th>Risk</th><th>Type</th><th>Protocol</th><th>Packets</th><th>RSSI</th><th>Frequency</th><th>Last Seen</th><th>Actions</th>';
            html += '</tr></thead><tbody>';
            
            enrichedDevices.forEach(device => {
                const rssiClass = (device.rssi || -100) > -70 ? 'rssi-strong' : (device.rssi || -100) > -90 ? 'rssi-medium' : 'rssi-weak';
                
                // Risk badge styling
                let riskBadge = '';
                let riskClass = '';
                if (device.riskLevel === 'vulnerable') {
                    riskBadge = '🔴 High';
                    riskClass = 'risk-high';
                } else if (device.riskLevel === 'moderate') {
                    riskBadge = '🟡 Med';
                    riskClass = 'risk-medium';
                } else if (device.riskLevel === 'secure') {
                    riskBadge = '🟢 Low';
                    riskClass = 'risk-low';
                } else {
                    riskBadge = '⚪ —';
                    riskClass = 'risk-unknown';
                }
                
                // Escape all user-derived data for XSS prevention
                const safeNodeId = escapeHtml(device.nodeId);
                const safeDeviceType = escapeHtml(device.deviceType || 'Unknown');
                const safeProtocol = escapeHtml(device.protocol || '—');
                
                html += '<tr>';
                html += `<td><code>0x${safeNodeId}</code></td>`;
                html += `<td><span class="${riskClass}">${riskBadge}</span></td>`;
                html += `<td>${safeDeviceType}</td>`;
                html += `<td><code>${safeProtocol}</code></td>`;
                html += `<td>${device.packetCount || 0}</td>`;
                html += `<td><span class="${rssiClass}">${device.rssi || '—'} dBm</span></td>`;
                html += `<td>${(device.frequency || 0).toFixed(3)} MHz</td>`;
                html += `<td>${this.formatLastSeen(device.lastSeenSecondsAgo)}</td>`;
                html += `<td><button data-action="target-device" data-value="${safeNodeId}" class="btn btn-primary btn-small">🎯 Target</button></td>`;
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
        // Show loading state
        this.el.packetsContent.innerHTML = '<div class="loading-container"><div class="loading-spinner"></div><p>Loading packets...</p></div>';
        
        try {
            const data = await this.get('/api/replay/slots');
            if (!data || !data.slots || data.slots.length === 0) {
                this.el.packetsContent.innerHTML = '<p class="placeholder">No packets captured for replay yet. Target a device or frequency first, then packets will be stored here for replay.</p>';
                return;
            }
            
            // Group packets by packet ID to detect flooding
            const grouped = this.groupPacketsByPacketId(data.slots);
            
            let html = '<div class="table-wrapper">';
            html += '<table class="table"><thead><tr>';
            html += '<th>Protocol</th><th>Node ID</th><th>Packet ID</th><th>Size</th><th>RSSI</th><th>Frequency</th><th>Captured</th><th>Message</th><th>Actions</th>';
            html += '</tr></thead><tbody>';
            
            grouped.forEach(group => {
                group.packets.forEach((pkt, idx) => {
                    const rssiClass = (pkt.rssi || -100) > -70 ? 'rssi-strong' : (pkt.rssi || -100) > -90 ? 'rssi-medium' : 'rssi-weak';
                    const isRelay = group.packets.length > 1 && idx > 0;
                    const rowClass = isRelay ? 'relay-row' : '';
                    
                    html += `<tr class="${rowClass}">`;
                    html += `<td><code>${pkt.protocol || 'Unknown'}</code></td>`;
                    html += `<td>${pkt.nodeId ? '<code>0x' + pkt.nodeId + '</code>' : '—'}</td>`;
                    
                    // Show packet ID with relay indicator
                    if (isRelay) {
                        html += `<td><code>0x${pkt.packetId || '—'}</code> <span class="relay-badge">↻ Relay</span></td>`;
                    } else if (group.packets.length > 1) {
                        html += `<td><code>0x${pkt.packetId || '—'}</code> <span class="origin-badge">⚡ +${group.packets.length - 1} relays</span></td>`;
                    } else {
                        html += `<td>${pkt.packetId ? '<code>0x' + pkt.packetId + '</code>' : '—'}</td>`;
                    }
                    
                    html += `<td>${pkt.length || 0} B</td>`;
                    html += `<td><span class="${rssiClass}">${pkt.rssi || '—'} dBm</span></td>`;
                    html += `<td>${(pkt.frequencyMHz || 0).toFixed(3)} MHz</td>`;
                    html += `<td>${this.formatDuration(pkt.capturedSecondsAgo || 0)} ago</td>`;
                    html += `<td>${pkt.decryptedText || '—'}</td>`;
                    html += `<td><button data-action="replay-packet" data-value="${pkt.originalIndex}" class="btn btn-primary btn-small">🔁 Replay</button></td>`;
                    html += '</tr>';
                });
            });
            
            html += '</tbody></table></div>';
            this.el.packetsContent.innerHTML = html;
        } catch (error) {
            console.error('Failed to load packets:', error);
            this.el.packetsContent.innerHTML = '<div class="error-state"><p class="error">Failed to load packets</p><button data-action="retry-packets" class="btn btn-primary">Retry</button></div>';
        }
    }
    
    groupPacketsByPacketId(slots) {
        // Group packets by packetId to identify flooding/relay chains
        const groups = new Map();
        
        slots.forEach((pkt, idx) => {
            pkt.originalIndex = idx; // Preserve original index for replay
            const key = pkt.packetId || `unique_${idx}`; // Fallback for packets without ID
            
            if (!groups.has(key)) {
                groups.set(key, { packets: [], isFlooding: false });
            }
            groups.get(key).packets.push(pkt);
        });
        
        // Mark flooding events (same packet ID, different node IDs, within 30s)
        groups.forEach((group, key) => {
            if (group.packets.length > 1 && key.startsWith('0x')) {
                // Check if captured within 30 seconds and have different node IDs
                const times = group.packets.map(p => p.capturedSecondsAgo);
                const nodeIds = new Set(group.packets.map(p => p.nodeId).filter(id => id));
                const timeSpread = Math.max(...times) - Math.min(...times);
                
                if (timeSpread <= 30 && nodeIds.size > 1) {
                    group.isFlooding = true;
                    // Sort by capture time (most recent first)
                    group.packets.sort((a, b) => a.capturedSecondsAgo - b.capturedSecondsAgo);
                }
            }
        });
        
        // Convert to array and sort by most recent capture time
        return Array.from(groups.values()).sort((a, b) => {
            return Math.min(...a.packets.map(p => p.capturedSecondsAgo)) - 
                   Math.min(...b.packets.map(p => p.capturedSecondsAgo));
        });
    }

    async showFrequency() {
        // Show loading state
        this.el.frequencyContent.innerHTML = '<div class="loading-container"><div class="loading-spinner"></div><p>Loading frequency data...</p></div>';
        
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
        console.log('[Network] Loading network map...');
        console.log('[Network] NetworkMap available?', typeof NetworkMap !== 'undefined');
        console.log('[Network] window.NetworkMap?', typeof window.NetworkMap !== 'undefined');
        
        if (!this.networkMap) {
            console.log('[Network] Initializing NetworkMap...');
            // Check both direct and window scope
            const NetworkMapClass = window.NetworkMap || NetworkMap;
            if (typeof NetworkMapClass !== 'undefined') {
                try {
                    // Use network-info as the details panel (it exists in HTML)
                    this.networkMap = new NetworkMapClass('network-canvas', 'network-info');
                    console.log('[Network] NetworkMap initialized successfully');
                } catch (error) {
                    console.error('[Network] Failed to initialize NetworkMap:', error);
                    // Show error in network info panel
                    const infoEl = document.getElementById('network-info');
                    if (infoEl) {
                        infoEl.innerHTML = '<div class="error-state"><p class="error">Network visualization unavailable: ' + error.message + '</p><p style="font-size: 0.9rem; margin-top: 0.5rem;">Check browser console for details.</p></div>';
                    }
                }
            } else {
                console.error('[Network] NetworkMap class not found! Scripts may not have loaded.');
                console.error('[Network] Available globals:', Object.keys(window).filter(k => k.includes('Map') || k.includes('Network') || k.includes('War')));
                // Show error message
                const infoEl = document.getElementById('network-info');
                if (infoEl) {
                    infoEl.innerHTML = '<div class="error-state"><p class="error">Network Map unavailable</p><p style="font-size: 0.9rem; margin-top: 0.5rem;">The network-map.js script failed to load. Try refreshing the page or check SD card/LittleFS.</p></div>';
                }
            }
        }
        
        // Force canvas resize on tab switch
        if (this.networkMap && this.networkMap.canvas) {
            // Trigger resize to ensure canvas dimensions are correct
            const resizeEvent = new Event('resize');
            window.dispatchEvent(resizeEvent);
        }
        
        try {
            const data = await this.get('/api/devices');
            console.log('[Network] Devices data:', data);
            
            if (data && data.devices) {
                // Fetch security data to enrich devices with vulnerability info
                let securityData = null;
                try {
                    securityData = await this.get('/api/recon/security');
                } catch (err) {
                    console.warn('[Network] Security data not available for network map');
                }
                
                // Create score map from security data
                const scoreMap = new Map();
                if (securityData && securityData.devices) {
                    securityData.devices.forEach(secDev => {
                        scoreMap.set(secDev.nodeIdDecimal, {
                            score: secDev.score,
                            riskLevel: secDev.riskLevel
                        });
                    });
                }
                
                // Enrich devices with security scores for network map coloring
                const enrichedDevices = data.devices.map(device => {
                    const secInfo = scoreMap.get(device.nodeIdDecimal) || { score: 100, riskLevel: 'unknown' };
                    return {
                        ...device,
                        securityScore: secInfo.score,
                        riskLevel: secInfo.riskLevel
                    };
                });
                
                if (this.networkMap) {
                    this.networkMap.updateDevices(enrichedDevices);
                    console.log('[Network] Updated map with', enrichedDevices.length, 'enriched devices');
                } else {
                    console.warn('[Network] NetworkMap not initialized, cannot update');
                }
                
                // Update network info panel
                const infoEl = document.getElementById('network-info');
                if (infoEl) {
                    if (data.devices.length === 0) {
                        infoEl.innerHTML = '<p class="placeholder">No devices discovered yet. Start reconnaissance to populate the network map.</p>';
                    } else {
                        infoEl.innerHTML = `<p style="color: var(--text-secondary); font-size: 0.9rem;">Showing ${data.devices.length} device(s). Click a node for details.</p>`;
                    }
                }
            }
        } catch (error) {
            console.error('[Network] Failed to load devices:', error);
            const infoEl = document.getElementById('network-info');
            if (infoEl) {
                infoEl.innerHTML = '<p class="error">Failed to load network data: ' + error.message + '</p>';
            }
        }
    }

    async showStats() {
        console.log('[Stats] Loading war room...');
        
        if (!this.warRoom) {
            console.log('[Stats] Initializing WarRoom...');
            if (typeof WarRoom !== 'undefined') {
                try {
                    this.warRoom = new WarRoom('war-room-container');
                    console.log('[Stats] WarRoom initialized successfully');
                } catch (error) {
                    console.error('[Stats] Failed to initialize WarRoom:', error);
                }
            } else {
                console.error('[Stats] WarRoom class not found!');
            }
        }
        
        if (!this.warRoom) {
            // Fallback if WarRoom class not available
            const container = document.getElementById('war-room-container');
            if (container) {
                container.innerHTML = '<div class="placeholder"><div style="font-size: 3rem; margin-bottom: 1rem; opacity: 0.5;">📊</div><p>Statistics visualization unavailable.</p><p style="font-size: 0.9rem; margin-top: 0.5rem;">War Room script may not be loaded. Check browser console.</p></div>';
            }
            return;
        }
        
        try {
            const data = await this.get('/api/status');
            console.log('[Stats] Status data:', data);
            
            if (data && this.warRoom) {
                this.warRoom.update(data);
                console.log('[Stats] War room updated');
            }
            
            // Load anomalies and temporal data
            try {
                const anomalies = await this.get('/api/anomalies');
                if (anomalies && anomalies.anomalies && anomalies.anomalies.length > 0) {
                    this.showAnomalies(anomalies);
                }
            } catch (err) {
                console.warn('[Stats] Failed to load anomalies:', err);
            }
            
            try {
                const temporal = await this.get('/api/temporal');
                if (temporal && temporal.beacons && temporal.beacons.length > 0) {
                    this.showBeacons(temporal);
                }
            } catch (err) {
                console.warn('[Stats] Failed to load temporal data:', err);
            }
        } catch (error) {
            console.error('[Stats] Failed to update:', error);
            const container = document.getElementById('war-room-container');
            if (container) {
                container.innerHTML = '<div class="error-state"><p class="error">Failed to load statistics: ' + error.message + '</p></div>';
            }
        }
    }
    
    showAnomalies(data) {
        let html = '<div style="background: rgba(255, 99, 71, 0.1); padding: 1rem; border-radius: 8px; border-left: 3px solid var(--danger); margin-bottom: 1rem;">';
        html += '<h4 style="color: var(--danger); margin: 0 0 1rem 0;">🚨 Network Anomalies (' + data.unacknowledged + ' new)</h4>';
        
        data.anomalies.slice(0, 5).forEach(anom => {
            const typeEmoji = {'packet_size': '📦', 'relay_hops': '↻', 'rate_limit': '⚡', 'rssi_variance': '📡', 'replay': '🔁', 'timing': '⏱️'}[anom.type] || '⚠️';
            html += '<div style="background: rgba(255,255,255,0.03); padding: 0.75rem; border-radius: 4px; margin-bottom: 0.5rem; font-size: 0.9rem;">';
            html += `<div style="display: flex; justify-content: space-between;"><span>${typeEmoji} <strong>0x${anom.nodeId.toString(16)}</strong></span>`;
            html += `<span style="color: var(--text-secondary); font-size: 0.85rem;">${new Date(anom.timestamp).toLocaleTimeString()}</span></div>`;
            html += `<div style="color: var(--text-secondary); margin-top: 0.25rem;">${anom.description}</div></div>`;
        });
        
        html += '</div>';
        const warRoom = document.getElementById('war-room-container');
        if (warRoom) warRoom.insertAdjacentHTML('afterbegin', html);
    }
    
    showBeacons(data) {
        if (!data.beacons || data.beacons.length === 0) return;
        
        let html = '<div style="background: rgba(74, 144, 226, 0.1); padding: 1rem; border-radius: 8px; border-left: 3px solid var(--primary); margin-bottom: 1rem;">';
        html += '<h4 style="color: var(--primary); margin: 0 0 1rem 0;">📡 Detected Beacons (' + data.beacons.length + ')</h4>';
        html += '<p style="font-size: 0.85rem; color: var(--text-secondary); margin: 0 0 1rem 0;">Devices with \u226570% periodicity confidence</p>';
        
        data.beacons.slice(0, 5).forEach(beacon => {
            const intervalSec = (beacon.avgInterval / 1000).toFixed(1);
            html += '<div style="background: rgba(255,255,255,0.03); padding: 0.75rem; border-radius: 4px; margin-bottom: 0.5rem; font-size: 0.9rem;">';
            html += `<div style="display: flex; justify-content: space-between;"><span><strong>0x${beacon.nodeId.toString(16)}</strong></span>`;
            html += `<span style="color: var(--success); font-weight: 600;">${beacon.periodicityScore}% confidence</span></div>`;
            html += `<div style="color: var(--text-secondary); margin-top: 0.25rem;">Interval: ${intervalSec}s • Packets: ${beacon.packetCount}</div></div>`;
        });
        
        html += '</div>';
        const warRoom = document.getElementById('war-room-container');
        if (warRoom) warRoom.insertAdjacentHTML('afterbegin', html);
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
                this.el.gpsContent.innerHTML = `
                    <div class="placeholder">
                        <div style="font-size: 3rem; margin-bottom: 1rem; opacity: 0.5;">📍</div>
                        <p>No GPS data captured yet.</p>
                        <p style="font-size: 0.9rem; margin-top: 0.5rem;">Device positions will appear here once discovered during reconnaissance.</p>
                    </div>
                `;
            }
        } catch (error) {
            console.error('Failed to load GPS:', error);
            this.el.gpsContent.innerHTML = '<div class="error-state"><p class="error">Failed to load GPS data</p><button data-action="retry-gps" class="btn btn-primary">Retry</button></div>';
        }
        
        // Load security analysis from /api/recon/security
        try {
            const secData = await this.get('/api/recon/security');
            
            if (secData && secData.status === 'success' && secData.devices) {
                const devices = secData.devices;
                const summary = secData.summary || {};
                let html = '<div style="padding: 1rem;">';
                
                // Summary Card
                const totalDevices = summary.totalDevices || 0;
                const vulnerable = summary.vulnerable || 0;
                const moderate = summary.moderate || 0;
                const secure = summary.secure || 0;
                
                if (totalDevices > 0) {
                    html += '<div style="background: rgba(255, 255, 255, 0.05); padding: 1.5rem; border-radius: 8px; margin-bottom: 1.5rem; border-left: 3px solid var(--primary);">';
                    html += '<h4 style="margin: 0 0 1rem 0; color: var(--primary);">🔐 Network Security Overview</h4>';
                    html += '<div style="display: grid; grid-template-columns: repeat(3, 1fr); gap: 1rem;">';
                    
                    html += '<div><div style="font-size: 0.75rem; color: var(--text-secondary); margin-bottom: 0.25rem;">Secure</div>';
                    html += '<div style="font-size: 1.5rem; font-weight: 700; color: var(--success); font-family: monospace;">' + secure + '</div></div>';
                    
                    html += '<div><div style="font-size: 0.75rem; color: var(--text-secondary); margin-bottom: 0.25rem;">Moderate Risk</div>';
                    html += '<div style="font-size: 1.5rem; font-weight: 700; color: var(--warning); font-family: monospace;">' + moderate + '</div></div>';
                    
                    html += '<div><div style="font-size: 0.75rem; color: var(--text-secondary); margin-bottom: 0.25rem;">Vulnerable</div>';
                    html += '<div style="font-size: 1.5rem; font-weight: 700; color: var(--danger); font-family: monospace;">' + vulnerable + '</div></div>';
                    
                    html += '</div></div>';
                    
                    // Device List - Sort by vulnerability score (lowest first = most vulnerable)
                    if (devices.length > 0) {
                        html += '<h4 style="color: var(--primary); margin-bottom: 1rem;">📋 Device Security Assessment</h4>';
                        
                        // Sort devices by vulnerability score (ascending)
                        const sortedDevices = [...devices].sort((a, b) => a.score - b.score);
                        
                        sortedDevices.forEach(dev => {
                            const riskColor = dev.riskLevel === 'secure' ? 'var(--success)' : 
                                             dev.riskLevel === 'moderate' ? 'var(--warning)' : 'var(--danger)';
                            
                            html += '<div style="background: rgba(255, 255, 255, 0.03); padding: 1rem; border-radius: 8px; margin-bottom: 1rem; border-left: 3px solid ' + riskColor + ';">';
                            
                            html += '<div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 0.75rem;">';
                            html += '<div><strong style="color: var(--primary); font-family: monospace;">0x' + dev.nodeId + '</strong>';
                            html += '<span style="margin-left: 0.5rem; color: var(--text-secondary); font-size: 0.9rem;">' + dev.protocol + '</span></div>';
                            html += '<div style="background: ' + riskColor + '; color: #000; padding: 0.25rem 0.75rem; border-radius: 4px; font-size: 0.85rem; font-weight: 600; text-transform: uppercase;">' + dev.riskLevel + '</div>';
                            html += '</div>';
                            
                            html += '<div style="font-size: 0.85rem; color: var(--text-secondary); margin-bottom: 0.5rem;">';
                            html += 'Score: <strong style="color: ' + riskColor + ';">' + dev.score + '/100</strong> • ';
                            html += 'RSSI: <strong>' + dev.bestRSSI + ' dBm</strong> • ';
                            html += 'Packets: <strong>' + dev.packetCount + '</strong>';
                            html += '</div>';
                            
                            if (dev.findings && dev.findings.length > 0) {
                                html += '<ul style="margin: 0.5rem 0 0 1.5rem; padding: 0; color: var(--text-secondary); font-size: 0.9rem; line-height: 1.6;">';
                                dev.findings.forEach(finding => {
                                    html += '<li>' + finding + '</li>';
                                });
                                html += '</ul>';
                            }
                            
                            html += '</div>';
                        });
                    }
                } else {
                    html += '<div class="placeholder"><div style="font-size: 3rem; margin-bottom: 1rem; opacity: 0.5;">🔐</div>';
                    html += '<p>No devices to analyze yet.</p>';
                    html += '<p style="font-size: 0.9rem; margin-top: 0.5rem;">Security assessment will appear once devices are discovered.</p></div>';
                }
                
                html += '</div>';
                this.el.securityContent.innerHTML = html;
            } else {
                // Fallback to basic statistics if security endpoint returns no data
                const statsData = await this.get('/api/statistics');
                if (statsData && statsData.statistics) {
                    const stats = statsData.statistics;
                    let html = '<div style="padding: 1rem;">';
                    html += '<p style="color: var(--text-secondary); margin-bottom: 1rem;">Basic packet statistics:</p>';
                    html += '<div style="display: grid; grid-template-columns: repeat(2, 1fr); gap: 1rem;">';
                    html += '<div style="background: rgba(74, 144, 226, 0.1); padding: 1rem; border-radius: 8px; border-left: 3px solid var(--primary);">';
                    html += '<div style="font-size: 0.75rem; color: var(--text-secondary); margin-bottom: 0.5rem;">Total Packets</div>';
                    html += '<div style="font-size: 1.8rem; font-weight: 700; color: var(--primary); font-family: monospace;">' + (stats.totalPackets || 0) + '</div></div>';
                    html += '<div style="background: rgba(80, 200, 120, 0.1); padding: 1rem; border-radius: 8px; border-left: 3px solid var(--secondary);">';
                    html += '<div style="font-size: 0.75rem; color: var(--text-secondary); margin-bottom: 0.5rem;">Devices</div>';
                    html += '<div style="font-size: 1.8rem; font-weight: 700; color: var(--secondary); font-family: monospace;">' + (stats.totalDevices || 0) + '</div></div>';
                    html += '</div><p style="margin-top: 1.5rem; font-size: 0.9rem; color: var(--text-secondary); font-style: italic;">Detailed security analysis available after capturing encrypted packets.</p></div>';
                    this.el.securityContent.innerHTML = html;
                } else {
                    this.el.securityContent.innerHTML = `
                        <div class="placeholder">
                            <div style="font-size: 3rem; margin-bottom: 1rem; opacity: 0.5;">🔐</div>
                            <p>No analysis available yet.</p>
                            <p style="font-size: 0.9rem; margin-top: 0.5rem;">Statistics will appear as packets are captured.</p>
                        </div>
                    `;
                }
            }
        } catch (error) {
            console.error('Failed to load security data:', error);
            this.el.securityContent.innerHTML = '<div class="error-state"><p class="error">Failed to load security data</p><button data-action="retry-security" class="btn btn-primary">Retry</button></div>';
        }
        
        // Load frequency analysis from /api/activity
        try {
            const freqData = await this.get('/api/activity');
            if (freqData && freqData.activities && freqData.activities.length > 0) {
                let html = '<div style="padding: 1rem;">';
                
                // Sort by packet count
                const activeFreqs = freqData.activities
                    .filter(f => f.packets > 0)
                    .sort((a, b) => b.packets - a.packets)
                    .slice(0, 10); // Top 10
                
                if (activeFreqs.length > 0) {
                    html += '<p style="margin-bottom: 1rem; color: var(--text-secondary);">Top ' + activeFreqs.length + ' most active frequencies:</p>';
                    html += '<div style="display: flex; flex-direction: column; gap: 1rem;">';
                    
                    activeFreqs.forEach(freq => {
                        const maxPackets = Math.max(...activeFreqs.map(f => f.packets));
                        const barWidth = ((freq.packets / maxPackets) * 100).toFixed(1);
                        html += '<div style="background: rgba(255,255,255,0.03); padding: 1rem; border-radius: 8px; border-left: 3px solid var(--primary);">';
                        html += `<div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 0.5rem;">`;
                        html += `<strong style="color: var(--primary);">${freq.protocol}</strong>`;
                        html += `<span style="font-family: 'Courier New', monospace; color: var(--text-secondary);">${freq.frequencyMHz.toFixed(3)} MHz</span>`;
                        html += `</div>`;
                        html += `<div style="font-size: 0.85rem; color: var(--text-secondary); margin-bottom: 0.5rem;">`;
                        html += `SF${freq.spreadingFactor} • BW ${freq.bandwidthKHz}kHz • Config #${freq.configIndex}`;
                        html += `</div>`;
                        html += `<div style="display: flex; justify-content: space-between; font-size: 0.9rem; margin-bottom: 0.25rem;">`;
                        html += `<span>Packets: <strong>${freq.packets}</strong></span>`;
                        html += `<span>Avg RSSI: <strong class="${freq.avgRSSI > -70 ? 'rssi-strong' : freq.avgRSSI > -90 ? 'rssi-medium' : 'rssi-weak'}">${freq.avgRSSI.toFixed(1)} dBm</strong></span>`;
                        html += `</div>`;
                        html += `<div style="height: 6px; background: rgba(255,255,255,0.05); border-radius: 3px; overflow: hidden;">`;
                        html += `<div style="height: 100%; width: ${barWidth}%; background: var(--primary); border-radius: 3px; transition: width 0.3s;"></div>`;
                        html += `</div></div>`;
                    });
                    
                    html += '</div>';
                } else {
                    html += '<p style="text-align: center; color: var(--text-secondary); padding: 2rem;">No frequency activity detected yet. Start reconnaissance to see active frequencies.</p>';
                }
                
                html += '</div>';
                const freqAnalysisEl = document.getElementById('frequency-analysis-content');
                if (freqAnalysisEl) freqAnalysisEl.innerHTML = html;
            } else {
                const freqAnalysisEl = document.getElementById('frequency-analysis-content');
                if (freqAnalysisEl) {
                    freqAnalysisEl.innerHTML = `
                        <div class="placeholder">
                            <div style="font-size: 3rem; margin-bottom: 1rem; opacity: 0.5;">📊</div>
                            <p>No frequency data yet.</p>
                            <p style="font-size: 0.9rem; margin-top: 0.5rem;">Frequency analysis will appear during reconnaissance.</p>
                        </div>
                    `;
                }
            }
        } catch (error) {
            console.error('Failed to load frequency analysis:', error);
            const freqAnalysisEl = document.getElementById('frequency-analysis-content');
            if (freqAnalysisEl) {
                freqAnalysisEl.innerHTML = '<div class="error-state"><p class="error">Failed to load frequency data</p><button data-action="retry-freq-analysis" class="btn btn-primary">Retry</button></div>';
            }
        }
    }

    async showSettings() {
        // Show loading state
        this.el.settingsContent.innerHTML = '<div class="loading-container"><div class="loading-spinner"></div><p>Loading configuration...</p></div>';
        
        // Load WiFi status
        await this.loadWiFiStatus();
        
        try {
            const response = await this.get('/api/config');
            console.log('[Settings] API response:', response);
            
            if (!response || !response.config) {
                this.el.settingsContent.innerHTML = '<p class="placeholder">Configuration unavailable.</p>';
                return;
            }
            
            const config = response.config;
            
            let html = '<div style="padding: 1rem;">';
            
            // System Configuration
            html += '<div class="info-section"><h3>⚙️ System Configuration</h3><div class="content-area">';
            html += '<div class="status-row"><span>Scan Dwell Time</span><span class="status-value">' + config.scanDwellTime + ' ms</span></div>';
            html += '<div class="status-row"><span>Queue Size</span><span class="status-value">' + config.queueSize + '</span></div>';
            html += '<div class="status-row"><span>Max Devices</span><span class="status-value">' + config.maxDevices + '</span></div>';
            html += '<div class="status-row"><span>Device Timeout</span><span class="status-value">' + (config.deviceTimeout / 1000) + ' seconds</span></div>';
            html += '</div></div>';

            
            html += '</div>';
            this.el.settingsContent.innerHTML = html;
        } catch (error) {
            console.error('Failed to load settings:', error);
            this.el.settingsContent.innerHTML = '<div class="error-state"><p class="error">Failed to load configuration</p><p style="font-size: 0.85em; color: #999; margin-top: 0.5rem;">' + error.message + '</p></div>';
        }
        
        // Setup OTA form handler
        this.setupOTAUpload();
    }
    
    async loadWiFiStatus() {
        try {
            const wifi = await this.get('/api/wifi/status');
            console.log('[WiFi] Status:', wifi);
            
            // Update WiFi status elements
            const deviceIdEl = document.getElementById('wifi-device-id');
            const modeEl = document.getElementById('wifi-mode');
            const ssidEl = document.getElementById('wifi-ssid');
            const ipEl = document.getElementById('wifi-ip');
            const rssiEl = document.getElementById('wifi-rssi');
            const storedEl = document.getElementById('wifi-stored');
            const apSsidEl = document.getElementById('wifi-ap-ssid');
            
            if (deviceIdEl) {
                deviceIdEl.innerHTML = '<code style="background: var(--bg-secondary); padding: 2px 6px; border-radius: 4px;">' + 
                    (wifi.deviceId || '—') + '</code>';
            }
            if (modeEl) {
                if (wifi.mode === 'setup') {
                    modeEl.innerHTML = '<span style="color: var(--warning);">📱 Setup Mode (AP)</span>';
                } else if (wifi.wifiMode === 'STA') {
                    modeEl.innerHTML = '<span style="color: var(--success);">✓ Connected to Hotspot</span>';
                } else {
                    modeEl.textContent = wifi.wifiMode || 'AP';
                }
            }
            if (ssidEl) ssidEl.textContent = wifi.ssid || '—';
            if (ipEl) ipEl.textContent = wifi.ip || '—';
            if (rssiEl) {
                if (wifi.wifiMode === 'STA' && wifi.rssi) {
                    const rssiVal = wifi.rssi;
                    const rssiClass = rssiVal > -60 ? 'var(--success)' : rssiVal > -75 ? 'var(--warning)' : 'var(--danger)';
                    rssiEl.innerHTML = '<span style="color: ' + rssiClass + ';">' + rssiVal + ' dBm</span>';
                } else {
                    rssiEl.textContent = 'N/A';
                }
            }
            if (storedEl) {
                storedEl.textContent = wifi.hasStoredCredentials ? wifi.storedSSID : 'None';
            }
            if (apSsidEl) {
                apSsidEl.innerHTML = '<code style="background: var(--bg-secondary); padding: 2px 6px; border-radius: 4px;">' + 
                    (wifi.apSSID || 'LoRa-XXXXXX') + '</code>';
            }
            
            // Show setup banner if in setup mode
            this.updateSetupBanner(wifi.mode === 'setup', wifi.apSSID);
            
        } catch (error) {
            console.error('Failed to load WiFi status:', error);
        }
    }
    
    updateSetupBanner(isSetupMode, apSSID) {
        const banner = document.getElementById('setup-banner');
        const bannerSsid = document.getElementById('banner-ssid');
        if (banner) {
            banner.style.display = isSetupMode ? 'block' : 'none';
            if (bannerSsid && apSSID) {
                bannerSsid.textContent = apSSID;
            }
            if (isSetupMode) {
                document.body.classList.add('setup-mode');
            } else {
                document.body.classList.remove('setup-mode');
            }
        }
    }
    
    showWiFiSetupModal() {
        const modal = document.getElementById('wifi-modal');
        if (modal) {
            modal.style.display = 'flex';
            
            // Focus on SSID input
            setTimeout(() => {
                const ssidInput = document.getElementById('hotspot-ssid');
                if (ssidInput) ssidInput.focus();
            }, 100);
            
            // Setup form submission
            const form = document.getElementById('wifi-form');
            if (form && !form.hasAttribute('data-initialized')) {
                form.setAttribute('data-initialized', 'true');
                form.addEventListener('submit', async (e) => {
                    e.preventDefault();
                    await this.submitWiFiCredentials();
                });
            }
        }
    }
    
    closeModal() {
        const modal = document.getElementById('wifi-modal');
        if (modal) {
            modal.style.display = 'none';
        }
    }
    
    async submitWiFiCredentials() {
        const ssidInput = document.getElementById('hotspot-ssid');
        const passwordInput = document.getElementById('hotspot-password');
        
        const ssid = ssidInput ? ssidInput.value.trim() : '';
        const password = passwordInput ? passwordInput.value : '';
        
        if (!ssid) {
            showToast('Please enter your hotspot name', 'warning');
            return;
        }
        
        try {
            showToast('Saving credentials...', 'info');
            
            const response = await this.post('/api/wifi/configure', {
                ssid: ssid,
                password: password
            });
            
            showToast('Credentials saved! Device restarting...', 'success');
            this.closeModal();
            
            // Show reconnection message
            setTimeout(() => {
                showToast('Connect to your hotspot WiFi, then refresh this page', 'info');
            }, 2000);
            
        } catch (error) {
            showToast('Failed to save credentials: ' + error.message, 'error');
        }
    }

    setupOTAUpload() {
        const otaForm = document.getElementById('ota-form');
        if (!otaForm) return;
        
        otaForm.addEventListener('submit', async (e) => {
            e.preventDefault();
            
            const fileInput = document.getElementById('firmware-file');
            const file = fileInput.files[0];
            
            if (!file) {
                showToast('Please select a firmware file', 'warning');
                return;
            }
            
            if (!file.name.endsWith('.bin')) {
                showToast('Only .bin files are supported', 'error');
                return;
            }
            
            if (file.size > 2 * 1024 * 1024) {
                showToast('Firmware file is too large (max 2MB)', 'error');
                return;
            }
            
            if (!confirm('Upload firmware and reboot device? This will disconnect temporarily.')) {
                return;
            }
            
            const uploadBtn = document.getElementById('ota-upload-btn');
            const progressContainer = document.getElementById('ota-progress');
            const progressBar = document.getElementById('ota-progress-bar');
            const progressText = document.getElementById('ota-progress-text');
            
            try {
                // Disable button and show progress
                uploadBtn.disabled = true;
                progressContainer.style.display = 'block';
                
                const formData = new FormData();
                formData.append('firmware', file);
                
                const xhr = new XMLHttpRequest();
                
                xhr.upload.addEventListener('progress', (e) => {
                    if (e.lengthComputable) {
                        const percent = Math.round((e.loaded / e.total) * 100);
                        progressBar.style.width = percent + '%';
                        progressText.textContent = percent + '%';
                    }
                });
                
                xhr.addEventListener('load', () => {
                    if (xhr.status === 200) {
                        showToast('Firmware uploaded successfully! Device rebooting...', 'success');
                        progressText.textContent = 'Rebooting...';
                        
                        // Wait 10 seconds then try to reconnect
                        setTimeout(() => {
                            window.location.reload();
                        }, 10000);
                    } else {
                        showToast('Upload failed: HTTP ' + xhr.status, 'error');
                        uploadBtn.disabled = false;
                        progressContainer.style.display = 'none';
                    }
                });
                
                xhr.addEventListener('error', () => {
                    showToast('Upload failed: Network error', 'error');
                    uploadBtn.disabled = false;
                    progressContainer.style.display = 'none';
                });
                
                xhr.open('POST', '/api/firmware/upload');
                xhr.send(formData);
                
            } catch (error) {
                showToast('Upload failed: ' + error.message, 'error');
                uploadBtn.disabled = false;
                progressContainer.style.display = 'none';
            }
        });
    }

    // ============ Button Handlers ============
    
    setupButtons() {
        document.addEventListener('click', (e) => {
            const btn = e.target.closest('button[data-action]');
            if (!btn) return;
            
            e.preventDefault();
            const action = btn.dataset.action;
            const value = btn.dataset.value;
            this.handleAction(action, value, e);
        });
    }

    async handleAction(action, value, event) {
        const btn = event ? event.target.closest('button') : null;
        const originalText = btn ? btn.innerHTML : '';
        const originalDisabled = btn ? btn.disabled : false;
        
        // Add loading state to button
        if (btn && !action.startsWith('retry-')) {
            btn.disabled = true;
            btn.classList.add('btn-loading');
        }
        
        try {
            switch(action) {
                case 'toggle-scan':
                case 'resume-recon':
                    await this.post('/api/scan/start', {});
                    showToast('Reconnaissance resumed', 'success');
                    await this.updateStatus();
                    break;
                case 'stop-capture':
                    await this.post('/api/capture/stop', {});
                    showToast('Capture stopped', 'success');
                    break;
                case 'export-kml':
                    try {
                        const gpsData = await this.get('/api/positions');
                        if (gpsData && gpsData.positions && gpsData.positions.length > 0) {
                            window.open('/api/export/kml', '_blank');
                            showToast('Exporting ' + gpsData.positions.length + ' GPS location(s)...', 'success');
                        } else {
                            showToast('No GPS data to export. Capture packets with location data first.', 'warning');
                        }
                    } catch (error) {
                        showToast('GPS export failed: ' + error.message, 'error');
                    }
                    break;
                case 'clear-packets':
                    await this.post('/api/replay/clear', {});
                    showToast('Packets cleared', 'success');
                    await this.showPackets();
                    break;
                case 'clear-devices':
                    await this.post('/api/devices/clear', {});
                    showToast('Devices cleared', 'success');
                    await this.showDevices();
                    await this.updateStatus();
                    break;
                case 'show-activity':
                    // Show frequency analysis section in Info tab
                    this.switchTab('info');
                    this.closeMobileMenu();
                    // Scroll to frequency analysis
                    setTimeout(() => {
                        const freqSection = document.getElementById('frequency-analysis-content');
                        if (freqSection) {
                            freqSection.scrollIntoView({ behavior: 'smooth', block: 'start' });
                        }
                    }, 100);
                    break;
                case 'show-security':
                    // Show security assessment modal
                    await this.showSecurityAssessment();
                    this.closeMobileMenu();
                    break;
                case 'export-pcap':
                    try {
                        // Check if there's an active session first
                        const statusData = await this.get('/api/status');
                        if (!statusData || statusData.mode === 'idle') {
                            showToast('No active reconnaissance session. Start scanning first.', 'warning');
                            break;
                        }
                        
                        showToast('Downloading PCAP capture...', 'info');
                        
                        // Try to download PCAP
                        const response = await fetch('/api/export/pcap');
                        
                        if (response.status === 404) {
                            const error = await response.json();
                            showToast(error.message || 'No PCAP file available', 'error');
                        } else if (response.status === 501) {
                            showToast('PCAP export is disabled on this device', 'error');
                        } else if (response.ok) {
                            // Download successful - trigger download
                            const blob = await response.blob();
                            const url = window.URL.createObjectURL(blob);
                            const a = document.createElement('a');
                            a.href = url;
                            a.download = 'lora_capture_' + Date.now() + '.pcap';
                            document.body.appendChild(a);
                            a.click();
                            window.URL.revokeObjectURL(url);
                            a.remove();
                            showToast('PCAP downloaded successfully', 'success');
                        } else {
                            showToast('PCAP export failed: HTTP ' + response.status, 'error');
                        }
                    } catch (error) {
                        showToast('PCAP export failed: ' + error.message, 'error');
                        console.error('PCAP export error:', error);
                    }
                    break;
                case 'diagnostics':
                    try {
                        showToast('Running diagnostics...', 'info');
                        const statusData = await this.get('/api/status');
                        if (statusData && statusData.status === 'success') {
                            const diag = statusData;
                            
                            // Radio is operational if we're receiving this response
                            const radioOK = true;
                            const wifiOK = (diag.clientCount !== undefined);
                            
                            // Create a nice HTML modal
                            let html = '<div style="position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.8); z-index: 9999; display: flex; align-items: center; justify-content: center; padding: 1rem;" id="diag-modal">';
                            html += '<div style="background: var(--background); border: 1px solid rgba(255,255,255,0.1); border-radius: 12px; max-width: 500px; width: 100%; max-height: 90vh; overflow-y: auto;">';
                            html += '<div style="padding: 1.5rem; border-bottom: 1px solid rgba(255,255,255,0.1);">';
                            html += '<h3 style="margin: 0; color: var(--primary);">🔧 System Diagnostics</h3></div>';
                            html += '<div style="padding: 1.5rem;">';
                            
                            // Radio Status
                            html += '<div style="margin-bottom: 1.5rem; padding: 1rem; background: rgba(255,255,255,0.03); border-radius: 8px; border-left: 3px solid ' + (radioOK ? 'var(--success)' : 'var(--danger)') + ';">';
                            html += '<div style="display: flex; justify-content: space-between; align-items: center;">';
                            html += '<strong>📡 Radio</strong>';
                            html += '<span style="color: ' + (radioOK ? 'var(--success)' : 'var(--danger)') + '; font-weight: 600;">' + (radioOK ? '✓ Operational' : '✗ Failed') + '</span>';
                            html += '</div></div>';
                            
                            // WiFi Status
                            html += '<div style="margin-bottom: 1.5rem; padding: 1rem; background: rgba(255,255,255,0.03); border-radius: 8px; border-left: 3px solid ' + (wifiOK ? 'var(--success)' : 'var(--warning)') + ';">';
                            html += '<div style="display: flex; justify-content: space-between; align-items: center;">';
                            html += '<strong>📶 WiFi AP</strong>';
                            html += '<span style="color: ' + (wifiOK ? 'var(--success)' : 'var(--warning)') + '; font-weight: 600;">' + (wifiOK ? '✓ Active (' + (diag.clientCount || 0) + ' clients)' : '✗ Inactive') + '</span>';
                            html += '</div></div>';
                            
                            // Memory
                            const heapFree = diag.freeHeap || 0;
                            const heapTotal = diag.heapSize || 1;
                            const heapPercent = ((heapFree / heapTotal) * 100).toFixed(1);
                            const heapColor = heapPercent > 50 ? 'var(--success)' : heapPercent > 25 ? 'var(--warning)' : 'var(--danger)';
                            html += '<div style="margin-bottom: 1.5rem; padding: 1rem; background: rgba(255,255,255,0.03); border-radius: 8px; border-left: 3px solid ' + heapColor + ';">';
                            html += '<div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 0.5rem;">';
                            html += '<strong>💾 Memory</strong>';
                            html += '<span style="font-family: monospace; color: var(--text-secondary);">' + this.formatBytes(heapFree) + ' / ' + this.formatBytes(heapTotal) + '</span>';
                            html += '</div>';
                            html += '<div style="height: 6px; background: rgba(255,255,255,0.1); border-radius: 3px; overflow: hidden;">';
                            html += '<div style="height: 100%; width: ' + heapPercent + '%; background: ' + heapColor + '; border-radius: 3px;"></div>';
                            html += '</div></div>';
                            
                            // Statistics
                            html += '<div style="display: grid; grid-template-columns: repeat(2, 1fr); gap: 1rem; margin-bottom: 1rem;">';
                            html += '<div style="padding: 1rem; background: rgba(74, 144, 226, 0.1); border-radius: 8px; text-align: center;">';
                            html += '<div style="font-size: 0.75rem; color: var(--text-secondary); margin-bottom: 0.25rem;">Uptime</div>';
                            html += '<div style="font-size: 1.25rem; font-weight: 700; color: var(--primary); font-family: monospace;">' + this.formatDuration(diag.uptime || 0) + '</div></div>';
                            html += '<div style="padding: 1rem; background: rgba(80, 200, 120, 0.1); border-radius: 8px; text-align: center;">';
                            html += '<div style="font-size: 0.75rem; color: var(--text-secondary); margin-bottom: 0.25rem;">Total Packets</div>';
                            html += '<div style="font-size: 1.25rem; font-weight: 700; color: var(--secondary); font-family: monospace;">' + (diag.totalPackets || 0) + '</div></div>';
                            html += '<div style="padding: 1rem; background: rgba(155, 89, 182, 0.1); border-radius: 8px; text-align: center;">';
                            html += '<div style="font-size: 0.75rem; color: var(--text-secondary); margin-bottom: 0.25rem;">Devices</div>';
                            html += '<div style="font-size: 1.25rem; font-weight: 700; color: var(--accent); font-family: monospace;">' + (diag.devices || 0) + '</div></div>';
                            
                            // Queue health metrics
                            if (diag.droppedPackets !== undefined) {
                                const totalReceived = (diag.totalPackets || 0) + (diag.droppedPackets || 0);
                                const dropRate = totalReceived > 0 ? ((diag.droppedPackets / totalReceived) * 100).toFixed(1) : 0;
                                const dropColor = dropRate > 10 ? 'var(--danger)' : dropRate > 5 ? 'var(--warning)' : 'var(--success)';
                                html += '<div style="padding: 1rem; background: rgba(231, 76, 60, 0.1); border-radius: 8px; text-align: center;">';
                                html += '<div style="font-size: 0.75rem; color: var(--text-secondary); margin-bottom: 0.25rem;">Dropped</div>';
                                html += '<div style="font-size: 1.25rem; font-weight: 700; color: ' + dropColor + '; font-family: monospace;">' + (diag.droppedPackets || 0) + '</div>';
                                html += '<div style="font-size: 0.7rem; margin-top: 0.25rem; color: ' + dropColor + ';">' + dropRate + '%</div></div>';
                                html += '<div style="padding: 1rem; background: rgba(52, 152, 219, 0.1); border-radius: 8px; text-align: center;">';
                                html += '<div style="font-size: 0.75rem; color: var(--text-secondary); margin-bottom: 0.25rem;">Peak Queue</div>';
                                html += '<div style="font-size: 1.25rem; font-weight: 700; color: var(--info); font-family: monospace;">' + (diag.peakQueueSize || 0) + '</div>';
                                html += '<div style="font-size: 0.7rem; margin-top: 0.25rem; color: var(--text-secondary);">of 100</div></div>';
                            }
                            html += '<div style="padding: 1rem; background: rgba(241, 196, 15, 0.1); border-radius: 8px; text-align: center;">';
                            html += '<div style="font-size: 0.75rem; color: var(--text-secondary); margin-bottom: 0.25rem;">Mode</div>';
                            html += '<div style="font-size: 1rem; font-weight: 600; color: var(--warning);">' + (this.formatMode(diag.mode) || '—') + '</div></div>';
                            html += '</div>';
                            
                            html += '</div>';
                            html += '<div style="padding: 1rem 1.5rem; border-top: 1px solid rgba(255,255,255,0.1); display: flex; justify-content: flex-end;">';
                            html += '<button onclick="document.getElementById(\'diag-modal\').remove()" class="btn btn-primary">Close</button>';
                            html += '</div></div></div>';
                            
                            // Add to page
                            const modal = document.createElement('div');
                            modal.innerHTML = html;
                            document.body.appendChild(modal.firstChild);
                            
                            // Close on background click
                            document.getElementById('diag-modal').addEventListener('click', (e) => {
                                if (e.target.id === 'diag-modal') {
                                    e.target.remove();
                                }
                            });
                            
                            showToast('Diagnostics completed', 'success');
                        } else {
                            showToast('Diagnostics data unavailable', 'warning');
                        }
                    } catch (error) {
                        showToast('Diagnostics failed: ' + error.message, 'error');
                    }
                    break;
                case 'reboot':
                    if (confirm('Reboot the device? This will disconnect temporarily.')) {
                        await this.post('/api/command', { command: 'b' });
                        showToast('Device rebooting...', 'warning');
                    }
                    break;
                case 'download-logs':
                    showToast('Log download not implemented', 'info');
                    break;
                case 'export-json':
                    window.open('/api/statistics', '_blank');
                    showToast('Exporting statistics...', 'info');
                    break;
                case 'center-map':
                case 'toggle-labels':
                case 'zoom-in':
                case 'zoom-out':
                    // These will be handled by network-map.js
                    break;
                case 'target-device':
                    await this.post('/api/capture/start', { nodeId: value });
                    showToast(`Targeting device 0x${value}`, 'success');
                    await this.updateStatus();
                    break;
                case 'replay-packet':
                    await this.post('/api/replay/transmit', { slotIndex: value, repeatCount: 1, delayMs: 1000 });
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
                case 'retry-freq-analysis':
                    await this.showInfo();
                    break;
                case 'wifi-setup':
                    this.showWiFiSetupModal();
                    break;
                case 'dismiss-setup':
                    // User chose to keep using device AP
                    const banner = document.getElementById('setup-banner');
                    if (banner) {
                        banner.style.display = 'none';
                        document.body.classList.remove('setup-mode');
                    }
                    showToast('Continuing with device AP. Go to Settings to configure hotspot later.', 'info');
                    break;
                case 'wifi-clear':
                    if (confirm('Clear stored hotspot credentials? The device will restart in setup mode.')) {
                        try {
                            await this.post('/api/wifi/clear', {});
                            showToast('Credentials cleared. Device restarting...', 'success');
                        } catch (error) {
                            showToast('Failed to clear credentials: ' + error.message, 'error');
                        }
                    }
                    break;
                case 'close-modal':
                    this.closeModal();
                    break;
            }
        } catch (error) {
            console.error('Action failed:', error);
            showToast('Action failed: ' + error.message, 'error');
        } finally {
            // Restore button state
            if (btn) {
                btn.disabled = originalDisabled;
                btn.classList.remove('btn-loading');
                if (originalText && !action.startsWith('retry-')) {
                    setTimeout(() => {
                        btn.innerHTML = originalText;
                    }, 300);
                }
            }
        }
    }

    // ============ Mobile Menu ============
    
    setupMobileMenu() {
        if (this.el.mobileMenuToggle) {
            this.el.mobileMenuToggle.addEventListener('click', (e) => {
                e.stopPropagation();
                const sidebar = document.querySelector('.sidebar');
                if (sidebar) {
                    const isActive = sidebar.classList.toggle('active');
                    this.el.mobileMenuOverlay.classList.toggle('active', isActive);
                    this.el.mobileMenuToggle.classList.toggle('active', isActive);
                    
                    // Prevent body scroll when menu is open
                    document.body.style.overflow = isActive ? 'hidden' : '';
                }
            });
        }
        
        if (this.el.mobileMenuOverlay) {
            this.el.mobileMenuOverlay.addEventListener('click', () => {
                this.closeMobileMenu();
            });
        }
        
        // Close mobile menu when clicking a button
        document.addEventListener('click', (e) => {
            if (e.target.closest('.sidebar button[data-action]')) {
                const sidebar = document.querySelector('.sidebar');
                if (sidebar && sidebar.classList.contains('active')) {
                    this.closeMobileMenu();
                }
            }
        });
    }
    
    closeMobileMenu() {
        const sidebar = document.querySelector('.sidebar');
        if (sidebar) {
            sidebar.classList.remove('active');
        }
        if (this.el.mobileMenuOverlay) {
            this.el.mobileMenuOverlay.classList.remove('active');
        }
        if (this.el.mobileMenuToggle) {
            this.el.mobileMenuToggle.classList.remove('active');
        }
        document.body.style.overflow = '';
    }
    
    async showSecurityAssessment() {
        try {
            // Use backend security assessment for consistency
            const secData = await this.get('/api/recon/security');
            if (!secData || !secData.devices || secData.devices.length === 0) {
                showToast('No devices to assess', 'warning');
                return;
            }
            
            const devices = secData.devices;
            const summary = secData.summary || {};
            const totalDevices = summary.totalDevices || 0;
            const vulnerable = summary.vulnerable || 0;
            const moderate = summary.moderate || 0;
            const secure = summary.secure || 0;
            
            // Create modal
            let html = '<div style="position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.8); z-index: 9999; display: flex; align-items: center; justify-content: center; padding: 1rem;" id="security-modal">';
            html += '<div style="background: var(--background); border: 1px solid rgba(255,255,255,0.1); border-radius: 12px; max-width: 500px; width: 100%; max-height: 90vh; overflow-y: auto;">';
            html += '<div style="padding: 1.5rem; border-bottom: 1px solid rgba(255,255,255,0.1);">';
            html += '<h3 style="margin: 0; color: var(--danger);">🔒 Security Assessment</h3></div>';
            html += '<div style="padding: 1.5rem;">';
            
            // Summary
            const vulnPercent = totalDevices > 0 ? ((vulnerable / totalDevices) * 100).toFixed(0) : 0;
            const vulnColor = vulnPercent > 50 ? 'var(--danger)' : vulnPercent > 25 ? 'var(--warning)' : 'var(--success)';
            
            html += '<div style="margin-bottom: 1.5rem; padding: 1rem; background: rgba(255,255,255,0.03); border-radius: 8px; border-left: 3px solid ' + vulnColor + ';">';
            html += '<div style="font-size: 2rem; font-weight: 700; color: ' + vulnColor + '; text-align: center; margin-bottom: 0.5rem;">' + vulnPercent + '%</div>';
            html += '<div style="text-align: center; color: var(--text-secondary);">High Risk Devices</div>';
            html += '</div>';
            
            // Details
            html += '<div style="display: grid; grid-template-columns: repeat(2, 1fr); gap: 1rem; margin-bottom: 1rem;">';
            html += '<div style="padding: 1rem; background: rgba(74, 144, 226, 0.1); border-radius: 8px; text-align: center;">';
            html += '<div style="font-size: 1.5rem; font-weight: 700; color: var(--primary);">' + totalDevices + '</div>';
            html += '<div style="font-size: 0.75rem; color: var(--text-secondary);">Total Devices</div></div>';
            
            html += '<div style="padding: 1rem; background: rgba(231, 76, 60, 0.1); border-radius: 8px; text-align: center;">';
            html += '<div style="font-size: 1.5rem; font-weight: 700; color: var(--danger);">' + vulnerable + '</div>';
            html += '<div style="font-size: 0.75rem; color: var(--text-secondary);">Vulnerable</div></div>';
            
            html += '<div style="padding: 1rem; background: rgba(241, 196, 15, 0.1); border-radius: 8px; text-align: center;">';
            html += '<div style="font-size: 1.5rem; font-weight: 700; color: var(--warning);">' + moderate + '</div>';
            html += '<div style="font-size: 0.75rem; color: var(--text-secondary);">Moderate Risk</div></div>';
            
            html += '<div style="padding: 1rem; background: rgba(46, 204, 113, 0.1); border-radius: 8px; text-align: center;">';
            html += '<div style="font-size: 1.5rem; font-weight: 700; color: var(--success);">' + secure + '</div>';
            html += '<div style="font-size: 0.75rem; color: var(--text-secondary);">Secure</div></div>';
            html += '</div>';
            
            // Recommendations
            const recommendations = secData.recommendations || [];
            if (recommendations.length > 0) {
                html += '<div style="padding: 1rem; background: rgba(241, 196, 15, 0.05); border-radius: 8px; border: 1px solid rgba(241, 196, 15, 0.3);">';
                html += '<div style="font-weight: 600; margin-bottom: 0.5rem; color: var(--warning);">⚠️ Recommendations</div>';
                html += '<ul style="margin: 0; padding-left: 1.5rem; font-size: 0.9rem;">';
                recommendations.forEach(rec => {
                    html += '<li>' + rec + '</li>';
                });
                html += '</ul></div>';
            }
            
            html += '</div>';
            html += '<div style="padding: 1rem 1.5rem; border-top: 1px solid rgba(255,255,255,0.1); display: flex; justify-content: flex-end;">';
            html += '<button onclick="document.getElementById(\'security-modal\').remove()" class="btn btn-primary">Close</button>';
            html += '</div></div></div>';
            
            document.body.insertAdjacentHTML('beforeend', html);
        } catch (error) {
            showToast('Security assessment failed: ' + error.message, 'error');
        }
    }

    // ============ API Helpers ============
    
    async get(endpoint) {
        try {
            console.log('[GET]', endpoint);
            const response = await fetch(endpoint);
            console.log('[GET]', endpoint, 'status:', response.status);
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            const data = await response.json();
            console.log('[GET]', endpoint, 'success');
            return data;
        } catch (error) {
            console.error('[GET]', endpoint, 'error:', error);
            throw error;
        }
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

    formatLastSeen(secondsAgo) {
        // Backend now provides lastSeenSecondsAgo directly
        if (secondsAgo === undefined || secondsAgo === null) return 'Never';
        
        if (secondsAgo === 0) return 'Just now';
        
        if (secondsAgo < 60) {
            return `${Math.floor(secondsAgo)}s ago`;
        } else if (secondsAgo < 3600) {
            const mins = Math.floor(secondsAgo / 60);
            return `${mins}m ago`;
        } else if (secondsAgo < 86400) {
            const hours = Math.floor(secondsAgo / 3600);
            return `${hours}h ago`;
        } else {
            const days = Math.floor(secondsAgo / 86400);
            return `${days}d ago`;
        }
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


