#include "node_tracker.h"
#include "../logger.h"

NodeTracker::NodeTracker() : nodeCount_(0) {
    memset(nodes_, 0, sizeof(nodes_));
}

void NodeTracker::updateNode(uint32_t nodeId, const char* protocol, float rssi) {
    if (nodeId == 0) {
        LOG_WARN("NodeTracker", "Ignoring node with ID 0");
        return;
    }
    
    TrackedNode* node = findNodeInternal(nodeId);
    
    if (!node) {
        // New node
        if (isFull()) {
            LOG_WARN("NodeTracker", "Cannot add node 0x%08X - tracker full (%d/%d)",
                     nodeId, nodeCount_, MAX_NODES);
            return;
        }
        
        node = &nodes_[nodeCount_++];
        initializeNode(node, nodeId, protocol, rssi);
        LOG_INFO("NodeTracker", "New node: 0x%08X (%s)", nodeId, protocol);
        return;
    }
    
    // Update existing node
    // Cap packetCount at UINT16_MAX to prevent overflow and division by zero
    if (node->packetCount < UINT16_MAX) {
        node->packetCount++;
    }
    
    // Running average of RSSI
    node->avgRSSI = (node->avgRSSI * (node->packetCount - 1) + rssi) / node->packetCount;
    
    if (rssi > node->bestRSSI) {
        node->bestRSSI = rssi;
    }
    
    node->lastSeen = millis();
    node->active = true;
}

TrackedNode* NodeTracker::findNodeInternal(uint32_t nodeId) {
    for (uint8_t i = 0; i < nodeCount_; i++) {
        if (nodes_[i].nodeId == nodeId) {
            return &nodes_[i];
        }
    }
    return nullptr;
}

TrackedNode* NodeTracker::findNode(uint32_t nodeId) {
    return findNodeInternal(nodeId);
}

const TrackedNode* NodeTracker::findNode(uint32_t nodeId) const {
    for (uint8_t i = 0; i < nodeCount_; i++) {
        if (nodes_[i].nodeId == nodeId) {
            return &nodes_[i];
        }
    }
    return nullptr;
}

const TrackedNode* NodeTracker::getByIndex(uint8_t index) const {
    if (index >= nodeCount_) {
        return nullptr;
    }
    return &nodes_[index];
}

void NodeTracker::clear() {
    nodeCount_ = 0;
    memset(nodes_, 0, sizeof(nodes_));
    LOG_INFO("NodeTracker", "All tracked nodes cleared");
}

void NodeTracker::initializeNode(TrackedNode* node, uint32_t nodeId,
                                  const char* protocol, float rssi) {
    node->nodeId = nodeId;
    node->protocol = protocol;  // String assignment
    node->packetCount = 1;
    node->avgRSSI = rssi;
    node->bestRSSI = rssi;
    node->firstSeen = millis();
    node->lastSeen = millis();
    node->active = true;
}
