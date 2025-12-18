/**
 * Format Utilities - Consistent formatting across the codebase
 * 
 * Ensures node IDs, timestamps, and other values are displayed
 * consistently in serial output, web UI, and JSON responses.
 */

#ifndef FORMAT_UTILS_H
#define FORMAT_UTILS_H

#include <Arduino.h>

namespace FormatUtils {

/**
 * Format a 32-bit node ID as uppercase hex with 0x prefix
 * 
 * Output format: "0x3EC288" (no leading zeros, uppercase)
 * This is the canonical format used throughout the codebase.
 * 
 * @param nodeId The node ID to format
 * @return String in format "0xXXXXXX" or "0xXXXXXXXX"
 */
inline String formatNodeId(uint32_t nodeId) {
    char buf[12];
    snprintf(buf, sizeof(buf), "0x%X", nodeId);
    return String(buf);
}

/**
 * Format a node ID for JSON (without 0x prefix, uppercase)
 * 
 * Output format: "3EC288" (for JSON nodeId fields)
 * Frontend can add prefix if needed.
 * 
 * @param nodeId The node ID to format
 * @return String in format "XXXXXX" or "XXXXXXXX"
 */
inline String formatNodeIdJson(uint32_t nodeId) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%X", nodeId);
    return String(buf);
}

/**
 * Format a node ID with full 8-digit padding (for alignment in tables)
 * 
 * Output format: "0x003EC288" (always 8 hex digits)
 * 
 * @param nodeId The node ID to format
 * @return String in format "0xXXXXXXXX"
 */
inline String formatNodeIdPadded(uint32_t nodeId) {
    char buf[12];
    snprintf(buf, sizeof(buf), "0x%08X", nodeId);
    return String(buf);
}

/**
 * Estimate power class based on RSSI
 * 
 * Used consistently for device classification.
 * 
 * @param rssi Signal strength in dBm
 * @return 0=low (<10mW), 1=medium (10-100mW), 2=high (>100mW)
 */
inline uint8_t estimatePowerClass(float rssi) {
    if (rssi > -70) return 2;  // High power (>100mW)
    if (rssi > -90) return 1;  // Medium power (10-100mW)
    return 0;                  // Low power (<10mW)
}

/**
 * Format duration in human-readable form
 * 
 * @param seconds Duration in seconds
 * @return String like "5s", "2m 30s", "1h 5m"
 */
inline String formatDuration(uint32_t seconds) {
    if (seconds < 60) {
        return String(seconds) + "s";
    } else if (seconds < 3600) {
        uint32_t mins = seconds / 60;
        uint32_t secs = seconds % 60;
        return String(mins) + "m " + String(secs) + "s";
    } else {
        uint32_t hours = seconds / 3600;
        uint32_t mins = (seconds % 3600) / 60;
        return String(hours) + "h " + String(mins) + "m";
    }
}

} // namespace FormatUtils

#endif // FORMAT_UTILS_H
