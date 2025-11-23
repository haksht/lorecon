/**
 * Connection Manager - Intelligent WebSocket + Polling Hybrid
 * Automatically falls back to polling if WebSocket fails
 */

class ConnectionManager {
    constructor(app) {
        this.app = app;
        this.mode = 'websocket'; // 'websocket' or 'polling'
        this.ws = null;
        this.wsRetries = 0;
        this.maxWsRetries = 3;
        this.pollTimer = null;
        this.pollingInterval = 2000; // 2 seconds
        this.reconnectDelay = 2000;
        this.isConnected = false;
        this.lastMessageTime = Date.now();
        
        // Activity-based adaptive polling
        this.recentActivity = false;
        this.activityCheckInterval = null;
    }
    
    /**
     * Initialize connection - try WebSocket first
     */
    async init() {
        console.log('[Connection] Initializing...');
        await this.tryWebSocket();
    }
    
    /**
     * Attempt WebSocket connection
     */
    async tryWebSocket() {
        if (this.wsRetries >= this.maxWsRetries) {
            console.log('[Connection] Max WebSocket retries reached, switching to polling');
            this.switchToPolling();
            return;
        }
        
        try {
            const wsUrl = `ws://${window.location.host}/ws`;
            console.log(`[Connection] Attempting WebSocket connection to ${wsUrl}...`);
            
            this.ws = new WebSocket(wsUrl);
            
            // Set connection timeout
            const timeout = setTimeout(() => {
                if (this.ws.readyState === WebSocket.CONNECTING) {
                    console.log('[Connection] WebSocket connection timeout');
                    this.ws.close();
                    this.wsRetries++;
                    setTimeout(() => this.tryWebSocket(), this.reconnectDelay);
                }
            }, 5000);
            
            this.ws.onopen = () => {
                clearTimeout(timeout);
                console.log('[Connection] ✓ WebSocket connected');
                this.mode = 'websocket';
                this.wsRetries = 0;
                this.isConnected = true;
                this.lastMessageTime = Date.now();
                this.app.setConnected(true);
                
                // Update debug panel
                const debugWsEl = document.getElementById('debug-ws-status');
                if (debugWsEl) {
                    debugWsEl.textContent = 'WebSocket: Connected ✓';
                    debugWsEl.style.color = '#10b981';
                }
                
                // Start heartbeat monitor
                this.startHeartbeatMonitor();
            };
            
            this.ws.onmessage = (event) => {
                this.lastMessageTime = Date.now();
                this.recentActivity = true;
                this.handleMessage(event.data);
            };
            
            this.ws.onerror = (error) => {
                clearTimeout(timeout);
                console.error('[Connection] WebSocket error:', error);
            };
            
            this.ws.onclose = () => {
                clearTimeout(timeout);
                console.log('[Connection] WebSocket closed');
                this.isConnected = false;
                this.app.setConnected(false);
                this.stopHeartbeatMonitor();
                
                // Attempt reconnect or fallback
                this.wsRetries++;
                if (this.wsRetries < this.maxWsRetries) {
                    console.log(`[Connection] Reconnecting in ${this.reconnectDelay}ms (attempt ${this.wsRetries}/${this.maxWsRetries})...`);
                    setTimeout(() => this.tryWebSocket(), this.reconnectDelay);
                } else {
                    console.log('[Connection] Switching to polling mode');
                    this.switchToPolling();
                }
            };
            
        } catch (error) {
            console.error('[Connection] WebSocket initialization error:', error);
            this.wsRetries++;
            if (this.wsRetries < this.maxWsRetries) {
                setTimeout(() => this.tryWebSocket(), this.reconnectDelay);
            } else {
                this.switchToPolling();
            }
        }
    }
    
    /**
     * Switch to polling mode
     */
    switchToPolling() {
        console.log('[Connection] Switching to polling mode');
        this.mode = 'polling';
        this.isConnected = true; // Mark as "connected" via polling
        this.app.setConnected(true);
        
        // Start adaptive polling
        this.startPolling();
        this.startActivityMonitor();
    }
    
    /**
     * Start polling updates
     */
    startPolling() {
        if (this.pollTimer) {
            clearInterval(this.pollTimer);
        }
        
        // Initial update
        this.poll();
        
        // Set interval based on activity
        const interval = this.recentActivity ? 2000 : 5000;
        this.pollTimer = setInterval(() => this.poll(), interval);
        
        console.log(`[Connection] Polling started (${interval}ms interval)`);
    }
    
    /**
     * Poll for updates
     */
    async poll() {
        try {
            const data = await fetch('/api/status').then(r => r.json());
            this.handleUpdate(data);
            
            // Update connected state
            if (!this.isConnected) {
                this.isConnected = true;
                this.app.setConnected(true);
            }
        } catch (error) {
            console.error('[Connection] Polling error:', error);
            if (this.isConnected) {
                this.isConnected = false;
                this.app.setConnected(false);
            }
        }
    }
    
    /**
     * Start activity monitor for adaptive polling
     */
    startActivityMonitor() {
        if (this.activityCheckInterval) {
            clearInterval(this.activityCheckInterval);
        }
        
        // Check activity every 10 seconds and adjust polling rate
        this.activityCheckInterval = setInterval(() => {
            const wasActive = this.recentActivity;
            this.recentActivity = false; // Reset for next interval
            
            if (wasActive !== this.recentActivity) {
                // Activity state changed, restart polling with new interval
                this.startPolling();
            }
        }, 10000);
    }
    
    /**
     * Start heartbeat monitor for WebSocket
     */
    startHeartbeatMonitor() {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
        }
        
        // Check if we've received a message recently
        this.heartbeatTimer = setInterval(() => {
            const timeSinceLastMessage = Date.now() - this.lastMessageTime;
            
            // If no message in 30 seconds, consider connection stale
            if (timeSinceLastMessage > 30000) {
                console.log('[Connection] WebSocket appears stale, reconnecting...');
                this.ws?.close();
            }
        }, 10000);
    }
    
    /**
     * Stop heartbeat monitor
     */
    stopHeartbeatMonitor() {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
            this.heartbeatTimer = null;
        }
    }
    
    /**
     * Handle incoming message (WebSocket or polling)
     */
    handleMessage(data) {
        try {
            const message = typeof data === 'string' ? JSON.parse(data) : data;
            this.handleUpdate(message);
        } catch (error) {
            console.error('[Connection] Error parsing message:', error);
        }
    }
    
    /**
     * Handle update data
     */
    handleUpdate(data) {
        // Notify app of update
        if (this.app && typeof this.app.handleRealtimeUpdate === 'function') {
            this.app.handleRealtimeUpdate(data);
        }
    }
    
    /**
     * Send message via WebSocket
     */
    send(data) {
        if (this.mode === 'websocket' && this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(typeof data === 'string' ? data : JSON.stringify(data));
            return true;
        }
        return false;
    }
    
    /**
     * Close connection
     */
    close() {
        console.log('[Connection] Closing connection...');
        
        this.stopHeartbeatMonitor();
        
        if (this.ws) {
            this.ws.close();
            this.ws = null;
        }
        
        if (this.pollTimer) {
            clearInterval(this.pollTimer);
            this.pollTimer = null;
        }
        
        if (this.activityCheckInterval) {
            clearInterval(this.activityCheckInterval);
            this.activityCheckInterval = null;
        }
        
        this.isConnected = false;
    }
    
    /**
     * Get connection status info
     */
    getStatus() {
        return {
            mode: this.mode,
            isConnected: this.isConnected,
            wsRetries: this.wsRetries,
            recentActivity: this.recentActivity
        };
    }
}

// Export for use in app.js
window.ConnectionManager = ConnectionManager;
