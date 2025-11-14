/**
 * Packet Processor Implementation
 */

#include "packet_processor.h"
#include "packet_logger.h"
#include "recon_state.h"
#include "oled_display.h"
#include "text_packet_diagnostic.h"
#include "config.h"

#ifdef ENABLE_PSK_TESTING
#include "psk_decryption_simple.h"
#endif

// Constructor
PacketProcessor::PacketProcessor() {
    lastPacketData.reserve(Config::PacketProcessing::MAX_PACKET_SIZE);
}

// Queue a packet for processing
bool PacketProcessor::queuePacket(const uint8_t* data, size_t length, float rssi, float snr) {
    if (isQueueFull()) {
        Serial.println("[QUEUE] Full - dropping packet!");
        return false;
    }
    
    QueuedPacket qp;
    memcpy(qp.data, data, length);
    qp.length = length;
    qp.rssi = rssi;
    qp.snr = snr;
    qp.timestamp = millis();
    packetQueue.push(qp);
    
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
    
    // Try PSK decryption and store message in PacketInfo if successful
    #ifdef ENABLE_PSK_TESTING
    const char* decryptedMsg = PSKDecryption::getLastMessage();
    if (decryptedMsg && strlen(decryptedMsg) > 0) {
        strncpy(info.message, decryptedMsg, sizeof(info.message) - 1);
        info.message[sizeof(info.message) - 1] = '\0';
        info.hasMessage = true;
        PSKDecryption::clearLastMessage();
    }
    #endif
    
    // Enhanced packet analysis for Meshtastic (extract GPS position silently)
    if (strcmp(info.protocol, "Meshtastic") == 0) {
        geoIntel.extractPosition(qp.data, qp.length, info.nodeId);
    }
    
    // Track as targetable device in ALL modes if we have a real node ID
    if (info.nodeId != 0) {
        reconState.addTargetableDevice(info.nodeId, reconState.scanState.currentConfig, 
                                      qp.rssi, info.protocol, qp.data, qp.length);
        reconState.updateNode(info.nodeId, info.protocol, qp.rssi);
    }
    
    // Mode-specific handling
    if (reconState.scanState.mode == MODE_RECONNAISSANCE) {
        handleReconPacket(info, qp.data, qp.length, qp.rssi, qp.snr, display);
    } else if (reconState.scanState.mode == MODE_TARGETED_CAPTURE) {
        handleTargetedPacket(info, qp.data, qp.length, qp.rssi, qp.snr, display);
    }
    
    // Log to SD card if available
    if (packetLogger.isAvailable()) {
        packetLogger.logPacket(qp.data, qp.length, qp.rssi, qp.snr, info.protocol, info.nodeId);
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
    
    #ifdef ENABLE_PSK_TESTING
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
        
        if (!decrypted && length < 40) {
            Serial.println("(no readable content found)");
        }
    }
    #endif
}
