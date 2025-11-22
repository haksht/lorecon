/* =================================================================
   Protocol Statistics Dashboard
   Real-time breakdown of captured packets by protocol
   ================================================================= */

class ProtocolStats {
    constructor(canvasId, containerId) {
        this.canvas = document.getElementById(canvasId);
        this.container = document.getElementById(containerId);
        this.ctx = this.canvas ? this.canvas.getContext('2d') : null;
        
        this.stats = {
            Meshtastic: { count: 0, color: '#4a90e2' },
            LoRaWAN: { count: 0, color: '#f59e0b' },
            Helium: { count: 0, color: '#10b981' },
            Unknown: { count: 0, color: '#6b7280' }
        };
        
        this.total = 0;
        this.animationFrame = null;
        
        if (this.canvas) {
            this.setupCanvas();
            window.addEventListener('resize', () => this.setupCanvas());
        }
    }
    
    setupCanvas() {
        if (!this.canvas) return;
        
        // Set canvas size to match container
        const rect = this.canvas.parentElement.getBoundingClientRect();
        this.canvas.width = rect.width;
        this.canvas.height = Math.min(300, rect.width * 0.6); // Maintain aspect ratio
    }
    
    update(devices) {
        // Reset counts
        Object.keys(this.stats).forEach(key => {
            this.stats[key].count = 0;
        });
        this.total = 0;
        
        // Count by protocol
        if (devices && Array.isArray(devices)) {
            devices.forEach(device => {
                const protocol = device.protocol || 'Unknown';
                let matched = false;
                
                // Match protocol to category
                for (const key of Object.keys(this.stats)) {
                    if (protocol.includes(key)) {
                        this.stats[key].count += device.packetCount || 1;
                        this.total += device.packetCount || 1;
                        matched = true;
                        break;
                    }
                }
                
                // Default to Unknown
                if (!matched) {
                    this.stats.Unknown.count += device.packetCount || 1;
                    this.total += device.packetCount || 1;
                }
            });
        }
        
        this.render();
    }
    
    render() {
        if (!this.ctx || !this.canvas) return;
        
        const width = this.canvas.width;
        const height = this.canvas.height;
        
        // Clear canvas
        this.ctx.clearRect(0, 0, width, height);
        
        if (this.total === 0) {
            this.drawEmptyState();
            return;
        }
        
        // Draw pie chart or bar chart depending on space
        if (width > 400) {
            this.drawPieChart(width, height);
        } else {
            this.drawBarChart(width, height);
        }
    }
    
    drawEmptyState() {
        const width = this.canvas.width;
        const height = this.canvas.height;
        
        this.ctx.fillStyle = '#6b7280';
        this.ctx.font = '16px -apple-system, system-ui, sans-serif';
        this.ctx.textAlign = 'center';
        this.ctx.textBaseline = 'middle';
        this.ctx.fillText('No packets captured yet', width / 2, height / 2);
        this.ctx.font = '12px -apple-system, system-ui, sans-serif';
        this.ctx.fillText('Data will appear as devices are discovered', width / 2, height / 2 + 25);
    }
    
    drawPieChart(width, height) {
        const centerX = width / 3;
        const centerY = height / 2;
        const radius = Math.min(centerX, centerY) - 20;
        
        let currentAngle = -Math.PI / 2; // Start at top
        
        // Draw slices
        Object.entries(this.stats).forEach(([protocol, data]) => {
            if (data.count === 0) return;
            
            const sliceAngle = (data.count / this.total) * 2 * Math.PI;
            
            // Draw slice
            this.ctx.beginPath();
            this.ctx.moveTo(centerX, centerY);
            this.ctx.arc(centerX, centerY, radius, currentAngle, currentAngle + sliceAngle);
            this.ctx.closePath();
            this.ctx.fillStyle = data.color;
            this.ctx.fill();
            
            // Draw border
            this.ctx.strokeStyle = '#1a1a2e';
            this.ctx.lineWidth = 2;
            this.ctx.stroke();
            
            currentAngle += sliceAngle;
        });
        
        // Draw center circle (donut chart)
        this.ctx.beginPath();
        this.ctx.arc(centerX, centerY, radius * 0.5, 0, 2 * Math.PI);
        this.ctx.fillStyle = '#1a1a2e';
        this.ctx.fill();
        
        // Draw total in center
        this.ctx.fillStyle = '#eaeaea';
        this.ctx.font = 'bold 24px -apple-system, system-ui, sans-serif';
        this.ctx.textAlign = 'center';
        this.ctx.textBaseline = 'middle';
        this.ctx.fillText(this.total.toString(), centerX, centerY - 10);
        this.ctx.font = '12px -apple-system, system-ui, sans-serif';
        this.ctx.fillStyle = '#b0b0b0';
        this.ctx.fillText('packets', centerX, centerY + 12);
        
        // Draw legend
        this.drawLegend(width * 0.6, 30);
    }
    
    drawBarChart(width, height) {
        const protocols = Object.entries(this.stats).filter(([_, data]) => data.count > 0);
        if (protocols.length === 0) return;
        
        const barHeight = 30;
        const spacing = 15;
        const maxBarWidth = width - 150;
        const startY = (height - (protocols.length * (barHeight + spacing))) / 2;
        
        protocols.forEach(([protocol, data], index) => {
            const y = startY + index * (barHeight + spacing);
            const barWidth = (data.count / this.total) * maxBarWidth;
            const percentage = ((data.count / this.total) * 100).toFixed(1);
            
            // Draw bar
            this.ctx.fillStyle = data.color;
            this.ctx.fillRect(100, y, barWidth, barHeight);
            
            // Draw label
            this.ctx.fillStyle = '#eaeaea';
            this.ctx.font = '14px -apple-system, system-ui, sans-serif';
            this.ctx.textAlign = 'right';
            this.ctx.textBaseline = 'middle';
            this.ctx.fillText(protocol, 90, y + barHeight / 2);
            
            // Draw count and percentage
            this.ctx.textAlign = 'left';
            this.ctx.fillText(`${data.count} (${percentage}%)`, 110 + barWidth, y + barHeight / 2);
        });
    }
    
    drawLegend(x, y) {
        const lineHeight = 30;
        let currentY = y;
        
        Object.entries(this.stats).forEach(([protocol, data]) => {
            if (data.count === 0) return;
            
            const percentage = ((data.count / this.total) * 100).toFixed(1);
            
            // Draw color box
            this.ctx.fillStyle = data.color;
            this.ctx.fillRect(x, currentY, 20, 20);
            this.ctx.strokeStyle = '#1a1a2e';
            this.ctx.lineWidth = 1;
            this.ctx.strokeRect(x, currentY, 20, 20);
            
            // Draw text
            this.ctx.fillStyle = '#eaeaea';
            this.ctx.font = '14px -apple-system, system-ui, sans-serif';
            this.ctx.textAlign = 'left';
            this.ctx.textBaseline = 'middle';
            this.ctx.fillText(`${protocol}:`, x + 30, currentY + 10);
            
            // Draw count and percentage
            this.ctx.font = 'bold 14px -apple-system, system-ui, sans-serif';
            this.ctx.fillStyle = data.color;
            this.ctx.fillText(`${data.count} (${percentage}%)`, x + 30 + this.ctx.measureText(`${protocol}:`).width + 10, currentY + 10);
            
            currentY += lineHeight;
        });
    }
    
    destroy() {
        if (this.animationFrame) {
            cancelAnimationFrame(this.animationFrame);
        }
    }
}
