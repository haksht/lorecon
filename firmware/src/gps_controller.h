/**
 * GPS Controller
 *
 * Manages the built-in GPS module on the T-Beam Supreme (L76K or MAX-M10S).
 * Uses TinyGPS++ for NMEA sentence parsing via hardware UART (Serial2).
 *
 * Usage:
 *   g_gpsController = new GpsController();
 *   g_gpsController->initialize();      // once, after PMU init
 *   // In main loop:
 *   g_gpsController->update();          // non-blocking UART drain
 *   if (g_gpsController->hasFix()) { ... }
 *
 * Only compiled when HAS_GPS is defined.
 */

#ifndef GPS_CONTROLLER_H
#define GPS_CONTROLLER_H

#ifdef HAS_GPS

#include <Arduino.h>
#include <TinyGPS++.h>
#include "config.h"
#include "logger.h"

class GpsController {
public:
    GpsController();

    /**
     * Initialize GPS UART and enable the GPS module.
     * Requires PMU to be initialized first (GPS needs ALDO4 power).
     * @return true on success
     */
    bool initialize();

    /**
     * Drain the GPS UART buffer into TinyGPS++.
     * Non-blocking  -  call every main loop iteration.
     */
    void update();

    // Note: none of these are const  -  TinyGPS++ accessors mutate internal state.

    /** True if GPS has a valid location fix with age < 5 seconds */
    bool hasFix();

    /** Latitude in decimal degrees (valid only when hasFix() == true) */
    double getLatitude();

    /** Longitude in decimal degrees (valid only when hasFix() == true) */
    double getLongitude();

    /** Altitude in metres (may be 0 if no 3D fix) */
    double getAltitude();

    /** Number of satellites used in the fix */
    uint32_t getSatellites();

    /** Age of the last fix in milliseconds */
    uint32_t getAge();

    /**
     * True if GPS HDOP is reasonable (< 5.0)  -  indicates good geometry.
     * Use alongside hasFix() for quality-gated packet tagging.
     */
    bool hasGoodFix();

private:
    TinyGPSPlus _gps;
    bool _initialized;

    // Serial2 is used for GPS (hardware UART on ESP32-S3)
    // Baud rate: 9600 (L76K default), pins from Config::Hardware
};

// Global singleton  -  set by lora_recon_tool.cpp after initialization
extern GpsController* g_gpsController;

#endif // HAS_GPS

#endif // GPS_CONTROLLER_H
