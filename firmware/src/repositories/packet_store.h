#ifndef PACKET_STORE_H
#define PACKET_STORE_H

#include <cstdint>
#include <cstring>
#include "../data_structures.h"

/**
 * PacketStore - Repository for captured packets (replay slots)
 * 
 * Encapsulates the fixed-size circular buffer of captured packets
 * that can be replayed for testing or analysis.
 * 
 * Thread Safety: NOT thread-safe. Caller must ensure single-thread access
 * or provide external synchronization.
 */
class PacketStore {
public:
    static constexpr size_t MAX_SLOTS = 10;  // From Config::Replay::MAX_PACKETS
    
    PacketStore();
    
    /**
     * Capture a packet to the replay store
     * @return true if captured, false if store is full
     */
    bool capturePacket(const uint8_t* data, size_t length,
                       uint8_t configIndex, int16_t rssi,
                       uint32_t nodeId, uint32_t packetId,
                       uint8_t hopCount, uint32_t destId = 0xFFFFFFFF,
                       uint8_t channel = 0, bool wantAck = false,
                       bool viaMqtt = false, uint8_t priority = 0,
                       const char* protocol = nullptr,
                       const char* decryptedText = nullptr);
    
    /**
     * Get a captured packet by slot index
     * @param index Slot index (0 to count()-1)
     * @return Reference to packet, or empty packet if index invalid
     */
    const CapturedPacket& getPacket(uint8_t index) const;
    
    /**
     * Clear all replay slots
     */
    void clear();
    
    /**
     * Get current number of captured packets
     */
    uint8_t count() const { return numCaptured_; }
    
    /**
     * Get maximum capacity
     */
    constexpr size_t capacity() const { return MAX_SLOTS; }
    
    /**
     * Check if store is full
     */
    bool isFull() const { return numCaptured_ >= MAX_SLOTS; }
    
    /**
     * Check if store is empty
     */
    bool isEmpty() const { return numCaptured_ == 0; }
    
    // Iterator support for range-based for loops
    const CapturedPacket* begin() const { return slots_; }
    const CapturedPacket* end() const { return slots_ + numCaptured_; }

private:
    CapturedPacket slots_[MAX_SLOTS];
    uint8_t numCaptured_;
    
    // Empty packet returned for invalid index access
    static CapturedPacket emptyPacket_;
    static bool emptyInitialized_;
    
    void initializeEmptyPacket();
};

#endif // PACKET_STORE_H
