/**
 * Packet Processor Implementation
 */

#include "packet_processor.h"
#include "gps_controller.h"
#include "packet_logger.h"
#include "recon_state.h"
#include "oled_display.h"
#include "text_packet_diagnostic.h"
#include "psk_decryption_simple.h"
#include "meshcore_decryption.h"
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
                 reconState.scanState.droppedPackets.load());
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
    // Store for potential replay capture (static buffer  -  no heap allocation)
    if (qp.length <= Config::PacketProcessing::MAX_PACKET_SIZE) {
        memcpy(lastPacketData, qp.data, qp.length);
        lastPacketLength = qp.length;

        // Also update recon state (locked  -  read by serial command handler)
        {
            ReconState::ScopedLock lock(reconState);
            memcpy(reconState.scanState.lastPacket, qp.data, qp.length);
            reconState.scanState.lastPacketLength = qp.length;
        }
    }
    reconState.scanState.totalPackets++;
    
    // Analyze raw packet for diagnostics (timing, encryption detection)
    TextPacketDiagnostic::analyzePacket(qp.data, qp.length, qp.rssi, qp.snr);
    
    // Analyze packet using ProtocolAnalyzer.
    // Pass the sync word from the active scan config so identifyProtocol() can detect
    // unicast Meshtastic packets on Meshtastic-only configs (0x2B, 0x48).
    PacketInfo info = protocolAnalyzer.analyze(qp.data, qp.length, qp.rssi,
        reconState.getScanConfig(qp.configIndex).syncWord);
    
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
    
    // Process packet (same pipeline regardless of mode)
    handlePacket(info, qp.data, qp.length, qp.rssi, qp.snr, display);
    
    // Capture sniffer GPS fix once  -  used for both SD log and PacketEvent below.
    // On boards without GPS (Heltec V3, T3-S3) this is always false.
    bool snifferHasGPS = false;
    double snifferLat = 0.0, snifferLon = 0.0, snifferAlt = 0.0;
#ifdef HAS_GPS
    if (g_gpsController && g_gpsController->hasGoodFix()) {
        snifferHasGPS = true;
        snifferLat = g_gpsController->getLatitude();
        snifferLon = g_gpsController->getLongitude();
        snifferAlt = g_gpsController->getAltitude();
    }
#endif

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
            // Use GPS position extracted from the Meshtastic packet payload
            record.latitudeDeg = loggedPoint->latitude;
            record.longitudeDeg = loggedPoint->longitude;
            record.altitudeM = loggedPoint->altitude;
        } else if (snifferHasGPS) {
            // Fall back to sniffer's own GPS: where WE were when we received this packet
            record.hasPosition = true;
            record.latitudeDeg = snifferLat;
            record.longitudeDeg = snifferLon;
            record.altitudeM = snifferAlt;
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
        evt.lat = snifferLat;
        evt.lon = snifferLon;
        evt.alt = snifferAlt;
        evt.hasPosition = snifferHasGPS;

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

    // Auto-capture packet for replay with all header fields.
    // Use getLastMessageSafe() to copy under mutex  -  getLastMessage() returns a raw
    // pointer with no lock, creating a race with setLastMessage() on another core.
    char decryptedTextBuf[256];
    PSKDecryption::getLastMessageSafe(decryptedTextBuf, sizeof(decryptedTextBuf));
    const char* decryptedText = decryptedTextBuf;
    if (reconState.capturePacketForReplay(data, length, reconState.scanState.currentConfig,
                                           rssi, snr, protocol, decryptedText,
                                           hdr.nodeId, hdr.packetId, hdr.hopCount,
                                           hdr.destId, hdr.channel, hdr.wantAck, hdr.viaMqtt, hdr.priority)) {
        if (decrypted && decryptedText[0] != '\0') {
            Serial.printf("   [OK] Packet auto-captured with text: \"%s\"\n", decryptedText);
        } else if (decrypted) {
            Serial.println("   [OK] Packet auto-captured");
        } else {
            Serial.println("   [OK] Packet auto-captured (encrypted)");
        }
    }
}

// Handle a captured packet (same pipeline for recon and targeted modes)
void PacketProcessor::handlePacket(PacketInfo& info, const uint8_t* data, size_t length,
                                   float rssi, float snr, OLEDDisplay* display) {
    const char* modeTag = (reconState.scanState.mode == MODE_TARGETED_CAPTURE) ? "CAPTURE" : "RECON";
    Serial.printf("\n[%s] Packet #%d: %s, 0x%08X, %d bytes, %.1f dBm, %.1f dB SNR\n",
                  modeTag, reconState.scanState.totalPackets.load(), info.protocol, info.nodeId, length, rssi, snr);

    // Update display with packet info
    if (display && display->isOn()) {
        display->showPacketReceived(rssi, snr, info.protocol, info.nodeId);
    }

    // Track RF activity
    reconState.updateRFActivity(reconState.scanState.currentConfig, rssi);

    // Protocol-specific key testing
    if (strcmp(info.protocol, "LoRaWAN") == 0) {
        LoRaWANKeys::analyzePacket(data, length);
    }

    // Try decryption and capture packet for replay
    if (strcmp(info.protocol, "MeshCore") == 0) {
        // MeshCore: AES-128-ECB with known public channel / hashtag room keys
        bool decrypted = MeshCoreDecryption::tryDecrypt(data, length);
        char decryptedTextBuf[256] = {0};
        if (decrypted) {
            MeshCoreDecryption::getLastMessageSafe(decryptedTextBuf, sizeof(decryptedTextBuf));
            Serial.printf("   [MeshCore] Decrypted: \"%s\"\n", decryptedTextBuf);
        }
        reconState.capturePacketForReplay(data, length, reconState.scanState.currentConfig,
                                           rssi, snr, "MeshCore", decryptedTextBuf,
                                           info.nodeId, 0, info.hopCount, 0, 0, false, false, 0);
    } else if (length >= 20) {
        MeshtasticHeader hdr = findAndExtractMeshtasticHeader(data, length);
        tryDecryptAndCapture(data, length, rssi, snr, info.protocol, hdr);
    }
}
