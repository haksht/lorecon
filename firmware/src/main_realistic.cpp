/**
 * ESP32 LoRa Reconnaissance Tool
 * 
 * A realistic LoRa packet capture and analysis tool for ESP32-S3 + SX1262
 * 
 * Capabilities:
 * - Sequential frequency scanning across ISM band
 * - Protocol identification (Meshtastic, TTN, generic LoRa)
 * - Packet capture with signal analysis
 * - Basic node tracking and behavior analysis
 * - Local storage of captured data
 * 
 * Limitations:
 * - Single radio = sequential scanning only (not simultaneous)
 * - Range limited by antenna and environment
 * - Cannot break encryption (only analyzes headers/metadata)
 * - Detection depends on being tuned to correct parameters during transmission
 */

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Hardware configuration - Heltec WiFi LoRa 32 V3
#define LORA_NSS    8
#define LORA_DIO1   14
#define LORA_RST    12
#define LORA_BUSY   13

// SPI pins
#define SCK         9
#define MISO        11
#define MOSI        10

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

// Scanning configuration
struct ScanConfig {
  float frequency;
  float bandwidth;
  int spreadingFactor;
  uint8_t syncWord;
  const char* description;
};

// Realistic frequency configurations for ISM band reconnaissance
ScanConfig scanConfigs[] = {
  // Common Meshtastic frequencies (US)
  {906.875, 250.0, 11, 0x2b, "Meshtastic LongFast 906.875MHz"},
  {902.125, 250.0, 11, 0x2b, "Meshtastic LongFast 902.125MHz"}, 
  {903.875, 250.0, 10, 0x2b, "Meshtastic MedFast"},
  
  // TTN/LoRaWAN US channels
  {903.9, 125.0, 7, 0x12, "TTN US Ch 0 (SF7)"},
  {904.1, 125.0, 8, 0x12, "TTN US Ch 1 (SF8)"},
  {904.3, 125.0, 9, 0x12, "TTN US Ch 2 (SF9)"},
  {904.5, 125.0, 10, 0x12, "TTN US Ch 3 (SF10)"},
  
  // ISM band exploration
  {915.0, 125.0, 7, 0x12, "ISM 915MHz SF7"},
  {920.0, 250.0, 8, 0x12, "ISM 920MHz SF8"},
  {925.0, 500.0, 9, 0x12, "ISM 925MHz SF9"},
};

const int NUM_CONFIGS = sizeof(scanConfigs) / sizeof(ScanConfig);
int currentConfigIndex = 0;
unsigned long lastScanTime = 0;
const unsigned long SCAN_INTERVAL = 10000; // 10 seconds per config

// Packet capture state
volatile bool packetReceived = false;
String lastPacket = "";
int totalPackets = 0;

// Simple node tracking
struct DetectedNode {
  uint32_t nodeId;
  String protocol;
  int packetCount;
  float avgRSSI;
  unsigned long lastSeen;
  bool active;
};

const int MAX_NODES = 10;
DetectedNode nodes[MAX_NODES];
int nodeCount = 0;

// Interrupt handler
void setFlag(void) {
  packetReceived = true;
}

// Apply scan configuration to radio
bool applyScanConfig(const ScanConfig& config) {
  Serial.printf("📡 Scanning: %s\n", config.description);
  
  int state = radio.setFrequency(config.frequency);
  if (state != RADIOLIB_ERR_NONE) return false;
  
  state = radio.setBandwidth(config.bandwidth);
  if (state != RADIOLIB_ERR_NONE) return false;
  
  state = radio.setSpreadingFactor(config.spreadingFactor);
  if (state != RADIOLIB_ERR_NONE) return false;
  
  state = radio.setSyncWord(config.syncWord);
  if (state != RADIOLIB_ERR_NONE) return false;
  
  // Standard LoRa parameters
  radio.setCodingRate(5);      // 4/5 
  radio.setPreambleLength(8);  
  radio.setCRC(false);         // Promiscuous mode
  radio.explicitHeader();
  
  // Receive only mode (no transmission)
  radio.setCurrentLimit(100);
  radio.setOutputPower(0);
  
  return true;
}

// Basic protocol identification based on packet structure
const char* identifyProtocol(const String& packet) {
  if (packet.length() == 0) return "Empty";
  
  // Check for Meshtastic patterns
  if (packet.length() >= 4) {
    uint8_t b0 = packet[0], b1 = packet[1], b2 = packet[2], b3 = packet[3];
    if (b0 == 0xFF && b1 == 0xFF && b2 == 0xFF && b3 == 0xFF) {
      return "Meshtastic";
    }
  }
  
  // TTN/LoRaWAN typically has specific length ranges
  if (packet.length() >= 12 && packet.length() <= 51) {
    return "TTN/LoRaWAN";
  }
  
  // Short packets might be beacons
  if (packet.length() <= 8) {
    return "Beacon";
  }
  
  return "Unknown LoRa";
}

// Extract node ID for tracking (basic implementation)
uint32_t extractNodeId(const String& packet) {
  if (packet.length() >= 8) {
    // Simple node ID extraction (varies by protocol)
    return ((uint32_t)packet[4] << 24) | ((uint32_t)packet[5] << 16) | 
           ((uint32_t)packet[6] << 8) | (uint32_t)packet[7];
  }
  return 0;
}

// Track detected nodes
void trackNode(uint32_t nodeId, const char* protocol, float rssi) {
  if (nodeId == 0) return;
  
  // Find existing or create new
  int nodeIndex = -1;
  for (int i = 0; i < nodeCount; i++) {
    if (nodes[i].nodeId == nodeId) {
      nodeIndex = i;
      break;
    }
  }
  
  if (nodeIndex == -1 && nodeCount < MAX_NODES) {
    nodeIndex = nodeCount++;
    nodes[nodeIndex].nodeId = nodeId;
    nodes[nodeIndex].protocol = protocol;
    nodes[nodeIndex].packetCount = 0;
    nodes[nodeIndex].avgRSSI = rssi;
    Serial.printf("🔍 New node detected: %08X (%s)\n", nodeId, protocol);
  }
  
  if (nodeIndex >= 0) {
    nodes[nodeIndex].packetCount++;
    nodes[nodeIndex].avgRSSI = (nodes[nodeIndex].avgRSSI + rssi) / 2.0;
    nodes[nodeIndex].lastSeen = millis();
    nodes[nodeIndex].active = true;
  }
}

// Save packet to storage (simplified)
void savePacket(const String& packet, float rssi, float snr, const char* protocol) {
  if (!LittleFS.begin()) return;
  
  File logFile = LittleFS.open("/packet_log.json", "a");
  if (logFile) {
    JsonDocument doc;
    doc["timestamp"] = millis();
    doc["protocol"] = protocol;
    doc["length"] = packet.length();
    doc["rssi"] = rssi;
    doc["snr"] = snr;
    doc["frequency"] = scanConfigs[currentConfigIndex].frequency;
    
    // Store first 32 bytes as hex for analysis
    String hexData = "";
    for (int i = 0; i < min((int)packet.length(), 32); i++) {
      char hex[3];
      sprintf(hex, "%02X", (uint8_t)packet[i]);
      hexData += hex;
    }
    doc["data"] = hexData;
    
    serializeJson(doc, logFile);
    logFile.println();
    logFile.close();
  }
}

// Print summary of detected activity
void printSummary() {
  Serial.println("\n📊 RECONNAISSANCE SUMMARY");
  Serial.println("================================");
  Serial.printf("Total packets captured: %d\n", totalPackets);
  Serial.printf("Active nodes tracked: %d\n", nodeCount);
  Serial.printf("Current frequency: %.3f MHz\n", scanConfigs[currentConfigIndex].frequency);
  
  if (nodeCount > 0) {
    Serial.println("\n🎯 Detected Nodes:");
    for (int i = 0; i < nodeCount; i++) {
      if (nodes[i].active) {
        Serial.printf("  %08X (%s): %d packets, %.1f dBm avg\n", 
                      nodes[i].nodeId, nodes[i].protocol.c_str(), 
                      nodes[i].packetCount, nodes[i].avgRSSI);
      }
    }
  }
  Serial.println("================================\n");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("🔍 ESP32 LoRa Reconnaissance Tool");
  Serial.println("==================================");
  Serial.println("Realistic LoRa packet capture and analysis");
  Serial.println();
  
  // Initialize storage
  if (LittleFS.begin()) {
    Serial.println("✅ Storage initialized");
  } else {
    Serial.println("⚠️ Storage initialization failed");
  }
  
  // Initialize SPI
  SPI.begin(SCK, MISO, MOSI, LORA_NSS);
  
  // Initialize radio
  Serial.print("🔧 Initializing SX1262 radio... ");
  int state = radio.begin();
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("✅");
    
    // Set interrupt
    radio.setDio1Action(setFlag);
    
    Serial.printf("📡 Scanning %d frequency configurations\n", NUM_CONFIGS);
    Serial.printf("⏱️ %d seconds per configuration\n", SCAN_INTERVAL/1000);
    Serial.println("🎯 Starting reconnaissance scan...\n");
    
    // Start with first configuration
    if (applyScanConfig(scanConfigs[currentConfigIndex])) {
      radio.startReceive();
      lastScanTime = millis();
    }
    
  } else {
    Serial.printf("❌ Failed! Error: %d\n", state);
    Serial.println("Check wiring and connections.");
    while(1) delay(1000);
  }
}

void loop() {
  // Time to switch frequencies?
  if (millis() - lastScanTime >= SCAN_INTERVAL) {
    currentConfigIndex = (currentConfigIndex + 1) % NUM_CONFIGS;
    
    // Print summary every full cycle
    if (currentConfigIndex == 0) {
      printSummary();
    }
    
    // Apply new configuration
    if (applyScanConfig(scanConfigs[currentConfigIndex])) {
      radio.startReceive();
      lastScanTime = millis();
    }
  }
  
  // Check for received packets
  if (packetReceived) {
    packetReceived = false;
    
    int state = radio.readData(lastPacket);
    
    if (state == RADIOLIB_ERR_NONE && lastPacket.length() > 0) {
      totalPackets++;
      
      float rssi = radio.getRSSI();
      float snr = radio.getSNR();
      const char* protocol = identifyProtocol(lastPacket);
      
      Serial.printf("📦 Packet #%d captured!\n", totalPackets);
      Serial.printf("   Config: %s\n", scanConfigs[currentConfigIndex].description);
      Serial.printf("   Protocol: %s | Length: %d bytes\n", protocol, lastPacket.length());
      Serial.printf("   Signal: RSSI %.1f dBm, SNR %.1f dB\n", rssi, snr);
      
      // Show hex data (first 16 bytes)
      Serial.print("   Data: ");
      for (int i = 0; i < min((int)lastPacket.length(), 16); i++) {
        if (lastPacket[i] < 16) Serial.print("0");
        Serial.print(lastPacket[i], HEX);
        Serial.print(" ");
      }
      if (lastPacket.length() > 16) Serial.print("...");
      Serial.println();
      
      // Track nodes
      uint32_t nodeId = extractNodeId(lastPacket);
      if (nodeId != 0) {
        trackNode(nodeId, protocol, rssi);
      }
      
      // Save to storage
      savePacket(lastPacket, rssi, snr, protocol);
      
      Serial.println();
      
    } else if (state != RADIOLIB_ERR_NONE) {
      Serial.printf("⚠️ Receive error: %d\n", state);
    }
    
    // Restart receiving
    radio.startReceive();
  }
  
  // Periodic activity check
  static unsigned long lastActivityCheck = 0;
  if (millis() - lastActivityCheck >= 30000) {  // Every 30 seconds
    float currentRSSI = radio.getRSSI(false);
    if (currentRSSI > -110.0) {
      Serial.printf("📶 RF activity detected: %.1f dBm on %s\n", 
                    currentRSSI, scanConfigs[currentConfigIndex].description);
    }
    lastActivityCheck = millis();
  }
  
  delay(10);
}