#include "packet_store.h"
#include "../logger.h"

// Static members
CapturedPacket PacketStore::emptyPacket_;
bool PacketStore::emptyInitialized_ = false;

PacketStore::PacketStore() : numCaptured_(0) {
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
                                 uint8_t configIndex, int16_t rssi,
                                 uint32_t nodeId, uint32_t packetId,
                                 const char* protocol,
                                 const char* decryptedText) {
    if (isFull()) {
        LOG_WARN("PacketStore", "Cannot capture - store full (%d/%d slots)", 
                 numCaptured_, MAX_SLOTS);
        return false;
    }
    
    if (data == nullptr || length == 0) {
        LOG_WARN("PacketStore", "Cannot capture - invalid data");
        return false;
    }
    
    // Clamp length to max packet size
    if (length > sizeof(slots_[0].data)) {
        LOG_WARN("PacketStore", "Truncating packet from %d to %d bytes",
                 length, sizeof(slots_[0].data));
        length = sizeof(slots_[0].data);
    }
    
    CapturedPacket& slot = slots_[numCaptured_];
    
    // Copy packet data
    memcpy(slot.data, data, length);
    slot.length = length;
    slot.configIndex = configIndex;
    slot.originalRSSI = rssi;
    slot.captureTime = millis();
    slot.nodeId = nodeId;
    slot.packetId = packetId;
    
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
    
    slot.valid = true;
    numCaptured_++;
    
    LOG_INFO("PacketStore", "Packet captured to slot #%d (%d bytes, %s)",
             numCaptured_, length, protocol ? protocol : "unknown");
    
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
    memset(slots_, 0, sizeof(slots_));
    LOG_INFO("PacketStore", "All replay slots cleared");
}
