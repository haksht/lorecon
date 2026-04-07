/**
 * Security Scorer - Unified device security assessment
 * 
 * Provides consistent security scoring across UI and API.
 * Eliminates duplicate implementations and ensures identical
 * scoring logic everywhere.
 * 
 * SCORING METHODOLOGY:
 * ====================
 * Devices start at 100 points (fully secure) and lose points for risk factors:
 * 
 * Risk Factor              | Points Lost | Rationale
 * -------------------------|-------------|------------------------------------------
 * Physical proximity       | -15         | RSSI > -50 dBm means device is very close
 *                          |             | (within ~5m), easier to intercept/attack
 * Router device            | -10         | Routers forward messages, larger attack
 *                          |             | surface, can be used to inject traffic
 * High packet count (>100) | -15         | Chatty devices leak more metadata,
 *                          |             | easier to fingerprint and track
 * Low packet count (<5)    | -5          | May indicate battery issues, unreliable
 * Outdated firmware        | -20         | Old firmware (v1.x, v2.0) has known vulns
 * MeshCore public channel  | -40         | Public PSK ships in all firmware builds;
 *                          |             | any passive observer can read all traffic
 * MeshCore hashtag channel | -20         | Key derivable from room name alone;
 *                          |             | not a secret
 * 
 * RATING THRESHOLDS:
 * - Score >= 80: "secure" (green)
 * - Score >= 60: "moderate" (yellow)
 * - Score < 60:  "vulnerable" (red)
 * 
 * ROUTER DETECTION:
 * =================
 * A device is marked as a router when it has relayed >= 2 packets.
 * Relay detection uses Meshtastic hop count: if hop_count < 3 (default hop limit),
 * the packet was forwarded by an intermediate node. See device_repository.cpp.
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
    const char* ratingEmoji; // "[OK] SECURE", "[!] MODERATE", " VULNERABLE"
    
    // Individual finding flags
    bool physicalProximity;  // Signal too strong
    bool possibleUnencrypted;// Heavy traffic without confirmed encryption
    bool isRouter;           // Elevated attack surface
    bool chatty;             // High packet count
    bool intermittent;       // Low packet count
    bool outdatedFirmware;   // Old firmware version
    bool meshCorePublic;     // MeshCore public channel — globally known PSK
    bool meshCoreHashtag;    // MeshCore hashtag channel — key derivable from name
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

    // Check 2b: MeshCore channel security
    // "public"  → globally known PSK, anyone with MeshCore firmware can read
    // "#room"   → key derivable from room name alone, not a real secret
    // "unknown" → custom PSK, genuinely private
    // ""        → not yet decrypted (no verdict)
    if (strcmp(device.protocol, "MeshCore") == 0) {
        if (strcmp(device.meshCoreChannel, "public") == 0) {
            result.score = result.score > 40 ? result.score - 40 : 0;
            result.meshCorePublic = true;
        } else if (device.meshCoreChannel[0] == '#') {
            result.score = result.score > 20 ? result.score - 20 : 0;
            result.meshCoreHashtag = true;
        }
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
    
    // Check 6: Firmware version check — only on confirmed versions, not heuristic estimates.
    // Meshtastic/LoRaWAN versions contain "(est)" and are too unreliable to penalise on.
    // MeshCore returns confirmed "MeshCore v0" / "MeshCore v1" from the header version field.
    bool isConfirmedVersion = (strstr(device.firmwareVersion, "(est)") == nullptr &&
                               strstr(device.firmwareVersion, "Unknown") == nullptr);
    if (isConfirmedVersion) {
        if (strstr(device.firmwareVersion, "v1.x") != nullptr ||
            strstr(device.firmwareVersion, "v2.0") != nullptr) {
            result.score = result.score > 20 ? result.score - 20 : 0;
            result.outdatedFirmware = true;
        }
    }
    
    // Determine rating
    if (result.score >= 80) {
        result.rating = "secure";
        result.ratingEmoji = "[OK] SECURE";
    } else if (result.score >= 60) {
        result.rating = "moderate";
        result.ratingEmoji = "[!] MODERATE";
    } else {
        result.rating = "vulnerable";
        result.ratingEmoji = " VULNERABLE";
    }
    
    return result;
}

/**
 * Check if any findings were detected
 */
inline bool hasFindings(const Assessment& a) {
    return a.physicalProximity || a.possibleUnencrypted || a.isRouter ||
           a.chatty || a.intermittent || a.outdatedFirmware ||
           a.meshCorePublic || a.meshCoreHashtag;
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
