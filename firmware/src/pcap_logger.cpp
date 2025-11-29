/**
 * PCAP Logger Implementation
 */

#include "pcap_logger.h"
#include "logger.h"
#include <time.h>

// PCAP Global Header structure
struct PCAPGlobalHeader {
    uint32_t magic_number;   // 0xa1b2c3d4
    uint16_t version_major;  // 2
    uint16_t version_minor;  // 4
    int32_t  thiszone;       // GMT to local correction (0)
    uint32_t sigfigs;        // Accuracy of timestamps (0)
    uint32_t snaplen;        // Max length of captured packets (65535)
    uint32_t network;        // Data link type (147 = User0 for LoRa)
} __attribute__((packed));

// PCAP Packet Header structure
struct PCAPPacketHeader {
    uint32_t ts_sec;         // Timestamp seconds
    uint32_t ts_usec;        // Timestamp microseconds
    uint32_t incl_len;       // Number of bytes of packet data saved
    uint32_t orig_len;       // Actual length of packet
} __attribute__((packed));

// ============================================================================
// PCAPLogger Implementation
// ============================================================================

PCAPLogger::PCAPLogger() 
    : fileOpen(false)
    , packetCount(0)
    , fileSize(0)
{
}

PCAPLogger::~PCAPLogger() {
    close();
}

bool PCAPLogger::begin(const char* filename) {
    if (fileOpen) {
        LOG_WARN("PCAP file already open, closing first");
        close();
    }
    
    // Ensure SD is mounted
    if (!SD.begin()) {
        LOG_ERROR("SD card not available");
        return false;
    }
    
    // Open file for writing
    file = SD.open(filename, FILE_WRITE);
    if (!file) {
        LOG_ERROR("Failed to create PCAP file: %s", filename);
        return false;
    }
    
    currentFilename = String(filename);
    fileOpen = true;
    packetCount = 0;
    fileSize = 0;
    
    // Write PCAP global header
    if (!writeGlobalHeader()) {
        LOG_ERROR("Failed to write PCAP global header");
        close();
        return false;
    }
    
    LOG_INFO("PCAP capture started: %s", filename);
    return true;
}

void PCAPLogger::close() {
    if (fileOpen && file) {
        file.flush();
        file.close();
        LOG_INFO("PCAP capture closed: %s (%u packets, %u bytes)", 
                 currentFilename.c_str(), packetCount, fileSize);
        fileOpen = false;
    }
}

bool PCAPLogger::writeGlobalHeader() {
    PCAPGlobalHeader header;
    header.magic_number = PCAP_MAGIC_NUMBER;
    header.version_major = PCAP_VERSION_MAJOR;
    header.version_minor = PCAP_VERSION_MINOR;
    header.thiszone = 0;
    header.sigfigs = 0;
    header.snaplen = PCAP_SNAPLEN;
    header.network = PCAP_LINKTYPE_USER0;
    
    size_t written = file.write((uint8_t*)&header, sizeof(header));
    if (written != sizeof(header)) {
        LOG_ERROR("Failed to write PCAP global header");
        return false;
    }
    
    fileSize += written;
    return true;
}

bool PCAPLogger::writePacket(const uint8_t* data, size_t length,
                             uint32_t timestampMs,
                             float rssiDbm, 
                             float snrDb,
                             float frequencyMHz,
                             uint8_t spreadingFactor,
                             uint32_t bandwidth,
                             uint8_t codingRate) {
    if (!fileOpen || !file) {
        LOG_ERROR("PCAP file not open");
        return false;
    }
    
    // Create LoRa pseudo-header with metadata
    LoRaPseudoHeader pseudoHeader;
    pseudoHeader.frequencyMHz = frequencyMHz;
    pseudoHeader.rssiDbm = rssiDbm;
    pseudoHeader.snrDb = snrDb;
    pseudoHeader.spreadingFactor = spreadingFactor;
    pseudoHeader.bandwidth = bandwidth;
    pseudoHeader.codingRate = codingRate;
    pseudoHeader.reserved = 0;
    
    // Calculate total packet length (pseudo-header + actual data)
    uint32_t totalLength = sizeof(LoRaPseudoHeader) + length;
    
    // Write PCAP packet header
    PCAPPacketHeader pktHeader;
    pktHeader.ts_sec = timestampMs / 1000;
    pktHeader.ts_usec = (timestampMs % 1000) * 1000;
    pktHeader.incl_len = totalLength;
    pktHeader.orig_len = totalLength;
    
    size_t written = file.write((uint8_t*)&pktHeader, sizeof(pktHeader));
    if (written != sizeof(pktHeader)) {
        LOG_ERROR("Failed to write PCAP packet header");
        return false;
    }
    fileSize += written;
    
    // Write LoRa pseudo-header
    written = file.write((uint8_t*)&pseudoHeader, sizeof(pseudoHeader));
    if (written != sizeof(pseudoHeader)) {
        LOG_ERROR("Failed to write LoRa pseudo-header");
        return false;
    }
    fileSize += written;
    
    // Write actual packet data
    written = file.write(data, length);
    if (written != length) {
        LOG_ERROR("Failed to write packet data");
        return false;
    }
    fileSize += written;
    
    // Flush periodically (every 10 packets) to prevent data loss
    packetCount++;
    if (packetCount % 10 == 0) {
        file.flush();
    }
    
    return true;
}

// ============================================================================
// PCAPSession Implementation
// ============================================================================

PCAPSession::PCAPSession() {
}

String PCAPSession::generateSessionFilename() {
    // Generate filename: capture_YYYYMMDD_HHMMSS.pcap
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    char filename[64];
    snprintf(filename, sizeof(filename), 
             "/capture_%04d%02d%02d_%02d%02d%02d.pcap",
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec);
    
    return String(filename);
}

bool PCAPSession::startSession(const char* sessionName) {
    String filename;
    
    if (sessionName && strlen(sessionName) > 0) {
        // Use provided session name
        filename = String("/") + sessionName;
        if (!filename.endsWith(".pcap")) {
            filename += ".pcap";
        }
    } else {
        // Generate timestamped filename
        filename = generateSessionFilename();
    }
    
    return pcapLogger.begin(filename.c_str());
}

void PCAPSession::endSession() {
    pcapLogger.close();
}

bool PCAPSession::logPacket(const uint8_t* data, size_t length,
                           float rssiDbm, float snrDb, float frequencyMHz) {
    return pcapLogger.writePacket(data, length, millis(), 
                                  rssiDbm, snrDb, frequencyMHz);
}
