#ifndef PSK_DECRYPTION_SIMPLE_H
#define PSK_DECRYPTION_SIMPLE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
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
  
  // Thread-safe access to last decrypted message (copies to caller buffer)
  static void getLastMessageSafe(char* buffer, size_t bufferSize);
  static void clearLastMessage();

  // Legacy accessor (for compatibility - prefer getLastMessageSafe)
  static const char* getLastMessage() { return lastMessage; }

  // Battery telemetry from last successful DeviceMetrics decryption
  // batteryLevel: 0-100 (%), or -1 if not present in last packet
  // batteryVoltage: >0.0 if present, 0.0 if not
  static void getLastBattery(int16_t& level, float& voltage);
  static void clearLastBattery();

private:
  static bool extractMessageText(const uint8_t* data, size_t length, String& message);
  static void setLastMessage(const char* msg);  // Thread-safe internal setter
  
  // Storage for last decrypted message (protected by mutex)
  static constexpr size_t MAX_MESSAGE_LEN = 256;
  static char lastMessage[MAX_MESSAGE_LEN];
  static SemaphoreHandle_t messageMutex;

  // Storage for last DeviceMetrics battery telemetry (protected by messageMutex)
  static int16_t lastBatteryLevel;    // -1 = not present
  static float lastBatteryVoltage;    // 0.0 = not present

  static void setLastBattery(int16_t level, float voltage);
};

// External access to PSK stats
extern PSKStats pskStats;

#endif // PSK_DECRYPTION_SIMPLE_H