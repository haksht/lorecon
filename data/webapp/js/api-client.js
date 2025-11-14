/**
 * API Client - Communication with ESP32 Backend
 * 
 * Handles REST API calls and WebSocket communication
 */

class APIClient {
    constructor() {
        // Auto-detect ESP32 IP (works when connected to ESP32 AP)
        this.baseURL = window.location.origin;
        this.apiURL = `${this.baseURL}/api`;
        this.ws = null;
        this.wsReconnectInterval = 2000;  // Faster reconnect
        this.wsReconnectTimer = null;
        this.eventHandlers = {};
        this.reconnectAttempts = 0;
        
        // Handle page visibility - reconnect when app comes to foreground
        document.addEventListener('visibilitychange', () => {
            if (!document.hidden && (!this.ws || this.ws.readyState !== WebSocket.OPEN)) {
                console.log('Page visible, ensuring WebSocket connection...');
                this.connectWebSocket();
            }
        });
    }

    /**
     * Initialize WebSocket connection
     */
    connectWebSocket() {
        const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsURL = `${wsProtocol}//${window.location.host}/ws`;
        
        console.log('Connecting to WebSocket:', wsURL);
        
        this.ws = new WebSocket(wsURL);
        
        this.ws.onopen = () => {
            console.log('✓ WebSocket connected');
            this.reconnectAttempts = 0;
            this.emit('wsconnect');
            this.clearReconnectTimer();
            // Server handles ping/pong, no need for client-side pings
        };
        
        this.ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                console.log('WebSocket message:', data);
                
                // Emit event based on message type
                if (data.type) {
                    this.emit(data.type, data);
                }
                
                // Generic message event
                this.emit('message', data);
            } catch (error) {
                console.error('Error parsing WebSocket message:', error);
            }
        };
        
        this.ws.onerror = (error) => {
            console.error('WebSocket error:', error);
            this.emit('wserror', error);
        };
        
        this.ws.onclose = () => {
            console.log('✗ WebSocket disconnected');
            this.emit('wsdisconnect');
            this.scheduleReconnect();
        };
    }

    /**
     * Schedule WebSocket reconnection with exponential backoff
     */
    scheduleReconnect() {
        // Don't reconnect if page is hidden
        if (document.hidden) {
            console.log('Page hidden, skipping reconnect');
            return;
        }
        
        this.clearReconnectTimer();
        
        // Exponential backoff: 2s, 4s, 8s, max 16s
        const delay = Math.min(this.wsReconnectInterval * Math.pow(2, this.reconnectAttempts), 16000);
        this.reconnectAttempts++;
        
        console.log(`Reconnecting in ${delay}ms (attempt ${this.reconnectAttempts})...`);
        
        this.wsReconnectTimer = setTimeout(() => {
            console.log('Attempting to reconnect WebSocket...');
            this.connectWebSocket();
        }, delay);
    }

    /**
     * Clear reconnection timer
     */
    clearReconnectTimer() {
        if (this.wsReconnectTimer) {
            clearTimeout(this.wsReconnectTimer);
            this.wsReconnectTimer = null;
        }
    }

    /**
     * Register event handler
     */
    on(event, handler) {
        if (!this.eventHandlers[event]) {
            this.eventHandlers[event] = [];
        }
        this.eventHandlers[event].push(handler);
    }

    /**
     * Emit event to registered handlers
     */
    emit(event, data) {
        if (this.eventHandlers[event]) {
            this.eventHandlers[event].forEach(handler => handler(data));
        }
    }

    /**
     * Send message via WebSocket
     */
    sendMessage(data) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(data));
        } else {
            console.warn('WebSocket not connected, cannot send message');
        }
    }

    /**
     * Generic API request
     */
    async request(endpoint, options = {}) {
        const url = `${this.apiURL}${endpoint}`;
        
        try {
            const response = await fetch(url, {
                ...options,
                headers: {
                    'Content-Type': 'application/json',
                    ...options.headers
                }
            });
            
            // Check if response is JSON
            const contentType = response.headers.get('content-type');
            if (contentType && contentType.includes('application/json')) {
                return await response.json();
            } else {
                return await response.text();
            }
        } catch (error) {
            console.error(`API request failed: ${endpoint}`, error);
            throw error;
        }
    }

    // =================================================================
    // Device Management
    // =================================================================

    async getDevices() {
        return await this.request('/devices');
    }

    async getDevice(nodeId) {
        return await this.request(`/device?nodeId=${nodeId}`);
    }

    async startCapture(nodeId) {
        // Send as form data (application/x-www-form-urlencoded)
        const formData = new URLSearchParams();
        formData.append('nodeId', nodeId);
        
        return await this.request('/capture/start', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded'
            },
            body: formData.toString()
        });
    }

    async stopCapture() {
        return await this.request('/capture/stop', { method: 'POST' });
    }

    // =================================================================
    // Geographic Data
    // =================================================================

    async getPositions() {
        return await this.request('/positions');
    }

    async exportGeoJSON() {
        return await this.request('/export/geojson');
    }

    async exportKML() {
        return await this.request('/export/kml');
    }

    // =================================================================
    // Status & Statistics
    // =================================================================

    async getStatus() {
        return await this.request('/status');
    }

    async getStatistics() {
        return await this.request('/statistics');
    }

    async getActivity() {
        return await this.request('/activity');
    }

    async getConfig() {
        return await this.request('/config');
    }

    // =================================================================
    // Scan Control
    // =================================================================

    async startScan() {
        return await this.request('/scan/start', { method: 'POST' });
    }

    async stopScan() {
        return await this.request('/scan/stop', { method: 'POST' });
    }
}

// Export global instance
window.apiClient = new APIClient();
