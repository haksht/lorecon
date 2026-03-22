/**
 * PMU Controller — AXP2101 Power Management for T-Beam Supreme
 *
 * The T-Beam Supreme uses an AXP2101 PMIC to power all peripherals.
 * NOTHING works (radio, GPS, SD, display) unless the PMIC is initialized
 * and the correct power channels are enabled.
 *
 * PMIC I2C bus: SDA=42, SCL=41 (separate from display I2C at 17/18)
 *
 * Channel assignments:
 *   ALDO1 → 3300 mV  display + onboard sensors
 *   ALDO2 → 3300 mV  additional sensors
 *   ALDO3 → 3300 mV  SX1262 LoRa radio (also provides TCXO reference)
 *   ALDO4 → 3300 mV  GPS module (L76K / MAX-M10S)
 *   BLDO1 → 3300 mV  SD card
 *
 * Only compiled when HAS_AXP2101 is defined.
 */

#ifndef PMU_CONTROLLER_H
#define PMU_CONTROLLER_H

#ifdef HAS_AXP2101

#include <Arduino.h>
#include <Wire.h>
// Define chip variant so XPowersLib.h exposes the XPowersPMU typedef
// for AXP2101 (avoids including the .tpp implementation file directly).
#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>
#include "../config.h"
#include "../logger.h"

namespace PMUController {

bool initialize();
float getBatteryVoltage();
bool isCharging();
uint8_t getBatteryPercent();
void shutdown();

} // namespace PMUController

#else // !HAS_AXP2101

// Stub implementations for boards without AXP2101
namespace PMUController {
    inline bool initialize() { return true; }
    inline float getBatteryVoltage() { return 0.0f; }
    inline bool isCharging() { return false; }
    inline uint8_t getBatteryPercent() { return 0; }
    inline void shutdown() {}
} // namespace PMUController

#endif // HAS_AXP2101

#endif // PMU_CONTROLLER_H
