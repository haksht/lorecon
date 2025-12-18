/**
 * Geographic Intelligence Module
 * 
 * Extracts and manages GPS coordinates from captured Meshtastic packets.
 * Provides KML/GeoJSON export for mapping tools.
 */

#ifndef GEO_INTELLIGENCE_H
#define GEO_INTELLIGENCE_H

#include <Arduino.h>
#include "data_structures.h"
#include "config.h"

class GeoIntelligence {
public:
    GeoIntelligence();
    
    // Extract GPS from Meshtastic position packet
    bool extractPosition(const uint8_t* data, size_t length, uint32_t nodeId);
    
    // Extract GPS from decrypted position payload (portnum + protobuf data)
    bool extractPositionFromDecrypted(const uint8_t* decrypted, size_t length, uint32_t nodeId);
    
    // Query functions
    uint8_t getPointCount() const { return numPoints; }
    const GeoPoint& getPoint(uint8_t index) const;
    const GeoPoint* findNodePosition(uint32_t nodeId) const;
    
    // Export functions
    void exportKML(String& output) const;
    void exportGeoJSON(String& output) const;
    void printSummary() const;
    
    // Management
    void clear();
    
private:
    GeoPoint points[Config::Tracking::MAX_GEO_POINTS];
    uint8_t numPoints;
    
    // Store/update point with deduplication
    bool storePoint(const GeoPoint& point);
    
    // Meshtastic protobuf parsing helpers
    bool parseProtobufPosition(const uint8_t* payload, size_t length, GeoPoint& point);
    int32_t decodeVarint(const uint8_t* data, size_t maxLen, size_t& bytesRead);
    
    // Coordinate conversion (Meshtastic uses integer lat/lon)
    float convertCoordinate(int32_t raw) const;
};

// Global instance
extern GeoIntelligence geoIntel;

#endif // GEO_INTELLIGENCE_H
