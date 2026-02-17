/**
 * Packet Processor Implementation
 */

#include "packet_processor.h"
#include "packet_logger.h"
#include "recon_state.h"
#include "oled_display.h"
#include "text_packet_diagnostic.h"
#include "psk_decryption_simple.h"
#include "lorawan_keys.h"
#include "config.h"
#include "logger.h"

// Constructor
PacketProcessor::PacketProcessor() : lastPacketLength(0) {
    memset(lastPacketData, 0, sizeof(lastPacketData));
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
        LOG_WARN("Queue full (100 packets) - dropping! Total drops: %u", 
                 reconState.scanState.droppedPackets);
        return false;
    }
    
    // Validate packet length to prevent buffer overflow
    if (length > Config::PacketProcessing::MAX_PACKET_SIZE) {
        LOG_WARN("Rejecting oversized packet (%d bytes > %d max)", 
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
        LOG_DEBUG("%d packets buffered", packetQueue.size());
    }
    
    while (!packetQueue.empty()) {
        QueuedPacket qp = packetQueue.front();
        packetQueue.pop();
        processSinglePacket(qp, display);
    }
}

// Process a single packet
void PacketProcessor::processSinglePacket(const QueuedPacket& qp, OLEDDisplay* display) {
    // Store for potential replay capture (static buffer — no heap allocation)
    if (qp.length <= Config::PacketProcessing::MAX_PACKET_SIZE) {
        memcpy(lastPacketData, qp.data, qp.length);
        lastPacketLength = qp.length;

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
                                      qp.rssi, info.protocol, qp.data, qp.length, info.hopCount);
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

// Find Meshtastic header (FF FF FF FF) in packet and extract all fields
PacketProcessor::MeshtasticHeader PacketProcessor::findAndExtractMeshtasticHeader(const uint8_t* data, size_t length) {
    MeshtasticHeader hdr = {};
    hdr.payload = data;
    hdr.payloadLen = length;
    hdr.destId = 0xFFFFFFFF;
    hdr.found = false;

    // Try to locate Meshtastic header if not at start (routed packets)
    if (data[0] != 0xFF && length >= 16) {
        size_t searchLimit = (length >= 4) ? min(length - 4, size_t(20)) : 0;
        for (size_t i = 0; i < searchLimit; i++) {
            if (data[i] == 0xFF && data[i+1] == 0xFF && data[i+2] == 0xFF && data[i+3] == 0xFF) {
                hdr.payload = data + i;
                hdr.payloadLen = length - i;
                LOG_DEBUG("Found Meshtastic header at offset %d in %d-byte packet", i, length);
                break;
            }
        }
    }

    // Extract fields if we have a valid Meshtastic header
    if (hdr.payloadLen >= 16 && hdr.payload[0] == 0xFF && hdr.payload[1] == 0xFF &&
        hdr.payload[2] == 0xFF && hdr.payload[3] == 0xFF) {
        hdr.found = true;
        // Destination ID at bytes 0-3 (little-endian)
        hdr.destId = ((uint32_t)hdr.payload[0]) | ((uint32_t)hdr.payload[1] << 8) |
                     ((uint32_t)hdr.payload[2] << 16) | ((uint32_t)hdr.payload[3] << 24);
        // Source/From ID at bytes 4-7 (little-endian)
        hdr.nodeId = ((uint32_t)hdr.payload[4]) | ((uint32_t)hdr.payload[5] << 8) |
                     ((uint32_t)hdr.payload[6] << 16) | ((uint32_t)hdr.payload[7] << 24);
        // Packet ID at offset 8-11 (little-endian)
        if (hdr.payloadLen >= 12) {
            hdr.packetId = ((uint32_t)hdr.payload[8]) | ((uint32_t)hdr.payload[9] << 8) |
                           ((uint32_t)hdr.payload[10] << 16) | ((uint32_t)hdr.payload[11] << 24);
        }
        // Flags at byte 12
        if (hdr.payloadLen >= 13) {
            uint8_t flags = hdr.payload[12];
            hdr.hopCount = flags & 0x07;           // Bits 0-2: hop count
            hdr.wantAck = (flags >> 3) & 0x01;     // Bit 3: want acknowledgment
            hdr.viaMqtt = (flags >> 4) & 0x01;     // Bit 4: via MQTT gateway
            hdr.priority = (flags >> 5) & 0x03;    // Bits 5-6: priority (0-3)
        }
        // Channel at byte 13
        if (hdr.payloadLen >= 14) {
            hdr.channel = hdr.payload[13];
        }
    }

    return hdr;
}

// Common: attempt PSK decryption and auto-capture for replay
void PacketProcessor::tryDecryptAndCapture(const uint8_t* data, size_t length, float rssi, float snr,
                                            const char* protocol, const MeshtasticHeader& hdr) {
    // Attempt decryption
    bool decrypted = PSKDecryption::testDefaultPSKs(hdr.payload, hdr.payloadLen);

    // Auto-capture packet for replay with all header fields
    const char* decryptedText = PSKDecryption::getLastMessage();
    if (reconState.capturePacketForReplay(data, length, reconState.scanState.currentConfig,
                                           rssi, snr, protocol, decryptedText,
                                           hdr.nodeId, hdr.packetId, hdr.hopCount,
                                           hdr.destId, hdr.channel, hdr.wantAck, hdr.viaMqtt, hdr.priority)) {
        if (decrypted && decryptedText && decryptedText[0] != '\0') {
            Serial.printf("   ✅ Packet auto-captured with text: \"%s\"\n", decryptedText);
        } else if (decrypted) {
            Serial.println("   ✅ Packet auto-captured");
        } else {
            Serial.println("   ✅ Packet auto-captured (encrypted)");
        }
    }
}

// Handle packet in reconnaissance mode
void PacketProcessor::handleReconPacket(const PacketInfo& info, const uint8_t* data, size_t length,
                                        float rssi, float snr, OLEDDisplay* display) {
    LOG_INFO("Packet #%d: %s, 0x%08X, %d bytes, %.1f dBm, %.1f dB SNR",
             reconState.scanState.totalPackets, info.protocol, info.nodeId, length, rssi, snr);

    // Update display with packet info
    if (display && display->isOn()) {
        display->showPacketReceived(rssi, snr, info.protocol, info.nodeId);
    }

    // Track RF activity (for situational awareness)
    reconState.updateRFActivity(reconState.scanState.currentConfig, rssi);

    // Protocol-specific key testing
    if (strcmp(info.protocol, "LoRaWAN") == 0) {
        LoRaWANKeys::analyzePacket(data, length);
    }

    // Try decryption and capture packet for replay
    if (length >= 20) {
        MeshtasticHeader hdr = findAndExtractMeshtasticHeader(data, length);
        tryDecryptAndCapture(data, length, rssi, snr, info.protocol, hdr);
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
        if (length >= 40) {
            LOG_DEBUG("Analyzing %d-byte packet (text message size)", length);
        }

        MeshtasticHeader hdr = findAndExtractMeshtasticHeader(data, length);
        tryDecryptAndCapture(data, length, rssi, snr, info.protocol, hdr);

        // Check if decryption failed on small packets
        const char* decryptedText = PSKDecryption::getLastMessage();
        if ((!decryptedText || decryptedText[0] == '\0') && length < 40) {
            Serial.println("(no readable content found)");
        }
    }
}
