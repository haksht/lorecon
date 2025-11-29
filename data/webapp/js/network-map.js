/**
 * Network Map Visualization
 * 
 * Interactive canvas-based visualization of discovered LoRa devices.
 * Positions nodes based on RSSI (signal strength = estimated distance).
 */

class NetworkMap {
    constructor(canvasId, detailsPanelId) {
        this.canvas = document.getElementById(canvasId);
        this.detailsPanel = document.getElementById(detailsPanelId);
        this.ctx = this.canvas.getContext('2d');
        
        // State
        this.devices = [];
        this.selectedNode = null;
        this.hoveredNode = null;
        
        // Layout
        this.centerX = 0;
        this.centerY = 0;
        this.nodeRadius = 20;
        
        // Animation
        this.animationFrame = null;
        this.pulsePhase = 0;
        
        // Colors
        this.colors = {
            meshtastic: '#00d4aa',
            lorawan: '#4a90e2',
            helium: '#9b59b6',
            unknown: '#95a5a6',
            background: '#1a1a2e',
            grid: '#2a2a3e',
            text: '#ffffff',
            weakSignal: '#e74c3c',
            mediumSignal: '#f39c12',
            strongSignal: '#27ae60'
        };
        
        this.setupCanvas();
        this.setupEventListeners();
        this.startAnimation();
    }
    
    setupCanvas() {
        // Make canvas responsive
        const resizeCanvas = () => {
            const container = this.canvas.parentElement;
            this.canvas.width = container.clientWidth;
            this.canvas.height = container.clientHeight;
            this.centerX = this.canvas.width / 2;
            this.centerY = this.canvas.height / 2;
            this.redraw();
        };
        
        window.addEventListener('resize', resizeCanvas);
        resizeCanvas();
    }
    
    setupEventListeners() {
        // Mouse events for desktop
        this.canvas.addEventListener('mousemove', (e) => this.handleMouseMove(e));
        this.canvas.addEventListener('click', (e) => this.handleClick(e));
        this.canvas.addEventListener('mouseleave', () => {
            this.hoveredNode = null;
            this.canvas.style.cursor = 'default';
        });
        
        // Touch events for mobile/iPhone
        this.canvas.addEventListener('touchstart', (e) => this.handleTouch(e), { passive: false });
        this.canvas.addEventListener('touchend', (e) => {
            // Clear hover state on touch end
            this.hoveredNode = null;
        }, { passive: true });
    }
    
    handleTouch(e) {
        e.preventDefault(); // Prevent scroll/zoom
        const touch = e.touches[0];
        const rect = this.canvas.getBoundingClientRect();
        const x = touch.clientX - rect.left;
        const y = touch.clientY - rect.top;
        
        const node = this.findNodeAtPosition(x, y);
        if (node) {
            this.selectedNode = node;
            this.showNodeDetails(node);
            this.canvas.style.cursor = 'pointer';
        } else {
            this.selectedNode = null;
            this.hideNodeDetails();
            this.canvas.style.cursor = 'default';
        }
    }
    
    handleMouseMove(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        // Check if mouse is over any node
        const node = this.findNodeAtPosition(x, y);
        this.hoveredNode = node;
        this.canvas.style.cursor = node ? 'pointer' : 'default';
    }
    
    handleClick(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        const node = this.findNodeAtPosition(x, y);
        if (node) {
            this.selectedNode = node;
            this.showNodeDetails(node);
        } else {
            this.selectedNode = null;
            this.hideNodeDetails();
        }
    }
    
    findNodeAtPosition(x, y) {
        for (const device of this.devices) {
            if (!device.position) continue;
            
            const dx = x - device.position.x;
            const dy = y - device.position.y;
            const distance = Math.sqrt(dx * dx + dy * dy);
            
            if (distance <= this.nodeRadius) {
                return device;
            }
        }
        return null;
    }
    
    updateDevices(devices) {
        // Sort by vulnerability score (highest first = most vulnerable)
        devices.sort((a, b) => {
            const scoreA = this.calculateVulnerabilityScore(a);
            const scoreB = this.calculateVulnerabilityScore(b);
            return scoreB - scoreA; // Descending order
        });
        
        // Calculate positions for all devices
        this.devices = devices.map((device, index) => {
            // Convert RSSI to distance (higher RSSI = closer = smaller radius)
            // RSSI ranges typically from -120 (far) to -30 (close)
            const rssi = device.rssi || device.avgRSSI || -100;
            const normalizedRSSI = Math.max(-120, Math.min(-30, rssi));
            const distance = this.rssiToDistance(normalizedRSSI);
            
            // Arrange nodes in a circle, with distance from center based on RSSI
            const angle = (index / devices.length) * 2 * Math.PI;
            const x = this.centerX + Math.cos(angle) * distance;
            const y = this.centerY + Math.sin(angle) * distance;
            
            return {
                ...device,
                position: { x, y },
                angle: angle,
                vulnerabilityScore: this.calculateVulnerabilityScore(device)
            };
        });
    }
    
    calculateVulnerabilityScore(device) {
        let score = 0;
        
        // High RSSI = device is close = more vulnerable to attack (0-3 points)
        const rssi = device.rssi || device.avgRSSI || -100;
        if (rssi > -50) score += 3;
        else if (rssi > -70) score += 2;
        else if (rssi > -90) score += 1;
        
        // Unencrypted or default PSK = highly vulnerable (0-4 points)
        if (device.hasDefaultPSK) score += 4;
        if (device.encrypted === false) score += 3;
        
        // Router devices are high-value targets (0-2 points)
        if (device.isRouter) score += 2;
        
        // High packet count = active device = valuable target (0-1 point)
        if (device.packetCount > 100) score += 1;
        
        return score; // Range: 0-10
    }
    
    rssiToDistance(rssi) {
        // Map RSSI to canvas distance
        // -30 dBm (strong) = 50px from center
        // -120 dBm (weak) = 300px from center
        const minRadius = 50;
        const maxRadius = Math.min(this.canvas.width, this.canvas.height) * 0.4;
        const normalized = (rssi + 120) / 90; // Normalize to 0-1
        return minRadius + (1 - normalized) * (maxRadius - minRadius);
    }
    
    startAnimation() {
        const animate = () => {
            this.pulsePhase += 0.05;
            this.redraw();
            this.animationFrame = requestAnimationFrame(animate);
        };
        animate();
    }
    
    stopAnimation() {
        if (this.animationFrame) {
            cancelAnimationFrame(this.animationFrame);
            this.animationFrame = null;
        }
    }
    
    redraw() {
        // Clear canvas
        this.ctx.fillStyle = this.colors.background;
        this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
        
        // Draw grid and center
        this.drawGrid();
        this.drawCenter();
        
        // Draw signal range circles
        this.drawRangeCircles();
        
        // Draw connection lines (if devices have hop relationships)
        this.drawConnections();
        
        // Draw nodes
        this.drawNodes();
        
        // Draw hover tooltip
        if (this.hoveredNode) {
            this.drawTooltip(this.hoveredNode);
        }
    }
    
    drawGrid() {
        this.ctx.strokeStyle = this.colors.grid;
        this.ctx.lineWidth = 1;
        this.ctx.setLineDash([5, 5]);
        
        // Draw crosshair at center
        this.ctx.beginPath();
        this.ctx.moveTo(this.centerX, 0);
        this.ctx.lineTo(this.centerX, this.canvas.height);
        this.ctx.moveTo(0, this.centerY);
        this.ctx.lineTo(this.canvas.width, this.centerY);
        this.ctx.stroke();
        
        this.ctx.setLineDash([]);
    }
    
    drawCenter() {
        // Draw sniffer device at center
        const pulse = Math.sin(this.pulsePhase) * 5;
        
        this.ctx.fillStyle = '#ff6b6b';
        this.ctx.beginPath();
        this.ctx.arc(this.centerX, this.centerY, 15 + pulse, 0, 2 * Math.PI);
        this.ctx.fill();
        
        // Label
        this.ctx.fillStyle = this.colors.text;
        this.ctx.font = 'bold 12px Arial';
        this.ctx.textAlign = 'center';
        this.ctx.fillText('SNIFFER', this.centerX, this.centerY + 35);
    }
    
    drawRangeCircles() {
        const ranges = [50, 150, 250];
        this.ctx.strokeStyle = this.colors.grid;
        this.ctx.lineWidth = 1;
        this.ctx.setLineDash([2, 4]);
        
        for (const radius of ranges) {
            this.ctx.beginPath();
            this.ctx.arc(this.centerX, this.centerY, radius, 0, 2 * Math.PI);
            this.ctx.stroke();
        }
        
        this.ctx.setLineDash([]);
    }
    
    drawConnections() {
        // Draw lines from center to each node
        this.ctx.strokeStyle = 'rgba(74, 144, 226, 0.2)';
        this.ctx.lineWidth = 1;
        
        for (const device of this.devices) {
            if (!device.position) continue;
            
            this.ctx.beginPath();
            this.ctx.moveTo(this.centerX, this.centerY);
            this.ctx.lineTo(device.position.x, device.position.y);
            this.ctx.stroke();
        }
    }
    
    drawNodes() {
        for (const device of this.devices) {
            if (!device.position) continue;
            
            const isSelected = this.selectedNode && this.selectedNode.nodeIdDecimal === device.nodeIdDecimal;
            const isHovered = this.hoveredNode && this.hoveredNode.nodeIdDecimal === device.nodeIdDecimal;
            
            // Get color based on protocol
            const protocol = (device.protocol || 'unknown').toLowerCase();
            let nodeColor = this.colors.unknown;
            if (protocol.includes('meshtastic')) nodeColor = this.colors.meshtastic;
            else if (protocol.includes('lorawan')) nodeColor = this.colors.lorawan;
            else if (protocol.includes('helium')) nodeColor = this.colors.helium;
            
            // Draw outer glow if selected
            if (isSelected) {
                const glowPulse = Math.sin(this.pulsePhase * 2) * 5 + 5;
                this.ctx.fillStyle = nodeColor + '40';
                this.ctx.beginPath();
                this.ctx.arc(device.position.x, device.position.y, this.nodeRadius + glowPulse, 0, 2 * Math.PI);
                this.ctx.fill();
            }
            
            // Draw main node circle
            this.ctx.fillStyle = nodeColor;
            this.ctx.beginPath();
            this.ctx.arc(device.position.x, device.position.y, this.nodeRadius, 0, 2 * Math.PI);
            this.ctx.fill();
            
            // Draw border if hovered
            if (isHovered || isSelected) {
                this.ctx.strokeStyle = '#ffffff';
                this.ctx.lineWidth = 3;
                this.ctx.stroke();
            }
            
            // Draw signal strength indicator
            this.drawSignalBars(device);
            
            // Draw node ID
            this.ctx.fillStyle = this.colors.text;
            this.ctx.font = 'bold 10px Arial';
            this.ctx.textAlign = 'center';
            const shortId = device.nodeId ? device.nodeId.substring(0, 4) : '????';
            this.ctx.fillText(shortId, device.position.x, device.position.y + this.nodeRadius + 15);
        }
    }
    
    drawSignalBars(device) {
        const rssi = device.rssi || device.avgRSSI || -100;
        let bars = 0;
        if (rssi > -70) bars = 3;
        else if (rssi > -85) bars = 2;
        else if (rssi > -100) bars = 1;
        
        const barWidth = 3;
        const barGap = 2;
        const startX = device.position.x - 7;
        const startY = device.position.y - 10;
        
        for (let i = 0; i < 3; i++) {
            this.ctx.fillStyle = i < bars ? this.colors.strongSignal : 'rgba(255, 255, 255, 0.2)';
            const barHeight = 4 + (i * 3);
            this.ctx.fillRect(startX + i * (barWidth + barGap), startY - barHeight, barWidth, barHeight);
        }
    }
    
    drawTooltip(device) {
        const padding = 10;
        const lineHeight = 18;
        const lines = [
            `ID: ${device.nodeId || 'Unknown'}`,
            `Protocol: ${device.protocol || 'Unknown'}`,
            `RSSI: ${(device.rssi || device.avgRSSI || 0).toFixed(1)} dBm`,
            `Packets: ${device.packetCount || 0}`
        ];
        
        // Calculate tooltip dimensions
        this.ctx.font = '12px Arial';
        const maxWidth = Math.max(...lines.map(line => this.ctx.measureText(line).width));
        const tooltipWidth = maxWidth + padding * 2;
        const tooltipHeight = lines.length * lineHeight + padding * 2;
        
        // Position tooltip near mouse
        let x = device.position.x + this.nodeRadius + 10;
        let y = device.position.y - tooltipHeight / 2;
        
        // Keep tooltip on screen
        if (x + tooltipWidth > this.canvas.width) x = device.position.x - this.nodeRadius - tooltipWidth - 10;
        if (y < 0) y = 0;
        if (y + tooltipHeight > this.canvas.height) y = this.canvas.height - tooltipHeight;
        
        // Draw tooltip background
        this.ctx.fillStyle = 'rgba(0, 0, 0, 0.9)';
        this.ctx.fillRect(x, y, tooltipWidth, tooltipHeight);
        
        // Draw tooltip border
        this.ctx.strokeStyle = '#4a90e2';
        this.ctx.lineWidth = 2;
        this.ctx.strokeRect(x, y, tooltipWidth, tooltipHeight);
        
        // Draw text
        this.ctx.fillStyle = this.colors.text;
        this.ctx.textAlign = 'left';
        lines.forEach((line, index) => {
            this.ctx.fillText(line, x + padding, y + padding + (index + 1) * lineHeight);
        });
    }
    
    showNodeDetails(device) {
        if (!this.detailsPanel) {
            console.error('Details panel element not found');
            return;
        }
        
        const rssi = device.rssi || device.avgRSSI || 0;
        const lastSeen = device.lastSeen ? new Date(device.lastSeen * 1000).toLocaleTimeString() : 'Unknown';
        const vulnScore = device.vulnerabilityScore || this.calculateVulnerabilityScore(device);
        
        // Determine vulnerability level and color
        let vulnLevel = 'Low';
        let vulnColor = 'var(--success)';
        if (vulnScore >= 7) {
            vulnLevel = 'High';
            vulnColor = 'var(--danger)';
        } else if (vulnScore >= 4) {
            vulnLevel = 'Medium';
            vulnColor = 'var(--warning)';
        }
        
        let html = '<div class="node-details-card" style="padding: 1rem; background: rgba(0,0,0,0.5); border-radius: 8px; border: 1px solid rgba(255,255,255,0.1);">';
        html += `<h3 style="margin: 0 0 1rem 0; color: var(--primary); font-size: 1.1rem;">📡 Device 0x${device.nodeId || 'Unknown'}</h3>`;
        
        html += '<div style="display: grid; gap: 0.5rem; font-size: 0.9rem;">';
        html += `<div style="display: flex; justify-content: space-between;"><span style="color: var(--text-secondary);">Protocol:</span><strong>${device.protocol || 'Unknown'}</strong></div>`;
        html += `<div style="display: flex; justify-content: space-between;"><span style="color: var(--text-secondary);">Device Type:</span><strong>${device.deviceType || 'Unknown'}</strong></div>`;
        html += `<div style="display: flex; justify-content: space-between;"><span style="color: var(--text-secondary);">RSSI:</span><strong>${rssi.toFixed(1)} dBm</strong></div>`;
        html += `<div style="display: flex; justify-content: space-between;"><span style="color: var(--text-secondary);">Packets:</span><strong>${device.packetCount || 0}</strong></div>`;
        html += `<div style="display: flex; justify-content: space-between;"><span style="color: var(--text-secondary);">Last Seen:</span><strong>${lastSeen}</strong></div>`;
        
        // Vulnerability indicator
        html += `<div style="margin-top: 0.5rem; padding: 0.5rem; background: rgba(255,255,255,0.05); border-radius: 4px; border-left: 3px solid ${vulnColor};">`;
        html += `<div style="display: flex; justify-content: space-between; align-items: center;">`;
        html += `<span style="color: var(--text-secondary);">Vulnerability:</span>`;
        html += `<strong style="color: ${vulnColor};">${vulnLevel} (${vulnScore}/10)</strong>`;
        html += '</div>';
        
        // Vulnerability details
        if (device.hasDefaultPSK) {
            html += '<div style="margin-top: 0.25rem; font-size: 0.8rem; color: var(--danger);">⚠️ Default PSK detected</div>';
        }
        if (device.encrypted === false) {
            html += '<div style="margin-top: 0.25rem; font-size: 0.8rem; color: var(--warning);">⚠️ Unencrypted</div>';
        }
        if (device.isRouter) {
            html += '<div style="margin-top: 0.25rem; font-size: 0.8rem; color: var(--primary);">🔀 Router/Gateway</div>';
        }
        html += '</div>';
        
        html += '</div>';
        html += '</div>';
        
        this.detailsPanel.innerHTML = html;
        this.detailsPanel.style.display = 'block';
    }
        
        this.detailsPanel.innerHTML = `
            <h3>Device Details</h3>
            <div class="detail-row">
                <span class="label">Node ID:</span>
                <span class="value">${device.nodeId || 'Unknown'}</span>
            </div>
            <div class="detail-row">
                <span class="label">Protocol:</span>
                <span class="value">${device.protocol || 'Unknown'}</span>
            </div>
            <div class="detail-row">
                <span class="label">Type:</span>
                <span class="value">${device.deviceType || 'Unknown'}</span>
            </div>
            <div class="detail-row">
                <span class="label">RSSI:</span>
                <span class="value">${rssi.toFixed(1)} dBm</span>
            </div>
            <div class="detail-row">
                <span class="label">Packets:</span>
                <span class="value">${device.packetCount || 0}</span>
            </div>
            <div class="detail-row">
                <span class="label">Last Seen:</span>
                <span class="value">${lastSeen}</span>
            </div>
            <div class="detail-row">
                <span class="label">Power Class:</span>
                <span class="value">${device.powerClass || 'Unknown'}</span>
            </div>
            <div class="detail-row">
                <span class="label">Router:</span>
                <span class="value">${device.isRouter ? 'Yes' : 'No'}</span>
            </div>
            <button class="btn-primary" onclick="app.startTargetedCapture(${device.nodeIdDecimal})">
                Target This Device
            </button>
        `;
        
        this.detailsPanel.style.display = 'block';
    }
    
    hideNodeDetails() {
        if (!this.detailsPanel) return;
        this.detailsPanel.style.display = 'none';
    }
    
    destroy() {
        this.stopAnimation();
        window.removeEventListener('resize', this.setupCanvas);
    }
}

// Export for use in main app
window.NetworkMap = NetworkMap;
