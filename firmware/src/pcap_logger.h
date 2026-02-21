/**
 * PCAP Logger - Standard Packet Capture Format for Post-Analysis
 * 
 * Exports LoRa packets in standard PCAP format compatible with:
 * - Wireshark
 * - LoRa_Craft Python toolkit
 * - Scapy
 * - Other standard packet analysis tools
 * 
 * Format: PCAP with custom DLT (Data Link Type) for LoRa
 * Benefits:
 * - Standard format, future-proof
 * - Offline analysis with powerful desktop tools
 * - Preserves RSSI/SNR metadata
 * - Can be shared with other researchers
 */

#ifndef PCAP_LOGGER_H
#define PCAP_LOGGER_H

#include <Arduino.h>
#include <SD.h>

// PCAP file format constants
#define PCAP_MAGIC_NUMBER       0xa1b2c3d4
#define PCAP_VERSION_MAJOR      2
#define PCAP_VERSION_MINOR      4
#define PCAP_SNAPLEN            65535
#define PCAP_LINKTYPE_USER0     147  // User-defined link type for LoRa

// LoRa-specific pseudo-header (stored before each packet)
// This allows tools to extract RSSI, SNR, frequency metadata
struct LoRaPseudoHeader {
    float frequencyMHz;
    float rssiDbm;
    float snrDb;
    uint8_t spreadingFactor;
    uint32_t bandwidth;
    uint8_t codingRate;
    uint16_t reserved;  // Padding to 4-byte alignment
} __attribute__((packed));

/**
 * PCAPLogger - Write packets in standard PCAP format
 * 
 * Usage:
 *   PCAPLogger pcap;
 *   pcap.begin("capture.pcap");
 *   pcap.writePacket(data, len, millis(), rssi, snr, freq);
 *   pcap.close();
 */
class PCAPLogger {
public:
    PCAPLogger();
    ~PCAPLogger();
    
    // Lifecycle
    bool begin(const char* filename);
    bool isOpen() const { return file && fileOpen; }
    void close();
    
    // Write packet (with metadata in pseudo-header)
    bool writePacket(const uint8_t* data, size_t length,
                    uint32_t timestampMs,
                    float rssiDbm, 
                    float snrDb,
                    float frequencyMHz,
                    uint8_t spreadingFactor = 7,
                    uint32_t bandwidth = 125000,
                    uint8_t codingRate = 5);
    
    // Flush buffered data to FAT (call before reading the file via another handle)
    void flush() { if (fileOpen && file) file.flush(); }

    // Statistics
    uint32_t getPacketCount() const { return packetCount; }
    size_t getFileSize() const { return fileSize; }
    String getFilename() const { return currentFilename; }
    
private:
    File file;
    bool fileOpen;
    String currentFilename;
    uint32_t packetCount;
    size_t fileSize;
    
    // Write PCAP global header (once per file)
    bool writeGlobalHeader();
    
    // Write PCAP packet header + data
    bool writePacketRecord(const uint8_t* data, size_t length, uint32_t timestampMs);
};

/**
 * PCAPSession - Manages automatic PCAP capture sessions
 * 
 * Integrates with existing packet_logger.cpp to add PCAP export option
 */
class PCAPSession {
public:
    PCAPSession();
    
    // Session management
    bool startSession(const char* sessionName = nullptr);
    void endSession();
    bool isActive() const { return pcapLogger.isOpen(); }
    
    // Log packet (simplified interface)
    bool logPacket(const uint8_t* data, size_t length,
                  float rssiDbm, float snrDb, float frequencyMHz);

    // Flush buffered data to FAT
    void flush() { pcapLogger.flush(); }
    
    // Info
    String getSessionFile() const { return pcapLogger.getFilename(); }
    uint32_t getPacketCount() const { return pcapLogger.getPacketCount(); }
    
private:
    PCAPLogger pcapLogger;
    String generateSessionFilename();
};

#endif // PCAP_LOGGER_H
