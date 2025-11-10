# PC Integration Architecture

**Version:** 1.0  
**Date:** November 10, 2025  
**Purpose:** Leverage PC capabilities for enhanced data analysis, visualization, and control

---

## Table of Contents

1. [Overview](#overview)
2. [Integration Modes](#integration-modes)
3. [Data Flow Architecture](#data-flow-architecture)
4. [SD Card Logging System](#sd-card-logging-system)
5. [PC Software Suite](#pc-software-suite)
6. [Real-Time Analysis](#real-time-analysis)
7. [Offline Analysis Tools](#offline-analysis-tools)
8. [Mapping & Visualization](#mapping--visualization)
9. [Database Architecture](#database-architecture)
10. [Implementation Guide](#implementation-guide)

---

## Overview

The ESP32 boards excel at real-time RF capture but are limited in:
- Storage capacity (onboard flash)
- Processing power (240 MHz dual-core)
- Visualization capabilities
- Long-term data analysis
- Machine learning inference

A PC integration strategy addresses these limitations while maintaining field portability:

```
┌─────────────────────────────────────────────────────────────┐
│                    FIELD OPERATION                          │
│  ┌──────────────┐                      ┌──────────────┐    │
│  │   Board 1    │ Logging to SD Card   │   Board 2    │    │
│  │   MONITOR    │◄────────────────────►│   ATTACK     │    │
│  │              │                      │              │    │
│  │  [SD Card]   │   Autonomous Mode    │  [SD Card]   │    │
│  └──────────────┘                      └──────────────┘    │
└─────────────────────────────────────────────────────────────┘
                         ▼ Transfer ▼
┌─────────────────────────────────────────────────────────────┐
│                    PC ANALYSIS HUB                          │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  • Import & Parse Logs                               │  │
│  │  • Database Storage (SQLite/PostgreSQL)              │  │
│  │  • Interactive Mapping (Google Earth, Leaflet)       │  │
│  │  • Signal Analysis & Statistics                      │  │
│  │  • Machine Learning (Device Fingerprinting)          │  │
│  │  • Report Generation (PDF, HTML)                     │  │
│  │  • Attack Pattern Recognition                        │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## Integration Modes

### Mode 1: Autonomous Field Operation + Offline Analysis (Recommended)

**Field:**
- Boards operate independently
- Log all data to SD card (JSON/CSV format)
- No PC required during capture
- Battery-powered, fully portable

**Lab:**
- Remove SD card or USB transfer
- Import logs into PC analysis suite
- Generate maps, reports, graphs
- Store in database for long-term analysis

**Benefits:**
- ✅ Maximum portability
- ✅ No PC dependency in field
- ✅ Extended battery life (no USB)
- ✅ Can review data days/weeks later
- ✅ Multiple sessions can be compared

**Use Cases:**
- Outdoor reconnaissance
- Mobile operations
- Long-duration monitoring
- Multi-day campaigns

### Mode 2: Real-Time USB Tethered Operation

**Setup:**
- Board 1 connected to PC via USB
- Board 2 optionally tethered or autonomous
- Live serial streaming to PC
- Instant visualization and analysis

**Benefits:**
- ✅ Real-time mapping and tracking
- ✅ Immediate attack feedback
- ✅ Live signal strength graphs
- ✅ Interactive device selection from PC
- ✅ No SD card capacity limits

**Use Cases:**
- Lab testing
- Fixed-position monitoring
- Training and demonstrations
- Vulnerability assessment with instant feedback

### Mode 3: WiFi Network Streaming (Advanced)

**Setup:**
- Boards connected to PC via WiFi
- Real-time data streaming (MQTT/WebSocket)
- Untethered but with live analysis
- PC acts as coordinator/controller

**Benefits:**
- ✅ Wireless operation with real-time data
- ✅ Multi-board coordination from PC
- ✅ Remote monitoring possible
- ✅ Cloud storage integration

**Use Cases:**
- Distributed sensor networks
- Remote monitoring stations
- Multi-operator coordination
- Research lab environments

### Mode 4: Hybrid Operation

**Best of all worlds:**
- Real-time streaming when USB connected
- Auto-fallback to SD logging when disconnected
- Sync SD logs to PC when reconnected
- Seamless transition between modes

---

## Data Flow Architecture

### Complete System Data Flow

```
┌─────────────────────────────────────────────────────────────┐
│                      ESP32 BOARDS                           │
│                                                              │
│  Radio RX → Packet Parser → Decryption → Classification     │
│                    ↓                                         │
│              ┌─────┴─────┐                                  │
│              │  Router    │                                  │
│              └─────┬─────┘                                  │
│                    ├──────────┬──────────┬─────────┐        │
│                    ▼          ▼          ▼         ▼        │
│                 OLED      SD Card    Serial    Memory       │
│              (Display)   (Log File)  (Stream)  (Replay)     │
└────────────────────┬──────────┬──────────┬──────────────────┘
                     │          │          │
                     │          │          │
         ┌───────────┘          │          └──────────┐
         ▼                      ▼                     ▼
   Field Display         USB Transfer          Real-Time Stream
   (Immediate)           (Offline)             (Live Analysis)
         │                      │                     │
         │                      ▼                     ▼
         │              ┌──────────────────────────────────────┐
         │              │         PC SOFTWARE SUITE            │
         │              │                                      │
         │              │  ┌──────────────────────────────┐   │
         │              │  │    Data Import Module        │   │
         │              │  │  • SD Card Reader            │   │
         │              │  │  • Serial Parser             │   │
         │              │  │  • File Format Converter     │   │
         │              │  └────────────┬─────────────────┘   │
         │              │               ▼                     │
         │              │  ┌──────────────────────────────┐   │
         │              │  │    Database Layer            │   │
         │              │  │  • SQLite (Embedded)         │   │
         │              │  │  • PostgreSQL (Server)       │   │
         │              │  │  • Time-series Optimization  │   │
         │              │  └────────────┬─────────────────┘   │
         │              │               ▼                     │
         │              │  ┌──────────────────────────────┐   │
         │              │  │    Analysis Engine           │   │
         │              │  │  • Signal Processing         │   │
         │              │  │  • Pattern Recognition       │   │
         │              │  │  • Statistics & Aggregation  │   │
         │              │  │  • ML Feature Extraction     │   │
         │              │  └────────────┬─────────────────┘   │
         │              │               ▼                     │
         └──────────────┼──►┌──────────────────────────────┐  │
                        │   │   Visualization Layer        │  │
                        │   │  • Interactive Maps          │  │
                        │   │  • Time-series Graphs        │  │
                        │   │  • Network Topology          │  │
                        │   │  • 3D RF Heatmaps            │  │
                        │   │  • Attack Timeline           │  │
                        │   └──────────────────────────────┘  │
                        │                                      │
                        │  ┌──────────────────────────────┐   │
                        │  │    Export & Reporting        │   │
                        │  │  • PDF Reports               │   │
                        │  │  • KML/GeoJSON (Maps)        │   │
                        │  │  • CSV/Excel (Data)          │   │
                        │  │  • HTML Dashboard            │   │
                        │  │  • Video Animations          │   │
                        │  └──────────────────────────────┘   │
                        └──────────────────────────────────────┘
```

---

## SD Card Logging System

### Hardware Requirements

**SD Card Specifications:**
- **Capacity:** 8-32 GB (more than sufficient)
- **Speed Class:** Class 10 / UHS-1 (10 MB/s minimum)
- **Format:** FAT32 (best compatibility)
- **Connector:** Micro SD card slot on each ESP32 board

**ESP32 Integration:**
- SPI interface to SD card module
- CS pin configurable (typically GPIO 5)
- 3.3V power supply
- Cost: ~$2-3 per module

### File Format Design

#### Log File Structure

```
SD Card Root:
/
├── config.json              # Board configuration
├── sessions/
│   ├── 2025-11-10_083045/   # Session folder (timestamp)
│   │   ├── session_info.json
│   │   ├── devices.jsonl     # Discovered devices (JSON Lines)
│   │   ├── packets.jsonl     # Captured packets
│   │   ├── attacks.jsonl     # Attack logs (Board 2 only)
│   │   ├── triangulation.jsonl  # Position data
│   │   └── raw_rf.bin        # Optional: Raw IQ samples
│   ├── 2025-11-10_143022/   # Second session
│   └── 2025-11-11_091500/
└── exports/
    ├── devices.kml
    └── coverage_map.geojson
```

#### JSON Lines Format (Recommended)

**Why JSON Lines:**
- One JSON object per line
- Easy to parse incrementally
- Append-only (SD card friendly)
- Robust to crashes (partial writes won't corrupt file)
- Simple to filter with grep/awk

**devices.jsonl Example:**
```json
{"timestamp":1699603245,"node_id":"0x9EA3D744","frequency":906.875,"rssi":-65,"sf":10,"bw":250,"device_type":"Meshtastic","firmware":"2.3.8","first_seen":1699603000,"last_seen":1699603245,"packet_count":42}
{"timestamp":1699603256,"node_id":"0x12345678","frequency":903.875,"rssi":-72,"sf":11,"bw":250,"device_type":"Meshtastic","firmware":"2.2.1","first_seen":1699603256,"last_seen":1699603256,"packet_count":1}
```

**packets.jsonl Example:**
```json
{"timestamp":1699603245,"session_id":"2025-11-10_083045","node_id":"0x9EA3D744","packet_id":"0x80B24533","frequency":906.875,"rssi":-65,"snr":8.5,"sf":10,"bw":250,"size":128,"port_num":"0x01","packet_type":"TEXT_MESSAGE_APP","encrypted":false,"decrypted":true,"psk_key":"key_2","payload_hex":"54686973206973206120746573742e","payload_text":"This is a test.","to_node":"0xFFFFFFFF","from_node":"0x9EA3D744","hop_count":2,"channel":0}
{"timestamp":1699603247,"session_id":"2025-11-10_083045","node_id":"0x9EA3D744","packet_id":"0x80B24534","frequency":906.875,"rssi":-66,"snr":8.2,"sf":10,"bw":250,"size":64,"port_num":"0x03","packet_type":"POSITION_APP","encrypted":false,"decrypted":true,"psk_key":"key_2","latitude":37.7749,"longitude":-122.4194,"altitude":10,"gps_accuracy":5}
```

**attacks.jsonl Example (Board 2):**
```json
{"timestamp":1699603300,"attack_type":"PACKET_FLOOD","target_node":"0x12345678","target_frequency":906.875,"duration_ms":60000,"packets_sent":3000,"packets_per_sec":50,"result":"target_unresponsive","vulnerability_found":true,"severity":"HIGH","description":"Target stopped responding after 45 seconds of 50pps flood"}
{"timestamp":1699603420,"attack_type":"MALFORMED_PACKETS","target_node":"0x12345678","target_frequency":906.875,"duration_ms":120000,"packets_sent":100,"fuzz_types":["header","length","protobuf"],"result":"target_stable","vulnerability_found":false,"severity":"INFO","description":"Target handled all malformed packets gracefully"}
```

**triangulation.jsonl Example:**
```json
{"timestamp":1699603245,"node_id":"0x9EA3D744","rssi_board1":-65,"rssi_board2":-78,"board1_lat":37.7749,"board1_lon":-122.4194,"board2_lat":37.7750,"board2_lon":-122.4200,"estimated_distance_m":150,"estimated_bearing_deg":45,"confidence":0.75}
```

### SD Card Management

**ESP32 Implementation:**

```cpp
#include <SD.h>
#include <FS.h>
#include <ArduinoJson.h>

class SDCardLogger {
private:
    String sessionFolder;
    File devicesFile;
    File packetsFile;
    File attacksFile;
    bool sdAvailable;
    
public:
    bool initialize() {
        if (!SD.begin(SD_CS_PIN)) {
            Serial.println("❌ SD Card initialization failed");
            sdAvailable = false;
            return false;
        }
        
        Serial.println("✅ SD Card initialized");
        sdAvailable = true;
        
        // Create session folder with timestamp
        sessionFolder = createSessionFolder();
        
        // Open log files in append mode
        String basePath = "/sessions/" + sessionFolder;
        devicesFile = SD.open(basePath + "/devices.jsonl", FILE_APPEND);
        packetsFile = SD.open(basePath + "/packets.jsonl", FILE_APPEND);
        attacksFile = SD.open(basePath + "/attacks.jsonl", FILE_APPEND);
        
        return true;
    }
    
    void logDevice(const DeviceInfo& device) {
        if (!sdAvailable) return;
        
        StaticJsonDocument<512> doc;
        doc["timestamp"] = millis();
        doc["node_id"] = String(device.nodeId, HEX);
        doc["frequency"] = device.frequency;
        doc["rssi"] = device.rssi;
        doc["sf"] = device.spreadingFactor;
        doc["bw"] = device.bandwidth;
        doc["device_type"] = device.deviceType;
        doc["packet_count"] = device.packetCount;
        
        serializeJson(doc, devicesFile);
        devicesFile.println();
        devicesFile.flush();  // Ensure written to SD
    }
    
    void logPacket(const PacketInfo& packet) {
        if (!sdAvailable) return;
        
        StaticJsonDocument<1024> doc;
        doc["timestamp"] = millis();
        doc["node_id"] = String(packet.nodeId, HEX);
        doc["packet_id"] = String(packet.packetId, HEX);
        doc["frequency"] = packet.frequency;
        doc["rssi"] = packet.rssi;
        doc["snr"] = packet.snr;
        doc["size"] = packet.size;
        doc["packet_type"] = packet.typeName;
        doc["decrypted"] = packet.decrypted;
        
        if (packet.hasText) {
            doc["payload_text"] = packet.text;
        }
        
        if (packet.hasGPS) {
            doc["latitude"] = packet.latitude;
            doc["longitude"] = packet.longitude;
            doc["altitude"] = packet.altitude;
        }
        
        serializeJson(doc, packetsFile);
        packetsFile.println();
        packetsFile.flush();
    }
    
    void logAttack(const AttackResult& result) {
        if (!sdAvailable) return;
        
        StaticJsonDocument<1024> doc;
        doc["timestamp"] = millis();
        doc["attack_type"] = attackTypeToString(result.type);
        doc["target_node"] = String(result.targetNode, HEX);
        doc["duration_ms"] = result.duration;
        doc["packets_sent"] = result.packetsSent;
        doc["result"] = result.resultDescription;
        doc["vulnerability_found"] = result.vulnerabilityFound;
        doc["severity"] = result.severity;
        
        serializeJson(doc, attacksFile);
        attacksFile.println();
        attacksFile.flush();
    }
    
    String createSessionFolder() {
        // Format: 2025-11-10_083045
        struct tm timeinfo;
        getLocalTime(&timeinfo);
        char folderName[32];
        strftime(folderName, sizeof(folderName), "%Y-%m-%d_%H%M%S", &timeinfo);
        
        String path = "/sessions/" + String(folderName);
        SD.mkdir(path.c_str());
        
        return String(folderName);
    }
};
```

**Data Integrity:**
- Flush after every write (prevents corruption)
- Session folders isolate different capture runs
- JSON Lines format survives partial writes
- CRC checksums optional for critical data

**Storage Estimates:**

| Data Type | Size per Entry | Entries per Hour | Storage per Hour |
|-----------|----------------|------------------|------------------|
| Device discovery | ~200 bytes | 10-50 | 2-10 KB |
| Packet capture | ~400 bytes | 100-1000 | 40-400 KB |
| Attack logs | ~300 bytes | 5-20 | 1.5-6 KB |
| Triangulation | ~200 bytes | 60-600 | 12-120 KB |
| **Total** | | | **~60-540 KB/hour** |

**With 16 GB SD card:**
- ~30,000 - 270,000 hours of logging
- Practically unlimited for field use
- Can log for weeks/months continuously

---

## PC Software Suite

### Architecture Overview

**Recommended Tech Stack:**

**Option A: Python (Recommended for flexibility)**
- **Backend:** Python 3.10+
- **Database:** SQLite (embedded) or PostgreSQL (server)
- **Analysis:** Pandas, NumPy, SciPy
- **Mapping:** Folium (Leaflet), Google Earth (KML)
- **Visualization:** Matplotlib, Plotly, Dash
- **GUI:** PyQt5 or web-based (Flask/FastAPI)
- **ML:** scikit-learn, TensorFlow (optional)

**Option B: Web-Based Dashboard**
- **Backend:** Node.js + Express or Python FastAPI
- **Frontend:** React + Leaflet.js
- **Database:** PostgreSQL + TimescaleDB
- **Real-time:** WebSocket for live streaming
- **Deployment:** Docker containers

**Option C: Desktop Application**
- **Framework:** Electron (cross-platform)
- **Language:** TypeScript/JavaScript
- **Database:** SQLite (local)
- **Mapping:** Leaflet.js or Mapbox GL
- **Charts:** Chart.js or D3.js

### Component Modules

#### 1. Data Import & Parser

**Features:**
- Scan SD card or USB connection
- Parse JSON Lines files
- Validate data integrity
- Import into database
- Handle duplicate detection
- Merge multiple sessions

**Python Example:**
```python
import json
import sqlite3
from pathlib import Path

class LogImporter:
    def __init__(self, db_path="lora_analysis.db"):
        self.conn = sqlite3.connect(db_path)
        self.setup_database()
    
    def setup_database(self):
        # Create tables for devices, packets, attacks, triangulation
        self.conn.execute('''
            CREATE TABLE IF NOT EXISTS sessions (
                id INTEGER PRIMARY KEY,
                session_id TEXT UNIQUE,
                start_time INTEGER,
                end_time INTEGER,
                board_id TEXT,
                location TEXT
            )
        ''')
        
        self.conn.execute('''
            CREATE TABLE IF NOT EXISTS devices (
                id INTEGER PRIMARY KEY,
                session_id TEXT,
                node_id TEXT,
                frequency REAL,
                rssi REAL,
                device_type TEXT,
                firmware TEXT,
                first_seen INTEGER,
                last_seen INTEGER,
                packet_count INTEGER,
                FOREIGN KEY (session_id) REFERENCES sessions(session_id)
            )
        ''')
        
        self.conn.execute('''
            CREATE TABLE IF NOT EXISTS packets (
                id INTEGER PRIMARY KEY,
                session_id TEXT,
                timestamp INTEGER,
                node_id TEXT,
                packet_id TEXT,
                frequency REAL,
                rssi REAL,
                snr REAL,
                packet_type TEXT,
                decrypted BOOLEAN,
                payload_text TEXT,
                latitude REAL,
                longitude REAL,
                altitude REAL,
                FOREIGN KEY (session_id) REFERENCES sessions(session_id)
            )
        ''')
        
        self.conn.execute('''
            CREATE TABLE IF NOT EXISTS attacks (
                id INTEGER PRIMARY KEY,
                session_id TEXT,
                timestamp INTEGER,
                attack_type TEXT,
                target_node TEXT,
                duration_ms INTEGER,
                packets_sent INTEGER,
                result TEXT,
                vulnerability_found BOOLEAN,
                severity TEXT,
                FOREIGN KEY (session_id) REFERENCES sessions(session_id)
            )
        ''')
        
        self.conn.commit()
    
    def import_session(self, session_folder):
        session_path = Path(session_folder)
        session_id = session_path.name
        
        print(f"Importing session: {session_id}")
        
        # Import devices
        devices_file = session_path / "devices.jsonl"
        if devices_file.exists():
            self.import_devices(devices_file, session_id)
        
        # Import packets
        packets_file = session_path / "packets.jsonl"
        if packets_file.exists():
            self.import_packets(packets_file, session_id)
        
        # Import attacks
        attacks_file = session_path / "attacks.jsonl"
        if attacks_file.exists():
            self.import_attacks(attacks_file, session_id)
        
        print(f"✅ Session {session_id} imported successfully")
    
    def import_devices(self, filepath, session_id):
        with open(filepath, 'r') as f:
            for line in f:
                if not line.strip():
                    continue
                device = json.loads(line)
                self.conn.execute('''
                    INSERT INTO devices 
                    (session_id, node_id, frequency, rssi, device_type, 
                     first_seen, last_seen, packet_count)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                ''', (
                    session_id,
                    device['node_id'],
                    device['frequency'],
                    device['rssi'],
                    device.get('device_type', 'Unknown'),
                    device.get('first_seen', device['timestamp']),
                    device.get('last_seen', device['timestamp']),
                    device.get('packet_count', 0)
                ))
        self.conn.commit()
    
    def import_packets(self, filepath, session_id):
        with open(filepath, 'r') as f:
            for line in f:
                if not line.strip():
                    continue
                packet = json.loads(line)
                self.conn.execute('''
                    INSERT INTO packets
                    (session_id, timestamp, node_id, packet_id, frequency,
                     rssi, snr, packet_type, decrypted, payload_text,
                     latitude, longitude, altitude)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                ''', (
                    session_id,
                    packet['timestamp'],
                    packet['node_id'],
                    packet.get('packet_id', ''),
                    packet['frequency'],
                    packet['rssi'],
                    packet.get('snr', 0),
                    packet.get('packet_type', 'UNKNOWN'),
                    packet.get('decrypted', False),
                    packet.get('payload_text', None),
                    packet.get('latitude', None),
                    packet.get('longitude', None),
                    packet.get('altitude', None)
                ))
        self.conn.commit()
```

#### 2. Signal Analysis Engine

**Features:**
- RSSI statistics (min/max/avg/std dev)
- Packet rate analysis
- Channel occupancy
- Signal quality metrics (SNR, packet loss)
- Device activity patterns
- Frequency usage heatmaps

**Analysis Examples:**

```python
import pandas as pd
import numpy as np
from scipy import stats

class SignalAnalyzer:
    def __init__(self, db_connection):
        self.conn = db_connection
    
    def get_device_statistics(self, session_id, node_id):
        query = '''
            SELECT rssi, snr, frequency, timestamp
            FROM packets
            WHERE session_id = ? AND node_id = ?
            ORDER BY timestamp
        '''
        df = pd.read_sql_query(query, self.conn, params=(session_id, node_id))
        
        stats = {
            'packet_count': len(df),
            'rssi_mean': df['rssi'].mean(),
            'rssi_std': df['rssi'].std(),
            'rssi_min': df['rssi'].min(),
            'rssi_max': df['rssi'].max(),
            'snr_mean': df['snr'].mean(),
            'frequency_primary': df['frequency'].mode()[0],
            'active_duration_sec': (df['timestamp'].max() - df['timestamp'].min()) / 1000
        }
        
        # Calculate packet rate
        time_span_sec = stats['active_duration_sec']
        stats['packets_per_minute'] = (stats['packet_count'] / time_span_sec) * 60
        
        return stats
    
    def detect_anomalies(self, session_id, node_id):
        """Detect unusual signal patterns"""
        query = '''
            SELECT timestamp, rssi, snr
            FROM packets
            WHERE session_id = ? AND node_id = ?
            ORDER BY timestamp
        '''
        df = pd.read_sql_query(query, self.conn, params=(session_id, node_id))
        
        # Z-score anomaly detection
        df['rssi_zscore'] = np.abs(stats.zscore(df['rssi']))
        anomalies = df[df['rssi_zscore'] > 3]  # >3 std deviations
        
        return {
            'anomaly_count': len(anomalies),
            'anomaly_timestamps': anomalies['timestamp'].tolist(),
            'sudden_drops': len(anomalies[anomalies['rssi'] < df['rssi'].mean() - 2*df['rssi'].std()])
        }
    
    def analyze_channel_occupancy(self, session_id):
        """Calculate time each frequency was active"""
        query = '''
            SELECT frequency, COUNT(*) as packet_count, 
                   MIN(timestamp) as first, MAX(timestamp) as last
            FROM packets
            WHERE session_id = ?
            GROUP BY frequency
        '''
        df = pd.read_sql_query(query, self.conn, params=(session_id,))
        
        df['duration_sec'] = (df['last'] - df['first']) / 1000
        df['occupancy_percent'] = (df['packet_count'] / df['packet_count'].sum()) * 100
        
        return df.sort_values('occupancy_percent', ascending=False)
```

#### 3. Interactive Mapping

**Features:**
- Display devices on interactive map
- GPS positions from POSITION_APP packets
- RSSI heatmaps
- Triangulation visualization
- Coverage area estimation
- Device movement tracking
- Click for device details

**Python + Folium Example:**

```python
import folium
from folium.plugins import HeatMap, MarkerCluster

class LoRaMapper:
    def __init__(self, db_connection):
        self.conn = db_connection
    
    def create_device_map(self, session_id, output_file="map.html"):
        # Get all packets with GPS data
        query = '''
            SELECT node_id, latitude, longitude, rssi, packet_type, payload_text
            FROM packets
            WHERE session_id = ? AND latitude IS NOT NULL AND longitude IS NOT NULL
        '''
        df = pd.read_sql_query(query, self.conn, params=(session_id,))
        
        if len(df) == 0:
            print("No GPS data found in session")
            return
        
        # Create base map centered on mean position
        center_lat = df['latitude'].mean()
        center_lon = df['longitude'].mean()
        m = folium.Map(location=[center_lat, center_lon], zoom_start=13)
        
        # Add device markers with clustering
        marker_cluster = MarkerCluster().add_to(m)
        
        for _, row in df.iterrows():
            # Color by RSSI
            if row['rssi'] > -60:
                color = 'green'
            elif row['rssi'] > -80:
                color = 'orange'
            else:
                color = 'red'
            
            popup_text = f"""
            <b>Node:</b> {row['node_id']}<br>
            <b>RSSI:</b> {row['rssi']} dBm<br>
            <b>Type:</b> {row['packet_type']}<br>
            <b>Text:</b> {row['payload_text'] or 'N/A'}
            """
            
            folium.Marker(
                location=[row['latitude'], row['longitude']],
                popup=folium.Popup(popup_text, max_width=300),
                icon=folium.Icon(color=color, icon='info-sign')
            ).add_to(marker_cluster)
        
        # Add RSSI heatmap layer
        heat_data = [[row['latitude'], row['longitude'], abs(row['rssi'])] 
                     for _, row in df.iterrows()]
        HeatMap(heat_data, name='RSSI Heatmap', overlay=True, control=True).add_to(m)
        
        # Add layer control
        folium.LayerControl().add_to(m)
        
        # Save to HTML
        m.save(output_file)
        print(f"✅ Map saved to {output_file}")
    
    def create_triangulation_map(self, session_id, output_file="triangulation.html"):
        """Visualize triangulation estimates"""
        query = '''
            SELECT node_id, board1_lat, board1_lon, board2_lat, board2_lon,
                   estimated_distance_m, estimated_bearing_deg, confidence,
                   rssi_board1, rssi_board2
            FROM triangulation
            WHERE session_id = ?
        '''
        df = pd.read_sql_query(query, self.conn, params=(session_id,))
        
        if len(df) == 0:
            return
        
        # Create map
        center_lat = (df['board1_lat'].mean() + df['board2_lat'].mean()) / 2
        center_lon = (df['board1_lon'].mean() + df['board2_lon'].mean()) / 2
        m = folium.Map(location=[center_lat, center_lon], zoom_start=14)
        
        # Add board positions
        folium.Marker(
            [df['board1_lat'].iloc[0], df['board1_lon'].iloc[0]],
            popup="Board 1 (Monitor)",
            icon=folium.Icon(color='blue', icon='record')
        ).add_to(m)
        
        folium.Marker(
            [df['board2_lat'].iloc[0], df['board2_lon'].iloc[0]],
            popup="Board 2 (Attack)",
            icon=folium.Icon(color='purple', icon='record')
        ).add_to(m)
        
        # Add estimated target positions
        for _, row in df.iterrows():
            # Calculate estimated position based on bearing and distance
            # (Simplified - use proper geodesic calculations in production)
            est_lat = row['board1_lat'] + (row['estimated_distance_m'] / 111000) * \
                      np.cos(np.radians(row['estimated_bearing_deg']))
            est_lon = row['board1_lon'] + (row['estimated_distance_m'] / (111000 * np.cos(np.radians(row['board1_lat'])))) * \
                      np.sin(np.radians(row['estimated_bearing_deg']))
            
            # Confidence circle
            folium.Circle(
                location=[est_lat, est_lon],
                radius=row['estimated_distance_m'] * (1 - row['confidence']),  # Larger = less confident
                popup=f"Target: {row['node_id']}<br>Confidence: {row['confidence']*100:.1f}%",
                color='red',
                fill=True,
                fillOpacity=0.2
            ).add_to(m)
        
        m.save(output_file)
        print(f"✅ Triangulation map saved to {output_file}")
```

#### 4. Attack Analysis & Reporting

**Features:**
- Attack success/failure rates
- Vulnerability severity classification
- Timeline visualization
- Target response analysis
- Comparative attack effectiveness
- Automated CVE draft generation

**Report Generator:**

```python
from datetime import datetime
from reportlab.lib.pagesizes import letter
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table
from reportlab.lib.styles import getSampleStyleSheet

class AttackReporter:
    def __init__(self, db_connection):
        self.conn = db_connection
    
    def generate_pdf_report(self, session_id, output_file="attack_report.pdf"):
        doc = SimpleDocTemplate(output_file, pagesize=letter)
        story = []
        styles = getSampleStyleSheet()
        
        # Title
        title = Paragraph(f"LoRa Security Assessment Report<br/>Session: {session_id}", 
                         styles['Title'])
        story.append(title)
        story.append(Spacer(1, 12))
        
        # Executive Summary
        summary = self.get_executive_summary(session_id)
        story.append(Paragraph("Executive Summary", styles['Heading1']))
        story.append(Paragraph(summary, styles['BodyText']))
        story.append(Spacer(1, 12))
        
        # Vulnerabilities Found
        vulns = self.get_vulnerabilities(session_id)
        story.append(Paragraph("Vulnerabilities Discovered", styles['Heading1']))
        
        for vuln in vulns:
            story.append(Paragraph(f"<b>{vuln['severity']}</b>: {vuln['description']}", 
                                 styles['BodyText']))
            story.append(Spacer(1, 6))
        
        # Attack Timeline Table
        story.append(Paragraph("Attack Timeline", styles['Heading1']))
        timeline_data = self.get_attack_timeline(session_id)
        table = Table(timeline_data)
        story.append(table)
        
        # Build PDF
        doc.build(story)
        print(f"✅ Report saved to {output_file}")
    
    def get_executive_summary(self, session_id):
        query = '''
            SELECT COUNT(*) as total_attacks,
                   SUM(CASE WHEN vulnerability_found = 1 THEN 1 ELSE 0 END) as vulns_found,
                   COUNT(DISTINCT target_node) as targets_tested
            FROM attacks
            WHERE session_id = ?
        '''
        cursor = self.conn.execute(query, (session_id,))
        row = cursor.fetchone()
        
        return f"""
        This security assessment tested {row[2]} target devices with {row[0]} different attack types.
        {row[1]} vulnerabilities were discovered during testing. Detailed findings and recommendations
        are provided in the sections below.
        """
    
    def get_vulnerabilities(self, session_id):
        query = '''
            SELECT attack_type, target_node, severity, result as description
            FROM attacks
            WHERE session_id = ? AND vulnerability_found = 1
            ORDER BY 
                CASE severity
                    WHEN 'CRITICAL' THEN 1
                    WHEN 'HIGH' THEN 2
                    WHEN 'MEDIUM' THEN 3
                    WHEN 'LOW' THEN 4
                    ELSE 5
                END
        '''
        cursor = self.conn.execute(query, (session_id,))
        return [{'severity': row[2], 'description': f"{row[0]} on {row[1]}: {row[3]}"} 
                for row in cursor.fetchall()]
```

---

## Real-Time Analysis

### USB Tethered Mode

**Live Serial Streaming:**

```python
import serial
import json
import threading
from queue import Queue

class LiveMonitor:
    def __init__(self, port="COM3", baudrate=115200):
        self.serial = serial.Serial(port, baudrate, timeout=1)
        self.data_queue = Queue()
        self.running = False
        
    def start(self):
        self.running = True
        self.thread = threading.Thread(target=self._read_serial)
        self.thread.start()
    
    def stop(self):
        self.running = False
        self.thread.join()
    
    def _read_serial(self):
        while self.running:
            line = self.serial.readline().decode('utf-8', errors='ignore').strip()
            if not line:
                continue
            
            # Parse JSON packets
            if line.startswith('{') and line.endswith('}'):
                try:
                    data = json.loads(line)
                    self.data_queue.put(data)
                except json.JSONDecodeError:
                    pass
    
    def get_latest_data(self):
        data = []
        while not self.data_queue.empty():
            data.append(self.data_queue.get())
        return data

# Usage with real-time dashboard
monitor = LiveMonitor("COM3")
monitor.start()

while True:
    new_data = monitor.get_latest_data()
    for packet in new_data:
        # Update live map, graphs, etc.
        print(f"Node {packet['node_id']}: {packet['rssi']} dBm")
    time.sleep(0.1)
```

**Web Dashboard with WebSocket:**

```javascript
// Real-time web dashboard (JavaScript)
const ws = new WebSocket('ws://localhost:8080');

ws.onmessage = function(event) {
    const data = JSON.parse(event.data);
    
    // Update live map
    addMarkerToMap(data.latitude, data.longitude, data.node_id);
    
    // Update RSSI graph
    addDataPoint(data.timestamp, data.rssi);
    
    // Update device list
    updateDeviceTable(data);
};
```

---

## Offline Analysis Tools

### Batch Processing Scripts

**Command-line tools for quick analysis:**

```bash
# Import all sessions from SD card
python tools/import_logs.py /media/sd_card/sessions/

# Generate map from session
python tools/create_map.py --session 2025-11-10_083045 --output map.html

# Export to KML for Google Earth
python tools/export_kml.py --session 2025-11-10_083045 --output devices.kml

# Generate attack report
python tools/attack_report.py --session 2025-11-10_083045 --format pdf

# Compare multiple sessions
python tools/compare_sessions.py session1 session2 session3

# Device fingerprinting analysis
python tools/fingerprint_devices.py --session 2025-11-10_083045
```

### Machine Learning Integration

**Device Classification:**

```python
from sklearn.ensemble import RandomForestClassifier
from sklearn.preprocessing import StandardScaler

class DeviceFingerprintML:
    def __init__(self):
        self.model = RandomForestClassifier(n_estimators=100)
        self.scaler = StandardScaler()
    
    def extract_features(self, packets_df):
        """Extract features from packet timing and RF characteristics"""
        features = []
        for node_id in packets_df['node_id'].unique():
            node_packets = packets_df[packets_df['node_id'] == node_id]
            
            # Timing features
            inter_packet_times = node_packets['timestamp'].diff()
            
            # RF features
            feature_vector = [
                node_packets['rssi'].mean(),
                node_packets['rssi'].std(),
                node_packets['snr'].mean(),
                inter_packet_times.mean(),
                inter_packet_times.std(),
                len(node_packets),  # Total packets
                node_packets['frequency'].mode()[0]  # Primary frequency
            ]
            
            features.append(feature_vector)
        
        return np.array(features)
    
    def train(self, training_data, labels):
        """Train classifier on known device types"""
        X = self.scaler.fit_transform(training_data)
        self.model.fit(X, labels)
    
    def predict(self, unknown_packets):
        """Classify unknown devices"""
        features = self.extract_features(unknown_packets)
        X = self.scaler.transform(features)
        predictions = self.model.predict(X)
        confidence = self.model.predict_proba(X)
        
        return predictions, confidence
```

---

## Database Architecture

### Schema Design

**Normalized database for efficient queries:**

```sql
-- Sessions table
CREATE TABLE sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id TEXT UNIQUE NOT NULL,
    start_time INTEGER,
    end_time INTEGER,
    board_id TEXT,
    location TEXT,
    notes TEXT
);

-- Devices table
CREATE TABLE devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id TEXT,
    node_id TEXT,
    frequency REAL,
    spreading_factor INTEGER,
    bandwidth REAL,
    device_type TEXT,
    firmware_version TEXT,
    first_seen INTEGER,
    last_seen INTEGER,
    packet_count INTEGER,
    avg_rssi REAL,
    FOREIGN KEY (session_id) REFERENCES sessions(session_id),
    INDEX idx_node (node_id),
    INDEX idx_session_node (session_id, node_id)
);

-- Packets table (time-series optimized)
CREATE TABLE packets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id TEXT,
    timestamp INTEGER,
    node_id TEXT,
    packet_id TEXT,
    frequency REAL,
    rssi REAL,
    snr REAL,
    spreading_factor INTEGER,
    bandwidth REAL,
    size INTEGER,
    packet_type TEXT,
    encrypted BOOLEAN,
    decrypted BOOLEAN,
    psk_key TEXT,
    payload_hex TEXT,
    payload_text TEXT,
    latitude REAL,
    longitude REAL,
    altitude REAL,
    hop_count INTEGER,
    channel INTEGER,
    FOREIGN KEY (session_id) REFERENCES sessions(session_id),
    INDEX idx_timestamp (timestamp),
    INDEX idx_node_time (node_id, timestamp)
);

-- Attacks table
CREATE TABLE attacks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id TEXT,
    timestamp INTEGER,
    attack_type TEXT,
    target_node TEXT,
    target_frequency REAL,
    duration_ms INTEGER,
    packets_sent INTEGER,
    packets_per_sec REAL,
    result TEXT,
    vulnerability_found BOOLEAN,
    severity TEXT,
    description TEXT,
    recommendations TEXT,
    FOREIGN KEY (session_id) REFERENCES sessions(session_id),
    INDEX idx_target (target_node)
);

-- Triangulation table
CREATE TABLE triangulation (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id TEXT,
    timestamp INTEGER,
    node_id TEXT,
    rssi_board1 REAL,
    rssi_board2 REAL,
    board1_lat REAL,
    board1_lon REAL,
    board2_lat REAL,
    board2_lon REAL,
    estimated_distance_m REAL,
    estimated_bearing_deg REAL,
    confidence REAL,
    FOREIGN KEY (session_id) REFERENCES sessions(session_id)
);

-- Vulnerabilities summary table
CREATE TABLE vulnerabilities (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id TEXT,
    node_id TEXT,
    vulnerability_type TEXT,
    severity TEXT,
    discovered_timestamp INTEGER,
    proof_of_concept TEXT,
    remediation TEXT,
    cve_id TEXT,
    FOREIGN KEY (session_id) REFERENCES sessions(session_id)
);
```

### Query Examples

**Useful queries for analysis:**

```sql
-- Find most active devices
SELECT node_id, COUNT(*) as packet_count, AVG(rssi) as avg_rssi
FROM packets
WHERE session_id = '2025-11-10_083045'
GROUP BY node_id
ORDER BY packet_count DESC
LIMIT 10;

-- Devices with GPS positions
SELECT DISTINCT node_id, 
       AVG(latitude) as avg_lat, 
       AVG(longitude) as avg_lon,
       COUNT(*) as position_reports
FROM packets
WHERE session_id = '2025-11-10_083045' 
  AND latitude IS NOT NULL
GROUP BY node_id;

-- Attack success rate by type
SELECT attack_type,
       COUNT(*) as total_attacks,
       SUM(CASE WHEN vulnerability_found = 1 THEN 1 ELSE 0 END) as successful,
       AVG(CASE WHEN vulnerability_found = 1 THEN 1.0 ELSE 0.0 END) * 100 as success_rate
FROM attacks
GROUP BY attack_type
ORDER BY success_rate DESC;

-- Packet rate over time (5-minute buckets)
SELECT 
    (timestamp / 300000) * 300000 as time_bucket,
    COUNT(*) as packets,
    AVG(rssi) as avg_rssi
FROM packets
WHERE session_id = '2025-11-10_083045'
GROUP BY time_bucket
ORDER BY time_bucket;

-- Devices vulnerable to specific attack
SELECT DISTINCT d.node_id, d.device_type, d.firmware_version, a.severity
FROM devices d
JOIN attacks a ON d.node_id = a.target_node AND d.session_id = a.session_id
WHERE a.vulnerability_found = 1
  AND a.attack_type = 'PACKET_FLOOD'
ORDER BY a.severity;
```

---

## Implementation Guide

### Phase 1: SD Card Logging (Weeks 1-2)

**Hardware:**
1. Add SD card module to ESP32 boards
2. Wire SPI interface (MISO, MOSI, SCK, CS)
3. Test card detection and formatting

**Software:**
1. Implement `SDCardLogger` class
2. Add logging calls to existing capture code
3. Create session folders automatically
4. Test write performance and integrity

**Deliverables:**
- SD cards logging all capture data
- JSON Lines format files
- Session folder organization

### Phase 2: Basic PC Import (Weeks 3-4)

**Development:**
1. Setup Python environment
2. Create SQLite database schema
3. Implement `LogImporter` class
4. Build command-line import tool

**Testing:**
1. Import sample session data
2. Verify database integrity
3. Test queries for analysis

**Deliverables:**
- Working import pipeline
- Populated database
- Basic query tools

### Phase 3: Visualization & Mapping (Weeks 5-6)

**Development:**
1. Install Folium/Leaflet
2. Implement `LoRaMapper` class
3. Create interactive maps with markers
4. Add RSSI heatmaps
5. Export to KML/GeoJSON

**Testing:**
1. Generate maps from real data
2. Verify GPS accuracy
3. Test with multiple sessions

**Deliverables:**
- Interactive HTML maps
- KML files for Google Earth
- GeoJSON for web apps

### Phase 4: Real-Time Streaming (Weeks 7-8)

**Development:**
1. Implement serial streaming on ESP32
2. Create `LiveMonitor` Python class
3. Build web dashboard (optional)
4. Add WebSocket support

**Testing:**
1. Test USB tethered mode
2. Verify real-time updates
3. Measure latency

**Deliverables:**
- Live monitoring capability
- Real-time map updates
- Dashboard (if web-based)

### Phase 5: Advanced Analysis (Weeks 9-10)

**Development:**
1. Implement signal analysis algorithms
2. Add attack reporting
3. Create PDF report generator
4. Integrate ML device fingerprinting (optional)

**Testing:**
1. Generate sample reports
2. Validate analysis accuracy
3. Test batch processing

**Deliverables:**
- Complete analysis suite
- Automated reporting
- ML classifier (optional)

---

## Summary

### Key Benefits of PC Integration

1. **Unlimited Storage** - SD cards provide weeks/months of logging
2. **Powerful Analysis** - PC processing for complex algorithms
3. **Beautiful Visualization** - Interactive maps and graphs
4. **Long-term Tracking** - Database for historical comparison
5. **Professional Reports** - Automated PDF/HTML generation
6. **Machine Learning** - Device classification and pattern recognition
7. **Field Portability** - ESP32 boards work standalone, analyze later
8. **Scalability** - Handle data from multiple boards and sessions

### Recommended Starting Point

**Minimal Viable Product (MVP):**
1. ✅ Add SD card logging to ESP32 (Phase 1)
2. ✅ Create Python import tool (Phase 2)
3. ✅ Build basic mapping (Phase 3)
4. ⏳ Add real-time streaming later (Phase 4)
5. ⏳ Advanced analysis as needed (Phase 5)

**Timeline:** 6-8 weeks for complete system

**Effort:** Moderate (mostly Python development)

**Result:** Professional-grade reconnaissance and analysis platform

---

## Addendum: Current Implementation Status & Integration Path

### Existing Tools Foundation

The `tools/` directory already contains a solid foundation for PC integration:

#### ✅ **`live_visualizer.py`** - Real-Time Streaming (Mode 2)

**What it does:**
- Connects to ESP32 via serial port (USB tethered)
- Parses serial output in real-time
- Creates 4 live matplotlib graphs:
  - Signal strength (RSSI) over time per device
  - Discovered devices list with stats
  - Packet histogram by device
  - Reconnaissance summary dashboard
- Updates every 200ms for responsive visualization
- Auto-detects available serial ports

**Current strengths:**
- ✅ Already implements real-time analysis mode
- ✅ Multi-device tracking and visualization
- ✅ Clean matplotlib implementation
- ✅ Perfect for demonstrations and live monitoring
- ✅ Handles serial parsing robustly

**Enhancement opportunities:**
- Add interactive map panel (Folium in separate window)
- Implement database logging of live data
- Add WebSocket mode for web dashboard
- Export live session data to JSONL format

#### ✅ **`analyze_logs.py`** - Offline Analysis Foundation (Mode 1)

**What it does:**
- Loads packets from JSONL files
- Protocol distribution analysis
- Frequency usage statistics
- Signal quality analysis (RSSI/SNR)
- Temporal pattern detection
- Packet size analysis
- Node information extraction
- CSV export capability
- Plot generation (protocol pie chart, RSSI histogram, frequency usage, temporal scatter)

**Current strengths:**
- ✅ Uses JSONL format (matches recommended architecture)
- ✅ Comprehensive statistical analysis
- ✅ Good visualization suite
- ✅ CSV export for spreadsheet analysis

**What it needs to complete the vision:**
- Database import functionality (SQLite/PostgreSQL)
- Interactive mapping for GPS positions
- Attack log analysis module
- PDF report generation
- Triangulation data analysis
- Session comparison capabilities

#### ✅ **`requirements.txt`** - Dependencies

**Current:**
```txt
pyserial>=3.5
matplotlib>=3.5.0
```

**Recommended additions:**
```txt
# Current
pyserial>=3.5
matplotlib>=3.5.0

# Data processing
pandas>=2.0.0
numpy>=1.24.0

# Database
# (SQLite is built into Python, no package needed)

# Mapping
folium>=0.14.0

# JSON handling
# (Built into Python)

# Report generation (optional)
reportlab>=4.0.0

# Web dashboard (optional, choose one)
flask>=2.3.0
# OR
fastapi>=0.100.0
uvicorn>=0.23.0  # for FastAPI

# Real-time web (optional)
websockets>=11.0
```

### Architecture Integration Map

```
┌─────────────────────────────────────────────────────────────┐
│                  CURRENT STATE                               │
│                                                              │
│  ESP32 → Serial → live_visualizer.py (Real-time graphs)    │
│                                                              │
│  (SD card logging not yet implemented)                      │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                  TARGET STATE                                │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  FIELD OPERATION (ESP32)                            │   │
│  │  • Real-time OLED display                           │   │
│  │  • SD card logging (JSONL format)                   │   │
│  │  • USB serial output                                │   │
│  └──────────────┬───────────────────┬──────────────────┘   │
│                 │                   │                       │
│                 ▼                   ▼                       │
│  ┌──────────────────────┐  ┌──────────────────────────┐   │
│  │  OFFLINE ANALYSIS    │  │  REAL-TIME MONITORING    │   │
│  │  (Mode 1)            │  │  (Mode 2)                │   │
│  │                      │  │                          │   │
│  │  SD Card → analyze   │  │  USB → live_visualizer   │   │
│  │  ├─ Database import  │  │  ├─ Live graphs         │   │
│  │  ├─ Mapping          │  │  ├─ Device tracking     │   │
│  │  ├─ Attack analysis  │  │  ├─ Real-time stats     │   │
│  │  ├─ Reports (PDF)    │  │  └─ Optional web dash   │   │
│  │  └─ Session compare  │  │                          │   │
│  └──────────────────────┘  └──────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### Implementation Priority Path

#### **Phase 1: SD Card Logging (Foundation)** ⭐ CRITICAL

**ESP32 Firmware Changes:**

1. Add SD card hardware support:
   - SD card module connected via SPI
   - Pins: MISO, MOSI, SCK, CS (typically GPIO 5)
   - Add to `platformio.ini` dependencies if needed

2. Implement `SDCardLogger` class (from main document):
   ```cpp
   // firmware/src/sd_logger.h
   // firmware/src/sd_logger.cpp
   ```

3. Integrate logging calls:
   ```cpp
   // In packet capture code
   logger.logPacket(packetInfo);
   
   // In device discovery code
   logger.logDevice(deviceInfo);
   
   // In attack execution code (Board 2)
   logger.logAttack(attackResult);
   ```

4. Session folder management:
   - Create timestamped folders: `2025-11-10_083045/`
   - Separate files: `devices.jsonl`, `packets.jsonl`, `attacks.jsonl`

**Deliverable:** ESP32 boards logging all data to SD card in JSONL format

#### **Phase 2: Enhanced analyze_logs.py** ⭐ HIGH PRIORITY

**Add these capabilities:**

1. **Database Import:**
   ```python
   def import_to_database(packets, db_path='lora_analysis.db'):
       # Create SQLite database
       # Use schema from main document
       # Import devices, packets, attacks
   ```

2. **Interactive Mapping:**
   ```python
   def create_interactive_map(packets, output='map.html'):
       import folium
       # Extract GPS positions
       # Create markers with device info
       # Add RSSI heatmap layer
   ```

3. **Attack Analysis:**
   ```python
   def analyze_attacks(attack_file):
       # Parse attacks.jsonl
       # Vulnerability classification
       # Success rate by attack type
       # Severity scoring
   ```

4. **Command-line enhancements:**
   ```bash
   python analyze_logs.py session_folder/ --import-db
   python analyze_logs.py session_folder/ --create-map
   python analyze_logs.py session_folder/ --attack-report
   python analyze_logs.py session_folder/ --compare other_session/
   ```

**Deliverable:** Enhanced `analyze_logs.py` with database and mapping

#### **Phase 3: New Tool Scripts**

Create these additional tools:

1. **`tools/import_session.py`** - Batch session import
   ```bash
   python tools/import_session.py /media/sd_card/sessions/2025-11-10_083045/
   ```

2. **`tools/create_map.py`** - Standalone map generator
   ```bash
   python tools/create_map.py --session 2025-11-10_083045 --output map.html
   python tools/create_map.py --session 2025-11-10_083045 --kml --output devices.kml
   ```

3. **`tools/attack_report.py`** - Attack analysis and reporting
   ```bash
   python tools/attack_report.py --session 2025-11-10_083045 --format pdf
   ```

4. **`tools/compare_sessions.py`** - Multi-session comparison
   ```bash
   python tools/compare_sessions.py session1/ session2/ session3/
   ```

5. **`tools/triangulation_analyzer.py`** - Position estimation
   ```bash
   python tools/triangulation_analyzer.py --session 2025-11-10_083045
   ```

**Deliverable:** Complete command-line tool suite

#### **Phase 4: Web Dashboard (Optional)**

For advanced real-time monitoring:

1. Flask/FastAPI backend
2. WebSocket for live data streaming
3. Leaflet.js for interactive mapping
4. Chart.js for live graphs
5. Device control interface

**Deliverable:** Web-based monitoring dashboard

### Quick Wins - Start Here

**If you want immediate value, do these first:**

#### 1. **Enhance `requirements.txt`** (5 minutes)

Add pandas and folium:
```bash
pip install pandas folium
```

#### 2. **Add database import to `analyze_logs.py`** (30 minutes)

```python
import sqlite3

def create_database_schema(db_path):
    conn = sqlite3.connect(db_path)
    # Use schema from PC_INTEGRATION_ARCHITECTURE.md
    conn.execute('''CREATE TABLE IF NOT EXISTS packets ...''')
    conn.commit()
    return conn

def import_packets_to_db(packets, db_path):
    conn = create_database_schema(db_path)
    for packet in packets:
        conn.execute('''INSERT INTO packets ...''', packet_values)
    conn.commit()
    print(f"✅ Imported {len(packets)} packets to {db_path}")
```

#### 3. **Add basic mapping to `analyze_logs.py`** (20 minutes)

```python
import folium

def create_map(packets, output='map.html'):
    # Filter packets with GPS
    gps_packets = [p for p in packets if 'latitude' in p and p['latitude']]
    
    if not gps_packets:
        print("No GPS data found")
        return
    
    # Create map
    center = [
        sum(p['latitude'] for p in gps_packets) / len(gps_packets),
        sum(p['longitude'] for p in gps_packets) / len(gps_packets)
    ]
    
    m = folium.Map(location=center, zoom_start=13)
    
    # Add markers
    for p in gps_packets:
        folium.Marker(
            [p['latitude'], p['longitude']],
            popup=f"Node: {p.get('node_id', 'Unknown')}<br>RSSI: {p.get('rssi', 0)} dBm"
        ).add_to(m)
    
    m.save(output)
    print(f"✅ Map saved to {output}")
```

#### 4. **Test with sample data** (10 minutes)

Create a test JSONL file to validate:
```json
{"timestamp":1699603245,"node_id":"0x9EA3D744","frequency":906.875,"rssi":-65,"latitude":37.7749,"longitude":-122.4194}
{"timestamp":1699603246,"node_id":"0x12345678","frequency":903.875,"rssi":-72,"latitude":37.7750,"longitude":-122.4195}
```

Then run:
```bash
python analyze_logs.py test_packets.jsonl --plot
```

### Integration Checklist

Use this to track implementation progress:

**ESP32 Firmware:**
- [ ] SD card hardware added to both boards
- [ ] `SDCardLogger` class implemented
- [ ] Session folder creation working
- [ ] Device logging active
- [ ] Packet logging active
- [ ] Attack logging active (Board 2)
- [ ] JSONL format validated
- [ ] SD card capacity monitoring
- [ ] Error handling for full/missing card

**PC Tools - Offline Analysis:**
- [ ] `analyze_logs.py` enhanced with database import
- [ ] Interactive mapping added
- [ ] Attack analysis module added
- [ ] `import_session.py` created
- [ ] `create_map.py` created
- [ ] `attack_report.py` created
- [ ] `compare_sessions.py` created
- [ ] PDF report generation working
- [ ] KML export for Google Earth
- [ ] GeoJSON export for web mapping

**PC Tools - Real-Time:**
- [ ] `live_visualizer.py` database logging added
- [ ] Interactive map panel integrated
- [ ] WebSocket mode (optional)
- [ ] Web dashboard (optional)

**Dependencies:**
- [ ] `requirements.txt` updated
- [ ] All packages installed
- [ ] Documentation updated

**Testing:**
- [ ] Sample session data created
- [ ] Import pipeline tested
- [ ] Maps generating correctly
- [ ] Reports formatting properly
- [ ] Multi-session comparison working
- [ ] Real-time streaming validated

### Recommended Starting Command

Once Phase 1 (SD logging) and Phase 2 (enhanced analyze_logs) are complete:

```bash
# Field operation - let ESP32 run for session
# Remove SD card

# Import and analyze on PC
cd tools
python analyze_logs.py ../sd_card/sessions/2025-11-10_083045/ \
    --import-db \
    --create-map \
    --attack-report \
    --plot

# Output:
# ✅ Imported 1,247 packets to lora_analysis.db
# ✅ Map saved to map.html
# ✅ Attack report saved to attack_report.pdf
# ✅ Analysis plots saved to lora_analysis.png
```

---

**Document Version:** 1.1  
**Last Updated:** November 10, 2025  
**Author:** ESP32 LoRa Sniffer Project  
**Status:** Design specification with current implementation analysis
