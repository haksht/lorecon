/**
 * Security Scorer - Unified device security assessment
 * 
 * Provides consistent security scoring across UI and API.
 * Eliminates duplicate implementations and ensures identical
 * scoring logic everywhere.
 */

#ifndef SECURITY_SCORER_H
#define SECURITY_SCORER_H

#include <Arduino.h>
#include "../data_structures.h"

namespace SecurityScorer {

/**
 * Security assessment result
 */
struct Assessment {
    uint8_t score;           // 0-100 (100 = secure)
    const char* rating;      // "secure", "moderate", "vulnerable"
    const char* ratingEmoji; // "✅ SECURE", "⚠️ MODERATE", "🔴 VULNERABLE"
    
    // Individual finding flags
    bool physicalProximity;  // Signal too strong
    bool possibleUnencrypted;// Heavy traffic without confirmed encryption
    bool isRouter;           // Elevated attack surface
    bool chatty;             // High packet count
    bool intermittent;       // Low packet count
    bool outdatedFirmware;   // Old firmware version
};

/**
 * Assess security of a targetable device
 * 
 * @param device The device to assess
 * @return Assessment with score, rating, and findings
 */
inline Assessment assess(const TargetableDevice& device) {
    Assessment result = {};
    result.score = 100;
    
    // Check 1: Signal strength (too strong = physical proximity risk)
    if (device.bestRSSI > -50) {
        result.score = result.score > 15 ? result.score - 15 : 0;
        result.physicalProximity = true;
    }
    
    // Check 2: Encryption status (heavy traffic without confirmed encryption)
    bool maybeUnencrypted = (device.packetCount > 10 && strcmp(device.protocol, "Meshtastic") == 0);
    if (maybeUnencrypted) {
        result.possibleUnencrypted = true;
        // Note: doesn't affect score - need PSK test confirmation
    }
    
    // Check 3: Routing device (higher attack surface)
    if (device.isRouter) {
        result.score = result.score > 10 ? result.score - 10 : 0;
        result.isRouter = true;
    }
    
    // Check 4: High packet count (chatty = info leakage)
    if (device.packetCount > 100) {
        result.score = result.score > 15 ? result.score - 15 : 0;
        result.chatty = true;
    }
    
    // Check 5: Low packet count (intermittent = weak battery?)
    if (device.packetCount < 5) {
        result.score = result.score > 5 ? result.score - 5 : 0;
        result.intermittent = true;
    }
    
    // Check 6: Firmware version check
    if (strstr(device.firmwareVersion, "v1.x") != nullptr || 
        strstr(device.firmwareVersion, "v2.0") != nullptr) {
        result.score = result.score > 20 ? result.score - 20 : 0;
        result.outdatedFirmware = true;
    }
    
    // Determine rating
    if (result.score >= 80) {
        result.rating = "secure";
        result.ratingEmoji = "✅ SECURE";
    } else if (result.score >= 60) {
        result.rating = "moderate";
        result.ratingEmoji = "⚠️ MODERATE";
    } else {
        result.rating = "vulnerable";
        result.ratingEmoji = "🔴 VULNERABLE";
    }
    
    return result;
}

/**
 * Check if any findings were detected
 */
inline bool hasFindings(const Assessment& a) {
    return a.physicalProximity || a.possibleUnencrypted || a.isRouter ||
           a.chatty || a.intermittent || a.outdatedFirmware;
}

/**
 * Get finding count
 */
inline uint8_t getFindingCount(const Assessment& a) {
    uint8_t count = 0;
    if (a.physicalProximity) count++;
    if (a.possibleUnencrypted) count++;
    if (a.isRouter) count++;
    if (a.chatty) count++;
    if (a.intermittent) count++;
    if (a.outdatedFirmware) count++;
    return count;
}

} // namespace SecurityScorer

#endif // SECURITY_SCORER_H
