#ifndef NODE_TRACKER_H
#define NODE_TRACKER_H

#include <cstdint>
#include <cstring>
#include "../data_structures.h"
#include "../config.h"

/**
 * NodeTracker - Repository for tracked "hot" nodes
 * 
 * Maintains a list of recently active nodes with RSSI statistics
 * for behavioral analysis. Tracks protocol association and activity.
 * 
 * Thread Safety: NOT thread-safe. Caller must ensure single-thread access.
 */
class NodeTracker {
public:
    static constexpr size_t MAX_NODES = Config::Tracking::MAX_HOT_NODES;
    
    NodeTracker();
    
    /**
     * Update or add a node with new observation
     * @param nodeId Unique node identifier
     * @param protocol Protocol name (e.g., "Meshtastic", "LoRaWAN")
     * @param rssi Signal strength in dBm
     */
    void updateNode(uint32_t nodeId, const char* protocol, float rssi);
    
    /**
     * Find a node by ID
     * @return Pointer to node, or nullptr if not found
     */
    TrackedNode* findNode(uint32_t nodeId);
    const TrackedNode* findNode(uint32_t nodeId) const;
    
    /**
     * Get node by index
     * @param index Index into nodes array (0 to count()-1)
     * @return Pointer to node, or nullptr if invalid index
     */
    const TrackedNode* getByIndex(uint8_t index) const;
    
    /**
     * Clear all tracked nodes
     */
    void clear();
    
    /**
     * Get current number of tracked nodes
     */
    uint8_t count() const { return nodeCount_; }
    
    /**
     * Get maximum capacity
     */
    constexpr size_t capacity() const { return MAX_NODES; }
    
    /**
     * Check if tracker is full
     */
    bool isFull() const { return nodeCount_ >= MAX_NODES; }
    
    /**
     * Check if tracker is empty
     */
    bool isEmpty() const { return nodeCount_ == 0; }
    
    // Iterator support for range-based for loops
    const TrackedNode* begin() const { return nodes_; }
    const TrackedNode* end() const { return nodes_ + nodeCount_; }
    TrackedNode* begin() { return nodes_; }
    TrackedNode* end() { return nodes_ + nodeCount_; }

private:
    TrackedNode nodes_[MAX_NODES];
    uint8_t nodeCount_;
    
    TrackedNode* findNodeInternal(uint32_t nodeId);
    void initializeNode(TrackedNode* node, uint32_t nodeId, 
                        const char* protocol, float rssi);
};

#endif // NODE_TRACKER_H
