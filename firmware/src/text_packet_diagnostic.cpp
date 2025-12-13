/**
 * Text Packet Diagnostic Tool
 * 
 * This diagnostic helps identify WHY text message packets aren't being captured.
 * It monitors for timing gaps, checks for plaintext packets, and provides detailed
 * packet analysis to understand what's happening with text messages.
 */

#include <Arduino.h>
#include "text_packet_diagnostic.h"

namespace TextPacketDiagnostic {

// Track timing between packets
static uint32_t lastPacketTime = 0;
static uint32_t maxGapMs = 0;
static uint32_t gapCount = 0;
static uint32_t largeGapCount = 0;

// Track packet types seen
static uint32_t encryptedPacketCount = 0;
static uint32_t plaintextPacketCount = 0;
static uint32_t unknownPacketCount = 0;

// Statistics
struct PacketTypeStats {
    uint32_t count;
    uint32_t minSize;
    uint32_t maxSize;
    uint32_t totalSize;
    float avgSize() const { return count > 0 ? (float)totalSize / count : 0; }
};

static PacketTypeStats routingStats = {0, 999, 0, 0};
static PacketTypeStats positionStats = {0, 999, 0, 0};
static PacketTypeStats textStats = {0, 999, 0, 0};
static PacketTypeStats otherStats = {0, 999, 0, 0};

// Verbose mode flag
static bool verboseMode = false;

void reset() {
    lastPacketTime = 0;
    maxGapMs = 0;
    gapCount = 0;
    largeGapCount = 0;
    encryptedPacketCount = 0;
    plaintextPacketCount = 0;
    unknownPacketCount = 0;
    
    routingStats = {0, 999, 0, 0};
    positionStats = {0, 999, 0, 0};
    textStats = {0, 999, 0, 0};
    otherStats = {0, 999, 0, 0};
}

/**
 * Analyze a captured packet and classify it
 * This runs BEFORE decryption to see raw packet characteristics
 */
void analyzePacket(const uint8_t* data, size_t length, float rssi, float snr) {
    uint32_t now = millis();
    
    // Track timing gaps (always do this)
    if (lastPacketTime > 0) {
        uint32_t gap = now - lastPacketTime;
        gapCount++;
        if (gap > maxGapMs) {
            maxGapMs = gap;
        }
        if (gap > 500) {  // More than 500ms gap
            largeGapCount++;
            if (verboseMode) {
                Serial.printf("[DIAG] ⚠️ Large gap detected: %u ms (packet might have been missed during this time)\n", 
                             (unsigned int)gap);
            }
        }
    }
    lastPacketTime = now;
    
    // Skip detailed analysis in quiet mode
    if (!verboseMode) {
        return;
    }
    
    // Print raw packet header analysis (ONLY in verbose mode)
    Serial.println("\n[DIAG] ═══════ RAW PACKET ANALYSIS ═══════");
    Serial.printf("[DIAG] Length: %d bytes, RSSI: %.1f dBm, SNR: %.1f dB\n", length, rssi, snr);
    
    if (length < 8) {
        Serial.println("[DIAG] ⚠️ Packet too short for Meshtastic");
        unknownPacketCount++;
        return;
    }
    
    // Check if Meshtastic
    if (length < 4 || !(data[0] == 0xFF && data[1] == 0xFF && data[2] == 0xFF && data[3] == 0xFF)) {
        Serial.println("[DIAG] ⚠️ Not a Meshtastic packet (header mismatch)");
        unknownPacketCount++;
        return;
    }
    
    // Extract node ID
    uint32_t nodeId = (uint32_t(data[4]) << 24) | (uint32_t(data[5]) << 16) | 
                      (uint32_t(data[6]) << 8) | uint32_t(data[7]);
    Serial.printf("[DIAG] Node ID: 0x%08X\n", nodeId);
    
    // Analyze payload area (after header)
    if (length > 12) {
        Serial.print("[DIAG] Payload start (bytes 12-27): ");
        for (size_t i = 12; i < length && i < 28; i++) {
            Serial.printf("%02X ", data[i]);
        }
        Serial.println();
        
        // Check for HIGH ENTROPY (encrypted data)
        uint8_t nonZeroCount = 0;
        uint8_t highByteCount = 0;  // Bytes > 0x80
        for (size_t i = 12; i < length && i < 32; i++) {
            if (data[i] != 0x00) nonZeroCount++;
            if (data[i] > 0x80) highByteCount++;
        }
        
        float density = (length > 12) ? (float)nonZeroCount / (length - 12) : 0.0f;
        bool likelyEncrypted = (density > 0.7 && highByteCount > 5);
        
        Serial.printf("[DIAG] Entropy analysis: %.1f%% non-zero, %d high bytes\n", 
                     density * 100, highByteCount);
        
        if (likelyEncrypted) {
            Serial.println("[DIAG] 🔒 HIGH ENTROPY - Likely ENCRYPTED packet");
            encryptedPacketCount++;
        } else {
            Serial.println("[DIAG] 📖 LOW ENTROPY - Might be PLAINTEXT or structured data");
            plaintextPacketCount++;
            
            // Check for protobuf field markers in plaintext
            Serial.println("[DIAG] Checking for protobuf field markers in plaintext:");
            for (size_t i = 12; i + 1 < length; i++) {
                if (data[i] == 0x08) {
                    uint8_t portnum = data[i+1];
                    Serial.printf("[DIAG]   Found field 1 (portnum) at offset %d: 0x%02X", i, portnum);
                    if (portnum == 0x01) Serial.print(" = TEXT_MESSAGE_APP ✉️");
                    else if (portnum == 0x03) Serial.print(" = POSITION_APP 📍");
                    else if (portnum == 0x05 || portnum == 0x07) Serial.print(" = ROUTING 🔄");
                    Serial.println();
                }
                if (data[i] == 0x0A) {
                    Serial.printf("[DIAG]   Found text marker (0x0A) at offset %d\n", i);
                }
            }
        }
    }
    
    // Check packet size characteristics
    Serial.printf("[DIAG] Packet size: %d bytes", length);
    if (length < 26) {
        Serial.println(" (small - likely ACK/routing)");
    } else if (length < 50) {
        Serial.println(" (medium - could be text message)");
    } else if (length < 100) {
        Serial.println(" (large - likely position/telemetry)");
    } else {
        Serial.println(" (very large - position with extended data)");
    }
    
    Serial.println("[DIAG] ═══════════════════════════════════");
}

/**
 * Analyze AFTER decryption to classify packet type
 */
void analyzeDecryptedPacket(const uint8_t* decrypted, size_t length, size_t originalLength) {
    if (length < 2) return;
    
    Serial.println("[DIAG] ─── DECRYPTED CONTENT ANALYSIS ───");
    
    // Look for protobuf field markers
    if (decrypted[0] == 0x08) {
        uint8_t portnum = decrypted[1];
        Serial.printf("[DIAG] ✓ Valid portnum field found: 0x%02X ", portnum);
        
        PacketTypeStats* stats = &otherStats;
        
        if (portnum == 0x01) {
            Serial.println("= TEXT_MESSAGE_APP ✉️");
            stats = &textStats;
            
            // Search for actual text content
            for (size_t i = 0; i + 1 < length; i++) {
                if (decrypted[i] == 0x0A && decrypted[i+1] > 0 && decrypted[i+1] < 200) {
                    uint8_t textLen = decrypted[i+1];
                    Serial.printf("[DIAG]   Found text field at offset %d, length %d: \"", i, textLen);
                    for (size_t j = 0; j < textLen && (i+2+j) < length; j++) {
                        char c = decrypted[i+2+j];
                        if (c >= 32 && c <= 126) Serial.print(c);
                        else Serial.printf("<%02X>", c);
                    }
                    Serial.println("\"");
                    break;
                }
            }
        } else if (portnum == 0x03) {
            Serial.println("= POSITION_APP 📍");
            stats = &positionStats;
        } else if (portnum == 0x05 || portnum == 0x07) {
            Serial.println("= ROUTING_APP 🔄");
            stats = &routingStats;
        } else {
            Serial.printf("= Unknown app (0x%02X)\n", portnum);
        }
        
        // Update statistics
        stats->count++;
        if (originalLength < stats->minSize) stats->minSize = originalLength;
        if (originalLength > stats->maxSize) stats->maxSize = originalLength;
        stats->totalSize += originalLength;
        
    } else {
        // Not a standard protobuf data message
        Serial.printf("[DIAG] ⚠️ Unexpected first byte: 0x%02X (expected 0x08 for portnum)\n", decrypted[0]);
        Serial.print("[DIAG]   First 16 bytes: ");
        for (size_t i = 0; i < length && i < 16; i++) {
            Serial.printf("%02X ", decrypted[i]);
        }
        Serial.println();
        
        otherStats.count++;
        if (originalLength < otherStats.minSize) otherStats.minSize = originalLength;
        if (originalLength > otherStats.maxSize) otherStats.maxSize = originalLength;
        otherStats.totalSize += originalLength;
    }
}

/**
 * Print comprehensive diagnostic report
 */
void printReport() {
    Serial.println("\n╔═══════════════════════════════════════════════════════╗");
    Serial.println("║       TEXT PACKET DIAGNOSTIC REPORT                  ║");
    Serial.println("╚═══════════════════════════════════════════════════════╝");
    
    // Timing analysis
    Serial.println("\n📊 TIMING ANALYSIS:");
    Serial.printf("  Total packet gaps: %u\n", (unsigned int)gapCount);
    Serial.printf("  Maximum gap: %u ms\n", (unsigned int)maxGapMs);
    Serial.printf("  Large gaps (>500ms): %u\n", (unsigned int)largeGapCount);
    
    if (largeGapCount > 0) {
        Serial.println("\n  ⚠️  WARNING: Large gaps detected!");
        Serial.println("     Text messages might be transmitted during these gaps.");
        Serial.println("     Your sniffer is NOT in continuous receive mode.");
    } else {
        Serial.println("\n  ✓ No significant timing gaps detected.");
    }
    
    // Encryption analysis
    Serial.println("\n🔒 ENCRYPTION ANALYSIS:");
    Serial.printf("  Encrypted packets: %u\n", (unsigned int)encryptedPacketCount);
    Serial.printf("  Plaintext packets: %u\n", (unsigned int)plaintextPacketCount);
    Serial.printf("  Unknown packets: %u\n", (unsigned int)unknownPacketCount);
    
    if (plaintextPacketCount > 0) {
        Serial.println("\n  ⚠️  PLAINTEXT PACKETS DETECTED!");
        Serial.println("     Some packets might be transmitted without encryption.");
        Serial.println("     Check if text messages are in plaintext packets above.");
    }
    
    // Packet type statistics
    Serial.println("\n📦 PACKET TYPE STATISTICS:");
    
    auto printStats = [](const char* name, const PacketTypeStats& stats) {
        if (stats.count > 0) {
            Serial.printf("  %s: %u packets\n", name, (unsigned int)stats.count);
            Serial.printf("    Size range: %u - %u bytes (avg: %.1f)\n", 
                         (unsigned int)stats.minSize, (unsigned int)stats.maxSize, stats.avgSize());
        } else {
            Serial.printf("  %s: NONE ❌\n", name);
        }
    };
    
    printStats("TEXT messages", textStats);
    printStats("POSITION messages", positionStats);
    printStats("ROUTING messages", routingStats);
    printStats("OTHER messages", otherStats);
    
    // Conclusions
    Serial.println("\n🔍 DIAGNOSTIC CONCLUSIONS:");
    
    if (textStats.count == 0) {
        Serial.println("\n  ❌ NO TEXT MESSAGES CAPTURED");
        Serial.println("     Possible reasons:");
        
        if (largeGapCount > 0) {
            Serial.println("     1. ⚠️  TIMING ISSUE (most likely)");
            Serial.println("        Text packets transmitted during processing gaps");
            Serial.println("        Solution: Implement interrupt-driven buffering");
        }
        
        if (encryptedPacketCount > 0 && plaintextPacketCount == 0) {
            Serial.println("     2. ⚠️  ALL PACKETS ENCRYPTED");
            Serial.println("        Text might be encrypted with different key");
            Serial.println("        Solution: Verify PSK on sending device");
        }
        
        if (plaintextPacketCount > 0) {
            Serial.println("     2. ⚠️  TEXT MIGHT BE IN PLAINTEXT");
            Serial.println("        Check plaintext packets above for text content");
            Serial.println("        Solution: Modify code to check plaintext packets");
        }
        
        Serial.println("     3. ⚠️  FREQUENCY/MODULATION MISMATCH");
        Serial.println("        Text packets on different channel parameters");
        Serial.println("        Solution: Verify channel settings match exactly");
    } else {
        Serial.printf("\n  ✓ SUCCESS: %u text messages captured\n", (unsigned int)textStats.count);
        Serial.printf("    Average size: %.1f bytes\n", textStats.avgSize());
    }
    
    Serial.println("\n═══════════════════════════════════════════════════════\n");
}

/**
 * Enable continuous diagnostic output
 */
void enableVerbose(bool enable) {
    verboseMode = enable;
    if (enable) {
        Serial.println("[DIAG] 📢 Verbose diagnostics ENABLED");
    } else {
        Serial.println("[DIAG] 🔇 Verbose diagnostics DISABLED (quiet mode for fast capture)");
    }
}

bool isVerbose() {
    return verboseMode;
}

void getStats(DiagnosticStats& stats) {
    stats.gapCount = gapCount;
    stats.maxGapMs = maxGapMs;
    stats.largeGapCount = largeGapCount;
    stats.encryptedPacketCount = encryptedPacketCount;
    stats.plaintextPacketCount = plaintextPacketCount;
    stats.unknownPacketCount = unknownPacketCount;
    stats.verboseMode = verboseMode;

    auto fillStats = [](PacketCategoryStats& dst, const PacketTypeStats& src) {
        if (src.count == 0) {
            dst.count = 0;
            dst.minSize = 0;
            dst.maxSize = 0;
            dst.averageSize = 0.0f;
        } else {
            dst.count = src.count;
            dst.minSize = src.minSize == 999 ? 0 : src.minSize;
            dst.maxSize = src.maxSize;
            dst.averageSize = src.avgSize();
        }
    };

    fillStats(stats.routing, routingStats);
    fillStats(stats.position, positionStats);
    fillStats(stats.text, textStats);
    fillStats(stats.other, otherStats);
}

} // namespace TextPacketDiagnostic
