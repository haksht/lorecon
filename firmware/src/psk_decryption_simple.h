#ifndef PSK_DECRYPTION_SIMPLE_H
#define PSK_DECRYPTION_SIMPLE_H

#include <Arduino.h>
#include "data_structures.h"  // For PSKStats struct

// PSK decryption interface
class PSKDecryption {
public:
  static void initialize();
  static bool testDefaultPSKs(const uint8_t* data, size_t length);
  static PSKStats getStats();
  static void resetStats();
  static void printStats();
  
  // Test helpers (made public for testing)
  static bool decodeBase64(const char* input, uint8_t* output, size_t maxLen);
  static uint8_t getDefaultPSKCount() { return 5; }  // NUM_DEFAULT_PSKS

private:
  static bool extractMessageText(const uint8_t* data, size_t length, String& message);
};

// External access to PSK stats
extern PSKStats pskStats;

#endif // PSK_DECRYPTION_SIMPLE_H