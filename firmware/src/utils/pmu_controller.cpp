#ifdef HAS_AXP2101

#include "pmu_controller.h"

namespace PMUController {

namespace {
    XPowersPMU pmu;
    bool _initialized = false;
}

/**
 * Initialize AXP2101 and enable all required power channels.
 * Must be called before radio, display, SD, or GPS initialization.
 * @return true if PMIC found and configured successfully
 */
bool initialize() {
    if (_initialized) return true;

    // AXP2101 is on a dedicated I2C bus separate from the display.
    // begin(wire, addr, sda, scl)  -  the Wire object configures itself internally.
    if (!pmu.begin(Wire1, AXP2101_SLAVE_ADDRESS,
                   Config::Hardware::PMU_SDA, Config::Hardware::PMU_SCL)) {
        LOG_ERROR("AXP2101 not found on I2C bus (SDA:%d SCL:%d)",
                  Config::Hardware::PMU_SDA, Config::Hardware::PMU_SCL);
        return false;
    }

    LOG_INFO("AXP2101 found, configuring power channels...");

    // Use per-channel public API (the generic enablePowerOutput/setPowerChannelVoltage
    // is protected in XPowersAXP2101 and only accessible via XPowersLibInterface).

    // ALDO1: Display and onboard sensors (BME280, QMC6310)
    pmu.setALDO1Voltage(3300);
    pmu.enableALDO1();

    // ALDO2: Additional sensors
    pmu.setALDO2Voltage(3300);
    pmu.enableALDO2();

    // ALDO3: SX1262 LoRa radio (also continuous TCXO reference voltage)
    pmu.setALDO3Voltage(3300);
    pmu.enableALDO3();

    // ALDO4: GPS module (L76K / MAX-M10S)
    pmu.setALDO4Voltage(3300);
    pmu.enableALDO4();

    // BLDO1: SD card power
    pmu.setBLDO1Voltage(3300);
    pmu.enableBLDO1();

    // BLDO2: SD card backup rail
    pmu.setBLDO2Voltage(3300);
    pmu.enableBLDO2();

    // Configure charging: 4.2V, 500mA (conservative for 18650)
    // Charging is enabled by default on AXP2101; just set the parameters.
    pmu.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);
    pmu.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_500MA);

    // Short settle delay so rails are stable before peripherals init
    delay(50);

    _initialized = true;
    LOG_INFO("AXP2101 initialized (ALDO1-4 + BLDO1-2 at 3300mV)");
    return true;
}

/**
 * Read battery voltage in volts (e.g. 3.85).
 * Returns 0.0 if PMIC not initialized.
 */
float getBatteryVoltage() {
    if (!_initialized) return 0.0f;
    return pmu.getBattVoltage() / 1000.0f;  // mV -> V
}

/**
 * Returns true if USB power is connected and battery is charging.
 */
bool isCharging() {
    if (!_initialized) return false;
    return pmu.isCharging();
}

/**
 * Returns estimated battery percentage (0 - 100).
 * AXP2101 has a built-in coulomb counter for this.
 */
uint8_t getBatteryPercent() {
    if (!_initialized) return 0;
    return static_cast<uint8_t>(pmu.getBatteryPercent());
}

/**
 * Cut all PMIC power rails and power off the board.
 * This is a hard power-off  -  the device will not restart until
 * the PMIC wakes it (e.g. button press or USB insertion).
 */
void shutdown() {
    if (!_initialized) return;
    pmu.shutdown();
}

} // namespace PMUController

#endif // HAS_AXP2101
