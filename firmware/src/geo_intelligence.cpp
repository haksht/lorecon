/**
 * Geographic Intelligence Module Implementation
 */

#include "geo_intelligence.h"
#include <ArduinoJson.h>

// Global instance
GeoIntelligence geoIntel;

GeoIntelligence::GeoIntelligence() : numPoints(0) {
    memset(points, 0, sizeof(points));
}

bool GeoIntelligence::extractPosition(const uint8_t* data, size_t length, uint32_t nodeId) {
    // Meshtastic position packet structure:
    // [0-3]: 0xFF 0xFF 0xFF 0xFF (header)
    // [4-7]: Node ID
    // [8]: Packet type (0x03 for position)
    // [9+]: Protobuf payload
    
    if (length < 12) return false;
    
    // Check for position packet (type 0x03 or 0x04)
    if (data[8] != 0x03 && data[8] != 0x04) return false;
    
    GeoPoint point;
    point.nodeId = nodeId;
    point.timestamp = millis();
    
    // Parse protobuf position data starting at offset 9
    if (parseProtobufPosition(data + 9, length - 9, point)) {
        // Store point (replace oldest if full)
        if (numPoints < MAX_GEO_POINTS) {
            points[numPoints++] = point;
        } else {
            // Replace oldest entry
            memmove(points, points + 1, sizeof(GeoPoint) * (MAX_GEO_POINTS - 1));
            points[MAX_GEO_POINTS - 1] = point;
        }
        
        Serial.println("\n📍 GPS POSITION EXTRACTED!");
        Serial.printf("   Node: 0x%08X\n", nodeId);
        Serial.printf("   Lat:  %f° %s\n", abs(point.latitude), point.latitude >= 0 ? "N" : "S");
        Serial.printf("   Lon:  %f° %s\n", abs(point.longitude), point.longitude >= 0 ? "E" : "W");
        Serial.printf("   Alt:  %.1f m\n", point.altitude);
        Serial.printf("   Precision: %d\n\n", point.precision);
        
        return true;
    }
    
    return false;
}

bool GeoIntelligence::parseProtobufPosition(const uint8_t* payload, size_t length, GeoPoint& point) {
    // Simplified protobuf parser for Meshtastic Position message
    // Format: field_tag (varint) + field_data
    // Position fields: latitude_i (1), longitude_i (2), altitude (3), precision (4)
    
    size_t offset = 0;
    int32_t latitudeRaw = 0, longitudeRaw = 0;
    bool hasLat = false, hasLon = false;
    
    while (offset < length - 1) {
        // Read field tag
        uint8_t tag = payload[offset++];
        uint8_t fieldNumber = tag >> 3;
        uint8_t wireType = tag & 0x07;
        
        if (wireType == 0) {  // Varint
            size_t bytesRead = 0;
            int32_t value = decodeVarint(payload + offset, length - offset, bytesRead);
            offset += bytesRead;
            
            switch (fieldNumber) {
                case 1:  // latitude_i
                    latitudeRaw = value;
                    hasLat = true;
                    break;
                case 2:  // longitude_i
                    longitudeRaw = value;
                    hasLon = true;
                    break;
                case 3:  // altitude (in meters)
                    point.altitude = (float)value;
                    break;
                case 4:  // precision (in bits)
                    point.precision = (int8_t)value;
                    break;
            }
        } else if (wireType == 2) {  // Length-delimited (skip)
            size_t bytesRead = 0;
            int32_t fieldLen = decodeVarint(payload + offset, length - offset, bytesRead);
            offset += bytesRead + fieldLen;
        } else {
            // Unsupported wire type, skip
            break;
        }
    }
    
    if (hasLat && hasLon) {
        point.latitude = convertCoordinate(latitudeRaw);
        point.longitude = convertCoordinate(longitudeRaw);
        point.valid = true;
        return true;
    }
    
    return false;
}

int32_t GeoIntelligence::decodeVarint(const uint8_t* data, size_t maxLen, size_t& bytesRead) {
    int32_t result = 0;
    int shift = 0;
    bytesRead = 0;
    
    for (size_t i = 0; i < maxLen && i < 5; i++) {
        uint8_t byte = data[i];
        result |= (int32_t)(byte & 0x7F) << shift;
        bytesRead++;
        
        if (!(byte & 0x80)) {
            // Handle zigzag encoding for signed integers
            if (result & 1) {
                result = -(result >> 1) - 1;
            } else {
                result = result >> 1;
            }
            return result;
        }
        shift += 7;
    }
    
    return result;
}

float GeoIntelligence::convertCoordinate(int32_t raw) const {
    // Meshtastic uses 1e-7 degrees encoding
    return (float)raw * 1e-7f;
}

const GeoPoint& GeoIntelligence::getPoint(uint8_t index) const {
    static GeoPoint emptyPoint;
    if (index >= numPoints) return emptyPoint;
    return points[index];
}

const GeoPoint* GeoIntelligence::findNodePosition(uint32_t nodeId) const {
    // Find most recent position for this node
    for (int i = numPoints - 1; i >= 0; i--) {
        if (points[i].nodeId == nodeId) {
            return &points[i];
        }
    }
    return nullptr;
}

void GeoIntelligence::exportKML(String& output) const {
    output = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    output += "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n";
    output += "  <Document>\n";
    output += "    <name>LoRa Device Positions</name>\n";
    
    for (uint8_t i = 0; i < numPoints; i++) {
        const GeoPoint& p = points[i];
        if (!p.valid) continue;
        
        output += "    <Placemark>\n";
        output += "      <name>Node 0x" + String(p.nodeId, HEX) + "</name>\n";
        output += "      <description>Alt: " + String(p.altitude, 1) + "m, Time: " + String(p.timestamp) + "</description>\n";
        output += "      <Point>\n";
        output += "        <coordinates>" + String(p.longitude, 7) + "," + String(p.latitude, 7) + "," + String(p.altitude, 1) + "</coordinates>\n";
        output += "      </Point>\n";
        output += "    </Placemark>\n";
    }
    
    output += "  </Document>\n";
    output += "</kml>\n";
}

void GeoIntelligence::exportGeoJSON(String& output) const {
    JsonDocument doc;
    doc["type"] = "FeatureCollection";
    JsonArray features = doc["features"].to<JsonArray>();
    
    for (uint8_t i = 0; i < numPoints; i++) {
        const GeoPoint& p = points[i];
        if (!p.valid) continue;
        
        JsonObject feature = features.add<JsonObject>();
        feature["type"] = "Feature";
        
        JsonObject properties = feature["properties"].to<JsonObject>();
        properties["nodeId"] = String("0x") + String(p.nodeId, HEX);
        properties["altitude"] = p.altitude;
        properties["timestamp"] = p.timestamp;
        properties["precision"] = p.precision;
        
        JsonObject geometry = feature["geometry"].to<JsonObject>();
        geometry["type"] = "Point";
        JsonArray coordinates = geometry["coordinates"].to<JsonArray>();
        coordinates.add(p.longitude);
        coordinates.add(p.latitude);
        coordinates.add(p.altitude);
    }
    
    serializeJsonPretty(doc, output);
}

void GeoIntelligence::printSummary() const {
    Serial.println("\n📍 GEOGRAPHIC INTELLIGENCE SUMMARY");
    Serial.println("===================================");
    Serial.printf("Total positions: %d\n", numPoints);
    
    if (numPoints == 0) {
        Serial.println("No GPS positions captured yet.\n");
        return;
    }
    
    Serial.println("\nNode ID       | Latitude    | Longitude   | Altitude | Age");
    Serial.println("--------------|-------------|-------------|----------|-------");
    
    for (uint8_t i = 0; i < numPoints; i++) {
        const GeoPoint& p = points[i];
        if (!p.valid) continue;
        
        uint32_t ageSeconds = (millis() - p.timestamp) / 1000;
        
        Serial.printf("0x%08X | %10.6f° | %10.6f° | %7.1fm | %5us\n",
                      p.nodeId,
                      p.latitude,
                      p.longitude,
                      p.altitude,
                      (unsigned int)ageSeconds);
    }
    
    Serial.println();
}

void GeoIntelligence::clear() {
    memset(points, 0, sizeof(points));
    numPoints = 0;
    Serial.println("[GEO] Intelligence cleared");
}
