# Deep Dive: Geographic Intelligence & Python Tools

## Executive Summary

This document covers two advanced features of your ESP32 LoRa sniffer system:

1. **Geographic Intelligence Module** - GPS extraction from Meshtastic packets, coordinate storage, KML/GeoJSON export
2. **Python Analysis Tools** - Post-capture log analysis and real-time visualization

You'll understand how GPS coordinates are extracted from Protobuf position packets, stored efficiently, exported for mapping applications, and analyzed using Python tools for insights and visualization.

---

## Table of Contents

### Part 1: Geographic Intelligence
1. [GPS Data in Meshtastic](#gps-data-in-meshtastic)
2. [Coordinate Extraction](#coordinate-extraction)
3. [Data Storage](#data-storage)
4. [Export Formats](#export-formats)

### Part 2: Python Tools
5. [Log Analysis Tool](#log-analysis-tool)
6. [Live Visualizer](#live-visualizer)
7. [Tool Integration](#tool-integration)

---

# PART 1: GEOGRAPHIC INTELLIGENCE

## GPS Data in Meshtastic

### Position Packet Structure

**Meshtastic Position Message (Protobuf):**

```protobuf
message Position {
    int32 latitude_i = 1;      // Latitude in 1e-7 degrees (fixed7)
    int32 longitude_i = 2;     // Longitude in 1e-7 degrees (fixed7)
    int32 altitude = 3;        // Altitude in meters (MSL)
    int32 time = 4;            // Timestamp (seconds since 1970)
    int32 precision_bits = 5;  // Precision bits (quality indicator)
}
```

**Example Position Packet:**

```
Latitude: 37.7749° N (San Francisco)
Longitude: -122.4194° W

Encoded in Protobuf:
  latitude_i  = 377749000  (37.7749 × 10^7)
  longitude_i = -1224194000 (-122.4194 × 10^7)
  altitude    = 15  (15 meters above sea level)
  precision   = 32  (32 bits precision ~1.8cm accuracy)

Wire format (hex):
  08 D8 FA E1 8B 02     Field 1: latitude_i (varint zigzag)
  10 B0 97 A1 C4 FC FF FF FF FF 01  Field 2: longitude_i (varint zigzag)
  18 0F                 Field 3: altitude = 15
  28 20                 Field 5: precision = 32 bits
```

### Why Integer Coordinates?

```
Floating-point GPS coordinates:
  ├─ Latitude: 37.7749° (8 bytes as double)
  ├─ Longitude: -122.4194° (8 bytes as double)
  └─ Total: 16 bytes

Integer coordinates (Meshtastic):
  ├─ Latitude: 377749000 (4 bytes as int32)
  ├─ Longitude: -1224194000 (4 bytes as int32)
  └─ Total: 8 bytes (50% smaller!)

Precision comparison:
  1e-7 degrees = 0.0000001°
  At equator: ~11 millimeters (1.1 cm)
  GPS accuracy: Typically 3-5 meters
  
  Conclusion: Integer format has MORE than enough precision

Benefits:
  ✅ 50% size reduction (critical for LoRa bandwidth)
  ✅ No floating-point rounding errors
  ✅ Faster integer math on embedded systems
  ✅ Protobuf varint encoding (often <8 bytes)
```

---

## Coordinate Extraction

### Parsing Position Packets

**GeoIntelligence Implementation:**

```cpp
bool GeoIntelligence::extractPosition(const uint8_t* data, size_t length, 
                                      uint32_t nodeId) {
    // Meshtastic position packet structure:
    // [0-3]: 0xFF 0xFF 0xFF 0xFF (header)
    // [4-7]: Node ID
    // [8]: Packet type (0x03 for position, 0x04 for compressed)
    // [9+]: Protobuf payload
    
    if (length < 12) return false;
    
    // Check for position packet type
    if (data[8] != 0x03 && data[8] != 0x04) return false;
    
    GeoPoint point;
    point.nodeId = nodeId;
    point.timestamp = millis();
    
    // Parse protobuf position data starting at offset 9
    if (parseProtobufPosition(data + 9, length - 9, point)) {
        // Store point (handle storage logic)
        storePoint(point);
        
        // Print extracted coordinates
        Serial.println("\n📍 GPS POSITION EXTRACTED!");
        Serial.printf("   Node: 0x%08X\n", nodeId);
        Serial.printf("   Lat:  %f° %s\n", 
                     abs(point.latitude), 
                     point.latitude >= 0 ? "N" : "S");
        Serial.printf("   Lon:  %f° %s\n", 
                     abs(point.longitude), 
                     point.longitude >= 0 ? "E" : "W");
        Serial.printf("   Alt:  %.1f m\n", point.altitude);
        Serial.printf("   Precision: %d\n\n", point.precision);
        
        return true;
    }
    
    return false;
}
```

**Protobuf Position Parsing:**

```cpp
bool GeoIntelligence::parseProtobufPosition(const uint8_t* payload, 
                                            size_t length, 
                                            GeoPoint& point) {
    size_t offset = 0;
    int32_t latitudeRaw = 0, longitudeRaw = 0;
    bool hasLat = false, hasLon = false;
    
    while (offset < length - 1) {
        // Read field tag
        uint8_t tag = payload[offset++];
        uint8_t fieldNumber = tag >> 3;
        uint8_t wireType = tag & 0x07;
        
        if (wireType == 0) {  // Varint encoding
            size_t bytesRead = 0;
            int32_t value = decodeVarint(payload + offset, 
                                        length - offset, 
                                        bytesRead);
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
                case 3:  // altitude (meters)
                    point.altitude = (float)value;
                    break;
                case 4:  // time (seconds since 1970)
                    // Could store timestamp if needed
                    break;
                case 5:  // precision_bits
                    point.precision = (int8_t)value;
                    break;
            }
        } else if (wireType == 2) {  // Length-delimited (skip)
            size_t bytesRead = 0;
            int32_t fieldLen = decodeVarint(payload + offset, 
                                          length - offset, 
                                          bytesRead);
            offset += bytesRead + fieldLen;
        } else {
            // Unsupported wire type, abort
            break;
        }
    }
    
    // Validate we got both latitude and longitude
    if (hasLat && hasLon) {
        point.latitude = convertCoordinate(latitudeRaw);
        point.longitude = convertCoordinate(longitudeRaw);
        point.valid = true;
        return true;
    }
    
    return false;
}
```

**Varint Decoding (with Zigzag):**

```cpp
int32_t GeoIntelligence::decodeVarint(const uint8_t* data, size_t maxLen, 
                                      size_t& bytesRead) {
    int32_t result = 0;
    int shift = 0;
    bytesRead = 0;
    
    for (size_t i = 0; i < maxLen && i < 5; i++) {
        uint8_t byte = data[i];
        result |= (int32_t)(byte & 0x7F) << shift;
        bytesRead++;
        
        // Check continuation bit
        if (!(byte & 0x80)) {
            // Decode zigzag encoding for signed integers
            // Zigzag: 0 = 0, 1 = -1, 2 = 1, 3 = -2, 4 = 2, ...
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
```

**Why Zigzag Encoding?**

```
Problem: Varint encodes unsigned efficiently but signed poorly

Negative number -1 as int32:
  Binary: 1111 1111 1111 1111 1111 1111 1111 1111
  Varint: 10 bytes! (all continuation bits set)

Solution: Zigzag encoding maps signed to unsigned:
  0 →  0
  -1 →  1
  1 →  2
  -2 →  3
  2 →  4
  
Zigzag formula:
  Encode: (n << 1) ^ (n >> 31)
  Decode: (n >> 1) ^ -(n & 1)

Result:
  -1 → 1 (varint: 1 byte)
  -122 → 243 (varint: 2 bytes)
  
GPS coordinates benefit:
  Longitude -122.4194° → -1224194000 (int32)
  Zigzag: 2448387999
  Varint: 5 bytes (instead of 10!)
```

**Coordinate Conversion:**

```cpp
float GeoIntelligence::convertCoordinate(int32_t raw) const {
    // Meshtastic uses 1e-7 degrees encoding
    return (float)raw * 1e-7f;
}

// Example:
//   raw = 377749000
//   result = 377749000 × 0.0000001
//          = 37.7749°
```

---

## Data Storage

### GeoPoint Structure

```cpp
struct GeoPoint {
    uint32_t nodeId;        // Device node ID (4 bytes)
    float latitude;         // Latitude in degrees (4 bytes)
    float longitude;        // Longitude in degrees (4 bytes)
    float altitude;         // Altitude in meters (4 bytes)
    uint32_t timestamp;     // millis() when captured (4 bytes)
    int8_t precision;       // Precision bits (1 byte)
    bool valid;            // Valid flag (1 byte)
    // Total: 22 bytes per point
};

#define MAX_GEO_POINTS 50  // Maximum stored positions

class GeoIntelligence {
private:
    GeoPoint points[MAX_GEO_POINTS];
    uint8_t numPoints;
    // ...
};
```

**Memory Usage:**

```
Storage calculation:
  Size per point: 22 bytes
  Maximum points: 50
  Total storage: 1100 bytes (~1.1 KB)

ESP32-S3 available RAM: 320 KB
Geographic storage: 0.34% of RAM
Very lightweight!

Why 50 points?
  ✅ Enough for typical reconnaissance session
  ✅ Tracks multiple devices over time
  ✅ Minimal memory footprint
  ❌ No SD card logging (future enhancement)
```

### Storage Strategy

**Ring Buffer (Oldest Replaced First):**

```cpp
void GeoIntelligence::storePoint(const GeoPoint& point) {
    if (numPoints < MAX_GEO_POINTS) {
        // Array not full, add to end
        points[numPoints++] = point;
    } else {
        // Array full, replace oldest (FIFO)
        // Shift all elements left by one
        memmove(points, points + 1, sizeof(GeoPoint) * (MAX_GEO_POINTS - 1));
        // Add new point at end
        points[MAX_GEO_POINTS - 1] = point;
    }
}
```

**Diagram:**

```
Initial state (empty):
  [0] [1] [2] ... [48] [49]
  
After 3 positions captured:
  [A] [B] [C] [ ] [ ] ... [ ]
   ↑           ↑
  oldest    newest

After 50 positions (array full):
  [A] [B] [C] [D] ... [Z]
   ↑                   ↑
  oldest            newest

51st position captured:
  [B] [C] [D] [E] ... [Z] [AA]
   ↑                       ↑
  oldest (A removed)    newest

Result: Always keeps most recent 50 positions
```

### Querying Positions

**Find Latest Position for Node:**

```cpp
const GeoPoint* GeoIntelligence::findNodePosition(uint32_t nodeId) const {
    // Search backwards (most recent first)
    for (int i = numPoints - 1; i >= 0; i--) {
        if (points[i].nodeId == nodeId) {
            return &points[i];
        }
    }
    return nullptr;  // Node not found
}

// Usage:
const GeoPoint* pos = geoIntel.findNodePosition(0x401ACD4E);
if (pos) {
    Serial.printf("Last known position: %.6f, %.6f\n", 
                 pos->latitude, pos->longitude);
} else {
    Serial.println("No position data for this node");
}
```

**Print Summary:**

```cpp
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
```

**Example Output:**

```
📍 GEOGRAPHIC INTELLIGENCE SUMMARY
===================================
Total positions: 8

Node ID       | Latitude    | Longitude   | Altitude | Age
--------------|-------------|-------------|----------|-------
0x401ACD4E |  37.774900° | -122.419400° |    15.0m |    45s
0x8B3C5A12 |  37.780100° | -122.425300° |    22.0m |    67s
0x401ACD4E |  37.775200° | -122.419800° |    16.0m |    12s
0xC4D8E619 |  37.769800° | -122.413500° |   189.0m |   134s
0x8B3C5A12 |  37.780500° | -122.425700° |    23.0m |    28s
0x401ACD4E |  37.775600° | -122.420100° |    17.0m |     3s
0xF7A2B8D3 |  37.771200° | -122.416400° |    45.0m |    89s
0x401ACD4E |  37.775900° | -122.420400° |    18.0m |     1s
```

---

## Export Formats

### KML Export (Google Earth)

**KML Structure:**

```xml
<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://www.opengis.net/kml/2.2">
  <Document>
    <name>LoRa Device Positions</name>
    
    <Placemark>
      <name>Node 0x401ACD4E</name>
      <description>Alt: 15.0m, Time: 45123ms</description>
      <Point>
        <coordinates>-122.4194000,37.7749000,15.0</coordinates>
      </Point>
    </Placemark>
    
    <Placemark>
      <name>Node 0x8B3C5A12</name>
      <description>Alt: 22.0m, Time: 67890ms</description>
      <Point>
        <coordinates>-122.4253000,37.7801000,22.0</coordinates>
      </Point>
    </Placemark>
    
  </Document>
</kml>
```

**Implementation:**

```cpp
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
        output += "      <description>Alt: " + String(p.altitude, 1) + 
                  "m, Time: " + String(p.timestamp) + "</description>\n";
        output += "      <Point>\n";
        output += "        <coordinates>" + String(p.longitude, 7) + "," + 
                  String(p.latitude, 7) + "," + String(p.altitude, 1) + 
                  "</coordinates>\n";
        output += "      </Point>\n";
        output += "    </Placemark>\n";
    }
    
    output += "  </Document>\n";
    output += "</kml>\n";
}

// Usage:
String kml;
geoIntel.exportKML(kml);
Serial.println(kml);  // Or save to SD card
```

**Using KML:**

```
1. Export KML from ESP32
2. Save to file: device_positions.kml
3. Open in Google Earth:
   - Drag and drop file into Google Earth
   - All positions appear as placemarks
   - Click placemark for details

4. Visualize:
   - Zoom to location
   - See device paths (if multiple positions)
   - Altitude shown in 3D view
```

### GeoJSON Export (Web Mapping)

**GeoJSON Structure:**

```json
{
  "type": "FeatureCollection",
  "features": [
    {
      "type": "Feature",
      "properties": {
        "nodeId": "0x401ACD4E",
        "altitude": 15.0,
        "timestamp": 45123,
        "precision": 32
      },
      "geometry": {
        "type": "Point",
        "coordinates": [-122.4194000, 37.7749000, 15.0]
      }
    },
    {
      "type": "Feature",
      "properties": {
        "nodeId": "0x8B3C5A12",
        "altitude": 22.0,
        "timestamp": 67890,
        "precision": 32
      },
      "geometry": {
        "type": "Point",
        "coordinates": [-122.4253000, 37.7801000, 22.0]
      }
    }
  ]
}
```

**Implementation:**

```cpp
void GeoIntelligence::exportGeoJSON(String& output) const {
    // Using ArduinoJson library
    JsonDocument doc;
    doc["type"] = "FeatureCollection";
    JsonArray features = doc["features"].to<JsonArray>();
    
    for (uint8_t i = 0; i < numPoints; i++) {
        const GeoPoint& p = points[i];
        if (!p.valid) continue;
        
        // Create feature object
        JsonObject feature = features.add<JsonObject>();
        feature["type"] = "Feature";
        
        // Properties
        JsonObject properties = feature["properties"].to<JsonObject>();
        properties["nodeId"] = String("0x") + String(p.nodeId, HEX);
        properties["altitude"] = p.altitude;
        properties["timestamp"] = p.timestamp;
        properties["precision"] = p.precision;
        
        // Geometry
        JsonObject geometry = feature["geometry"].to<JsonObject>();
        geometry["type"] = "Point";
        JsonArray coordinates = geometry["coordinates"].to<JsonArray>();
        coordinates.add(p.longitude);  // Note: Lon first in GeoJSON!
        coordinates.add(p.latitude);
        coordinates.add(p.altitude);
    }
    
    // Serialize to string
    serializeJsonPretty(doc, output);
}

// Usage:
String geojson;
geoIntel.exportGeoJSON(geojson);
Serial.println(geojson);  // Or save to SD card / send via WiFi
```

**Using GeoJSON:**

```javascript
// In web application:
fetch('device_positions.geojson')
  .then(response => response.json())
  .then(data => {
    // Add to Leaflet map
    L.geoJSON(data, {
      pointToLayer: function(feature, latlng) {
        return L.marker(latlng).bindPopup(
          `Node: ${feature.properties.nodeId}<br>` +
          `Alt: ${feature.properties.altitude}m`
        );
      }
    }).addTo(map);
  });
```

---

# PART 2: PYTHON TOOLS

## Log Analysis Tool

### Overview

**Purpose:** Analyze captured packet logs for statistical insights

```bash
# Analyze JSONL log file
python tools/analyze_logs.py packets.jsonl

# Export to CSV
python tools/analyze_logs.py packets.jsonl --format csv

# Generate plots
python tools/analyze_logs.py packets.jsonl --plot
```

### Log Format (JSONL)

**Example Log Entries:**

```json
{"timestamp":1000,"protocol":"Meshtastic","frequency":906.875,"rssi":-75.2,"snr":8.5,"length":45,"data":"01020304..."}
{"timestamp":1523,"protocol":"Meshtastic","frequency":908.000,"rssi":-82.1,"snr":6.2,"length":67,"data":"08091011..."}
{"timestamp":2045,"protocol":"Meshtastic","frequency":915.000,"rssi":-68.9,"snr":10.3,"length":34,"data":"12131415..."}
```

**Fields:**

```
timestamp:  Milliseconds since device boot
protocol:   Detected protocol (Meshtastic, LoRaWAN, etc.)
frequency:  Frequency in MHz (906.875, 915.0, etc.)
rssi:       Signal strength in dBm (-127 to -30 typical)
snr:        Signal-to-Noise Ratio in dB (0-15 typical)
length:     Packet length in bytes (5-255)
data:       Hex-encoded packet data
```

### Analysis Functions

**1. Protocol Distribution:**

```python
def analyze_protocols(packets):
    protocols = Counter(p['protocol'] for p in packets)
    
    print("=== Protocol Distribution ===")
    for protocol, count in protocols.most_common():
        percentage = (count / len(packets)) * 100
        print(f"{protocol:12}: {count:4} packets ({percentage:5.1f}%)")
    
    return protocols

# Example output:
# === Protocol Distribution ===
# Meshtastic  :  247 packets ( 89.5%)
# LoRaWAN     :   23 packets (  8.3%)
# Unknown     :    6 packets (  2.2%)
```

**2. Frequency Distribution:**

```python
def analyze_frequencies(packets):
    frequencies = Counter(p['frequency'] for p in packets)
    
    print("=== Frequency Distribution ===")
    for freq, count in sorted(frequencies.items()):
        percentage = (count / len(packets)) * 100
        print(f"{freq:8.3f} MHz: {count:4} packets ({percentage:5.1f}%)")
    
    return frequencies

# Example output:
# === Frequency Distribution ===
# 906.875 MHz:   89 packets ( 32.2%)
# 908.000 MHz:   71 packets ( 25.7%)
# 915.000 MHz:   45 packets ( 16.3%)
# 925.000 MHz:   42 packets ( 15.2%)
# Others...
```

**3. Signal Quality Analysis:**

```python
def analyze_signal_quality(packets):
    rssi_values = [p['rssi'] for p in packets if 'rssi' in p]
    snr_values = [p['snr'] for p in packets if 'snr' in p]
    
    print("=== Signal Quality Analysis ===")
    if rssi_values:
        print(f"RSSI: min={min(rssi_values):6.1f} dBm, "
              f"max={max(rssi_values):6.1f} dBm, "
              f"avg={sum(rssi_values)/len(rssi_values):6.1f} dBm")
    
    if snr_values:
        print(f"SNR:  min={min(snr_values):6.1f} dB, "
              f"max={max(snr_values):6.1f} dB, "
              f"avg={sum(snr_values)/len(snr_values):6.1f} dB")

# Example output:
# === Signal Quality Analysis ===
# RSSI: min= -102.3 dBm, max=  -45.7 dBm, avg=  -78.4 dBm
# SNR:  min=   2.1 dB, max=  12.8 dB, avg=   7.5 dB
```

**4. Temporal Analysis:**

```python
def analyze_temporal_patterns(packets):
    first_ts = min(p['timestamp'] for p in packets)
    rel_times = [(p['timestamp'] - first_ts) / 1000.0 for p in packets]
    
    print("=== Temporal Analysis ===")
    print(f"Capture duration: {max(rel_times):.1f} seconds")
    print(f"Total packets: {len(packets)}")
    print(f"Average rate: {len(packets) / max(rel_times):.2f} packets/sec")

# Example output:
# === Temporal Analysis ===
# Capture duration: 1847.3 seconds
# Total packets: 247
# Average rate: 0.13 packets/sec
```

**5. Node Extraction:**

```python
def extract_node_info(packets):
    nodes = defaultdict(list)
    
    for packet in packets:
        if packet['protocol'] == 'Meshtastic' and 'data' in packet:
            data = packet['data']
            if len(data) >= 16:
                # Extract node ID from bytes 4-7
                node_hex = data[8:16]
                if len(node_hex) == 8:
                    node_id = int(node_hex, 16)
                    nodes[node_id].append({
                        'timestamp': packet['timestamp'],
                        'rssi': packet.get('rssi', 0),
                        'frequency': packet['frequency']
                    })
    
    print("=== Node Analysis ===")
    print(f"Unique nodes detected: {len(nodes)}")
    
    for node_id, packets_list in list(nodes.items())[:10]:
        avg_rssi = sum(p['rssi'] for p in packets_list) / len(packets_list)
        frequencies = set(p['frequency'] for p in packets_list)
        print(f"Node 0x{node_id:08X}: {len(packets_list):3} packets, "
              f"avg RSSI {avg_rssi:6.1f} dBm")

# Example output:
# === Node Analysis ===
# Unique nodes detected: 12
# Node 0x401ACD4E:  45 packets, avg RSSI  -75.3 dBm
# Node 0x8B3C5A12:  38 packets, avg RSSI  -82.7 dBm
# ...
```

### Visualization (--plot)

**4-Panel Analysis Plot:**

```python
def plot_analysis(packets):
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(12, 8))
    
    # Panel 1: Protocol Distribution (Pie Chart)
    protocols = Counter(p['protocol'] for p in packets)
    ax1.pie(protocols.values(), labels=protocols.keys(), autopct='%1.1f%%')
    ax1.set_title('Protocol Distribution')
    
    # Panel 2: RSSI Histogram
    rssi_values = [p['rssi'] for p in packets if 'rssi' in p]
    ax2.hist(rssi_values, bins=20, alpha=0.7)
    ax2.set_xlabel('RSSI (dBm)')
    ax2.set_ylabel('Count')
    ax2.set_title('Signal Strength Distribution')
    
    # Panel 3: Frequency Usage (Histogram)
    frequencies = [p['frequency'] for p in packets]
    ax3.hist(frequencies, bins=15, alpha=0.7)
    ax3.set_xlabel('Frequency (MHz)')
    ax3.set_ylabel('Count')
    ax3.set_title('Frequency Usage')
    
    # Panel 4: RSSI Over Time (Scatter Plot)
    first_ts = min(p['timestamp'] for p in packets)
    rel_times = [(p['timestamp'] - first_ts) / 1000.0 for p in packets]
    ax4.scatter(rel_times, rssi_values[:len(rel_times)], alpha=0.6)
    ax4.set_xlabel('Time (seconds)')
    ax4.set_ylabel('RSSI (dBm)')
    ax4.set_title('Signal Strength Over Time')
    
    plt.tight_layout()
    plt.savefig('lora_analysis.png', dpi=150)
    plt.show()
```

---

## Live Visualizer

### Overview

**Purpose:** Real-time visualization of reconnaissance data via serial port

```bash
# Windows
python tools/live_visualizer.py COM3

# Linux/Mac
python tools/live_visualizer.py /dev/ttyUSB0

# Auto-detect port
python tools/live_visualizer.py
```

### Serial Data Parsing

**Patterns Recognized:**

```python
def parse_serial_line(self, line):
    # Pattern 1: Packet with RSSI
    # Example: [PACKET] Meshtastic, 14 bytes, -45.0 dBm, 8.2 dB SNR
    packet_match = re.search(
        r'\[(PACKET|RECON|CAPTURE)\].*?(-?\d+\.\d+)\s*dBm',
        line
    )
    
    if packet_match:
        rssi = float(packet_match.group(2))
        
        # Extract node ID: 0x401ACD4E
        node_match = re.search(r'0x([0-9A-Fa-f]{8})', line)
        if node_match:
            node_id = node_match.group(1).upper()
            return {
                'node_id': node_id,
                'rssi': rssi,
                'timestamp': time.time()
            }
    
    return None
```

### 4-Panel Dashboard

**Panel 1: RSSI Over Time (Line Graph):**

```
📡 Signal Strength Over Time
────────────────────────────────
  -40 ┤                    ╭─●
  -50 ┤           ╭────●───╯
  -60 ┤      ╭────╯
  -70 ┤  ●───╯
  -80 ┤●─╯
  -90 ┤
      └──────────────────────────
      0s    20s   40s   60s   80s

Legend:
  ● Node 401ACD.. (Meshtastic)
  ● Node 8B3C5A.. (Meshtastic)
```

**Panel 2: Device List (Text Table):**

```
🎯 DISCOVERED DEVICES
════════════════════════════════

1. 0x401ACD4E
   Type: Meshtastic
   RSSI: ████████ -45.2 dBm
   Pkts: 45
   Age:  5s ago

2. 0x8B3C5A12
   Type: Meshtastic
   RSSI: █████░░░ -78.5 dBm
   Pkts: 38
   Age:  12s ago

3. 0xC4D8E619
   Type: Meshtastic
   RSSI: ███░░░░░ -95.3 dBm
   Pkts: 21
   Age:  45s ago
```

**Panel 3: Packet Histogram (Bar Chart):**

```
📊 Packet Capture Statistics
────────────────────────────
        45 ┤ █
        40 ┤ █
        35 ┤ █ █
        30 ┤ █ █
        25 ┤ █ █
        20 ┤ █ █ █
        15 ┤ █ █ █
        10 ┤ █ █ █ █
         5 ┤ █ █ █ █
         0 └─────────────
            A B C D ...
```

**Panel 4: Summary Stats (Text):**

```
📈 RECONNAISSANCE SUMMARY
════════════════════════════════

⏱️  Runtime:       1847 seconds
📦 Total Packets:  247
📡 Devices Found:  12
⚡ Packet Rate:    0.13 pkt/sec

🏆 TOP DEVICE:
   ID: 0x401ACD4E
   Type: Meshtastic
   Packets: 45
   RSSI: -75.3 dBm
```

### Real-Time Update Loop

```python
def update_plots(self, frame):
    # Read serial data (non-blocking)
    while self.ser and self.ser.in_waiting:
        line = self.ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
            packet_info = self.parse_serial_line(line)
            if packet_info:
                self.update_data(packet_info)
    
    # Clear and redraw all 4 panels
    for ax in self.axes.flat:
        ax.clear()
    
    # Redraw each panel with updated data
    self.draw_rssi_plot(self.axes[0, 0])
    self.draw_device_list(self.axes[0, 1])
    self.draw_histogram(self.axes[1, 0])
    self.draw_summary(self.axes[1, 1])
    
    plt.tight_layout()
```

---

## Tool Integration

### Complete Workflow

**1. Capture Session:**

```bash
# Flash ESP32
pio run --target upload

# Connect and capture for 30 minutes
# ESP32 outputs to serial (115200 baud)
# Logs automatically saved to packets.jsonl (if SD card present)
```

**2. Live Monitoring:**

```bash
# In separate terminal, start visualizer
python tools/live_visualizer.py COM3

# Watch real-time graphs update
# See devices appear as they're discovered
# Monitor signal strength trends
```

**3. Post-Capture Analysis:**

```bash
# Analyze complete log file
python tools/analyze_logs.py packets.jsonl

# Output: Statistical summary, protocol/freq distribution

# Generate plots
python tools/analyze_logs.py packets.jsonl --plot

# Output: lora_analysis.png with 4 visualization panels

# Export to CSV for external analysis
python tools/analyze_logs.py packets.jsonl --format csv

# Output: packets.csv (importable to Excel, R, etc.)
```

**4. Conference Demo:**

```
Setup:
  1. Connect ESP32 to laptop
  2. Project laptop screen to audience
  3. Run live_visualizer.py
  4. Start reconnaissance

Live demo shows:
  ✅ Real-time device discovery
  ✅ Signal strength trending
  ✅ Packet capture statistics
  ✅ Network topology emerging

Audience sees:
  - Immediate feedback as devices detected
  - Visual representation of RF environment
  - Live "hacking" demonstration (ethical research)
```

---

## Summary

### Geographic Intelligence

**GPS Extraction:**
- ✅ Parses Protobuf position packets
- ✅ Extracts latitude/longitude (1e-7 degrees precision)
- ✅ Handles zigzag encoding and varints
- ✅ Stores up to 50 positions (ring buffer)

**Export Formats:**
- ✅ KML for Google Earth visualization
- ✅ GeoJSON for web mapping applications
- ✅ Preserves altitude, timestamp, precision metadata

**Use Cases:**
- 📍 Track device movement over time
- 📍 Map network coverage area
- 📍 Identify device locations for field testing

### Python Tools

**analyze_logs.py:**
- ✅ Statistical analysis of packet logs
- ✅ Protocol/frequency/signal quality analysis
- ✅ Node extraction and tracking
- ✅ CSV export and matplotlib visualization

**live_visualizer.py:**
- ✅ Real-time serial port monitoring
- ✅ 4-panel dashboard (RSSI, devices, histogram, summary)
- ✅ Auto-detecting device discovery
- ✅ Perfect for conference demonstrations

**Integration:**
- ✅ Seamless workflow from capture to analysis
- ✅ Visual feedback enhances understanding
- ✅ Export formats for external tools (GIS, Excel, web maps)

---

**You now understand the complete data pipeline: Capture → Storage → Analysis → Visualization!**

---

*Generated for ESP32-Sniffer deep-dive session*  
*Last updated: 2025-10-16*
