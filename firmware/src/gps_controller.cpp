/**
 * GPS Controller Implementation
 */

#ifdef HAS_GPS

#include "gps_controller.h"

// Fix validity window: discard fixes older than this
static constexpr uint32_t GPS_FIX_MAX_AGE_MS = 5000;

// HDOP threshold: below this is considered "good geometry"
static constexpr double GPS_HDOP_THRESHOLD = 5.0;

GpsController* g_gpsController = nullptr;

GpsController::GpsController()
    : _initialized(false)
{
}

bool GpsController::initialize() {
    if (_initialized) return true;

    // Assert GPS_EN HIGH to power the GPS module.
    // On T-Beam Supreme, this controls the L76K enable line.
    // The AXP2101 ALDO4 must already be on (done by PMUController::initialize()).
    pinMode(Config::Hardware::GPS_EN, OUTPUT);
    digitalWrite(Config::Hardware::GPS_EN, HIGH);

    // Brief settle time for GPS module power-on
    delay(100);

    // Initialize hardware UART (Serial2) for GPS NMEA output
    // L76K and MAX-M10S both default to 9600 baud, 8N1
    Serial2.begin(9600, SERIAL_8N1,
                  Config::Hardware::GPS_RX,
                  Config::Hardware::GPS_TX);

    _initialized = true;
    LOG_INFO("GPS initialized (UART RX:%d TX:%d EN:%d, waiting for fix...)",
             Config::Hardware::GPS_RX,
             Config::Hardware::GPS_TX,
             Config::Hardware::GPS_EN);

    return true;
}

void GpsController::update() {
    if (!_initialized) return;

    // Drain all available bytes into TinyGPS++ (non-blocking)
    while (Serial2.available() > 0) {
        _gps.encode(Serial2.read());
    }
}

bool GpsController::hasFix() {
    return _gps.location.isValid() && (_gps.location.age() < GPS_FIX_MAX_AGE_MS);
}

bool GpsController::hasGoodFix() {
    return hasFix() && _gps.hdop.isValid() && (_gps.hdop.hdop() < GPS_HDOP_THRESHOLD);
}

double GpsController::getLatitude() {
    return _gps.location.isValid() ? _gps.location.lat() : 0.0;
}

double GpsController::getLongitude() {
    return _gps.location.isValid() ? _gps.location.lng() : 0.0;
}

double GpsController::getAltitude() {
    return _gps.altitude.isValid() ? _gps.altitude.meters() : 0.0;
}

uint32_t GpsController::getSatellites() {
    return _gps.satellites.isValid() ? _gps.satellites.value() : 0;
}

uint32_t GpsController::getAge() {
    return _gps.location.age();
}

#endif // HAS_GPS
