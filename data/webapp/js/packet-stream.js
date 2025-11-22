/* =================================================================
   Live Packet Stream - Real-time packet feed with syntax highlighting
   Shows packets as they arrive with color-coded fields
   ================================================================= */

class PacketStream {
    constructor(containerId, maxPackets = 50) {
        this.container = document.getElementById(containerId);
        this.maxPackets = maxPackets;
        this.packets = [];
        this.paused = false;
        this.autoScroll = true;
    }
    
    addPacket(packet) {
        if (this.paused) return;
        
        // Add to beginning of array (newest first)
        this.packets.unshift({
            ...packet,
            timestamp: Date.now(),
            id: this.generateId()
        });
        
        // Trim to max
        if (this.packets.length > this.maxPackets) {
            this.packets = this.packets.slice(0, this.maxPackets);
        }
        
        this.render();
    }
    
    generateId() {
        return `pkt-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    }
    
    render() {
        if (!this.container) return;
        
        if (this.packets.length === 0) {
            this.container.innerHTML = `
                <div class="packet-stream-empty">
                    <p>📡 Waiting for packets...</p>
                    <p class="hint">Packets will appear here in real-time as they're captured</p>
                </div>
            `;
            return;
        }
        
        let html = '<div class="packet-stream-list">';
        
        this.packets.forEach(packet => {
            html += this.renderPacket(packet);
        });
        
        html += '</div>';
        this.container.innerHTML = html;
        
        // Auto-scroll to top if enabled
        if (this.autoScroll && this.container.scrollTop < 100) {
            this.container.scrollTop = 0;
        }
    }
    
    renderPacket(packet) {
        const age = this.formatAge(Date.now() - packet.timestamp);
        const protocolClass = this.getProtocolClass(packet.protocol);
        const encryptionBadge = this.getEncryptionBadge(packet);
        
        return `
            <div class="packet-item" data-id="${packet.id}">
                <div class="packet-header">
                    <span class="protocol-badge ${protocolClass}">${packet.protocol || 'Unknown'}</span>
                    ${encryptionBadge}
                    <span class="packet-age">${age}</span>
                </div>
                <div class="packet-body">
                    <div class="packet-field">
                        <span class="field-label">Node:</span>
                        <span class="field-value node-id">${packet.nodeId ? '0x' + packet.nodeId : '—'}</span>
                    </div>
                    <div class="packet-field">
                        <span class="field-label">Freq:</span>
                        <span class="field-value frequency">${packet.frequencyMHz ? packet.frequencyMHz.toFixed(3) + ' MHz' : '—'}</span>
                    </div>
                    <div class="packet-field">
                        <span class="field-label">RSSI:</span>
                        <span class="field-value rssi ${this.getRSSIClass(packet.rssi)}">${packet.rssi ? packet.rssi.toFixed(1) + ' dBm' : '—'}</span>
                    </div>
                    <div class="packet-field">
                        <span class="field-label">Size:</span>
                        <span class="field-value">${packet.length || 0} bytes</span>
                    </div>
                </div>
                ${this.renderPacketText(packet)}
            </div>
        `;
    }
    
    renderPacketText(packet) {
        if (packet.decryptedText) {
            return `
                <div class="packet-text">
                    <span class="field-label">📝 Text:</span>
                    <span class="field-value text-content">"${this.escapeHtml(packet.decryptedText)}"</span>
                </div>
            `;
        } else if (packet.encrypted) {
            return `
                <div class="packet-text encrypted">
                    <span class="field-label">🔒 Content:</span>
                    <span class="field-value">Encrypted</span>
                </div>
            `;
        }
        return '';
    }
    
    getProtocolClass(protocol) {
        if (!protocol) return 'protocol-unknown';
        if (protocol.includes('Meshtastic')) return 'protocol-meshtastic';
        if (protocol.includes('LoRaWAN')) return 'protocol-lorawan';
        if (protocol.includes('Helium')) return 'protocol-helium';
        return 'protocol-unknown';
    }
    
    getEncryptionBadge(packet) {
        if (packet.decryptedText) {
            return '<span class="encryption-badge plaintext">📖 Plain</span>';
        } else if (packet.encrypted !== false) {
            return '<span class="encryption-badge encrypted">🔒 Encrypted</span>';
        }
        return '';
    }
    
    getRSSIClass(rssi) {
        if (!rssi) return '';
        if (rssi > -60) return 'rssi-excellent';
        if (rssi > -75) return 'rssi-good';
        if (rssi > -90) return 'rssi-fair';
        return 'rssi-poor';
    }
    
    formatAge(ms) {
        const seconds = Math.floor(ms / 1000);
        if (seconds < 60) return `${seconds}s`;
        const minutes = Math.floor(seconds / 60);
        if (minutes < 60) return `${minutes}m`;
        const hours = Math.floor(minutes / 60);
        return `${hours}h`;
    }
    
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    clear() {
        this.packets = [];
        this.render();
    }
    
    pause() {
        this.paused = true;
    }
    
    resume() {
        this.paused = false;
    }
    
    toggleAutoScroll() {
        this.autoScroll = !this.autoScroll;
        return this.autoScroll;
    }
}
