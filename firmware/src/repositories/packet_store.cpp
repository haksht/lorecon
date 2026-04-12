#include "packet_store.h"
#include "../logger.h"

// Static members
CapturedPacket PacketStore::emptyPacket_;
bool PacketStore::emptyInitialized_ = false;

PacketStore::PacketStore() : numCaptured_(0), writeIndex_(0) {
    memset(slots_, 0, sizeof(slots_));
    initializeEmptyPacket();
}

void PacketStore::initializeEmptyPacket() {
    if (!emptyInitialized_) {
        memset(&emptyPacket_, 0, sizeof(emptyPacket_));
        emptyInitialized_ = true;
    }
}

bool PacketStore::capturePacket(const uint8_t* data, size_t length,
                                 uint8_t configIndex, int16_t rssi, float snr,
                                 uint32_t nodeId, uint32_t packetId,
                                 uint8_t hopCount, uint32_t destId,
                                 uint8_t channel, bool wantAck,
                                 bool viaMqtt, uint8_t priority,
                                 const char* protocol,
                                 const char* decryptedText,
                                 const char* meshCoreChannel) {
    if (data == nullptr || length == 0) {
        LOG_WARN("[PacketStore] Cannot capture - invalid data");
        return false;
    }
    
    // Deduplicate: if we already have this packetId, record relay metadata
    // instead of dropping the duplicate entirely
    if (packetId != 0) {
        for (uint8_t i = 0; i < numCaptured_; i++) {
            if (slots_[i].valid && slots_[i].packetId == packetId) {
                // Extract relay byte (byte 15 of Meshtastic header) from raw data
                uint8_t relayByte = (length >= 16) ? data[15] : 0;
                if (slots_[i].addRelaySighting(relayByte, rssi, snr, hopCount)) {
                    LOG_INFO("[PacketStore] Relay sighting #%d for 0x%08X (relay=0x%02X, rssi=%.0f, hop=%d)",
                             slots_[i].relayCount, packetId, relayByte, (float)rssi, hopCount);
                } else {
                    LOG_INFO("[PacketStore] Relay sightings full for 0x%08X (max %d)",
                             packetId, CapturedPacket::MAX_RELAY_SIGHTINGS);
                }
                return false;
            }
        }
    }
    
    // Clamp length to max packet size
    if (length > sizeof(slots_[0].data)) {
        LOG_WARN("[PacketStore] Truncating packet from %d to %d bytes",
                 length, sizeof(slots_[0].data));
        length = sizeof(slots_[0].data);
    }
    
    // Circular buffer: write to current position, then advance
    // When full, this overwrites the oldest packet
    CapturedPacket& slot = slots_[writeIndex_];
    
    // Copy packet data
    memcpy(slot.data, data, length);
    slot.length = length;
    slot.configIndex = configIndex;
    slot.originalRSSI = rssi;
    slot.snr = snr;
    slot.captureTime = millis();
    slot.nodeId = nodeId;
    slot.destId = destId;
    slot.packetId = packetId;
    slot.hopCount = hopCount;
    slot.channel = channel;
    slot.wantAck = wantAck;
    slot.viaMqtt = viaMqtt;
    slot.priority = priority;
    
    // Copy protocol string safely
    if (protocol != nullptr) {
        strncpy(slot.protocol, protocol, sizeof(slot.protocol) - 1);
        slot.protocol[sizeof(slot.protocol) - 1] = '\0';
    } else {
        slot.protocol[0] = '\0';
    }
    
    // Copy decrypted text if available
    if (decryptedText != nullptr && decryptedText[0] != '\0') {
        strncpy(slot.decryptedText, decryptedText, sizeof(slot.decryptedText) - 1);
        slot.decryptedText[sizeof(slot.decryptedText) - 1] = '\0';
    } else {
        slot.decryptedText[0] = '\0';
    }

    // Copy MeshCore channel name if available ("public", "#general", etc.)
    if (meshCoreChannel != nullptr && meshCoreChannel[0] != '\0') {
        strncpy(slot.meshCoreChannel, meshCoreChannel, sizeof(slot.meshCoreChannel) - 1);
        slot.meshCoreChannel[sizeof(slot.meshCoreChannel) - 1] = '\0';
    } else {
        slot.meshCoreChannel[0] = '\0';
    }
    
    slot.relayCount = 0;
    slot.valid = true;
    
    // Log with info about overwrite if buffer was full
    if (numCaptured_ >= MAX_SLOTS) {
        LOG_INFO("[PacketStore] Packet captured to slot #%d (replaced oldest, %d bytes, %s)",
                 writeIndex_ + 1, length, protocol ? protocol : "unknown");
    } else {
        numCaptured_++;
        LOG_INFO("[PacketStore] Packet captured to slot #%d (%d bytes, %s)",
                 numCaptured_, length, protocol ? protocol : "unknown");
    }
    
    // Advance write index circularly
    writeIndex_ = (writeIndex_ + 1) % MAX_SLOTS;
    
    return true;
}

const CapturedPacket& PacketStore::getPacket(uint8_t index) const {
    if (index >= numCaptured_) {
        return emptyPacket_;
    }
    return slots_[index];
}

void PacketStore::clear() {
    numCaptured_ = 0;
    writeIndex_ = 0;
    memset(slots_, 0, sizeof(slots_));
    LOG_INFO("[PacketStore] All replay slots cleared");
}
