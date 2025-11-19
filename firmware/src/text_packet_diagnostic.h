/**
 * Text Packet Diagnostic Tool - Header
 * 
 * Helps diagnose why text message packets aren't being captured.
 * Tracks timing, encryption, and packet types.
 */

#ifndef TEXT_PACKET_DIAGNOSTIC_H
#define TEXT_PACKET_DIAGNOSTIC_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

namespace TextPacketDiagnostic {

struct PacketCategoryStats {
	uint32_t count;
	uint32_t minSize;
	uint32_t maxSize;
	float averageSize;
};

struct DiagnosticStats {
	uint32_t gapCount;
	uint32_t maxGapMs;
	uint32_t largeGapCount;
	uint32_t encryptedPacketCount;
	uint32_t plaintextPacketCount;
	uint32_t unknownPacketCount;
	PacketCategoryStats routing;
	PacketCategoryStats position;
	PacketCategoryStats text;
	PacketCategoryStats other;
	bool verboseMode;
};

/**
 * Reset all diagnostic counters and statistics
 */
void reset();

/**
 * Analyze a captured packet BEFORE decryption
 * Call this immediately after radio.readData()
 * 
 * @param data Raw packet data
 * @param length Packet length in bytes
 * @param rssi Signal strength
 * @param snr Signal-to-noise ratio
 */
void analyzePacket(const uint8_t* data, size_t length, float rssi, float snr);

/**
 * Analyze packet AFTER successful decryption
 * Call this after PSK decryption succeeds
 * 
 * @param decrypted Decrypted payload data
 * @param length Length of decrypted data
 * @param originalLength Original encrypted packet length
 */
void analyzeDecryptedPacket(const uint8_t* decrypted, size_t length, size_t originalLength);

/**
 * Print comprehensive diagnostic report
 * Shows timing gaps, encryption analysis, and packet type statistics
 */
void printReport();

/**
 * Enable/disable verbose diagnostic output
 * @param enable true to enable verbose logging
 */
void enableVerbose(bool enable);

/**
 * Check if verbose mode is enabled
 */
bool isVerbose();

/**
 * Capture current diagnostic counters for external reporting.
 */
void getStats(DiagnosticStats& stats);

} // namespace TextPacketDiagnostic

#endif // TEXT_PACKET_DIAGNOSTIC_H
