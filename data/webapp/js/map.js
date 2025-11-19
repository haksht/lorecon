/**
 * Map Handler - Geographic Visualization with Leaflet.js
 */

class MapHandler {
    constructor() {
        this.map = null;
        this.markers = {};
        this.trails = {};
        this.initialized = false;
    }

    /**
     * Initialize map
     */
    init() {
        if (this.initialized) return;
        
        // Create map centered on default location
        this.map = L.map('map').setView([37.7749, -122.4194], 13);
        
        // Add OpenStreetMap tiles
        L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
            attribution: '© OpenStreetMap contributors',
            maxZoom: 19
        }).addTo(this.map);
        
        this.initialized = true;
        console.log('✓ Map initialized');
    }

    /**
     * Add or update device marker
     */
    addMarker(nodeId, lat, lon, info = {}) {
        if (!this.map) {
            console.warn('Map not initialized');
            return;
        }

        const position = [lat, lon];
        
        // Remove existing marker if present
        if (this.markers[nodeId]) {
            this.map.removeLayer(this.markers[nodeId]);
        }

        // Create custom icon based on device type
        const icon = L.divIcon({
            className: 'custom-marker',
            html: '<div class="marker-pin">📍</div>',
            iconSize: [30, 42],
            iconAnchor: [15, 42]
        });

        // Create marker with popup
        const marker = L.marker(position, { icon })
            .bindPopup(this.createPopupContent(nodeId, info))
            .addTo(this.map);

        this.markers[nodeId] = marker;

        // Update trail if exists
        if (this.trails[nodeId]) {
            this.trails[nodeId].positions.push(position);
            this.map.removeLayer(this.trails[nodeId].polyline);
        } else {
            this.trails[nodeId] = {
                positions: [position],
                polyline: null
            };
        }

        // Draw trail
        if (this.trails[nodeId].positions.length > 1) {
            this.trails[nodeId].polyline = L.polyline(
                this.trails[nodeId].positions,
                { color: '#4a90e2', weight: 2 }
            ).addTo(this.map);
        }

        return marker;
    }

    /**
     * Create popup content for marker
     */
    createPopupContent(nodeId, info) {
        return `
            <div class="marker-popup">
                <h3>Device ${nodeId}</h3>
                ${info.altitude ? `<p>Altitude: ${info.altitude}m</p>` : ''}
                ${info.timestamp ? `<p>Time: ${new Date(info.timestamp).toLocaleString()}</p>` : ''}
                ${info.rssi ? `<p>RSSI: ${info.rssi} dBm</p>` : ''}
            </div>
        `;
    }

    /**
     * Load positions from API and update map
     */
    async loadPositions() {
        try {
            const response = await apiClient.getPositions();
            
            if (response.status === 'success' && response.positions) {
                if (response.positions.length === 0) {
                    console.log('No GPS positions available yet');
                    return;
                }

                // Clear existing markers
                this.clearMarkers();

                // Add markers for each position
                response.positions.forEach(pos => {
                    this.addMarker(pos.nodeId, pos.lat, pos.lon, {
                        altitude: pos.alt,
                        timestamp: pos.timestamp
                    });
                });

                // Fit map to show all markers
                const bounds = L.latLngBounds(
                    response.positions.map(p => [p.lat, p.lon])
                );
                this.map.fitBounds(bounds, { padding: [50, 50] });

                console.log(`✓ Loaded ${response.positions.length} positions`);
            }
        } catch (error) {
            console.error('Failed to load positions:', error);
        }
    }

    /**
     * Clear all markers from map
     */
    clearMarkers() {
        Object.values(this.markers).forEach(marker => {
            this.map.removeLayer(marker);
        });
        this.markers = {};

        Object.values(this.trails).forEach(trail => {
            if (trail.polyline) {
                this.map.removeLayer(trail.polyline);
            }
        });
        this.trails = {};
    }

    /**
     * Center map on specific position
     */
    centerOn(lat, lon, zoom = 15) {
        if (this.map) {
            this.map.setView([lat, lon], zoom);
        }
    }
}

// Export global instance
window.mapHandler = new MapHandler();
