#ifndef PSK_DECRYPTION_SIMPLE_H
#define PSK_DECRYPTION_SIMPLE_H

#include <Arduino.h>
#include "data_structures.h"  // For PSKStats struct
#include "config.h"           // For Config::PSK::NUM_DEFAULT_KEYS

// PSK decryption interface
class PSKDecryption {
public:
  static void initialize();
  static bool testDefaultPSKs(const uint8_t* data, size_t length);
  static PSKStats getStats();
  static void resetStats();
  static void printStats();
  
  // Test helpers (made public for testing)
  static int decodeBase64(const char* input, uint8_t* output, size_t maxLen);  // Returns decoded byte count (0 on failure)
  static uint8_t getDefaultPSKCount() { return Config::PSK::NUM_DEFAULT_KEYS; }
  
  // Get last decrypted message (for web broadcast)
  static const char* getLastMessage() { return lastMessage; }
  static void clearLastMessage() { lastMessage[0] = '\0'; }

private:
  static bool extractMessageText(const uint8_t* data, size_t length, String& message);
  
  // Storage for last decrypted message
  static constexpr size_t MAX_MESSAGE_LEN = 256;
  static char lastMessage[MAX_MESSAGE_LEN];
};

// External access to PSK stats
extern PSKStats pskStats;

#endif // PSK_DECRYPTION_SIMPLE_H