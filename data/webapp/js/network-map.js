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
        
        if (!this.canvas) {
            console.error(`[NetworkMap] Canvas element '${canvasId}' not found!`);
            throw new Error(`Canvas element '${canvasId}' not found`);
        }
        
        this.ctx = this.canvas.getContext('2d');
        if (!this.ctx) {
            console.error('[NetworkMap] Failed to get 2D context');
            throw new Error('Failed to get 2D context');
        }
        
        console.log('[NetworkMap] Canvas initialized:', this.canvas.width, 'x', this.canvas.height);
        
        // State
        this.devices = [];
        this.selectedNode = null;
        this.hoveredNode = null;
        
        // Layout
        this.centerX = 0;
        this.centerY = 0;
        this.nodeRadius = 20;
        this.clickRadius = 50; // Generous click area - 2.5x node radius
        
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
            strongSignal: '#27ae60',
            threatHigh: '#e74c3c',      // Red - vulnerable
            threatMedium: '#f39c12',    // Orange - moderate risk
            threatLow: '#27ae60',       // Green - secure
            heatmapHigh: 'rgba(231, 76, 60, 0.3)',   // Red overlay
            heatmapMedium: 'rgba(243, 156, 18, 0.2)', // Orange overlay
            heatmapLow: 'rgba(39, 174, 96, 0.1)'      // Green overlay
        };
        
        this.showThreatColors = true;  // Toggle for threat vs protocol colors
        this.showHeatmap = true;        // Toggle for signal heatmap
        
        this.setupCanvas();
        this.setupEventListeners();
        this.startAnimation();
    }
    
    setupCanvas() {
        // Make canvas responsive
        const resizeCanvas = () => {
            const container = this.canvas.parentElement;
            if (!container) {
                console.error('[NetworkMap] Canvas has no parent element');
                return;
            }
            
            const oldWidth = this.canvas.width;
            const oldHeight = this.canvas.height;
            
            // Get actual container dimensions
            const rect = container.getBoundingClientRect();
            this.canvas.width = rect.width;
            this.canvas.height = this.canvas.offsetHeight || 600;
            
            this.centerX = this.canvas.width / 2;
            this.centerY = this.canvas.height / 2;
            
            console.log(`[NetworkMap] Canvas resized from ${oldWidth}x${oldHeight} to ${this.canvas.width}x${this.canvas.height}`);
            console.log(`[NetworkMap] Canvas offset dimensions: ${this.canvas.offsetWidth}x${this.canvas.offsetHeight}`);
            console.log(`[NetworkMap] New center point: (${this.centerX}, ${this.centerY})`);
            
            this.redraw();
        };
        
        window.addEventListener('resize', resizeCanvas);
        // Small delay to ensure canvas has rendered
        setTimeout(resizeCanvas, 100);
        resizeCanvas();
        
        console.log('[NetworkMap] Canvas setup complete:', this.canvas.width, 'x', this.canvas.height);
        console.log('[NetworkMap] Center point:', this.centerX, ',', this.centerY);
    }
    
    setupEventListeners() {
        console.log('[NetworkMap] Setting up event listeners on canvas:', this.canvas);
        console.log('[NetworkMap] Canvas element:', this.canvas.tagName, this.canvas.id);
        console.log('[NetworkMap] Canvas dimensions:', this.canvas.width, 'x', this.canvas.height);
        console.log('[NetworkMap] Canvas style.pointerEvents:', window.getComputedStyle(this.canvas).pointerEvents);
        console.log('[NetworkMap] Canvas style.cursor:', window.getComputedStyle(this.canvas).cursor);
        
        // Mouse events for desktop
        this.canvas.addEventListener('mousemove', (e) => {
            this.handleMouseMove(e);
        }, false);
        
        this.canvas.addEventListener('click', (e) => {
            e.preventDefault();
            e.stopPropagation();
            console.log('[NetworkMap] CLICK EVENT RECEIVED at client:', e.clientX, e.clientY);
            console.log('[NetworkMap] Canvas bounding rect:', this.canvas.getBoundingClientRect());
            this.handleClick(e);
        }, false);
        
        this.canvas.addEventListener('mouseleave', () => {
            this.hoveredNode = null;
            this.canvas.classList.remove('pointer-cursor');
            this.canvas.style.cursor = 'default';
        });
        
        // Touch events for mobile/iPhone
        this.canvas.addEventListener('touchstart', (e) => this.handleTouch(e), { passive: false });
        this.canvas.addEventListener('touchend', (e) => {
            // Clear hover state on touch end
            this.hoveredNode = null;
        }, { passive: true });
        
        // Test click
        console.log('[NetworkMap] Event listeners installed. Click the canvas to test.');
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
        
        // Update cursor using both class and style for maximum compatibility
        if (node) {
            this.canvas.classList.add('pointer-cursor');
            this.canvas.style.cursor = 'pointer';
        } else {
            this.canvas.classList.remove('pointer-cursor');
            this.canvas.style.cursor = 'default';
        }
        
        if (node !== this.hoveredNode) {
            this.hoveredNode = node;
            console.log('[NetworkMap] Hover state changed:', node ? `Device ${node.nodeId || node.source}` : 'none');
            
            // Trigger redraw to show hover state
            requestAnimationFrame(() => this.redraw());
        }
    }
    
    handleClick(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        console.log('[NetworkMap] ===== CLICK DEBUG =====');
        console.log('[NetworkMap] Client click:', e.clientX, e.clientY);
        console.log('[NetworkMap] Canvas rect:', rect);
        console.log('[NetworkMap] Canvas position - left:', rect.left, 'top:', rect.top);
        console.log('[NetworkMap] Canvas size:', this.canvas.width, 'x', this.canvas.height);
        console.log('[NetworkMap] Calculated canvas coords:', x.toFixed(0), y.toFixed(0));
        console.log('[NetworkMap] Canvas center:', this.centerX, this.centerY);
        
        const node = this.findNodeAtPosition(x, y);
        if (node) {
            console.log('[NetworkMap] Device selected:', node.nodeId || node.source);
            this.selectedNode = node;
            this.showNodeDetails(node);
        } else {
            console.log('[NetworkMap] Clicked on empty area');
            this.selectedNode = null;
            this.hideNodeDetails();
        }
        
        // Trigger redraw
        this.redraw();
    }
    
    findNodeAtPosition(x, y) {
        // Use larger click radius for easier clicking
        const clickRadius = this.clickRadius || 50;
        
        console.log(`[NetworkMap] Finding node at (${x.toFixed(0)}, ${y.toFixed(0)})`);
        console.log(`[NetworkMap] Total devices in array: ${this.devices.length}`);
        console.log(`[NetworkMap] Click radius: ${clickRadius}px, Node radius: ${this.nodeRadius}px`);
        console.log(`[NetworkMap] Canvas center: (${this.centerX}, ${this.centerY})`);
        
        if (!this.devices || this.devices.length === 0) {
            console.log('[NetworkMap] No devices to check');
            return null;
        }
        
        for (const device of this.devices) {
            if (!device.position) {
                console.log('[NetworkMap] Device has no position:', device.nodeId || device.source);
                continue;
            }
            
            const dx = x - device.position.x;
            const dy = y - device.position.y;
            const distance = Math.sqrt(dx * dx + dy * dy);
            
            console.log(`[NetworkMap] Device ${device.nodeId || device.source} at (${device.position.x.toFixed(0)}, ${device.position.y.toFixed(0)}), distance=${distance.toFixed(1)}px`);
            
            if (distance <= clickRadius) {
                console.log(`[NetworkMap] ✓ DEVICE FOUND! ${device.nodeId || device.source}, distance=${distance.toFixed(1)}px (threshold: ${clickRadius}px)`);
                return device;
            }
        }
        
        console.log('[NetworkMap] No device found within click radius');
        return null;
    }
    
    updateDevices(devices) {
        console.log('[NetworkMap] Updating with', devices.length, 'devices');
        
        if (!devices || devices.length === 0) {
            this.devices = [];
            console.log('[NetworkMap] No devices to display');
            return;
        }
        
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
            
            console.log(`[NetworkMap] Device ${index}: nodeId=${device.nodeId}, rssi=${rssi}, pos=(${x.toFixed(0)},${y.toFixed(0)}), distance=${distance.toFixed(0)}, center=(${this.centerX},${this.centerY})`);
            console.log('[NetworkMap] Canvas dimensions at positioning:', this.canvas.width, 'x', this.canvas.height);
            
            return {
                ...device,
                position: { x, y },
                angle: angle,
                vulnerabilityScore: this.calculateVulnerabilityScore(device)
            };
        });
        
        console.log('[NetworkMap] Positioned', this.devices.length, 'devices');
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
    
    getNodeColor(device) {
        if (this.showThreatColors) {
            // Use riskLevel from security API if available (from enriched data)
            if (device.riskLevel) {
                if (device.riskLevel === 'vulnerable') return this.colors.threatHigh;
                if (device.riskLevel === 'moderate') return this.colors.threatMedium;
                if (device.riskLevel === 'secure') return this.colors.threatLow;
            }
            // Fallback to calculated score
            const score = device.vulnerabilityScore || this.calculateVulnerabilityScore(device);
            if (score >= 7) return this.colors.threatHigh;      // 7-10: High threat
            if (score >= 4) return this.colors.threatMedium;    // 4-6: Medium threat
            return this.colors.threatLow;                        // 0-3: Low threat
        } else {
            // Use protocol-based colors
            const protocol = (device.protocol || 'unknown').toLowerCase();
            if (protocol.includes('meshtastic')) return this.colors.meshtastic;
            if (protocol.includes('lorawan')) return this.colors.lorawan;
            if (protocol.includes('helium')) return this.colors.helium;
            return this.colors.unknown;
        }
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
    
    getNodeType(device) {
        // Determine if node is transmitter, relay-only, or mixed
        const originated = device.originatedPackets || 0;
        const relayed = device.relayedPackets || 0;
        
        if (originated > 0 && relayed > 0) return 'mixed';
        if (relayed > 0) return 'relay';
        return 'transmitter';  // Default for devices with no relay info
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
        
        // Draw signal strength heatmap (before nodes)
        if (this.showHeatmap) {
            this.drawHeatmap();
        }
        
        // Draw connection lines (if devices have hop relationships)
        this.drawConnections();
        
        // Draw nodes
        this.drawNodes();
        
        // Draw hover tooltip
        if (this.hoveredNode) {
            this.drawTooltip(this.hoveredNode);
        }
        
        // Draw legend
        this.drawLegend();
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
    
    drawHeatmap() {
        // Draw signal strength heatmap for each device
        this.devices.forEach(device => {
            if (!device.position) return;
            
            const rssi = device.rssi || device.avgRSSI || -100;
            const score = device.vulnerabilityScore || this.calculateVulnerabilityScore(device);
            
            // Determine heatmap radius based on RSSI (stronger signal = larger radius)
            let heatmapRadius = 80;
            if (rssi > -50) heatmapRadius = 120;
            else if (rssi > -70) heatmapRadius = 100;
            else if (rssi > -90) heatmapRadius = 80;
            else heatmapRadius = 60;
            
            // Create radial gradient based on threat level
            const gradient = this.ctx.createRadialGradient(
                device.position.x, device.position.y, 0,
                device.position.x, device.position.y, heatmapRadius
            );
            
            // Color based on threat level
            let centerColor, edgeColor;
            if (score >= 7) {
                centerColor = 'rgba(231, 76, 60, 0.4)';  // Red
                edgeColor = 'rgba(231, 76, 60, 0)';
            } else if (score >= 4) {
                centerColor = 'rgba(243, 156, 18, 0.3)'; // Orange
                edgeColor = 'rgba(243, 156, 18, 0)';
            } else {
                centerColor = 'rgba(39, 174, 96, 0.2)';  // Green
                edgeColor = 'rgba(39, 174, 96, 0)';
            }
            
            gradient.addColorStop(0, centerColor);
            gradient.addColorStop(1, edgeColor);
            
            this.ctx.fillStyle = gradient;
            this.ctx.beginPath();
            this.ctx.arc(device.position.x, device.position.y, heatmapRadius, 0, 2 * Math.PI);
            this.ctx.fill();
        });
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
            
            // Get color based on threat level or protocol
            const nodeColor = this.getNodeColor(device);
            
            // Draw outer glow if selected
            if (isSelected) {
                const glowPulse = Math.sin(this.pulsePhase * 2) * 5 + 5;
                this.ctx.fillStyle = nodeColor + '40';
                this.ctx.beginPath();
                this.ctx.arc(device.position.x, device.position.y, this.nodeRadius + glowPulse, 0, 2 * Math.PI);
                this.ctx.fill();
            }
            
            // Determine node type (transmitter, relay, mixed)
            const nodeType = this.getNodeType(device);
            
            // Draw main node circle (solid for transmitters, hollow for relay-only)
            if (nodeType === 'relay') {
                // Hollow circle for relay-only nodes
                this.ctx.strokeStyle = nodeColor;
                this.ctx.lineWidth = 3;
                this.ctx.beginPath();
                this.ctx.arc(device.position.x, device.position.y, this.nodeRadius, 0, 2 * Math.PI);
                this.ctx.stroke();
            } else {
                // Solid circle for transmitters and mixed
                this.ctx.fillStyle = nodeColor;
                this.ctx.beginPath();
                this.ctx.arc(device.position.x, device.position.y, this.nodeRadius, 0, 2 * Math.PI);
                this.ctx.fill();
                
                // Add double circle for mixed nodes
                if (nodeType === 'mixed') {
                    this.ctx.strokeStyle = nodeColor;
                    this.ctx.lineWidth = 2;
                    this.ctx.beginPath();
                    this.ctx.arc(device.position.x, device.position.y, this.nodeRadius - 5, 0, 2 * Math.PI);
                    this.ctx.stroke();
                }
            }
            
            // Draw border if hovered
            if (isHovered || isSelected) {
                this.ctx.strokeStyle = '#ffffff';
                this.ctx.lineWidth = 3;
                this.ctx.beginPath();
                this.ctx.arc(device.position.x, device.position.y, this.nodeRadius + 3, 0, 2 * Math.PI);
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
            console.error('[NetworkMap] Details panel element not found - ID:', this.detailsPanelId || 'unknown');
            console.error('[NetworkMap] Available elements:', document.querySelectorAll('[id*="network"]'));
            return;
        }
        
        console.log('[NetworkMap] ===== SHOWING DEVICE DETAILS =====');
        console.log('[NetworkMap] Device:', device);
        console.log('[NetworkMap] Panel element:', this.detailsPanel);
        console.log('[NetworkMap] Panel display before:', this.detailsPanel.style.display);
        
        const rssi = device.rssi || device.avgRSSI || 0;
        const lastSeen = this.formatLastSeen(device.lastSeenSecondsAgo);
        const vulnScore = device.vulnerabilityScore || this.calculateVulnerabilityScore(device);
        
        // Determine vulnerability level and color - use riskLevel from security API if available
        let vulnLevel = 'Low';
        let vulnColor = '#50c878';
        if (device.riskLevel === 'vulnerable' || vulnScore >= 7) {
            vulnLevel = 'High';
            vulnColor = '#ff6b6b';
        } else if (device.riskLevel === 'moderate' || vulnScore >= 4) {
            vulnLevel = 'Medium';
            vulnColor = '#ffd93d';
        }
        
        let html = '<div class="node-details-card" style="padding: 1.5rem; background: rgba(26, 26, 46, 0.95); border-radius: 8px; border: 2px solid rgba(74, 144, 226, 0.5); margin: 0.5rem 0;">';
        html += `<h3 style="margin: 0 0 1rem 0; color: #4a90e2; font-size: 1.2rem; font-weight: 600;">📡 Device 0x${device.nodeId || 'Unknown'}</h3>`;
        
        html += '<div style="display: grid; gap: 0.75rem; font-size: 1rem;">';
        html += `<div style="display: flex; justify-content: space-between; padding: 0.5rem; background: rgba(255,255,255,0.05); border-radius: 4px;"><span style="color: #a0a0a0;">Protocol:</span><strong style="color: #e0e0e0;">${device.protocol || 'Unknown'}</strong></div>`;
        html += `<div style="display: flex; justify-content: space-between; padding: 0.5rem; background: rgba(255,255,255,0.05); border-radius: 4px;"><span style="color: #a0a0a0;">Device Type:</span><strong style="color: #e0e0e0;">${device.deviceType || 'Unknown'}</strong></div>`;
        html += `<div style="display: flex; justify-content: space-between; padding: 0.5rem; background: rgba(255,255,255,0.05); border-radius: 4px;"><span style="color: #a0a0a0;">RSSI:</span><strong style="color: #e0e0e0;">${rssi.toFixed(1)} dBm</strong></div>`;
        html += `<div style="display: flex; justify-content: space-between; padding: 0.5rem; background: rgba(255,255,255,0.05); border-radius: 4px;"><span style="color: #a0a0a0;">Packets:</span><strong style="color: #e0e0e0;">${device.packetCount || 0}</strong></div>`;
        html += `<div style="display: flex; justify-content: space-between; padding: 0.5rem; background: rgba(255,255,255,0.05); border-radius: 4px;"><span style="color: #a0a0a0;">Last Seen:</span><strong style="color: #e0e0e0;">${lastSeen}</strong></div>`;
        
        // Vulnerability indicator
        html += `<div style="margin-top: 0.5rem; padding: 1rem; background: rgba(255,255,255,0.08); border-radius: 6px; border-left: 4px solid ${vulnColor};">`;
        html += `<div style="display: flex; justify-content: space-between; align-items: center;">`;
        html += `<span style="color: #a0a0a0; font-weight: 600;">Vulnerability:</span>`;
        html += `<strong style="color: ${vulnColor}; font-size: 1.1rem;">${vulnLevel} (${vulnScore}/10)</strong>`;
        html += '</div>';
        
        // Vulnerability details
        if (device.hasDefaultPSK) {
            html += '<div style="margin-top: 0.5rem; font-size: 0.9rem; color: #ff6b6b; font-weight: 500;">⚠️ Default PSK detected</div>';
        }
        if (device.encrypted === false) {
            html += '<div style="margin-top: 0.5rem; font-size: 0.9rem; color: #ffd93d; font-weight: 500;">⚠️ Unencrypted</div>';
        }
        
        // Node type indicator
        const nodeType = this.getNodeType(device);
        const nodeTypeLabel = nodeType === 'transmitter' ? '⚡ Active Transmitter' : 
                             nodeType === 'relay' ? '↻ Relay Node' : '⚡↻ Mixed (TX+Relay)';
        const nodeTypeColor = nodeType === 'transmitter' ? '#50c878' : 
                             nodeType === 'relay' ? '#4a90e2' : '#9b59b6';
        html += `<div style="margin-top: 0.5rem; font-size: 0.9rem; color: ${nodeTypeColor}; font-weight: 500;">${nodeTypeLabel}</div>`;
        if (device.originatedPackets > 0) {
            html += `<div style="margin-top: 0.25rem; font-size: 0.85rem; color: #a0a0a0;">Originated: ${device.originatedPackets} packets</div>`;
        }
        if (device.relayedPackets > 0) {
            html += `<div style="margin-top: 0.25rem; font-size: 0.85rem; color: #a0a0a0;">Relayed: ${device.relayedPackets} packets</div>`;
        }
        
        if (device.isRouter) {
            html += '<div style="margin-top: 0.5rem; font-size: 0.9rem; color: #4a90e2; font-weight: 500;">🔀 Router/Gateway</div>';
        }
        html += '</div>';
        
        html += '</div>';
        html += '</div>';
        
        this.detailsPanel.innerHTML = html;
        this.detailsPanel.style.display = 'block';
        this.detailsPanel.style.visibility = 'visible';
        this.detailsPanel.style.opacity = '1';
        
        console.log('[NetworkMap] Panel innerHTML set, length:', html.length);
        console.log('[NetworkMap] Panel display after:', this.detailsPanel.style.display);
        console.log('[NetworkMap] Panel visibility:', this.detailsPanel.style.visibility);
        
        // Scroll the details panel into view
        setTimeout(() => {
            this.detailsPanel.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
            console.log('[NetworkMap] Scrolled panel into view');
        }, 100);
    }
    
    hideNodeDetails() {
        if (!this.detailsPanel) return;
        this.detailsPanel.style.display = 'none';
    }
    
    drawLegend() {
        const padding = 15;
        const startX = this.canvas.width - 180;
        const startY = 20;
        const lineHeight = 30;
        
        // Background
        this.ctx.fillStyle = 'rgba(26, 26, 46, 0.9)';
        this.ctx.fillRect(startX - padding, startY - padding, 160, 130);
        
        // Border
        this.ctx.strokeStyle = 'rgba(74, 144, 226, 0.5)';
        this.ctx.lineWidth = 2;
        this.ctx.strokeRect(startX - padding, startY - padding, 160, 130);
        
        // Title
        this.ctx.fillStyle = '#ffffff';
        this.ctx.font = 'bold 12px Arial';
        this.ctx.fillText('Node Types', startX, startY);
        
        // Legend items
        this.ctx.font = '11px Arial';
        
        // Active Transmitter (solid circle)
        this.ctx.fillStyle = this.colors.meshtastic;
        this.ctx.beginPath();
        this.ctx.arc(startX, startY + lineHeight, 8, 0, 2 * Math.PI);
        this.ctx.fill();
        this.ctx.fillStyle = '#e0e0e0';
        this.ctx.fillText('Active Transmitter', startX + 15, startY + lineHeight + 4);
        
        // Relay-Only (hollow circle)
        this.ctx.strokeStyle = this.colors.lorawan;
        this.ctx.lineWidth = 3;
        this.ctx.beginPath();
        this.ctx.arc(startX, startY + lineHeight * 2, 8, 0, 2 * Math.PI);
        this.ctx.stroke();
        this.ctx.fillStyle = '#e0e0e0';
        this.ctx.fillText('Relay Only', startX + 15, startY + lineHeight * 2 + 4);
        
        // Mixed (double circle)
        this.ctx.fillStyle = this.colors.helium;
        this.ctx.beginPath();
        this.ctx.arc(startX, startY + lineHeight * 3, 8, 0, 2 * Math.PI);
        this.ctx.fill();
        this.ctx.strokeStyle = this.colors.helium;
        this.ctx.lineWidth = 2;
        this.ctx.beginPath();
        this.ctx.arc(startX, startY + lineHeight * 3, 5, 0, 2 * Math.PI);
        this.ctx.stroke();
        this.ctx.fillStyle = '#e0e0e0';
        this.ctx.fillText('Mixed (TX+Relay)', startX + 15, startY + lineHeight * 3 + 4);
    }
    
    formatLastSeen(secondsAgo) {
        if (secondsAgo === undefined || secondsAgo === null) return 'Unknown';
        if (secondsAgo === 0) return 'Just now';
        if (secondsAgo < 60) return `${Math.floor(secondsAgo)}s ago`;
        if (secondsAgo < 3600) return `${Math.floor(secondsAgo / 60)}m ago`;
        if (secondsAgo < 86400) return `${Math.floor(secondsAgo / 3600)}h ago`;
        return `${Math.floor(secondsAgo / 86400)}d ago`;
    }
    
    destroy() {
        this.stopAnimation();
        window.removeEventListener('resize', this.setupCanvas);
    }
}

// Export for use in main app
window.NetworkMap = NetworkMap;
