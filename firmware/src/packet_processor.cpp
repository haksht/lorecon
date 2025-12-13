/**
 * Packet Processor Implementation
 */

#include "packet_processor.h"
#include "packet_logger.h"
#include "recon_state.h"
#include "oled_display.h"
#include "text_packet_diagnostic.h"
#include "psk_decryption_simple.h"
#include "config.h"

// Constructor
PacketProcessor::PacketProcessor() {
    lastPacketData.reserve(Config::PacketProcessing::MAX_PACKET_SIZE);
}

// Queue a packet for processing
// 
// QUEUE OVERFLOW BEHAVIOR:
// When queue is full (100 packets), incoming packets are DROPPED and counted.
// This occurs in high-traffic environments (e.g., conferences with 50+ devices).
// 
// Drop rate calculation: droppedPackets / (totalPackets + droppedPackets) * 100%
// Web UI shows warning when drop rate exceeds 5%.
// 
// Trade-off: Dropping packets preserves system stability vs implementing backpressure
// (pausing radio reception) which would create coverage gaps on current frequency.
// 
// Monitor drops via: Serial output, /api/status endpoint, or web UI warning toast.
bool PacketProcessor::queuePacket(const uint8_t* data, size_t length, float rssi, float snr,
                                  uint8_t configIndex, float frequencyMHz) {
    if (isQueueFull()) {
        reconState.scanState.droppedPackets++;
        Serial.printf("[QUEUE] Full (100 packets) - dropping packet! Total drops: %u\n", 
                      reconState.scanState.droppedPackets);
        return false;
    }
    
    // Validate packet length to prevent buffer overflow
    if (length > Config::PacketProcessing::MAX_PACKET_SIZE) {
        Serial.printf("[QUEUE] Rejecting oversized packet (%d bytes > %d max)\n", 
                      length, Config::PacketProcessing::MAX_PACKET_SIZE);
        reconState.scanState.droppedPackets++;
        return false;
    }
    
    QueuedPacket qp;
    memcpy(qp.data, data, length);
    qp.length = length;
    qp.rssi = rssi;
    qp.snr = snr;
    qp.timestamp = millis();
    qp.configIndex = configIndex;
    qp.frequencyMHz = frequencyMHz;
    packetQueue.push(qp);
    
    // Track peak queue size for observability
    size_t currentSize = packetQueue.size();
    if (currentSize > reconState.scanState.peakQueueSize) {
        reconState.scanState.peakQueueSize = currentSize;
    }
    
    return true;
}

// Process all queued packets
void PacketProcessor::processQueue(OLEDDisplay* display) {
    // Debug: Show queue size if it's building up
    if (packetQueue.size() > 1) {
        Serial.printf("[QUEUE] %d packets buffered\n", packetQueue.size());
    }
    
    while (!packetQueue.empty()) {
        QueuedPacket qp = packetQueue.front();
        packetQueue.pop();
        processSinglePacket(qp, display);
    }
}

// Process a single packet
void PacketProcessor::processSinglePacket(const QueuedPacket& qp, OLEDDisplay* display) {
    // Store for potential replay capture using vector
    if (qp.length <= Config::PacketProcessing::MAX_PACKET_SIZE) {
        lastPacketData.assign(qp.data, qp.data + qp.length);
        
        // Also update recon state for backward compatibility
        memcpy(reconState.scanState.lastPacket, qp.data, qp.length);
        reconState.scanState.lastPacketLength = qp.length;
    }
    reconState.scanState.totalPackets++;
    
    // Analyze raw packet for diagnostics (timing, encryption detection)
    TextPacketDiagnostic::analyzePacket(qp.data, qp.length, qp.rssi, qp.snr);
    
    // Analyze packet using ProtocolAnalyzer
    PacketInfo info = protocolAnalyzer.analyze(qp.data, qp.length, qp.rssi);
    
    // Enhanced packet analysis for Meshtastic (extract GPS position silently)
    bool positionExtracted = false;
    const GeoPoint* loggedPoint = nullptr;
    if (strcmp(info.protocol, "Meshtastic") == 0) {
        positionExtracted = geoIntel.extractPosition(qp.data, qp.length, info.nodeId);
        if (positionExtracted && info.nodeId != 0) {
            loggedPoint = geoIntel.findNodePosition(info.nodeId);
        }
    }
    
    // Update temporal metrics BEFORE updating device (needs old lastSeen for interval calc)
    if (info.nodeId != 0) {
        reconState.updateTrafficHistogram();
        reconState.updateDeviceTemporalMetrics(info.nodeId);
    }
    
    // Track as targetable device in ALL modes if we have a real node ID
    // This updates lastSeen timestamp, so must be AFTER temporal update
    if (info.nodeId != 0) {
        reconState.addTargetableDevice(info.nodeId, reconState.scanState.currentConfig, 
                                      qp.rssi, info.protocol, qp.data, qp.length);
        reconState.updateNode(info.nodeId, info.protocol, qp.rssi);
        // Anomaly detection uses updated avgRSSI, so must be AFTER addTargetableDevice
        reconState.checkForAnomalies(qp.data, qp.length, info.nodeId, qp.rssi);
    }
    
    // Mode-specific handling
    if (reconState.scanState.mode == MODE_RECONNAISSANCE) {
        handleReconPacket(info, qp.data, qp.length, qp.rssi, qp.snr, display);
    } else if (reconState.scanState.mode == MODE_TARGETED_CAPTURE) {
        handleTargetedPacket(info, qp.data, qp.length, qp.rssi, qp.snr, display);
    }
    
    // Log to SD card if available
    if (packetLogger.isAvailable()) {
        PacketLogRecord record;
        record.timestampMs = qp.timestamp;
        record.nodeId = info.nodeId;
        record.protocol = info.protocol;
        record.frequencyMHz = qp.frequencyMHz;
        record.configIndex = qp.configIndex;
        record.rssiDbm = qp.rssi;
        record.snrDb = qp.snr;
        record.lengthBytes = qp.length;
        record.packetType = positionExtracted ? "position" : (info.hasMessage ? "text" : (info.isRouter ? "routing" : "unknown"));
        record.encrypted = !info.hasMessage;  // False when successfully decrypted (now cleartext)
        record.pskResult = info.hasMessage ? "hit" : "none";
        record.pskId = nullptr;
        record.hasPosition = positionExtracted && loggedPoint;
        if (record.hasPosition) {
            record.latitudeDeg = loggedPoint->latitude;
            record.longitudeDeg = loggedPoint->longitude;
            record.altitudeM = loggedPoint->altitude;
        }
        record.hopCount = -1;
        record.isRouter = info.isRouter;
        record.powerClass = static_cast<int>(info.powerClass);
        packetLogger.logPacket(record, qp.data, qp.length);
    }
    
    // Emit packet event if callback is registered
    if (packetCallback && info.nodeId != 0) {
        PacketEvent evt;
        evt.nodeId = info.nodeId;
        evt.protocol = info.protocol;
        evt.rssi = qp.rssi;
        evt.snr = qp.snr;
        evt.length = qp.length;
        evt.message = info.hasMessage ? info.message : nullptr;
        evt.timestamp = qp.timestamp;
        
        packetCallback(evt);
    }
}

// Handle packet in reconnaissance mode
void PacketProcessor::handleReconPacket(const PacketInfo& info, const uint8_t* data, size_t length, 
                                        float rssi, float snr, OLEDDisplay* display) {
    Serial.printf("[RECON] Packet #%d: %s, %d bytes, %.1f dBm, %.1f dB SNR\n",
                  reconState.scanState.totalPackets, info.protocol, length, rssi, snr);
    
    // Update display with packet info
    if (display && display->isOn()) {
        display->showPacketReceived(rssi, snr, info.protocol, info.nodeId);
    }
    
    // Track RF activity (for situational awareness)
    reconState.updateRFActivity(reconState.scanState.currentConfig, rssi);
    
    // Try decryption and capture packet for replay (same as targeted mode)
    if (length >= 20) {
        const uint8_t* payload = data;
        size_t payloadLen = length;
        
        // Look for Meshtastic header if not at start
        if (data[0] != 0xFF && length >= 16) {
            for (size_t i = 0; i < min(length - 4, size_t(20)); i++) {
                if (data[i] == 0xFF && data[i+1] == 0xFF && data[i+2] == 0xFF && data[i+3] == 0xFF) {
                    payload = data + i;
                    payloadLen = length - i;
                    break;
                }
            }
        }
        
        // Attempt decryption
        bool decrypted = PSKDecryption::testDefaultPSKs(payload, payloadLen);
        
        // Extract node ID from packet header if it's a Meshtastic packet (starts with 0xFF 0xFF 0xFF 0xFF)
        uint32_t nodeId = 0;
        if (payloadLen >= 16 && payload[0] == 0xFF && payload[1] == 0xFF && 
            payload[2] == 0xFF && payload[3] == 0xFF) {
            nodeId = ((uint32_t)payload[4]) | ((uint32_t)payload[5] << 8) |
                     ((uint32_t)payload[6] << 16) | ((uint32_t)payload[7] << 24);
        }
        
        // Auto-capture packet for replay with decrypted text and node ID
        const char* decryptedText = PSKDecryption::getLastMessage();
        if (reconState.capturePacketForReplay(data, length, reconState.scanState.currentConfig, 
                                               rssi, info.protocol, decryptedText, nodeId)) {
            if (decryptedText && decryptedText[0] != '\0') {
                Serial.printf("   ✅ Packet auto-captured with text: \"%s\"\n", decryptedText);
            } else {
                Serial.println("   ✅ Packet auto-captured");
            }
        }
    }
}

// Handle packet in targeted capture mode
void PacketProcessor::handleTargetedPacket(const PacketInfo& info, const uint8_t* data, size_t length, 
                                           float rssi, float snr, OLEDDisplay* display) {
    // Show ALL packets with full decryption to find text messages
    if (length < 40) {
        Serial.printf("\n[SMALL %d bytes] ", length);
    } else {
        Serial.printf("\n🎯 [CAPTURE] Packet #%d: %s, %d bytes, %.1f dBm, %.1f dB SNR\n",
                      reconState.scanState.totalPackets, info.protocol, length, rssi, snr);
    }
    
    // Update display silently
    if (display && display->isOn()) {
        display->showPacketReceived(rssi, snr, info.protocol, info.nodeId);
    }
    
    // Track RF activity in targeted mode too
    reconState.updateRFActivity(reconState.scanState.currentConfig, rssi);
    
    // Try decryption on ALL packets to find text messages
    if (length >= 20) {
        if (length < 40) {
            Serial.printf("Decrypting... ");
        } else {
            Serial.printf("[PSK] Analyzing %d-byte packet (text message size)\n", length);
        }
        
        // For "Unknown" packets that might be routed Meshtastic, try to find the header
        const uint8_t* payload = data;
        size_t payloadLen = length;
        
        // If it doesn't start with FF FF FF FF, it might be a routed packet
        // Try to locate Meshtastic header within the packet
        if (data[0] != 0xFF && length >= 16) {
            // Look for 0xFF 0xFF 0xFF 0xFF pattern in first 20 bytes
            for (size_t i = 0; i < min(length - 4, size_t(20)); i++) {
                if (data[i] == 0xFF && data[i+1] == 0xFF && data[i+2] == 0xFF && data[i+3] == 0xFF) {
                    payload = data + i;
                    payloadLen = length - i;
                    Serial.printf("[ROUTED] Found Meshtastic header at offset %d in %d-byte packet\n", i, length);
                    break;
                }
            }
        }
        
        // Attempt decryption
        bool decrypted = PSKDecryption::testDefaultPSKs(payload, payloadLen);
        
        // Extract node ID from packet header if it's a Meshtastic packet (starts with 0xFF 0xFF 0xFF 0xFF)
        uint32_t nodeId = 0;
        if (payloadLen >= 16 && payload[0] == 0xFF && payload[1] == 0xFF && 
            payload[2] == 0xFF && payload[3] == 0xFF) {
            // Node ID is at bytes 4-7 (little-endian)
            nodeId = ((uint32_t)payload[4]) | ((uint32_t)payload[5] << 8) |
                     ((uint32_t)payload[6] << 16) | ((uint32_t)payload[7] << 24);
        }
        
        // Auto-capture packet for replay with decrypted text and node ID
        const char* decryptedText = PSKDecryption::getLastMessage();
        if (reconState.capturePacketForReplay(data, length, reconState.scanState.currentConfig, 
                                               rssi, info.protocol, decryptedText, nodeId)) {
            if (decrypted && decryptedText && decryptedText[0] != '\0') {
                Serial.printf("   ✅ Packet auto-captured with text: \"%s\"\n", decryptedText);
            } else {
                Serial.println("   ✅ Packet auto-captured (encrypted)");
            }
        }
        
        if (!decrypted && length < 40) {
            Serial.println("(no readable content found)");
        }
    }
}
