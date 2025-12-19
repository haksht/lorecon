/* ESP32 LoRa Recon - Modular Client
 * Refactored for maintainability with:
 * - Debug flag for conditional logging
 * - Shared table/card rendering utilities
 * - Action dispatch table pattern
 * - CSS utility classes (no inline styles)
 */

// ===== Debug Configuration =====
const DEBUG = {
    enabled: false,  // Set to true for verbose logging
    log: (...args) => DEBUG.enabled && console.log('[App]', ...args),
    warn: (...args) => console.warn('[App]', ...args),
    error: (...args) => console.error('[App]', ...args)
};

// ===== Security Utilities =====
// HTML escape function to prevent XSS attacks
function escapeHtml(text) {
    if (text === null || text === undefined) return '';
    const str = String(text);
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
}

// ===== UI Helper Utilities =====
// RSSI classification helper - eliminates 4+ duplicates across codebase
function formatRSSI(rssi, includeValue = true) {
    const value = rssi || -100;
    const className = value > -70 ? 'rssi-strong' : value > -90 ? 'rssi-medium' : 'rssi-weak';
    if (includeValue) {
        return `<span class="${className}">${value.toFixed ? value.toFixed(1) : value} dBm</span>`;
    }
    return className;
}

// Get color class based on percentage threshold
function getThresholdClass(value, highThreshold = 50, medThreshold = 25) {
    if (value > highThreshold) return 'text-success';
    if (value > medThreshold) return 'text-warning';
    return 'text-danger';
}

// Error state HTML helper
function renderErrorState(message, retryAction = null) {
    let html = '<div class="error-state"><p class="error">' + escapeHtml(message) + '</p>';
    if (retryAction) {
        html += `<button data-action="${retryAction}" class="btn btn-primary">Retry</button>`;
    }
    html += '</div>';
    return html;
}

/**
 * Standardized error handler for API/async operations
 * Logs to DEBUG and optionally shows user-facing toast
 * @param {Error} error - The caught error
 * @param {string} context - Description of what operation failed (e.g., 'loading devices')
 * @param {Object} [options={}] - Handler options
 * @param {boolean} [options.silent=false] - If true, skip toast notification
 * @param {boolean} [options.rethrow=false] - If true, rethrow after handling
 * @returns {null} Returns null for easy chaining in catch blocks
 */
function handleError(error, context, options = {}) {
    const { silent = false, rethrow = false } = options;
    
    DEBUG.error(`Error ${context}:`, error);
    
    if (!silent && typeof showToast === 'function') {
        const userMessage = error.message || 'Unknown error';
        showToast(`Failed ${context}: ${userMessage}`, 'error');
    }
    
    if (rethrow) throw error;
    return null;
}

// Loading state HTML helper
function renderLoadingState(message = 'Loading...') {
    return `<div class="loading-container"><div class="loading-spinner"></div><p>${escapeHtml(message)}</p></div>`;
}

// Placeholder state HTML helper
function renderPlaceholder(emoji, title, subtitle = '') {
    return `<div class="placeholder">
        <div class="emoji-large text-muted">${emoji}</div>
        <p>${escapeHtml(title)}</p>
        ${subtitle ? `<p class="text-sm mt-sm text-muted">${escapeHtml(subtitle)}</p>` : ''}
    </div>`;
}

// ===== Shared Table Rendering =====
/**
 * Table rendering utilities for consistent data tables
 * @namespace TableRenderer
 */
const TableRenderer = {
    /**
     * Render a complete HTML table with headers and rows
     * @param {string[]} columns - Array of column header names
     * @param {Array} rows - Array of row data objects
     * @param {Function} rowRenderer - Function(row, index) that returns HTML string for a table row
     * @param {Object} [options={}] - Optional configuration
     * @param {string} [options.tableClass='table'] - CSS class for table element
     * @param {string} [options.wrapperClass='table-wrapper'] - CSS class for wrapper div
     * @returns {string} Complete HTML table string
     * @example
     * TableRenderer.render(['Name', 'Value'], data, (row, i) => `<tr><td>${row.name}</td></tr>`);
     */
    render(columns, rows, rowRenderer, options = {}) {
        const { tableClass = 'table', wrapperClass = 'table-wrapper' } = options;
        let html = `<div class="${wrapperClass}">`;
        html += `<table class="${tableClass}"><thead><tr>`;
        columns.forEach(col => { html += `<th>${col}</th>`; });
        html += '</tr></thead><tbody>';
        rows.forEach((row, idx) => { html += rowRenderer(row, idx); });
        html += '</tbody></table></div>';
        return html;
    },
    
    /**
     * Common table cell formatters
     * @namespace TableRenderer.cell
     */
    cell: {
        /**
         * Render a code-formatted cell
         * @param {string} value - Cell value
         * @param {string} [prefix=''] - Optional prefix before value
         * @returns {string} HTML td element
         */
        code: (value, prefix = '') => `<td><code>${prefix}${escapeHtml(value)}</code></td>`,
        
        /**
         * Render a text cell (HTML escaped)
         * @param {string} value - Cell value
         * @returns {string} HTML td element
         */
        text: (value) => `<td>${escapeHtml(value)}</td>`,
        
        /**
         * Render a raw HTML cell (use with caution - no escaping)
         * @param {string} value - HTML content
         * @returns {string} HTML td element
         */
        html: (value) => `<td>${value}</td>`,
        
        /**
         * Render an RSSI signal strength cell with color coding
         * @param {number} value - RSSI value in dBm
         * @returns {string} HTML td element with styled span
         */
        rssi: (value) => `<td>${formatRSSI(value)}</td>`,
        
        /**
         * Render a badge cell
         * @param {string} text - Badge text
         * @param {string} className - CSS class for styling
         * @returns {string} HTML td element with span
         */
        badge: (text, className) => `<td><span class="${className}">${text}</span></td>`,
        
        /**
         * Render an action button cell
         * @param {string} action - data-action attribute value
         * @param {string} value - data-value attribute value
         * @param {string} text - Button label text
         * @param {string} [icon=''] - Optional emoji/icon prefix
         * @returns {string} HTML td element with button
         */
        button: (action, value, text, icon = '') => 
            `<td><button data-action="${action}" data-value="${escapeHtml(value)}" class="btn btn-primary btn-small">${icon} ${text}</button></td>`
    }
};

// ===== Shared Card Rendering =====
/**
 * Card rendering utilities for metrics, status, and info cards
 * @namespace CardRenderer
 */
const CardRenderer = {
    /**
     * Render a metric display card
     * @param {string} label - Metric label text
     * @param {string|number} value - Display value (can include HTML)
     * @param {string} [colorClass='text-primary'] - CSS color class for value
     * @param {string} [bgClass='card-primary'] - CSS background class for card
     * @returns {string} HTML card element
     * @example
     * CardRenderer.metric('Devices', 42, 'text-success', 'card-success');
     */
    metric(label, value, colorClass = 'text-primary', bgClass = 'card-primary') {
        return `<div class="metric-card ${bgClass}">
            <div class="metric-label">${escapeHtml(label)}</div>
            <div class="metric-value ${colorClass}">${value}</div>
        </div>`;
    },
    
    /**
     * Render a status indicator card with optional progress bar
     * @param {string} label - Status label
     * @param {string} value - Status value text
     * @param {string} colorClass - CSS color class (e.g., 'text-success')
     * @param {boolean} [showProgress=false] - Whether to show progress bar
     * @param {number} [percent=0] - Progress percentage (0-100)
     * @returns {string} HTML status card element
     * @example
     * CardRenderer.status('Memory', '45%', 'text-warning', true, 45);
     */
    status(label, value, colorClass, showProgress = false, percent = 0) {
        let html = `<div class="status-indicator card-base ${colorClass.replace('text-', 'card-')}">
            <div class="flex-between mb-sm">
                <strong>${escapeHtml(label)}</strong>
                <span class="${colorClass} text-bold">${value}</span>
            </div>`;
        if (showProgress) {
            html += `<div class="progress-track">
                <div class="progress-fill" style="width: ${percent}%; background: var(--${colorClass.replace('text-', '')})"></div>
            </div>`;
        }
        html += '</div>';
        return html;
    },
    
    /**
     * Render an item card with header badge and content
     * @param {string} header - Header text/HTML
     * @param {string} headerBadge - Badge text
     * @param {string} badgeClass - CSS class for badge color (e.g., 'text-success')
     * @param {string} content - Card body content HTML
     * @returns {string} HTML item card element
     */
    item(header, headerBadge, badgeClass, content) {
        const colorName = badgeClass.replace('text-', '');
        return `<div class="item-card item-card-${colorName}">
            <div class="item-card-header">
                <div>${header}</div>
                <div class="item-badge item-badge-${colorName}">${headerBadge}</div>
            </div>
            ${content}
        </div>`;
    },
    
    /**
     * Render a section with header
     * @param {string} title - Section title
     * @param {string} content - Section content HTML
     * @param {string} [headerClass='section-header'] - CSS class for header
     * @returns {string} HTML section with h4 header
     */
    section(title, content, headerClass = 'section-header') {
        return `<h4 class="${headerClass}">${title}</h4>${content}`;
    }
};

// ===== Modal Rendering =====
/**
 * Modal dialog rendering utilities
 * @namespace ModalRenderer
 */
const ModalRenderer = {
    /**
     * Create modal HTML structure
     * @param {string} id - Unique modal ID for targeting
     * @param {string} title - Modal title text
     * @param {string} content - Modal body HTML content
     * @param {string} [titleColor='text-primary'] - CSS class for title color
     * @returns {string} Complete modal HTML string
     * @example
     * const html = ModalRenderer.create('my-modal', 'Settings', '<p>Content</p>');
     */
    create(id, title, content, titleColor = 'text-primary') {
        return `<div class="modal-backdrop" id="${id}">
            <div class="modal-box">
                <div class="modal-header-section">
                    <h3 class="${titleColor}">${title}</h3>
                </div>
                <div class="modal-body-section">${content}</div>
                <div class="modal-footer-section">
                    <button onclick="document.getElementById('${id}').remove()" class="btn btn-primary">Close</button>
                </div>
            </div>
        </div>`;
    },
    
    /**
     * Show a modal by injecting HTML into DOM
     * Automatically sets up backdrop click to close
     * @param {string} html - Modal HTML from create()
     */
    show(html) {
        const container = document.createElement('div');
        container.innerHTML = html;
        document.body.appendChild(container.firstChild);
        
        // Close on backdrop click
        const modal = document.body.lastElementChild;
        modal.addEventListener('click', (e) => {
            if (e.target === modal) modal.remove();
        });
    }
};

// Dependency checks - ensure scripts loaded in correct order
if (typeof showToast === 'undefined') {
    DEBUG.error('FATAL: toast.js must be loaded before app.js');
    alert('Script loading error - please refresh the page');
}
if (typeof WarRoom === 'undefined') {
    DEBUG.warn('war-room.js not loaded - War Room tab will be disabled');
}
if (typeof NetworkMap === 'undefined') {
    DEBUG.warn('network-map.js not loaded - Network Map will be disabled');
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
            DEBUG.error('Init error:', error);
        }
    }
    
    async checkSetupMode() {
        try {
            const wifi = await this.get('/api/wifi/status');
            if (wifi && wifi.mode === 'setup') {
                this.isSetupMode = true;
                this.apSSID = wifi.apSSID;
                this.updateSetupBanner(true, wifi.apSSID);
                showToast('First run! Configure your phone hotspot to get started.', 'info');
            }
        } catch (error) {
            DEBUG.log('WiFi status check failed (may be normal on first load)');
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
                DEBUG.error('WebSocket message error:', error);
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
        
        // Update Resume Scan button based on mode
        this.updateScanButton(data.mode);
        
        // Update target banner
        this.updateTargetBanner(data);
    }
    
    updateScanButton(mode) {
        const scanBtn = document.querySelector('[data-action="toggle-scan"]');
        if (!scanBtn) return;
        
        const modeStr = (mode || '').toLowerCase();
        const isTargeted = modeStr.includes('target');
        
        // Update button appearance based on mode
        if (isTargeted) {
            scanBtn.classList.add('btn-disabled');
            scanBtn.title = 'Stop targeted capture first';
            const textEl = scanBtn.querySelector('span:last-child');
            if (textEl) textEl.textContent = 'Targeting Active';
        } else {
            scanBtn.classList.remove('btn-disabled');
            scanBtn.title = '';
            const textEl = scanBtn.querySelector('span:last-child');
            if (textEl) textEl.textContent = 'Resume Scan';
        }
    }

    updateTargetBanner(data) {
        if (!this.el.targetInfo || !data.target) return;
        
        const mode = (data.mode || '').toLowerCase();
        if (mode.includes('target') && data.target.frequency) {
            this.el.targetInfo.style.display = 'block';
            let html = `<strong>Frequency:</strong> ${data.target.frequency.toFixed(3)} MHz`;
            if (data.target.nodeId) {
                html += `<br><strong>Node:</strong> 0x${escapeHtml(String(data.target.nodeId))}`;
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
                DEBUG.error('Status update failed:', error);
                if (this.consecutiveErrors === 3) {
                    DEBUG.warn('Suppressing further connection errors...');
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
        
        // Update setup banner visibility (only show on info tab)
        if (this.isSetupMode !== undefined) {
            this.updateSetupBanner(this.isSetupMode, this.apSSID);
        }
        
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
    // Each show* method handles loading and rendering for one tab.
    // Tab handlers: showDevices (line ~581), showPackets (line ~693), 
    //               showNetwork (line ~858), showStats (line ~958),
    //               showInfo (line ~1059), showSettings (line ~1254)
    
    /**
     * Load and display the Devices tab
     * Fetches /api/devices and renders device table with targeting options
     */
    async showDevices() {
        // Show loading state
        this.el.devicesContent.innerHTML = renderLoadingState('Loading devices...');
        
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
            } catch (error) {
                DEBUG.warn('Security data not available, sorting by discovery order');
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
            html += '<th>Node ID</th><th>Risk</th><th>Type</th><th>Router</th><th>Power</th><th>Pkts (Orig/Relay)</th><th>RSSI (Avg/Best)</th><th>Frequency</th><th>First Seen</th><th>Last Seen</th><th>Actions</th>';
            html += '</tr></thead><tbody>';
            
            enrichedDevices.forEach(device => {
                const rssiClass = formatRSSI(device.rssi, false);
                
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
                
                // Router indicator
                const routerBadge = device.isRouter ? '✅' : '—';
                
                // Power class indicator
                const powerBadge = device.powerClass >= 2 ? '🔋 High' : (device.powerClass === 1 ? '🔋 Med' : '🪫 Low');
                
                // Packet counts
                const origPkts = device.originatedPackets || 0;
                const relayPkts = device.relayedPackets || 0;
                const totalPkts = device.packetCount || 0;
                const pktDisplay = `${totalPkts} (${origPkts}/${relayPkts})`;
                
                // RSSI display with best
                const avgRssi = device.avgRSSI || device.rssi || 0;
                const bestRssi = device.bestRSSI || avgRssi;
                const rssiDisplay = `${avgRssi.toFixed(0)} / ${bestRssi.toFixed(0)}`;
                
                // Escape all user-derived data for XSS prevention
                const safeNodeId = escapeHtml(device.nodeId);
                const safeDeviceType = escapeHtml(device.deviceType || 'Unknown');
                
                html += '<tr>';
                html += `<td><code>0x${safeNodeId}</code></td>`;
                html += `<td><span class="${riskClass}">${riskBadge}</span></td>`;
                html += `<td>${safeDeviceType}</td>`;
                html += `<td>${routerBadge}</td>`;
                html += `<td><small>${powerBadge}</small></td>`;
                html += `<td>${pktDisplay}</td>`;
                html += `<td><span class="${rssiClass}">${rssiDisplay} dBm</span></td>`;
                html += `<td>${(device.frequency || 0).toFixed(3)} MHz</td>`;
                html += `<td>${this.formatDuration(device.firstSeenSecondsAgo || 0)} ago</td>`;
                html += `<td>${this.formatLastSeen(device.lastSeenSecondsAgo)}</td>`;
                html += `<td><button data-action="target-device" data-value="${safeNodeId}" class="btn btn-primary btn-small">🎯 Target</button></td>`;
                html += '</tr>';
            });
            
            html += '</tbody></table></div>';
            this.el.devicesContent.innerHTML = html;
        } catch (error) {
            DEBUG.error('Failed to load devices:', error);
            this.el.devicesContent.innerHTML = renderErrorState('Failed to load devices', 'retry-devices');
        }
    }

    /**
     * Load and display the Packets tab
     * Fetches /api/replay/slots and renders packet table with relay button
     */
    async showPackets() {
        // Show loading state
        this.el.packetsContent.innerHTML = renderLoadingState('Loading packets...');
        
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
            html += '<th>Protocol</th><th>From</th><th>To</th><th>Hops</th><th>Ch</th><th>Flags</th><th>Packet ID</th><th>Size</th><th>RSSI</th><th>Frequency</th><th>Captured</th><th>Message</th><th>Actions</th>';
            html += '</tr></thead><tbody>';
            
            grouped.forEach(group => {
                group.packets.forEach((pkt, idx) => {
                    const rssiClass = formatRSSI(pkt.rssi, false);
                    const isRelay = group.packets.length > 1 && idx > 0;
                    const rowClass = isRelay ? 'relay-row' : '';
                    
                    // Build flags display
                    let flags = [];
                    if (pkt.wantAck) flags.push('ACK');
                    if (pkt.viaMqtt) flags.push('MQTT');
                    if (pkt.priority > 0) flags.push('P' + pkt.priority);
                    const flagsStr = flags.length > 0 ? flags.join(' ') : '—';
                    
                    // Destination display (broadcast or specific node)
                    let destDisplay = '—';
                    if (pkt.isBroadcast === true || pkt.isBroadcast === undefined) {
                        destDisplay = '<span class="broadcast-badge">📢</span>';
                    } else if (pkt.destId) {
                        destDisplay = '<code>0x' + escapeHtml(String(pkt.destId)) + '</code>';
                    }
                    
                    html += `<tr class="${rowClass}">`;
                    html += `<td><code>${escapeHtml(pkt.protocol || 'Unknown')}</code></td>`;
                    html += `<td>${pkt.nodeId ? '<code>0x' + escapeHtml(String(pkt.nodeId)) + '</code>' : '—'}</td>`;
                    html += `<td>${destDisplay}</td>`;
                    html += `<td>${pkt.hopCount !== undefined ? pkt.hopCount : '—'}</td>`;
                    html += `<td>${pkt.channel !== undefined ? pkt.channel : '—'}</td>`;
                    html += `<td><small>${flagsStr}</small></td>`;
                    
                    // Show packet ID with relay indicator
                    if (isRelay) {
                        html += `<td><code>0x${escapeHtml(String(pkt.packetId || '—'))}</code> <span class="relay-badge">↻ Relay</span></td>`;
                    } else if (group.packets.length > 1) {
                        html += `<td><code>0x${escapeHtml(String(pkt.packetId || '—'))}</code> <span class="origin-badge">⚡ +${group.packets.length - 1} relays</span></td>`;
                    } else {
                        html += `<td>${pkt.packetId ? '<code>0x' + escapeHtml(String(pkt.packetId)) + '</code>' : '—'}</td>`;
                    }
                    
                    html += `<td>${pkt.length || 0} B</td>`;
                    html += `<td><span class="${rssiClass}">${pkt.rssi || '—'} dBm</span></td>`;
                    html += `<td>${(pkt.frequencyMHz || 0).toFixed(3)} MHz</td>`;
                    html += `<td>${this.formatDuration(pkt.capturedSecondsAgo || 0)} ago</td>`;
                    html += `<td>${escapeHtml(pkt.decryptedText || '—')}</td>`;
                    html += `<td><button data-action="replay-packet" data-value="${pkt.originalIndex}" class="btn btn-primary btn-small">🔁 Replay</button></td>`;
                    html += '</tr>';
                });
            });
            
            html += '</tbody></table></div>';
            this.el.packetsContent.innerHTML = html;
        } catch (error) {
            DEBUG.error('Failed to load packets:', error);
            this.el.packetsContent.innerHTML = renderErrorState('Failed to load packets', 'retry-packets');
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
        this.el.frequencyContent.innerHTML = renderLoadingState('Loading frequency data...');
        
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
                const rssiClass = formatRSSI(act.avgRSSI, false);
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
            DEBUG.error('Failed to load frequency data:', error);
            this.el.frequencyContent.innerHTML = renderErrorState('Failed to load frequency data', 'retry-frequency');
        }
    }

    /**
     * Load and display the Network tab
     * Initializes NetworkMap canvas and fetches /api/devices for visualization
     */
    async showNetwork() {
        DEBUG.log('Loading network map...');
        DEBUG.log('NetworkMap available?', typeof NetworkMap !== 'undefined');
        DEBUG.log('window.NetworkMap?', typeof window.NetworkMap !== 'undefined');
        
        if (!this.networkMap) {
            DEBUG.log('Initializing NetworkMap...');
            // Check both direct and window scope
            const NetworkMapClass = window.NetworkMap || NetworkMap;
            if (typeof NetworkMapClass !== 'undefined') {
                try {
                    // Use network-info as the details panel (it exists in HTML)
                    this.networkMap = new NetworkMapClass('network-canvas', 'network-info');
                    DEBUG.log('NetworkMap initialized successfully');
                } catch (error) {
                    DEBUG.error('Failed to initialize NetworkMap:', error);
                    // Show error in network info panel
                    const infoEl = document.getElementById('network-info');
                    if (infoEl) {
                        infoEl.innerHTML = '<div class="error-state"><p class="error">Network visualization unavailable: ' + escapeHtml(error.message) + '</p><p class="help-text">Check browser console for details.</p></div>';
                    }
                }
            } else {
                DEBUG.error('NetworkMap class not found! Scripts may not have loaded.');
                DEBUG.error('Available globals:', Object.keys(window).filter(k => k.includes('Map') || k.includes('Network') || k.includes('War')));
                // Show error message
                const infoEl = document.getElementById('network-info');
                if (infoEl) {
                    infoEl.innerHTML = '<div class="error-state"><p class="error">Network Map unavailable</p><p class="help-text">The network-map.js script failed to load. Try refreshing the page or check SD card/LittleFS.</p></div>';
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
            DEBUG.log('Network devices data:', data);
            
            if (data && data.devices) {
                // Fetch security data to enrich devices with vulnerability info
                let securityData = null;
                try {
                    securityData = await this.get('/api/recon/security');
                } catch (error) {
                    DEBUG.warn('Security data not available for network map');
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
                    DEBUG.log('Updated map with', enrichedDevices.length, 'enriched devices');
                } else {
                    DEBUG.warn('NetworkMap not initialized, cannot update');
                }
                
                // Update network info panel
                const infoEl = document.getElementById('network-info');
                if (infoEl) {
                    if (data.devices.length === 0) {
                        infoEl.innerHTML = '<p class="placeholder">No devices discovered yet. Start reconnaissance to populate the network map.</p>';
                    } else {
                        infoEl.innerHTML = `<p class="text-secondary small-text">Showing ${data.devices.length} device(s). Click a node for details.</p>`;
                    }
                }
            }
        } catch (error) {
            DEBUG.error('Failed to load network devices:', error);
            const infoEl = document.getElementById('network-info');
            if (infoEl) {
                infoEl.innerHTML = '<p class="error">Failed to load network data: ' + escapeHtml(error.message) + '</p>';
            }
        }
    }

    /**
     * Load and display the Stats tab (War Room)
     * Initializes WarRoom tactical dashboard with real-time metrics
     */
    async showStats() {
        DEBUG.log('Loading war room...');
        
        if (!this.warRoom) {
            DEBUG.log('Initializing WarRoom...');
            if (typeof WarRoom !== 'undefined') {
                try {
                    this.warRoom = new WarRoom('war-room-container');
                    DEBUG.log('WarRoom initialized successfully');
                } catch (error) {
                    DEBUG.error('Failed to initialize WarRoom:', error);
                }
            } else {
                DEBUG.error('WarRoom class not found!');
            }
        }
        
        if (!this.warRoom) {
            // Fallback if WarRoom class not available
            const container = document.getElementById('war-room-container');
            if (container) {
                container.innerHTML = '<div class="placeholder"><div class="emoji-large-muted">📊</div><p>Statistics visualization unavailable.</p><p class="help-text">War Room script may not be loaded. Check browser console.</p></div>';
            }
            return;
        }
        
        try {
            const data = await this.get('/api/status');
            DEBUG.log('Stats status data:', data);
            
            if (data && this.warRoom) {
                this.warRoom.update(data);
                DEBUG.log('War room updated');
            }
            
            // Load anomalies and temporal data
            try {
                const anomalies = await this.get('/api/anomalies');
                if (anomalies && anomalies.anomalies && anomalies.anomalies.length > 0) {
                    this.showAnomalies(anomalies);
                }
            } catch (error) {
                DEBUG.warn('Failed to load anomalies:', error);
            }
            
            try {
                const temporal = await this.get('/api/temporal');
                if (temporal && temporal.beacons && temporal.beacons.length > 0) {
                    this.showBeacons(temporal);
                }
            } catch (error) {
                DEBUG.warn('Failed to load temporal data:', error);
            }
        } catch (error) {
            DEBUG.error('Failed to update stats:', error);
            const container = document.getElementById('war-room-container');
            if (container) {
                container.innerHTML = '<div class="error-state"><p class="error">Failed to load statistics: ' + error.message + '</p></div>';
            }
        }
    }
    
    showAnomalies(data) {
        let html = '<div class="alert-box alert-box-danger">';
        html += '<h4>🚨 Network Anomalies (' + data.unacknowledged + ' new)</h4>';
        
        data.anomalies.slice(0, 5).forEach(anom => {
            const typeEmoji = {'packet_size': '📦', 'relay_hops': '↻', 'rate_limit': '⚡', 'rssi_variance': '📡', 'replay': '🔁', 'timing': '⏱️'}[anom.type] || '⚠️';
            const safeNodeId = escapeHtml(anom.nodeId.toString(16));
            html += '<div class="alert-item">';
            html += `<div class="alert-item-row"><span>${typeEmoji} <strong>0x${safeNodeId}</strong></span>`;
            html += `<span class="alert-item-timestamp">${new Date(anom.timestamp).toLocaleTimeString()}</span></div>`;
            html += `<div class="alert-item-detail">${escapeHtml(anom.description)}</div></div>`;
        });
        
        html += '</div>';
        const warRoom = document.getElementById('war-room-container');
        if (warRoom) warRoom.insertAdjacentHTML('afterbegin', html);
    }
    
    showBeacons(data) {
        if (!data.beacons || data.beacons.length === 0) return;
        
        let html = '<div class="alert-box alert-box-primary">';
        html += '<h4>📡 Detected Beacons (' + data.beacons.length + ')</h4>';
        html += '<p class="smaller-text text-secondary mb-sm">Devices with \u226570% periodicity confidence</p>';
        
        data.beacons.slice(0, 5).forEach(beacon => {
            const intervalSec = (beacon.avgInterval / 1000).toFixed(1);
            const safeNodeId = escapeHtml(beacon.nodeId.toString(16));
            html += '<div class="alert-item">';
            html += `<div class="alert-item-row"><span><strong>0x${safeNodeId}</strong></span>`;
            html += `<span class="beacon-confidence">${beacon.periodicityScore}% confidence</span></div>`;
            html += `<div class="alert-item-detail">Interval: ${intervalSec}s • Packets: ${beacon.packetCount}</div></div>`;
        });
        
        html += '</div>';
        const warRoom = document.getElementById('war-room-container');
        if (warRoom) warRoom.insertAdjacentHTML('afterbegin', html);
    }

    /**
     * Load and display the Info tab
     * Loads GPS positions, security assessment, and frequency analysis
     */
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
                    html += `<td><code>0x${escapeHtml(String(pos.nodeId))}</code></td>`;
                    html += `<td>${pos.lat.toFixed(6)}</td>`;
                    html += `<td>${pos.lon.toFixed(6)}</td>`;
                    html += `<td>${pos.alt || '—'} m</td>`;
                    html += '</tr>';
                });
                
                html += '</tbody></table></div>';
                this.el.gpsContent.innerHTML = html;
            } else {
                this.el.gpsContent.innerHTML = renderPlaceholder('📍', 'No GPS data captured yet.', 'Device positions will appear here once discovered during reconnaissance.');
            }
        } catch (error) {
            DEBUG.error('Failed to load GPS:', error);
            this.el.gpsContent.innerHTML = renderErrorState('Failed to load GPS data', 'retry-gps');
        }
        
        // Load security analysis from /api/recon/security
        try {
            const secData = await this.get('/api/recon/security');
            
            if (secData && secData.status === 'success' && secData.devices) {
                const devices = secData.devices;
                const summary = secData.summary || {};
                let html = '<div class="p-md">';
                
                // Summary Card
                const totalDevices = summary.totalDevices || 0;
                const vulnerable = summary.vulnerable || 0;
                const moderate = summary.moderate || 0;
                const secure = summary.secure || 0;
                
                if (totalDevices > 0) {
                    html += '<div class="security-overview">';
                    html += '<h4>🔐 Network Security Overview</h4>';
                    html += '<div class="security-stats-grid">';
                    
                    html += '<div><div class="security-stat-label">Secure</div>';
                    html += '<div class="security-stat-value text-success">' + secure + '</div></div>';
                    
                    html += '<div><div class="security-stat-label">Moderate Risk</div>';
                    html += '<div class="security-stat-value text-warning">' + moderate + '</div></div>';
                    
                    html += '<div><div class="security-stat-label">Vulnerable</div>';
                    html += '<div class="security-stat-value text-danger">' + vulnerable + '</div></div>';
                    
                    html += '</div></div>';
                    
                    // Device List - Sort by vulnerability score (lowest first = most vulnerable)
                    if (devices.length > 0) {
                        html += '<h4 class="text-primary mb-md">📋 Device Security Assessment</h4>';
                        
                        // Sort devices by vulnerability score (ascending)
                        const sortedDevices = [...devices].sort((a, b) => a.score - b.score);
                        
                        sortedDevices.forEach(dev => {
                            const riskClass = dev.riskLevel === 'secure' ? 'secure' : 
                                             dev.riskLevel === 'moderate' ? 'moderate' : 'vulnerable';
                            const riskColor = dev.riskLevel === 'secure' ? 'text-success' : 
                                             dev.riskLevel === 'moderate' ? 'text-warning' : 'text-danger';
                            
                            html += '<div class="security-device-card security-device-card-' + riskClass + '">';
                            
                            html += '<div class="security-device-header">';
                            html += '<div class="security-device-info"><code>0x' + escapeHtml(dev.nodeId) + '</code>';
                            html += '<span class="security-device-protocol">' + escapeHtml(dev.protocol) + '</span></div>';
                            html += '<div class="risk-badge risk-badge-' + riskClass + '">' + escapeHtml(dev.riskLevel) + '</div>';
                            html += '</div>';
                            
                            html += '<div class="security-device-stats">';
                            html += 'Score: <strong class="' + riskColor + '">' + dev.score + '/100</strong> • ';
                            html += 'RSSI: <strong>' + dev.bestRSSI + ' dBm</strong> • ';
                            html += 'Packets: <strong>' + dev.packetCount + '</strong>';
                            html += '</div>';
                            
                            if (dev.findings && dev.findings.length > 0) {
                                html += '<ul class="security-findings">';
                                dev.findings.forEach(finding => {
                                    html += '<li>' + escapeHtml(finding) + '</li>';
                                });
                                html += '</ul>';
                            }
                            
                            html += '</div>';
                        });
                    }
                } else {
                    html += '<div class="placeholder"><div class="emoji-large">🔐</div>';
                    html += '<p>No devices to analyze yet.</p>';
                    html += '<p class="text-sm mt-sm">Security assessment will appear once devices are discovered.</p></div>';
                }
                
                html += '</div>';
                this.el.securityContent.innerHTML = html;
            } else {
                // Fallback to basic statistics if security endpoint returns no data
                const statsData = await this.get('/api/statistics');
                if (statsData && statsData.statistics) {
                    const stats = statsData.statistics;
                    let html = '<div class="p-md">';
                    html += '<p class="text-muted mb-md">Basic packet statistics:</p>';
                    html += '<div class="security-stats-grid-4">';
                    html += '<div class="security-stat-card security-stat-card-primary">';
                    html += '<div class="security-stat-label">Total Packets</div>';
                    html += '<div class="security-stat-value-lg text-primary">' + (stats.totalPackets || 0) + '</div></div>';
                    html += '<div class="security-stat-card security-stat-card-success">';
                    html += '<div class="security-stat-label">Devices</div>';
                    html += '<div class="security-stat-value-lg text-secondary">' + (stats.totalDevices || 0) + '</div></div>';
                    html += '</div><p class="mt-lg text-sm text-muted text-italic">Detailed security analysis available after capturing encrypted packets.</p></div>';
                    this.el.securityContent.innerHTML = html;
                } else {
                    this.el.securityContent.innerHTML = `
                        <div class="placeholder">
                            <div class="emoji-large">🔐</div>
                            <p>No analysis available yet.</p>
                            <p class="text-sm mt-sm">Statistics will appear as packets are captured.</p>
                        </div>
                    `;
                }
            }
        } catch (error) {
            DEBUG.error('Failed to load security data:', error);
            this.el.securityContent.innerHTML = renderErrorState('Failed to load security data', 'retry-security');
        }
        
        // Load frequency analysis from /api/activity
        try {
            const freqData = await this.get('/api/activity');
            if (freqData && freqData.activities && freqData.activities.length > 0) {
                let html = '<div class="p-md">';
                
                // Sort by packet count
                const activeFreqs = freqData.activities
                    .filter(f => f.packets > 0)
                    .sort((a, b) => b.packets - a.packets)
                    .slice(0, 10); // Top 10
                
                if (activeFreqs.length > 0) {
                    html += '<p class="text-muted mb-md">Top ' + activeFreqs.length + ' most active frequencies:</p>';
                    html += '<div class="flex-column-gap">';
                    
                    activeFreqs.forEach(freq => {
                        const maxPackets = Math.max(...activeFreqs.map(f => f.packets));
                        const barWidth = ((freq.packets / maxPackets) * 100).toFixed(1);
                        html += '<div class="freq-card">';
                        html += `<div class="freq-card-header">`;
                        html += `<strong>${escapeHtml(freq.protocol)}</strong>`;
                        html += `<span>${freq.frequencyMHz.toFixed(3)} MHz</span>`;
                        html += `</div>`;
                        html += `<div class="freq-card-config">`;
                        html += `SF${freq.spreadingFactor} • BW ${freq.bandwidthKHz}kHz • Config #${freq.configIndex}`;
                        html += `</div>`;
                        html += `<div class="freq-card-stats">`;
                        html += `<span>Packets: <strong>${freq.packets}</strong></span>`;
                        html += `<span>Avg RSSI: ${formatRSSI(freq.avgRSSI)}</span>`;
                        html += `</div>`;
                        html += `<div class="progress-track">`;
                        html += `<div class="progress-fill" style="width: ${barWidth}%; background: var(--primary);"></div>`;
                        html += `</div></div>`;
                    });
                    
                    html += '</div>';
                } else {
                    html += '<p class="text-center text-muted p-lg">No frequency activity detected yet. Start reconnaissance to see active frequencies.</p>';
                }
                
                html += '</div>';
                const freqAnalysisEl = document.getElementById('frequency-analysis-content');
                if (freqAnalysisEl) freqAnalysisEl.innerHTML = html;
            } else {
                const freqAnalysisEl = document.getElementById('frequency-analysis-content');
                if (freqAnalysisEl) {
                    freqAnalysisEl.innerHTML = renderPlaceholder('📊', 'No frequency data yet.', 'Frequency analysis will appear during reconnaissance.');
                }
            }
        } catch (error) {
            DEBUG.error('Failed to load frequency analysis:', error);
            const freqAnalysisEl = document.getElementById('frequency-analysis-content');
            if (freqAnalysisEl) {
                freqAnalysisEl.innerHTML = renderErrorState('Failed to load frequency data', 'retry-freq-analysis');
            }
        }
    }

    /**
     * Load and display the Settings tab
     * Shows WiFi status, firmware version, OTA update controls
     */
    async showSettings() {
        // Show loading state
        this.el.settingsContent.innerHTML = renderLoadingState('Loading configuration...');
        
        // Load WiFi status
        await this.loadWiFiStatus();
        
        try {
            const response = await this.get('/api/config');
            DEBUG.log('Settings API response:', response);
            
            if (!response || !response.config) {
                this.el.settingsContent.innerHTML = '<p class="placeholder">Configuration unavailable.</p>';
                return;
            }
            
            const config = response.config;
            
            let html = '<div class="p-md">';
            
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
            DEBUG.error('Failed to load settings:', error);
            this.el.settingsContent.innerHTML = renderErrorState('Failed to load configuration: ' + error.message);
        }
        
        // Setup OTA form handler
        this.setupOTAUpload();
    }
    
    async loadWiFiStatus() {
        try {
            const wifi = await this.get('/api/wifi/status');
            DEBUG.log('WiFi Status:', wifi);
            
            // Update WiFi status elements
            const deviceIdEl = document.getElementById('wifi-device-id');
            const modeEl = document.getElementById('wifi-mode');
            const ssidEl = document.getElementById('wifi-ssid');
            const ipEl = document.getElementById('wifi-ip');
            const rssiEl = document.getElementById('wifi-rssi');
            const storedEl = document.getElementById('wifi-stored');
            const apSsidEl = document.getElementById('wifi-ap-ssid');
            
            if (deviceIdEl) {
                deviceIdEl.innerHTML = '<code class="code-inline">' + 
                    escapeHtml(wifi.deviceId || '—') + '</code>';
            }
            if (modeEl) {
                if (wifi.mode === 'setup') {
                    modeEl.innerHTML = '<span class="text-warning">📱 Setup Mode (AP)</span>';
                } else if (wifi.wifiMode === 'STA') {
                    modeEl.innerHTML = '<span class="text-success">✓ Connected to Hotspot</span>';
                } else {
                    modeEl.textContent = wifi.wifiMode || 'AP';
                }
            }
            if (ssidEl) ssidEl.textContent = wifi.ssid || '—';
            if (ipEl) ipEl.textContent = wifi.ip || '—';
            if (rssiEl) {
                if (wifi.wifiMode === 'STA' && wifi.rssi) {
                    const rssiVal = wifi.rssi;
                    const rssiClass = rssiVal > -60 ? 'text-success' : rssiVal > -75 ? 'text-warning' : 'text-danger';
                    rssiEl.innerHTML = '<span class="' + rssiClass + '">' + rssiVal + ' dBm</span>';
                } else {
                    rssiEl.textContent = 'N/A';
                }
            }
            if (storedEl) {
                storedEl.textContent = wifi.hasStoredCredentials ? wifi.storedSSID : 'None';
            }
            if (apSsidEl) {
                apSsidEl.innerHTML = '<code class="code-inline">' + 
                    escapeHtml(wifi.apSSID || 'LoRa-XXXXXX') + '</code>';
            }
            
            // Update button text based on connection state
            const btnText = document.getElementById('wifi-setup-btn-text');
            if (btnText) {
                btnText.textContent = wifi.wifiMode === 'STA' ? 'Change Hotspot' : 'Configure Hotspot';
            }
            
            // Show setup banner if in setup mode
            this.updateSetupBanner(wifi.mode === 'setup', wifi.apSSID);
            
        } catch (error) {
            DEBUG.error('Failed to load WiFi status:', error);
        }
    }
    
    updateSetupBanner(isSetupMode, apSSID) {
        const banner = document.getElementById('setup-banner');
        const bannerSsid = document.getElementById('banner-ssid');
        if (banner) {
            // Only show banner on info tab
            const showBanner = isSetupMode && this.currentTab === 'info';
            banner.style.display = showBanner ? 'block' : 'none';
            if (bannerSsid && apSSID) {
                bannerSsid.textContent = apSSID;
            }
            if (isSetupMode) {
                document.body.classList.add('setup-mode');
            } else {
                document.body.classList.remove('setup-mode');
            }
        }
        // Store setup mode state for tab switching
        this.isSetupMode = isSetupMode;
        this.apSSID = apSSID;
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

    // Action dispatch table - cleaner than giant switch statement
    getActionHandlers() {
        return {
            'toggle-scan': () => this.actionToggleScan(),
            'resume-recon': () => this.actionToggleScan(),
            'stop-capture': () => this.actionStopCapture(),
            'export-kml': () => this.actionExportKML(),
            'clear-packets': () => this.actionClearPackets(),
            'clear-devices': () => this.actionClearDevices(),
            'show-activity': () => this.actionShowActivity(),
            'show-security': () => this.actionShowSecurity(),
            'export-pcap': () => this.actionExportPCAP(),
            'diagnostics': () => this.actionDiagnostics(),
            'reboot': () => this.actionReboot(),
            'download-logs': () => showToast('Log download not implemented', 'info'),
            'export-json': () => { window.open('/api/statistics', '_blank'); showToast('Exporting statistics...', 'info'); },
            'center-map': () => {}, // Handled by network-map.js
            'toggle-labels': () => {},
            'zoom-in': () => {},
            'zoom-out': () => {},
            'target-device': (v) => this.actionTargetDevice(v),
            'replay-packet': (v) => this.actionReplayPacket(v),
            'target-frequency': (v) => this.actionTargetFrequency(v),
            'retry-devices': () => this.showDevices(),
            'retry-packets': () => this.showPackets(),
            'retry-frequency': () => this.showFrequency(),
            'retry-gps': () => this.showInfo(),
            'retry-security': () => this.showInfo(),
            'retry-freq-analysis': () => this.showInfo(),
            'wifi-setup': () => this.showWiFiSetupModal(),
            'dismiss-setup': () => this.actionDismissSetup(),
            'wifi-clear': () => this.actionWifiClear(),
            'close-modal': () => this.closeModal()
        };
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
            const handlers = this.getActionHandlers();
            const handler = handlers[action];
            
            if (handler) {
                await handler(value);
            } else {
                DEBUG.warn('Unknown action:', action);
            }
        } catch (error) {
            handleError(error, `executing action '${action}'`);
        } finally {
            // Restore button state
            if (btn) {
                btn.disabled = originalDisabled;
                btn.classList.remove('btn-loading');
                if (originalText && !action.startsWith('retry-')) {
                    setTimeout(() => { btn.innerHTML = originalText; }, 300);
                }
            }
        }
    }

    // ============ Action Implementations ============
    
    async actionToggleScan() {
        const status = await this.get('/api/status');
        const currentMode = (status?.mode || '').toLowerCase();
        if (currentMode.includes('target')) {
            if (!confirm('Exit frequency targeting and return to reconnaissance mode?')) return;
        }
        await this.post('/api/scan/start', {});
        showToast('Reconnaissance resumed', 'success');
        await this.updateStatus();
    }
    
    async actionStopCapture() {
        // Confirm before stopping to prevent accidental taps
        if (!confirm('Stop capture and return to reconnaissance mode?')) return;
        await this.post('/api/capture/stop', {});
        showToast('Capture stopped', 'success');
        await this.updateStatus();
    }
    
    async actionExportKML() {
        const gpsData = await this.get('/api/positions');
        if (gpsData?.positions?.length > 0) {
            window.open('/api/export/kml', '_blank');
            showToast(`Exporting ${gpsData.positions.length} GPS location(s)...`, 'success');
        } else {
            showToast('No GPS data to export. Capture packets with location data first.', 'warning');
        }
    }
    
    async actionClearPackets() {
        await this.post('/api/replay/clear', {});
        showToast('Packets cleared', 'success');
        await this.showPackets();
    }
    
    async actionClearDevices() {
        await this.post('/api/devices/clear', {});
        showToast('Devices cleared', 'success');
        await this.showDevices();
        await this.updateStatus();
    }
    
    actionShowActivity() {
        this.switchTab('info');
        this.closeMobileMenu();
        setTimeout(() => {
            const freqSection = document.getElementById('frequency-analysis-content');
            if (freqSection) freqSection.scrollIntoView({ behavior: 'smooth', block: 'start' });
        }, 100);
    }
    
    async actionShowSecurity() {
        await this.showSecurityAssessment();
        this.closeMobileMenu();
    }
    
    async actionExportPCAP() {
        const statusData = await this.get('/api/status');
        if (!statusData || statusData.mode === 'idle') {
            showToast('No active reconnaissance session. Start scanning first.', 'warning');
            return;
        }
        
        showToast('Downloading PCAP capture...', 'info');
        const response = await fetch('/api/export/pcap');
        
        if (response.status === 404) {
            const error = await response.json();
            showToast(error.message || 'No PCAP file available', 'error');
        } else if (response.status === 501) {
            showToast('PCAP export is disabled on this device', 'error');
        } else if (response.ok) {
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
    }
    
    async actionDiagnostics() {
        showToast('Running diagnostics...', 'info');
        const diag = await this.get('/api/status');
        if (!diag || diag.status !== 'success') {
            showToast('Diagnostics data unavailable', 'warning');
            return;
        }
        
        const radioOK = true;
        const wifiOK = (diag.clientCount !== undefined);
        const heapFree = diag.freeHeap || 0;
        const heapTotal = diag.heapSize || 1;
        const heapPercent = ((heapFree / heapTotal) * 100).toFixed(1);
        const heapClass = getThresholdClass(heapPercent);
        
        let content = '';
        
        // Status indicators
        content += CardRenderer.status('📡 Radio', radioOK ? '✓ Operational' : '✗ Failed', radioOK ? 'text-success' : 'text-danger');
        content += CardRenderer.status('📶 WiFi AP', wifiOK ? `✓ Active (${diag.clientCount || 0} clients)` : '✗ Inactive', wifiOK ? 'text-success' : 'text-warning');
        content += CardRenderer.status('💾 Memory', `${this.formatBytes(heapFree)} / ${this.formatBytes(heapTotal)}`, heapClass, true, heapPercent);
        
        // Battery if available
        if (diag.batteryVoltage !== undefined) {
            const battPercent = diag.batteryPercent || 0;
            const battClass = getThresholdClass(battPercent, 50, 20);
            content += CardRenderer.status('🔋 Battery', `${(diag.batteryVoltage || 0).toFixed(2)}V (${battPercent}%)`, battClass, true, battPercent);
        }
        
        // Statistics grid
        content += '<div class="metric-grid mt-md">';
        content += CardRenderer.metric('Uptime', this.formatDuration(diag.uptime || 0), 'text-primary', 'card-primary');
        content += CardRenderer.metric('Total Packets', diag.totalPackets || 0, 'text-success', 'card-success');
        content += CardRenderer.metric('Devices', diag.devices || 0, 'text-accent', 'card-accent');
        
        if (diag.droppedPackets !== undefined) {
            const totalReceived = (diag.totalPackets || 0) + (diag.droppedPackets || 0);
            const dropRate = totalReceived > 0 ? ((diag.droppedPackets / totalReceived) * 100).toFixed(1) : 0;
            const dropClass = getThresholdClass(100 - dropRate, 95, 90);
            content += CardRenderer.metric('Dropped', `${diag.droppedPackets || 0} <span class="metric-unit">(${dropRate}%)</span>`, dropClass, 'card-danger');
            content += CardRenderer.metric('Peak Queue', `${diag.peakQueueSize || 0} <span class="metric-unit">of 100</span>`, 'text-info', 'card-info');
        }
        content += CardRenderer.metric('Mode', this.formatMode(diag.mode) || '—', 'text-warning', 'card-warning');
        content += '</div>';
        
        const modalHtml = ModalRenderer.create('diag-modal', '🔧 System Diagnostics', content);
        ModalRenderer.show(modalHtml);
        showToast('Diagnostics completed', 'success');
    }
    
    async actionReboot() {
        if (confirm('Reboot the device? This will disconnect temporarily.')) {
            await this.post('/api/command', { command: 'b' });
            showToast('Device rebooting...', 'warning');
        }
    }
    
    async actionTargetDevice(nodeId) {
        await this.post('/api/capture/start', { nodeId });
        showToast(`Targeting device 0x${nodeId}`, 'success');
        await this.updateStatus();
    }
    
    async actionReplayPacket(slotIndex) {
        await this.post('/api/replay/transmit', { slotIndex, repeatCount: 1, delayMs: 1000 });
        showToast(`Replaying packet ${parseInt(slotIndex) + 1}`, 'success');
    }
    
    async actionTargetFrequency(configIndex) {
        await this.post('/api/frequency/target', { configIndex });
        showToast(`Targeting frequency config ${configIndex}`, 'success');
        await this.updateStatus();
    }
    
    actionDismissSetup() {
        const banner = document.getElementById('setup-banner');
        if (banner) {
            banner.style.display = 'none';
            document.body.classList.remove('setup-mode');
        }
        showToast('Continuing with device AP. Go to Settings to configure hotspot later.', 'info');
    }
    
    async actionWifiClear() {
        if (confirm('Clear stored hotspot credentials? The device will restart in setup mode.')) {
            await this.post('/api/wifi/clear', {});
            showToast('Credentials cleared. Device restarting...', 'success');
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
            
            // Build modal content using CSS classes
            const vulnPercent = totalDevices > 0 ? ((vulnerable / totalDevices) * 100).toFixed(0) : 0;
            const vulnClass = vulnPercent > 50 ? 'danger' : vulnPercent > 25 ? 'warning' : 'success';
            
            let content = '';
            
            // Summary stat
            content += '<div class="security-modal-stat security-device-card-' + vulnClass + '">';
            content += '<div class="security-modal-stat-value text-' + vulnClass + '">' + vulnPercent + '%</div>';
            content += '<div class="security-modal-stat-label">High Risk Devices</div>';
            content += '</div>';
            
            // Details grid
            content += '<div class="security-stats-grid-4">';
            content += '<div class="security-stat-card security-stat-card-primary">';
            content += '<div class="security-stat-value text-primary">' + totalDevices + '</div>';
            content += '<div class="security-stat-label">Total Devices</div></div>';
            
            content += '<div class="security-stat-card security-stat-card-danger">';
            content += '<div class="security-stat-value text-danger">' + vulnerable + '</div>';
            content += '<div class="security-stat-label">Vulnerable</div></div>';
            
            content += '<div class="security-stat-card security-stat-card-warning">';
            content += '<div class="security-stat-value text-warning">' + moderate + '</div>';
            content += '<div class="security-stat-label">Moderate Risk</div></div>';
            
            content += '<div class="security-stat-card security-stat-card-success">';
            content += '<div class="security-stat-value text-success">' + secure + '</div>';
            content += '<div class="security-stat-label">Secure</div></div>';
            content += '</div>';
            
            // Recommendations - with XSS escaping
            const recommendations = secData.recommendations || [];
            if (recommendations.length > 0) {
                content += '<div class="security-recommendations">';
                content += '<div class="security-recommendations-header">⚠️ Recommendations</div>';
                content += '<ul>';
                recommendations.forEach(rec => {
                    content += '<li>' + escapeHtml(rec) + '</li>';
                });
                content += '</ul></div>';
            }
            
            const modalHtml = ModalRenderer.create('security-modal', '🔒 Security Assessment', content, 'text-danger');
            ModalRenderer.show(modalHtml);
        } catch (error) {
            showToast('Security assessment failed: ' + error.message, 'error');
        }
    }

    // ============ API Helpers ============
    
    async get(endpoint) {
        try {
            DEBUG.log('[GET]', endpoint);
            const response = await fetch(endpoint);
            DEBUG.log('[GET]', endpoint, 'status:', response.status);
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            const data = await response.json();
            DEBUG.log('[GET]', endpoint, 'success');
            return data;
        } catch (error) {
            DEBUG.error('[GET]', endpoint, 'error:', error);
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


