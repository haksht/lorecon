/* ESP32 LoRecon - Modular Client
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

// escapeHtml is defined in toast.js (loaded first) and exported as window.escapeHtml

// ===== API Token Management =====
const AUTH_TOKEN_KEY = 'esp32_lora_api_token';

function getStoredToken() {
    return localStorage.getItem(AUTH_TOKEN_KEY) || '';
}

function setStoredToken(token) {
    if (token) {
        localStorage.setItem(AUTH_TOKEN_KEY, token.trim());
    } else {
        localStorage.removeItem(AUTH_TOKEN_KEY);
    }
}

async function promptForToken() {
    // Try auto-fetch first (AP subnet 192.168.4.x, or auto-trusted LAN in STA mode)
    try {
        const resp = await fetch('/api/auth/token');
        if (resp.ok) {
            const data = await resp.json();
            if (data.token) {
                setStoredToken(data.token);
                showToast('API token retrieved automatically', 'success');
                return data.token;
            }
        }
    } catch (e) { /* not on AP subnet, fall through to manual prompt */ }

    // window.prompt() is suppressed by captive portal browsers — use custom modal
    const token = await ModalRenderer.prompt(
        'API Token Required',
        'Protected actions (replay, targeting, etc.) require authentication.<br>Find your token in the serial output at device boot.',
        'Paste token here',
        'password'
    );
    if (token) {
        setStoredToken(token);
        showToast('API token saved', 'success');
    }
    return token;
}

// ===== UI Helper Utilities =====
// RSSI classification helper
// Thresholds: -70 dBm (strong/medium) and -90 dBm (medium/weak)
// Keep in sync with estimatePowerClass() in firmware/src/utils/format_utils.h
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

// Mode detection helper — single place for firmware mode string logic
function isTargetedMode(mode) {
    return (mode || '').toLowerCase().includes('target');
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
    /** Shared modal shell — header/body/footer wrapper */
    _buildModal(id, titleClass, title, body, footer) {
        return `<div class="modal-backdrop" id="${id}">
            <div class="modal-box">
                <div class="modal-header-section"><h3 class="${titleClass}">${title}</h3></div>
                <div class="modal-body-section">${body}</div>
                <div class="modal-footer-section">${footer}</div>
            </div>
        </div>`;
    },

    /**
     * Create modal HTML structure
     * @param {string} id - Unique modal ID for targeting
     * @param {string} title - Modal title text
     * @param {string} content - Modal body HTML content
     * @param {string} [titleColor='text-primary'] - CSS class for title color
     * @returns {string} Complete modal HTML string
     */
    create(id, title, content, titleColor = 'text-primary') {
        return this._buildModal(id, titleColor, title, content,
            `<button onclick="document.getElementById('${id}').remove()" class="btn btn-primary">Close</button>`);
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
    },

    /**
     * Show a text input modal (replaces window.prompt() which is suppressed
     * by captive portal browsers on iOS Safari)
     * @param {string} title - Modal title
     * @param {string} message - Prompt message
     * @param {string} [placeholder=''] - Input placeholder text
     * @param {string} [inputType='text'] - Input type ('text' or 'password')
     * @returns {Promise<string|null>} entered value, or null if cancelled
     */
    prompt(title, message, placeholder = '', inputType = 'text') {
        return new Promise((resolve) => {
            const id = 'prompt-modal-' + Date.now();
            const inputId = 'prompt-input-' + Date.now();
            const html = this._buildModal(id, 'text-primary', title,
                `<p>${message}</p>
                <input type="${inputType}" id="${inputId}" class="modal-input" placeholder="${placeholder}">`,
                `<button data-confirm="cancel" class="btn btn-secondary">Cancel</button>
                <button data-confirm="ok" class="btn btn-primary">OK</button>`
            );

            const container = document.createElement('div');
            container.innerHTML = html;
            const modal = container.firstChild;
            document.body.appendChild(modal);

            const input = document.getElementById(inputId);
            if (input) setTimeout(() => input.focus(), 50);

            if (input) {
                input.addEventListener('keydown', (e) => {
                    if (e.key === 'Enter') {
                        const val = input.value.trim();
                        modal.remove();
                        resolve(val || null);
                    }
                });
            }

            modal.addEventListener('click', (e) => {
                const action = e.target.dataset.confirm;
                if (action === 'ok') {
                    const val = input ? input.value.trim() : null;
                    modal.remove();
                    resolve(val || null);
                } else if (action === 'cancel' || e.target === modal) {
                    modal.remove();
                    resolve(null);
                }
            });
        });
    },

    /**
     * Show a confirmation modal (replaces browser confirm() which is
     * suppressed by captive portal browsers)
     * @param {string} title - Modal title
     * @param {string} message - Confirmation message
     * @param {string} [confirmLabel='Confirm'] - Label for confirm button
     * @returns {Promise<boolean>} true if confirmed, false if cancelled
     */
    confirm(title, message, confirmLabel = 'Confirm') {
        return new Promise((resolve) => {
            const id = 'confirm-modal-' + Date.now();
            const html = this._buildModal(id, 'text-warning', title,
                `<p>${message}</p>`,
                `<button data-confirm="cancel" class="btn btn-secondary">Cancel</button>
                <button data-confirm="ok" class="btn btn-primary">${confirmLabel}</button>`
            );

            const container = document.createElement('div');
            container.innerHTML = html;
            const modal = container.firstChild;
            document.body.appendChild(modal);

            modal.addEventListener('click', (e) => {
                const action = e.target.dataset.confirm;
                if (action === 'ok') {
                    modal.remove();
                    resolve(true);
                } else if (action === 'cancel' || e.target === modal) {
                    modal.remove();
                    resolve(false);
                }
            });
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
            battery: document.getElementById('battery'),
            devicesContent: document.getElementById('devices-content'),
            packetsContent: document.getElementById('packets-content'),
            frequencyContent: document.getElementById('frequency-content'),
            settingsContent: document.getElementById('settings-content'),
            gpsContent: document.getElementById('gps-content'),
            securityContent: document.getElementById('security-content'),
            targetingBar: document.getElementById('targeting-bar'),
            targetingBarFreq: document.getElementById('targeting-bar-freq'),
            infoMode: document.getElementById('info-mode'),
            infoTargetingNotice: document.getElementById('info-targeting-notice'),
            infoTargetingFreq: document.getElementById('info-targeting-freq'),
            infoUptime: document.getElementById('info-uptime'),
            infoPackets: document.getElementById('info-packets'),
            infoDevices: document.getElementById('info-devices'),
            infoHeap: document.getElementById('info-heap'),
            infoMinHeap: document.getElementById('info-min-heap'),
            infoDroppedRow: document.getElementById('info-dropped-row'),
            infoDropped: document.getElementById('info-dropped'),
            infoGpsRow: document.getElementById('info-gps-row'),
            infoGps: document.getElementById('info-gps'),
            mobileMenuToggle: document.getElementById('mobile-menu-toggle'),
            mobileMenuOverlay: document.getElementById('mobile-menu-overlay')
        };

        // State
        this.statusTimer = null;
        this.lastDropWarning = null;  // Track queue warning timestamp
        this.currentTab = 'info';
        this.currentStatus = null;
        this.isMobile = window.innerWidth < 768;
        this.networkMap = null;
        this.warRoom = null;
        this.ws = null;
        this.wsRetryCount = 0;

        // Device sort state (persists across tab refreshes)
        this.allDevices = null;
        this.deviceSort = { col: 'securityScore', dir: 'asc' };

        // War room event tracking (detect mode/device-count changes)
        this.lastStatusMode = undefined;
        this.lastStatusDevices = undefined;

        // Silent-reboot detection: a drop in uptime between polls means
        // the device reset (uptime is millis()/1000 firmware-side).
        this.lastUptime = undefined;

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

            // Auto-refresh active tab content every 15s (devices, packets, frequency, network)
            const refreshableTabs = new Set(['devices', 'packets', 'frequency', 'network']);
            this.contentRefreshTimer = setInterval(() => {
                if (refreshableTabs.has(this.currentTab)) {
                    this.loadTabContent(this.currentTab);
                }
            }, 15000);

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
            this.wsRetryCount = 0;
            this.setConnected(true);
        };
        
        this.ws.onerror = (error) => {
            DEBUG.error('WebSocket error:', error);
            this.setConnected(false);
        };
        
        this.ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                if (data && data.type === 'status') {
                    this.handleStatusUpdate(data);
                }
            } catch (error) {
                DEBUG.error('WebSocket message error:', error);
            }
        };
        
        this.ws.onclose = () => {
            this.setConnected(false);
            this.wsRetryCount++;
            const delay = Math.min(30000, 1000 * Math.pow(2, this.wsRetryCount - 1));
            setTimeout(() => this.connectWebSocket(), delay);
        };
    }

    handleStatusUpdate(data) {
        this.currentStatus = data;
        const isTargeted = isTargetedMode(data.mode);

        // Silent-reboot detection: uptime is millis()/1000, so a drop between
        // polls means the device reset between them. Surface the reason from
        // /api/status so the user knows it rebooted instead of silently
        // attributing it to a data wipe.
        const uptime = data.uptime;
        if (typeof uptime === 'number') {
            if (this.lastUptime !== undefined && uptime < this.lastUptime - 2) {
                const reason = data.resetReason || 'unknown';
                showToast(`Device rebooted — reason: ${reason}`, 'error', 8000);
                DEBUG.warn(`Reboot detected: uptime ${this.lastUptime}s → ${uptime}s, reason=${reason}`);
            }
            this.lastUptime = uptime;
        }

        // Check for queue drops and show warning if significant
        if (data.droppedPackets > 0 && data.totalPackets > 0) {
            const totalReceived = data.totalPackets + data.droppedPackets;
            const dropRate = ((data.droppedPackets / totalReceived) * 100).toFixed(1);
            if (dropRate > 5 && (!this.lastDropWarning || Date.now() - this.lastDropWarning > 60000)) {
                showToast(`⚠️ Queue overload: ${data.droppedPackets} packets dropped (${dropRate}%)`, 'warning');
                this.lastDropWarning = Date.now();
            }
        }

        // Check SD card space
        if (data.storage?.available) {
            if (data.storage.loggingStopped && (!this.lastSdStopWarning || Date.now() - this.lastSdStopWarning > 300000)) {
                showToast('SD card full — logging stopped. Download CSV/PCAP and clear space.', 'error');
                this.lastSdStopWarning = Date.now();
            } else if (data.storage.lowSpace && (!this.lastSdSpaceWarning || Date.now() - this.lastSdSpaceWarning > 120000)) {
                showToast(`SD card low: ${this.formatStorageMB(data.storage.freeMB)} free`, 'warning');
                this.lastSdSpaceWarning = Date.now();
            }
        }
        
        // Update sidebar stats
        if (this.el.mode) {
            if (isTargeted && data.target && data.target.frequency) {
                this.el.mode.textContent = `🎯 ${this.formatTargetLabel(data.target, 'short')}`;
            } else {
                this.el.mode.textContent = '🔍 Scanning';
            }
            const modeCard = this.el.mode.closest('.stat-card');
            if (modeCard) {
                modeCard.classList.toggle('scanning-pulse', !isTargeted);
            }
        }
        if (this.el.devices) this.el.devices.textContent = data.devices || 0;
        if (this.el.packets) this.el.packets.textContent = data.capturedPackets || 0;
        if (this.el.battery) {
            const pct = data.batteryPercent;
            if (pct !== undefined && pct !== null && pct >= 0) {
                this.el.battery.textContent = `${pct}%`;
                // Color code battery level
                this.el.battery.className = 'stat-value ' + (pct > 50 ? 'text-success' : pct > 20 ? 'text-warning' : 'text-danger');
            } else {
                this.el.battery.textContent = '—';
                this.el.battery.className = 'stat-value';
            }
        }
        
        // Update info tab if visible
        if (this.currentTab === 'info') {
            if (this.el.infoTargetingNotice) {
                if (isTargeted && data.target && data.target.frequency) {
                    if (this.el.infoTargetingFreq) {
                        this.el.infoTargetingFreq.textContent = this.formatTargetLabel(data.target);
                    }
                    this.el.infoTargetingNotice.style.display = '';
                } else {
                    this.el.infoTargetingNotice.style.display = 'none';
                }
            }
            if (this.el.infoMode) {
                if (isTargeted && data.target && data.target.frequency) {
                    this.el.infoMode.textContent = 'Targeted Capture';
                } else {
                    this.el.infoMode.textContent = this.formatMode(data.mode);
                }
            }
            if (this.el.infoUptime) this.el.infoUptime.textContent = this.formatDuration(data.uptime || 0);
            if (this.el.infoPackets) this.el.infoPackets.textContent = data.totalPackets || 0;
            if (this.el.infoDevices) this.el.infoDevices.textContent = data.devices || 0;
            if (this.el.infoHeap) this.el.infoHeap.textContent = this.formatBytes(data.freeHeap || 0);
            if (this.el.infoMinHeap) {
                const minHeap = data.minFreeHeap;
                if (typeof minHeap === 'number') {
                    this.el.infoMinHeap.textContent = this.formatBytes(minHeap);
                    // Color-code: <30KB is danger, <60KB is warning, else neutral
                    this.el.infoMinHeap.className = 'status-value ' +
                        (minHeap < 30000 ? 'text-danger' :
                         minHeap < 60000 ? 'text-warning' : '');
                }
            }
            if (this.el.infoDroppedRow && this.el.infoDropped) {
                const dropped = data.droppedPackets || 0;
                const total = (data.totalPackets || 0) + dropped;
                const pct = total > 0 ? ((dropped / total) * 100).toFixed(1) : 0;
                this.el.infoDroppedRow.style.display = dropped > 0 ? '' : 'none';
                this.el.infoDropped.textContent = `${dropped} (${pct}%)`;
                this.el.infoDropped.className = 'status-value text-danger';
            }
            if (this.el.infoGpsRow && this.el.infoGps) {
                if (data.gps) {
                    this.el.infoGpsRow.style.display = '';
                    if (data.gps.hasFix) {
                        const lat = parseFloat(data.gps.lat).toFixed(5);
                        const lon = parseFloat(data.gps.lon).toFixed(5);
                        const alt = data.gps.alt !== undefined ? ` ${parseFloat(data.gps.alt).toFixed(0)}m` : '';
                        const sats = data.gps.satellites ? ` (${data.gps.satellites} sats)` : '';
                        this.el.infoGps.textContent = `${lat}, ${lon}${alt}${sats}`;
                        this.el.infoGps.className = 'status-value text-success';
                    } else {
                        const sats = data.gps.satellites ? ` — ${data.gps.satellites} sats` : '';
                        this.el.infoGps.textContent = `No fix${sats}`;
                        this.el.infoGps.className = 'status-value text-warning';
                    }
                } else {
                    this.el.infoGpsRow.style.display = 'none';
                }
            }
        }
        
        // Feed war room event log when meaningful state changes occur
        if (this.warRoom) {
            if (this.lastStatusMode !== undefined && this.lastStatusMode !== data.mode) {
                const nowTargeted = isTargetedMode(data.mode);
                const wasTargeted = isTargetedMode(this.lastStatusMode);
                if (nowTargeted && data.target) {
                    const freq = data.target.frequency ? data.target.frequency.toFixed(3) : '?';
                    this.warRoom.addEvent('mode', `Locked: ${freq} MHz (cfg #${data.target.configIndex !== undefined ? data.target.configIndex + 1 : '?'})`);
                } else if (wasTargeted) {
                    this.warRoom.addEvent('mode', 'Returned to scanning');
                }
            }
            if (this.lastStatusDevices !== undefined && (data.devices || 0) > this.lastStatusDevices) {
                const n = data.devices - this.lastStatusDevices;
                this.warRoom.addEvent('device', `${n} new device${n > 1 ? 's' : ''} detected`);
            }
        }
        this.lastStatusMode = data.mode;
        this.lastStatusDevices = data.devices || 0;

        // Update targeting bar
        this.updateTargetingBar(data);
    }
    
    updateTargetingBar(data) {
        if (!this.el.targetingBar) return;
        const isTargeted = isTargetedMode(data.mode);
        if (isTargeted && data.target && data.target.frequency) {
            if (this.el.targetingBarFreq) {
                this.el.targetingBarFreq.textContent = this.formatTargetLabel(data.target);
            }
            this.el.targetingBar.style.display = '';
        } else {
            this.el.targetingBar.style.display = 'none';
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
                // Dashboard tab: load network map, stats, GPS sequentially.
                // Parallel fan-out (3 calls × ~7 endpoints) was a heap/AsyncTCP
                // pressure spike on T-Beam Supreme — see HANDOFF.md silent-reboot notes.
                await this.showNetwork();
                await this.showStats();
                await this.showGPS();
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

    /**
     * Fetch devices enriched with security scores from /api/recon/security.
     * Shared by showDevices() and showNetwork() to avoid duplicate fetches.
     * @returns {Object|null} { devices: [...enriched] } or null on failure
     */
    async fetchEnrichedDevices() {
        const data = await this.get('/api/devices');
        if (!data || !data.devices) return null;

        const scoreMap = new Map();
        try {
            const secData = await this.get('/api/recon/security');
            if (secData && secData.devices) {
                secData.devices.forEach(d => {
                    scoreMap.set(d.nodeIdDecimal, { score: d.score, riskLevel: d.riskLevel });
                });
            }
        } catch (e) {
            DEBUG.warn('Security data not available');
        }

        return {
            ...data,
            devices: data.devices.map(device => {
                const secInfo = scoreMap.get(device.nodeIdDecimal) || { score: 100, riskLevel: 'unknown' };
                return { ...device, securityScore: secInfo.score, riskLevel: secInfo.riskLevel };
            })
        };
    }

    /**
     * Load and display the Devices tab
     * Fetches /api/devices and renders device table with filter/sort controls
     */
    async showDevices() {
        const isFirstLoad = !this.allDevices;

        // Only show spinner and build the shell on first visit.
        if (isFirstLoad) {
            this.el.devicesContent.innerHTML = renderLoadingState('Loading devices...');
        }

        try {
            const data = await this.fetchEnrichedDevices();
            if (!data || !data.devices || data.devices.length === 0) {
                this.allDevices = null;
                this.el.devicesContent.innerHTML = `
                    <div class="empty-state">
                        <div class="empty-icon">📡</div>
                        <h3>Scanning for Devices</h3>
                        <p>No LoRa devices detected yet. The scanner cycles through 29 frequency configurations every ~6 minutes.</p>
                        <p class="text-muted">Devices will appear here as they transmit packets in range.</p>
                    </div>`;
                return;
            }

            this.allDevices = data.devices;

            // On first load, build the shell (header + empty wrapper).
            // On subsequent refreshes, the shell already exists — just update the table
            // in place so scroll position is never lost.
            if (isFirstLoad || !document.getElementById('device-table-wrapper')) {
                this.el.devicesContent.innerHTML = `
                    <div class="devices-header" aria-hidden="true">
                        <span class="devices-title">Discovered Devices</span>
                        <span class="devices-count">${this.allDevices.length} device${this.allDevices.length !== 1 ? 's' : ''}</span>
                    </div>
                    <div id="device-table-wrapper"></div>`;
            } else {
                // Just update the count badge without touching the table wrapper.
                const badge = this.el.devicesContent.querySelector('.devices-count');
                if (badge) badge.textContent = `${this.allDevices.length} device${this.allDevices.length !== 1 ? 's' : ''}`;
            }

            this.renderDeviceTable();

        } catch (error) {
            DEBUG.error('Failed to load devices:', error);
            this.el.devicesContent.innerHTML = renderErrorState('Failed to load devices', 'retry-devices');
        }
    }

    /** Render (or re-render) the sorted device table into #device-table-wrapper */
    renderDeviceTable() {
        const wrapper = document.getElementById('device-table-wrapper');
        if (!wrapper || !this.allDevices) return;

        const filtered = [...this.allDevices];

        // --- Sort ---
        const { col, dir } = this.deviceSort;
        filtered.sort((a, b) => {
            let av = a[col], bv = b[col];
            if (typeof av === 'number' || typeof bv === 'number') {
                av = av ?? (dir === 'asc' ? Infinity : -Infinity);
                bv = bv ?? (dir === 'asc' ? Infinity : -Infinity);
                return dir === 'asc' ? av - bv : bv - av;
            }
            av = String(av ?? '').toLowerCase();
            bv = String(bv ?? '').toLowerCase();
            return dir === 'asc' ? av.localeCompare(bv) : bv.localeCompare(av);
        });

        // --- Sortable column headers ---
        const cols = [
            { key: 'nodeId',               label: 'Node ID' },
            { key: 'securityScore',        label: 'Risk' },
            { key: 'deviceType',           label: 'Type' },
            { key: 'firmwareVersion',      label: 'Firmware' },
            { key: 'isRouter',             label: 'Router' },
            { key: 'periodicityScore',     label: 'Beacon' },
            { key: 'powerClass',           label: 'Signal' },
            { key: 'batteryLevel',         label: 'Battery' },
            { key: 'packetCount',          label: 'Pkts (Orig/Relay)' },
            { key: 'avgRSSI',              label: 'RSSI / SNR' },
            { key: 'frequency',            label: 'Frequency' },
            { key: 'firstSeenSecondsAgo',  label: 'First Seen' },
            { key: 'lastSeenSecondsAgo',   label: 'Last Seen' },
        ];

        let html = '<div class="table-wrapper"><table class="table"><thead><tr>';
        cols.forEach(c => {
            const isActive = col === c.key;
            const arrow = isActive ? (dir === 'asc' ? ' ↑' : ' ↓') : ' ↕';
            html += `<th class="th-sort${isActive ? ' sort-active' : ''}" data-sort-col="${c.key}">${c.label}<span class="sort-arrow">${arrow}</span></th>`;
        });
        html += '</tr></thead><tbody>';

        // --- Rows ---
        filtered.forEach(device => {
            const rssiClass = formatRSSI(device.rssi, false);

            let riskBadge = '', riskClass = '';
            if      (device.riskLevel === 'vulnerable') { riskBadge = '🔴 High'; riskClass = 'risk-high';    }
            else if (device.riskLevel === 'moderate')   { riskBadge = '🟡 Med';  riskClass = 'risk-medium';  }
            else if (device.riskLevel === 'secure')     { riskBadge = '🟢 Low';  riskClass = 'risk-low';     }
            else                                        { riskBadge = '⚪ —';    riskClass = 'risk-unknown'; }

            const routerBadge = device.isRouter ? '✅' : '—';
            // Derive signal strength from bestRSSI — powerClass is only set at first packet
            const _sigRssi = device.bestRSSI || device.avgRSSI || device.rssi || -100;
            const powerBadge  = _sigRssi > -70 ? '📶 Strong' : (_sigRssi > -90 ? '📶 Med' : '📶 Weak');

            const battLevel = device.batteryLevel !== undefined ? device.batteryLevel : -1;
            const battVolt  = device.batteryVoltage || 0;
            let battBadge = '—';
            if (battLevel >= 0) {
                const battIcon = battLevel > 60 ? '🔋' : (battLevel > 20 ? '🪫' : '⚠️');
                const voltStr = battVolt > 0 ? ` (${battVolt.toFixed(2)}V)` : '';
                battBadge = `${battIcon} ${battLevel}%${voltStr}`;
            }

            const periodicityScore = device.periodicityScore || 0;
            const avgInterval      = device.avgPacketInterval || 0;
            let beaconBadge = '—';
            if      (periodicityScore >= 80) beaconBadge = `📡 ${periodicityScore}%`;
            else if (periodicityScore >= 50) beaconBadge = `📶 ${periodicityScore}%`;
            else if (periodicityScore >   0) beaconBadge = `〰 ${periodicityScore}%`;
            const intervalTooltip = avgInterval > 0 ? `Avg interval: ${(avgInterval/1000).toFixed(0)}s` : '';

            const origPkts  = device.originatedPackets || 0;
            const relayPkts = device.relayedPackets    || 0;
            const totalPkts = device.packetCount       || 0;
            const pktDisplay = `${totalPkts} (${origPkts}/${relayPkts})`;

            const avgRssi    = device.avgRSSI || device.rssi || 0;
            const bestRssi   = device.bestRSSI || avgRssi;
            const lastRssi   = device.lastRSSI !== undefined ? device.lastRSSI : avgRssi;
            const rssiStdDev = device.rssiStdDev || 0;
            const rssiDisplay  = `${lastRssi.toFixed(0)} / ${avgRssi.toFixed(0)} / ${bestRssi.toFixed(0)}`;
            const packetCount  = device.packetCount || 0;
            const rssiTooltip  = (rssiStdDev === 0 && packetCount < 2) ? 'σ=—' : (rssiStdDev > 10 ? `σ=${rssiStdDev.toFixed(1)} ⚠️ High variance` : `σ=${rssiStdDev.toFixed(1)}`);

            // Show firmware only when meaningful — LoRaWAN/ISM/Helium always return "Unknown" from firmware
            const firmware     = escapeHtml((device.firmwareVersion && device.firmwareVersion !== 'Unknown') ? device.firmwareVersion : '—');
            const safeNodeId   = escapeHtml(device.nodeId);
            // MeshCore deviceType is per-packet message type (Msg/ACK/Advert) — normalize to stable label
            let rawDevType = device.deviceType || '—';
            if (rawDevType.startsWith('MeshCore ')) rawDevType = 'MeshCore Node';
            const safeDevType  = escapeHtml(rawDevType);

            // Sender name + last message tooltip on Node ID cell
            const senderName  = device.senderName || '';
            const lastMsg     = device.lastMessage || '';
            const nodeIdTitle = lastMsg ? escapeHtml(lastMsg) : '';
            const senderSub   = senderName ? `<br><small style="opacity:0.6">${escapeHtml(senderName)}</small>` : '';

            // SNR display
            const snr = device.lastSNR !== undefined ? device.lastSNR : null;
            const snrStr = snr !== null ? ` / ${snr.toFixed(1)} dB` : '';
            const rssiTooltipFull = rssiStdDev > 10
                ? `σ=${rssiStdDev.toFixed(1)} ⚠️ High variance`
                : `σ=${rssiStdDev.toFixed(1)} dB`;

            html += '<tr>';
            html += `<td title="${nodeIdTitle}"><code>0x${safeNodeId}</code>${senderSub}</td>`;
            html += `<td><span class="${riskClass}">${riskBadge}</span></td>`;
            html += `<td>${safeDevType}</td>`;
            html += `<td><small>${firmware}</small></td>`;
            html += `<td>${routerBadge}</td>`;
            html += `<td><small title="${intervalTooltip}">${beaconBadge}</small></td>`;
            html += `<td><small>${powerBadge}</small></td>`;
            html += `<td><small>${battBadge}</small></td>`;
            html += `<td>${pktDisplay}</td>`;
            html += `<td><span class="${rssiClass}" title="${rssiTooltipFull}">${rssiDisplay} dBm${snrStr}</span></td>`;
            html += `<td>${(device.frequency || 0).toFixed(3)} MHz</td>`;
            html += `<td>${this.formatDuration(device.firstSeenSecondsAgo || 0)} ago</td>`;
            html += `<td>${this.formatLastSeen(device.lastSeenSecondsAgo)}</td>`;
            html += '</tr>';
        });

        html += '</tbody></table></div>';
        wrapper.innerHTML = html;

        // Attach sort handlers to freshly-rendered column headers
        wrapper.querySelectorAll('.th-sort').forEach(th => {
            th.addEventListener('click', () => {
                const c = th.dataset.sortCol;
                if (this.deviceSort.col === c) {
                    this.deviceSort.dir = this.deviceSort.dir === 'asc' ? 'desc' : 'asc';
                } else {
                    this.deviceSort.col = c;
                    // Numeric risk/packets/RSSI default desc; everything else asc
                    this.deviceSort.dir = ['securityScore','packetCount','periodicityScore','powerClass','batteryLevel'].includes(c) ? 'asc' : 'desc';
                }
                this.renderDeviceTable();
            });
        });
    }

    /**
     * Load and display the Packets tab
     * Fetches /api/replay/slots and renders packet table with relay button
     */
    async showPackets() {
        // Only show spinner on first visit — subsequent auto-refreshes update silently.
        const isFirstLoad = this.el.packetsContent.querySelector('.loading-container') !== null
            || this.el.packetsContent.querySelector('.empty-state') !== null
            || this.el.packetsContent.children.length === 0;
        if (isFirstLoad) {
            this.el.packetsContent.innerHTML = renderLoadingState('Loading packets...');
        }
        
        try {
            const data = await this.get('/api/replay/slots');
            if (!data || !data.slots || data.slots.length === 0) {
                this.el.packetsContent.innerHTML = `
                    <div class="empty-state">
                        <div class="empty-icon">📦</div>
                        <h3>No Packets Captured</h3>
                        <p>Packets are captured here as the scanner receives them across all frequencies. The replay buffer holds the last 10 unique packets.</p>
                        <p class="text-muted">Go to <a href="#" onclick="app.switchTab('frequency'); return false;">Frequencies</a> and click "🎯 Target" to lock the radio on one channel for focused capture.</p>
                    </div>`;
                return;
            }
            
            // Group packets by packet ID to detect flooding
            const grouped = this.groupPacketsByPacketId(data.slots);
            
            let html = '<div class="table-wrapper">';
            html += '<table class="table"><thead><tr>';
            html += '<th>Protocol</th><th>From</th><th>To</th><th title="Meshtastic: remaining hops allowed (set by originator, max 7). MeshCore: hops already traversed.">Hops</th><th>Ch</th><th title="Meshtastic flags: ACK=want_ack, MQTT=via_mqtt, S#=hop_start (originator\'s initial hop_limit)">Flags</th><th>Packet ID</th><th>Size</th><th>RSSI</th><th>SNR</th><th>Frequency</th><th>Captured</th><th>Message</th><th>Actions</th>';
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
                    if (pkt.hopStart > 0) flags.push('S' + pkt.hopStart);
                    const flagsStr = flags.length > 0 ? flags.join(' ') : '—';

                    // Destination display (broadcast or specific node)
                    let destDisplay = '—';
                    if (pkt.isBroadcast === true) {
                        destDisplay = '<span class="broadcast-badge">📢</span>';
                    } else if (pkt.destId) {
                        destDisplay = '<code>0x' + escapeHtml(String(pkt.destId)) + '</code>';
                    }

                    // Channel display: show name if known, otherwise just number
                    let channelDisplay = '—';
                    if (pkt.channelName) {
                        const tip = pkt.channel !== undefined ? ` title="Hash: ${pkt.channel}"` : '';
                        channelDisplay = `<span${tip}>${escapeHtml(pkt.channelName)}</span>`;
                    } else if (pkt.channel !== undefined) {
                        channelDisplay = `<span class="text-muted">${pkt.channel}</span>`;
                    }

                    html += `<tr class="${rowClass}">`;
                    html += `<td><code>${escapeHtml(pkt.protocol || 'Unknown')}</code></td>`;
                    html += `<td>${pkt.nodeId ? '<code>0x' + escapeHtml(String(pkt.nodeId)) + '</code>' : '—'}</td>`;
                    html += `<td>${destDisplay}</td>`;
                    html += `<td>${pkt.hopCount !== undefined ? pkt.hopCount : '—'}</td>`;
                    html += `<td>${channelDisplay}</td>`;
                    html += `<td><small>${flagsStr}</small></td>`;

                    // Show packet ID with relay sidecar info
                    const relays = pkt.relaySightings || [];
                    if (relays.length > 0) {
                        const relayTip = relays.map(r => `${r.relayByte} RSSI:${r.rssi} hop:${r.hopCount}`).join(', ');
                        html += `<td><code>0x${escapeHtml(String(pkt.packetId || '—'))}</code> <span class="origin-badge" title="${escapeHtml(relayTip)}">⚡ +${relays.length} relay${relays.length > 1 ? 's' : ''}</span></td>`;
                    } else if (isRelay) {
                        html += `<td><code>0x${escapeHtml(String(pkt.packetId || '—'))}</code> <span class="relay-badge">↻ Relay</span></td>`;
                    } else if (group.packets.length > 1) {
                        html += `<td><code>0x${escapeHtml(String(pkt.packetId || '—'))}</code> <span class="origin-badge">⚡ +${group.packets.length - 1} relays</span></td>`;
                    } else {
                        html += `<td>${pkt.packetId ? '<code>0x' + escapeHtml(String(pkt.packetId)) + '</code>' : '—'}</td>`;
                    }

                    html += `<td>${pkt.length || 0} B</td>`;
                    html += `<td>${pkt.rssi != null ? `<span class="${rssiClass}">${pkt.rssi} dBm</span>` : '—'}</td>`;
                    html += `<td>${pkt.snr !== undefined ? pkt.snr.toFixed(1) + ' dB' : '—'}</td>`;
                    html += `<td>${(pkt.frequencyMHz || 0).toFixed(3)} MHz</td>`;
                    html += `<td>${this.formatDuration(pkt.capturedSecondsAgo || 0)} ago</td>`;
                    html += `<td>${escapeHtml(pkt.decryptedText || '—')}</td>`;
                    html += `<td><button data-action="replay-packet" data-value="${pkt.storeIndex}" class="btn btn-primary btn-small">🔁 Replay</button></td>`;
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
        // Only show spinner on first load.
        const isFirstLoad = this.el.frequencyContent.querySelector('.loading-container') !== null
            || this.el.frequencyContent.children.length === 0;
        if (isFirstLoad) {
            this.el.frequencyContent.innerHTML = renderLoadingState('Loading frequency data...');
        }

        try {
            // Get activity data - now includes ALL configs with their names
            const activityData = await this.get('/api/activity');

            if (!activityData || !activityData.activities || activityData.activities.length === 0) {
                this.el.frequencyContent.innerHTML = '<p class="placeholder">Frequency configurations loading...</p>';
                return;
            }

            const allConfigs = activityData.activities;
            const totalConfigs = activityData.totalConfigs || allConfigs.length;

            const isTargeted = isTargetedMode(this.currentStatus?.mode);
            const lockedConfigIndex = isTargeted ? this.currentStatus?.target?.configIndex : null;
            const lockedFreq = isTargeted ? this.currentStatus?.target?.frequency : null;

            let html = '<div class="frequency-intro">';
            if (isTargeted && lockedFreq !== null) {
                const cfgLabel = lockedConfigIndex !== null ? ` <span class="text-muted">(config #${lockedConfigIndex + 1})</span>` : '';
                html += `<div class="targeting-notice">🎯 Currently targeting <strong>${lockedFreq.toFixed(3)} MHz</strong>${cfgLabel} &mdash; radio locked on this channel.</div>`;
            }
            html += `<p><strong>${totalConfigs} frequency configurations available.</strong> Target any config to focus packet capture on that frequency.</p>`;
            html += '</div>';

            html += '<div class="table-wrapper">';
            html += '<table class="table freq-table"><thead><tr>';
            html += '<th>Actions</th><th>Protocol</th><th>Frequency</th><th>SF</th><th>BW</th><th>Packets</th><th>Avg RSSI</th><th>Peak RSSI</th>';
            html += '</tr></thead><tbody>';

            allConfigs.forEach(act => {
                const isLocked = isTargeted && lockedConfigIndex !== null && act.configIndex === lockedConfigIndex;
                const isActive = act.packets > 0;
                const rssiClass = formatRSSI(act.avgRSSI, false);
                const rowClass = isLocked ? 'locked-row' : (isActive ? '' : 'inactive-row');
                html += `<tr class="${rowClass}">`;
                if (isLocked) {
                    html += `<td><span class="locked-badge">🔒 Locked</span></td>`;
                } else {
                    html += `<td><button data-action="target-frequency" data-value="${act.configIndex}" class="btn btn-primary btn-small">🎯 Target</button></td>`;
                }
                html += `<td><strong>${act.protocol}</strong> <span class="badge config-badge">#${act.configIndex + 1}</span></td>`;
                html += `<td>${act.frequencyMHz.toFixed(3)} MHz</td>`;
                html += `<td>SF${act.spreadingFactor}</td>`;
                html += `<td>${act.bandwidthKHz} kHz</td>`;
                if (isActive) {
                    html += `<td>${act.packets}</td>`;
                    html += `<td><span class="${rssiClass}">${act.avgRSSI} dBm</span></td>`;
                    html += `<td><span class="${formatRSSI(act.peakRSSI, false)}">${act.peakRSSI} dBm</span></td>`;
                } else {
                    html += '<td class="text-muted">—</td>';
                    html += '<td class="text-muted">—</td>';
                    html += '<td class="text-muted">—</td>';
                }
                html += '</tr>';
            });

            html += '</tbody></table></div>';
            this.el.frequencyContent.innerHTML = html;

            // Populate the activity analysis summary panel (lives in this same tab)
            this.renderFrequencyAnalysis(allConfigs);
        } catch (error) {
            DEBUG.error('Failed to load frequency data:', error);
            this.el.frequencyContent.innerHTML = renderErrorState('Failed to load frequency data', 'retry-frequency');
        }
    }

    /** Render top-active-frequency bars into #frequency-analysis-content */
    renderFrequencyAnalysis(allConfigs) {
        const freqAnalysisEl = document.getElementById('frequency-analysis-content');
        if (!freqAnalysisEl) return;

        const activeFreqs = allConfigs
            .filter(f => f.packets > 0)
            .sort((a, b) => b.packets - a.packets)
            .slice(0, 10);

        if (activeFreqs.length === 0) {
            freqAnalysisEl.innerHTML = renderPlaceholder('📊', 'No frequency activity yet.', 'Active frequencies will appear during scanning.');
            return;
        }

        const maxPackets = Math.max(...activeFreqs.map(f => f.packets));
        let html = '<div class="p-md">';
        html += '<p class="text-muted mb-md">Top ' + activeFreqs.length + ' most active frequencies:</p>';
        html += '<div class="flex-column-gap">';
        activeFreqs.forEach(freq => {
            const barWidth = ((freq.packets / maxPackets) * 100).toFixed(1);
            html += '<div class="freq-card">';
            html += `<div class="freq-card-header"><strong>${escapeHtml(freq.protocol)}</strong><span>${freq.frequencyMHz.toFixed(3)} MHz</span></div>`;
            html += `<div class="freq-card-config">SF${freq.spreadingFactor} • BW ${freq.bandwidthKHz}kHz • Config #${freq.configIndex + 1}</div>`;
            html += `<div class="freq-card-stats"><span>Packets: <strong>${freq.packets}</strong></span><span>Avg RSSI: ${formatRSSI(freq.avgRSSI)}</span></div>`;
            html += `<div class="progress-track"><div class="progress-fill" style="width: ${barWidth}%; background: var(--primary);"></div></div>`;
            html += '</div>';
        });
        html += '</div></div>';
        freqAnalysisEl.innerHTML = html;
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
            const data = await this.fetchEnrichedDevices();
            DEBUG.log('Network devices data:', data);

            if (data && data.devices) {
                if (this.networkMap) {
                    this.networkMap.updateDevices(data.devices);
                    DEBUG.log('Updated map with', data.devices.length, 'enriched devices');
                } else {
                    DEBUG.warn('NetworkMap not initialized, cannot update');
                }

                const infoEl = document.getElementById('network-info');
                if (infoEl) {
                    if (data.devices.length === 0) {
                        infoEl.innerHTML = '<p class="placeholder">No devices discovered yet. The network map will populate as devices are scanned.</p>';
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

            // Feed per-config activity data into war room frequency bars
            try {
                const activityData = await this.get('/api/activity');
                if (activityData && activityData.activities && this.warRoom) {
                    this.warRoom.updateActivityData(activityData.activities);
                }
            } catch (e) {
                DEBUG.warn('Failed to load activity for war room:', e);
            }

            // Remove previously inserted alert boxes to prevent duplication
            const container = document.getElementById('war-room-container');
            if (container) {
                container.querySelectorAll(':scope > .alert-box').forEach(el => el.remove());
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
                if (temporal && temporal.histogram) {
                    this.showHistogram(temporal.histogram);
                }
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
            // anom.timestamp is millis() uptime when event occurred; compute elapsed
            const currentUptimeMs = (this.currentStatus && this.currentStatus.uptime)
                ? this.currentStatus.uptime * 1000 : 0;
            const elapsedSec = Math.max(0, Math.floor((currentUptimeMs - anom.timestamp) / 1000));
            const mins = Math.floor(elapsedSec / 60);
            const hrs = Math.floor(mins / 60);
            const timeStr = elapsedSec < 60 ? `${elapsedSec}s ago`
                : hrs > 0 ? `${hrs}h ${mins % 60}m ago` : `${mins}m ago`;
            html += '<div class="alert-item">';
            html += `<div class="alert-item-row"><span>${typeEmoji} <strong>0x${safeNodeId}</strong></span>`;
            html += `<span class="alert-item-timestamp">${timeStr}</span></div>`;
            html += `<div class="alert-item-detail">${escapeHtml(anom.description)}</div></div>`;
        });
        
        html += '</div>';
        const warRoom = document.getElementById('war-room-container');
        if (warRoom) warRoom.insertAdjacentHTML('afterbegin', html);
    }
    
    showHistogram(histogram) {
        const maxPkts = Math.max(...histogram.map(h => h.packets), 1);
        const totalPkts = histogram.reduce((sum, h) => sum + h.packets, 0);
        if (totalPkts === 0) return;

        let html = '<div class="alert-box" style="margin-bottom: 15px;">';
        html += '<h4>📊 Traffic by Hour of Day — UTC (' + totalPkts + ' packets this session)</h4>';
        html += '<div style="display:flex; align-items:flex-end; gap:2px; height:60px; margin-top:8px;">';
        histogram.forEach(h => {
            const pct = (h.packets / maxPkts * 100).toFixed(0);
            const barH = Math.max(h.packets > 0 ? 4 : 1, pct * 0.6);
            const opacity = h.packets > 0 ? '1' : '0.2';
            const label = String(h.hour).padStart(2, '0') + ':00';
            html += `<div title="${label}: ${h.packets} pkts" style="flex:1; height:${barH}px; background:var(--primary); opacity:${opacity}; border-radius:2px 2px 0 0; min-width:4px;"></div>`;
        });
        html += '</div>';
        html += '<div style="display:flex; justify-content:space-between; margin-top:2px;">';
        html += '<span class="smaller-text text-secondary">00:00</span>';
        html += '<span class="smaller-text text-secondary">12:00</span>';
        html += '<span class="smaller-text text-secondary">23:00</span>';
        html += '</div></div>';

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
     * Load GPS positions into #gps-content (lives in Dashboard tab).
     * Called from both showInfo() and showNetwork() so the table stays
     * fresh regardless of which tab the user is on.
     */
    async showGPS() {
        try {
            const gpsData = await this.get('/api/positions');
            // Build display list: sniffer's own GPS first (if fix), then remote device positions
            const snifferGps = this.currentStatus && this.currentStatus.gps;
            const devicePositions = (gpsData && gpsData.positions) ? [...gpsData.positions].sort((a, b) => {
                const aId = parseInt(a.nodeId, 16) || 0;
                const bId = parseInt(b.nodeId, 16) || 0;
                return aId - bId;
            }) : [];

            const hasSnifferFix = snifferGps && snifferGps.hasFix;
            const hasDevicePositions = devicePositions.length > 0;

            if (!hasSnifferFix && !hasDevicePositions) {
                this.el.gpsContent.innerHTML = renderPlaceholder('📍', 'No GPS data captured yet.', 'Device positions will appear here once discovered during scanning.');
            } else {
                let html = '<div class="table-wrapper"><table class="table"><thead><tr>';
                html += '<th>Source</th><th>Node ID</th><th>Latitude</th><th>Longitude</th><th>Altitude</th>';
                html += '</tr></thead><tbody>';

                if (hasSnifferFix) {
                    html += '<tr>';
                    html += '<td><span class="badge-sniffer">Sniffer</span></td>';
                    html += '<td><span class="text-muted">(this device)</span></td>';
                    html += `<td>${parseFloat(snifferGps.lat).toFixed(6)}</td>`;
                    html += `<td>${parseFloat(snifferGps.lon).toFixed(6)}</td>`;
                    html += `<td>${snifferGps.alt !== undefined ? parseFloat(snifferGps.alt).toFixed(0) + ' m' : '—'}</td>`;
                    html += '</tr>';
                }

                devicePositions.forEach(pos => {
                    html += '<tr>';
                    html += '<td><span class="badge-device">Device</span></td>';
                    html += `<td><code>0x${escapeHtml(String(pos.nodeId))}</code></td>`;
                    html += `<td>${pos.lat.toFixed(6)}</td>`;
                    html += `<td>${pos.lon.toFixed(6)}</td>`;
                    html += `<td>${pos.alt !== undefined ? pos.alt + ' m' : '—'}</td>`;
                    html += '</tr>';
                });

                html += '</tbody></table></div>';
                this.el.gpsContent.innerHTML = html;
            }
        } catch (error) {
            DEBUG.error('Failed to load GPS:', error);
            this.el.gpsContent.innerHTML = renderErrorState('Failed to load GPS data', 'retry-gps');
        }
    }

    /**
     * Load and display the Info tab
     * Loads GPS positions, security assessment, and frequency analysis
     */
    async showInfo() {
        // Status already updated via handleStatusUpdate
        await this.showGPS();
        
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
        
    }

    /**
     * Load and display the Settings tab
     * Shows WiFi status, firmware version, OTA update controls
     */
    async showSettings() {
        // Show loading state
        this.el.settingsContent.innerHTML = renderLoadingState('Loading configuration...');

        await Promise.all([this.loadWiFiStatus(), this.loadSDStorage()]);
        
        try {
            const response = await this.get('/api/config');
            DEBUG.log('Settings API response:', response);
            
            if (!response || !response.config) {
                this.el.settingsContent.innerHTML = '<p class="placeholder">Configuration unavailable.</p>';
                return;
            }
            
            const config = response.config;

            let html = '<div class="p-md">';

            // Device Health — boot reason from this session, color-coded so a
            // crash/WDT/brownout stands out vs. a clean Power-on.
            const status = this.currentStatus || {};
            const reason = status.resetReason || 'unknown';
            const reasonClass = (reason === 'Power-on') ? 'text-success'
                : (reason === 'Software reset (esp_restart)') ? 'text-warning'
                : (reason === 'Unknown' || reason === 'unknown') ? ''
                : 'text-danger';  // Panic, WDT, Brownout, etc. — all bad
            const minHeap = (typeof status.minFreeHeap === 'number')
                ? this.formatBytes(status.minFreeHeap) : '—';
            html += '<div class="info-section"><h3>🩺 Device Health</h3><div class="content-area">';
            html += '<div class="status-row" title="Reason this device booted. Anything other than Power-on means the chip reset (panic, watchdog, brownout, or intentional restart).">';
            html += '<span>Last Reset Reason</span>';
            html += '<span class="status-value ' + reasonClass + '">' + escapeHtml(reason) + '</span></div>';
            html += '<div class="status-row" title="Lowest free heap seen since boot. Below ~30KB, heavy endpoints will start failing with 503s.">';
            html += '<span>Min Free Heap (since boot)</span>';
            html += '<span class="status-value">' + minHeap + '</span></div>';
            if (status.lastAction) {
                html += '<div class="status-row" title="Which API endpoint was running when the previous crash occurred (from RTC memory — survives panic/WDT/brownout).">';
                html += '<span>Last Action at Crash</span>';
                html += '<span class="status-value text-danger">' + escapeHtml(status.lastAction) + '</span></div>';
            }
            html += '</div></div>';

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
        
        // Setup OTA form handlers
        this.setupOTAForm({
            formId: 'ota-form', fileInputId: 'firmware-file', btnId: 'ota-upload-btn',
            progressId: 'ota-progress', progressBarId: 'ota-progress-bar', progressTextId: 'ota-progress-text',
            fieldName: 'firmware', endpoint: '/api/firmware/upload', maxSizeMB: 4, label: 'Firmware'
        });
        this.setupOTAForm({
            formId: 'ota-fs-form', fileInputId: 'filesystem-file', btnId: 'ota-fs-upload-btn',
            progressId: 'ota-fs-progress', progressBarId: 'ota-fs-progress-bar', progressTextId: 'ota-fs-progress-text',
            fieldName: 'filesystem', endpoint: '/api/filesystem/upload', maxSizeMB: 2, label: 'Filesystem'
        });
        this.setupAPPasswordForm();

        // Show stored token
        const stored = getStoredToken();
        const tokenDisplay = document.getElementById('stored-token-display');
        if (tokenDisplay) tokenDisplay.textContent = stored || '(none saved)';
    }
    
    async loadSDStorage() {
        try {
            const data = await this.get('/api/status');
            const section = document.getElementById('sd-storage-section');
            if (!data || !data.storage || !data.storage.available) {
                if (section) section.style.display = 'none';
                return;
            }
            if (section) section.style.display = '';
            const s = data.storage;
            this._setElText('sd-type', s.type);
            this._setElText('sd-total', this.formatStorageMB(s.totalMB));
            this._setElText('sd-used', this.formatStorageMB(s.usedMB));
            this._setElText('sd-free', this.formatStorageMB(s.freeMB));
            const pct = s.totalMB > 0 ? ((s.usedMB / s.totalMB) * 100).toFixed(1) : 0;
            const bar = document.getElementById('sd-usage-bar');
            if (bar) {
                bar.style.width = pct + '%';
                bar.style.background = pct > 90 ? 'var(--danger)' : pct > 75 ? 'var(--warning)' : 'var(--accent-primary)';
            }
            this._setElText('sd-usage-text', pct + '% used');
        } catch (error) {
            DEBUG.warn('Failed to load SD storage:', error);
        }
    }

    formatStorageMB(mb) {
        if (mb >= 1024) return (mb / 1024).toFixed(1) + ' GB';
        return mb + ' MB';
    }

    async loadWiFiStatus() {
        try {
            const wifi = await this.get('/api/wifi/status');
            DEBUG.log('WiFi Status:', wifi);
            
            // Update WiFi status elements
            this._setElHTML('wifi-device-id', `<code class="code-inline">${escapeHtml(wifi.deviceId || '—')}</code>`);
            this._setElHTML('wifi-ap-ssid',   `<code class="code-inline">${escapeHtml(wifi.apSSID || 'LoRa-XXXXXX')}</code>`);
            this._setElText('wifi-ssid',   wifi.ssid || '—');
            this._setElText('wifi-ip',     wifi.ip || '—');
            this._setElText('wifi-stored', wifi.hasStoredCredentials ? wifi.storedSSID : 'None');

            const modeEl = document.getElementById('wifi-mode');
            if (modeEl) {
                if (wifi.mode === 'setup') {
                    modeEl.innerHTML = '<span class="text-warning">📱 Setup Mode (AP)</span>';
                } else if (wifi.wifiMode === 'STA') {
                    modeEl.innerHTML = '<span class="text-success">✓ Connected to Hotspot</span>';
                } else {
                    modeEl.textContent = wifi.wifiMode || 'AP';
                }
            }

            if (wifi.wifiMode === 'STA' && wifi.rssi) {
                const rssiClass = wifi.rssi > -60 ? 'text-success' : wifi.rssi > -75 ? 'text-warning' : 'text-danger';
                this._setElHTML('wifi-rssi', `<span class="${rssiClass}">${wifi.rssi} dBm</span>`);
            } else {
                this._setElText('wifi-rssi', 'N/A');
            }

            // Update button text based on connection state
            const connectedToHotspot = wifi.wifiMode === 'STA' || wifi.wifiMode === 'AP_STA';
            this._setElText('wifi-setup-btn-text', connectedToHotspot ? 'Change Hotspot' : 'Configure Hotspot');
            
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

    setupOTAForm({ formId, fileInputId, btnId, progressId, progressBarId, progressTextId, fieldName, endpoint, maxSizeMB, label }) {
        const form = document.getElementById(formId);
        if (!form || form.dataset.initialized) return;
        form.dataset.initialized = 'true';

        form.addEventListener('submit', async (e) => {
            e.preventDefault();

            const file = document.getElementById(fileInputId)?.files[0];
            if (!file) { showToast(`Please select a ${label.toLowerCase()} file`, 'warning'); return; }
            if (!file.name.endsWith('.bin')) { showToast('Only .bin files are supported', 'error'); return; }
            if (file.size > maxSizeMB * 1024 * 1024) { showToast(`${label} file is too large (max ${maxSizeMB}MB)`, 'error'); return; }

            const confirmed = await ModalRenderer.confirm(
                `Upload ${label}`,
                `Upload ${label.toLowerCase()} and reboot device? This will disconnect temporarily.`,
                'Upload & Reboot'
            );
            if (!confirmed) return;

            const uploadBtn = document.getElementById(btnId);
            const progressContainer = document.getElementById(progressId);
            const progressBar = document.getElementById(progressBarId);
            const progressText = document.getElementById(progressTextId);

            try {
                uploadBtn.disabled = true;
                progressContainer.style.display = 'block';

                const formData = new FormData();
                formData.append(fieldName, file);

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
                        showToast(`${label} uploaded successfully! Device rebooting...`, 'success');
                        progressText.textContent = 'Rebooting...';
                        setTimeout(() => { window.location.reload(); }, 10000);
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
                xhr.open('POST', endpoint);
                const token = getStoredToken();
                if (token) xhr.setRequestHeader('X-API-Token', token);
                xhr.send(formData);

            } catch (error) {
                showToast('Upload failed: ' + error.message, 'error');
                uploadBtn.disabled = false;
                progressContainer.style.display = 'none';
            }
        });
    }

    setupAPPasswordForm() {
        const form = document.getElementById('ap-password-form');
        if (!form || form.dataset.initialized) return;
        form.dataset.initialized = 'true';

        form.addEventListener('submit', async (e) => {
            e.preventDefault();
            const password = document.getElementById('ap-password-input').value;
            if (password.length < 8) {
                showToast('Password must be at least 8 characters', 'error');
                return;
            }
            const confirmed = await ModalRenderer.confirm(
                'Update AP Password',
                'Update AP password and reboot device?',
                'Update'
            );
            if (!confirmed) return;
            try {
                await this.post('/api/wifi/ap-password', { password });
                showToast('AP password updated. Device rebooting...', 'success');
                document.getElementById('ap-password-input').value = '';
            } catch (error) {
                showToast('Failed: ' + error.message, 'error');
            }
        });
    }

    // ============ Button Handlers ============
    
    setupButtons() {
        document.addEventListener('click', async (e) => {
            // Handle buttons and clickable elements with data-action
            const actionEl = e.target.closest('[data-action]');
            if (!actionEl) return;
            if (actionEl.classList.contains('btn-disabled')) return;

            // Use custom modal confirm — native confirm() is suppressed by captive portal browsers
            const confirmMsg = actionEl.dataset.confirmMsg;
            if (confirmMsg) {
                const confirmed = await ModalRenderer.confirm('Confirm', confirmMsg);
                if (!confirmed) return;
            }

            e.preventDefault();
            const action = actionEl.dataset.action;
            const value = actionEl.dataset.value;
            this.handleAction(action, value, e);
        });
    }

    // Action dispatch table - cleaner than giant switch statement
    // Lazy-initialized: arrow functions capture `this` correctly on first call
    getActionHandlers() {
        if (this._actionHandlers) return this._actionHandlers;
        this._actionHandlers = {
            'stop-capture': () => this.actionStopCapture(),
            'download-report': () => this.actionDownloadReport(),
            'download-csv':    () => this.actionExportCSV(),
            'download-pcap':   () => this.actionExportPCAP(),
            'export-kml':      () => this.actionExportKML(),
            'export-geojson':  () => this.actionExportGeoJSON(),
            'clear-packets': () => this.actionClearPackets(),
            'clear-devices': () => this.actionClearDevices(),
            'diagnostics': () => this.actionDiagnostics(),
            'reboot': () => this.actionReboot(),
            'shutdown': () => this.actionShutdown(),
            'center-map':    () => { if (this.networkMap) this.networkMap.centerMap(); },
            'toggle-labels': () => {
                if (this.networkMap) {
                    const on = this.networkMap.toggleLabels();
                    showToast(`Labels ${on ? 'on' : 'off'}`, 'info');
                }
            },
            'zoom-in':  () => { if (this.networkMap) this.networkMap.zoomIn(); },
            'zoom-out': () => { if (this.networkMap) this.networkMap.zoomOut(); },
            'replay-packet': (v) => this.actionReplayPacket(v),
            'target-frequency': (v) => this.actionTargetFrequency(v),
            'retry-devices': () => this.showDevices(),
            'retry-packets': () => this.showPackets(),
            'retry-frequency': () => this.showFrequency(),
            'retry-gps': () => this.showInfo(),
            'retry-security': () => this.showInfo(),
            'retry-freq-analysis': () => this.showFrequency(),
            'wifi-setup': () => this.showWiFiSetupModal(),
            'dismiss-setup': () => this.actionDismissSetup(),
            'wifi-clear': () => this.actionWifiClear(),
            'fetch-token': () => this.actionFetchToken(),
            'close-modal': () => this.closeModal(),
            // Sidebar stat card navigation
            'goto-devices': () => this.switchTab('devices'),
            'goto-packets': () => this.switchTab('packets')
        };
        return this._actionHandlers;
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
                if (originalText && !action.startsWith('retry-') && !btn.hasAttribute('data-no-restore')) {
                    setTimeout(() => { btn.innerHTML = originalText; }, 300);
                }
            }
        }
    }

    // ============ Action Implementations ============
    
    async actionStopCapture() {
        await this.post('/api/capture/stop', {});
        showToast('Capture stopped — returning to scan', 'success');
        await this.updateStatus();
    }
    
    async actionExportKML() {
        const gpsData = await this.get('/api/positions');
        if (!gpsData?.positions?.length) {
            showToast('No GPS data to export. Capture packets with location data first.', 'warning');
            return;
        }
        showToast(`Exporting ${gpsData.positions.length} GPS location(s) as KML...`, 'info');
        try {
            const r = await this._fetchAndDownload('/api/export/kml', `lora-positions-${Date.now()}.kml`);
            if (!r.ok) { showToast('KML export failed: HTTP ' + r.status, 'error'); return; }
            showToast('KML downloaded successfully', 'success');
        } catch (err) {
            showToast('KML export failed: ' + err.message, 'error');
        }
    }

    async actionExportGeoJSON() {
        const gpsData = await this.get('/api/positions');
        if (!gpsData?.positions?.length) {
            showToast('No GPS data to export. Capture packets with location data first.', 'warning');
            return;
        }
        showToast(`Exporting ${gpsData.positions.length} GPS location(s) as GeoJSON...`, 'info');
        try {
            const r = await this._fetchAndDownload('/api/export/geojson', `lora-positions-${Date.now()}.geojson`);
            if (!r.ok) { showToast('GeoJSON export failed: HTTP ' + r.status, 'error'); return; }
            showToast('GeoJSON downloaded successfully', 'success');
        } catch (err) {
            showToast('GeoJSON export failed: ' + err.message, 'error');
        }
    }

    async actionExportCSV() {
        showToast('Downloading CSV log from SD card...', 'info');
        try {
            const r = await this._fetchAndDownload('/api/export/csv', `lora_capture_${Date.now()}.csv`);
            if (r.status === 404) {
                const err = await r.json();
                showToast(err.message || 'No CSV file available', 'error');
            } else if (r.ok) {
                showToast('CSV downloaded successfully', 'success');
            } else {
                showToast('CSV export failed: HTTP ' + r.status, 'error');
            }
        } catch (err) {
            showToast('CSV download failed: ' + err.message, 'error');
        }
    }

    async actionDownloadReport() {
        showToast('Downloading consolidated report...', 'info');
        try {
            const r = await this._fetchAndDownload('/api/export/report', `lorecon-report-${Date.now()}.json`);
            if (!r.ok) { showToast('Failed to generate report: HTTP ' + r.status, 'error'); return; }
            showToast('Report downloaded successfully', 'success');
        } catch (err) {
            showToast('Report download failed: ' + err.message, 'error');
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
    
    async actionExportPCAP() {
        showToast('Downloading PCAP capture...', 'info');
        try {
            const r = await this._fetchAndDownload('/api/export/pcap', `lora_capture_${Date.now()}.pcap`);
            if (r.status === 404) {
                const err = await r.json();
                showToast(err.message || 'No PCAP file available', 'error');
            } else if (r.status === 501) {
                showToast('PCAP export is disabled on this device', 'error');
            } else if (r.ok) {
                showToast('PCAP downloaded successfully', 'success');
            } else {
                showToast('PCAP export failed: HTTP ' + r.status, 'error');
            }
        } catch (err) {
            showToast('PCAP download failed: ' + err.message, 'error');
        }
    }
    
    async actionDiagnostics() {
        showToast('Running diagnostics...', 'info');
        const diag = await this.get('/api/status');
        if (!diag || diag.status !== 'success') {
            showToast('Diagnostics data unavailable', 'warning');
            return;
        }
        
        // Radio health: no dedicated endpoint; infer from mode being set
        // (if radio init failed, mode is typically absent from status)
        const radioOK = !!(diag.mode);
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
        const confirmed = await ModalRenderer.confirm(
            'Reboot Device',
            'Reboot the device? This will disconnect temporarily.',
            'Reboot'
        );
        if (confirmed) {
            await this.post('/api/command', { command: 'b' });
            showToast('Device rebooting...', 'warning');
        }
    }

    async actionShutdown() {
        const confirmed = await ModalRenderer.confirm(
            'Power Off',
            'Power off the device? You will need to press the button to turn it back on.',
            'Power Off'
        );
        if (confirmed) {
            await this.post('/api/command', { command: 's' });
            showToast('Device powering off...', 'warning');
        }
    }

    async actionTargetDevice(nodeId) {
        await this.post('/api/capture/start', { nodeId });
        showToast(`Locked on frequency for device 0x${nodeId} — capturing all traffic on that channel`, 'success');
        await this.updateStatus();
    }
    
    async actionReplayPacket(slotIndex) {
        await this.post('/api/replay/transmit', { slotIndex, repeatCount: 1, delayMs: 1000 });
        showToast(`Replaying packet ${parseInt(slotIndex) + 1}`, 'success');
    }
    
    async actionTargetFrequency(configIndex) {
        await this.post('/api/frequency/target', { configIndex });
        showToast(`Targeting frequency config #${configIndex + 1}`, 'success');
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
    
    async actionFetchToken() {
        const token = await promptForToken();
        if (token) {
            const tokenDisplay = document.getElementById('stored-token-display');
            if (tokenDisplay) tokenDisplay.textContent = token;
        }
    }

    async actionWifiClear() {
        const confirmed = await ModalRenderer.confirm(
            'Clear WiFi Credentials',
            'Clear stored hotspot credentials? The device will restart in setup mode.',
            'Clear & Restart'
        );
        if (confirmed) {
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
    
    // ============ API Helpers ============
    
    async get(endpoint, retry = true) {
        try {
            DEBUG.log('[GET]', endpoint);
            const headers = {};
            const token = getStoredToken();
            if (token) headers['X-API-Token'] = token;
            const response = await fetch(endpoint, { headers });
            DEBUG.log('[GET]', endpoint, 'status:', response.status);

            // Handle 401 — prompt for token and retry once (mirrors post() behaviour)
            if (response.status === 401 && retry) {
                const newToken = await promptForToken();
                if (newToken) return this.get(endpoint, false);
                throw new Error('Authentication required. Set your API token in Settings.');
            }
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

    // Set text/HTML on an element by ID, silently skipping if element not found
    _setElText(id, text) { const el = document.getElementById(id); if (el) el.textContent = text; }
    _setElHTML(id, html) { const el = document.getElementById(id); if (el) el.innerHTML = html; }

    // Fetch a URL and trigger a browser download if response is ok.
    // Returns the Response so callers can check status codes for custom error handling.
    // NOTE: blob() is only consumed on success — callers may still call .json() on non-ok responses.
    async _fetchAndDownload(url, filename) {
        const response = await fetch(url);
        if (response.ok) {
            const blob = await response.blob();
            const href = window.URL.createObjectURL(blob);
            const a = Object.assign(document.createElement('a'), { href, download: filename });
            document.body.appendChild(a); a.click();
            window.URL.revokeObjectURL(href); a.remove();
        }
        return response;
    }

    async post(endpoint, body = {}, retry = true) {
        const formData = new URLSearchParams();
        Object.entries(body).forEach(([key, value]) => {
            formData.append(key, value);
        });

        const headers = { 'Content-Type': 'application/x-www-form-urlencoded' };
        const token = getStoredToken();
        if (token) {
            headers['X-API-Token'] = token;
        }

        const response = await fetch(endpoint, {
            method: 'POST',
            headers,
            body: formData.toString()
        });

        // Handle 401 Unauthorized - prompt for token and retry once
        if (response.status === 401 && retry) {
            const newToken = await promptForToken();
            if (newToken) {
                return this.post(endpoint, body, false); // Retry with new token
            }
            throw new Error('Authentication required. Set your API token in Settings.');
        }

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

    // Format a targeting label: "915.000 MHz #3" (short) or "915.000 MHz (config #3)" (long)
    formatTargetLabel(target, style = 'long') {
        const cfgIdx = target.configIndex;
        const suffix = cfgIdx !== undefined
            ? (style === 'short' ? ` #${cfgIdx + 1}` : ` (config #${cfgIdx + 1})`)
            : '';
        return `${target.frequency.toFixed(3)} MHz${suffix}`;
    }
}

// Initialize app
let app;
document.addEventListener('DOMContentLoaded', () => {
    app = new ReconApp();
    window.app = app;
});


