/**
 * @file json_builders.h
 * @brief JSON response builders for API endpoints
 * 
 * Extracted from ReconService to separate data formatting from service logic.
 * All builders return JSON strings ready for HTTP responses.
 */

#ifndef JSON_BUILDERS_H
#define JSON_BUILDERS_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Forward declarations
class ReconState;
class GeoIntelligence;
struct TargetableDevice;

namespace JsonBuilders {

/**
 * Build JSON array of all targetable devices
 */
String buildDevicesJson(ReconState& reconState);

/**
 * Build JSON for a single device by index
 * @return Empty string if device not found
 */
String buildDeviceJson(ReconState& reconState, uint8_t deviceIndex);

/**
 * Build system status JSON
 */
String buildStatusJson(ReconState& reconState);

/**
 * Build statistics JSON with protocol/frequency distributions
 */
String buildStatisticsJson(ReconState& reconState);

/**
 * Build RF activity JSON for all frequency configs
 */
String buildActivityJson(ReconState& reconState);

/**
 * Build positions JSON from geo intelligence
 */
String buildPositionsJson(GeoIntelligence& geoIntel);

/**
 * Build GeoJSON FeatureCollection for mapping tools
 */
String buildGeoJson(GeoIntelligence& geoIntel);

/**
 * Build KML document for Google Earth/Maps
 */
String buildKml(GeoIntelligence& geoIntel);

/**
 * Build comprehensive recon summary JSON
 */
String buildReconSummaryJson(ReconState& reconState, GeoIntelligence& geoIntel);

/**
 * Build device type summary with aggregated stats
 */
String buildDeviceTypeSummaryJson(ReconState& reconState);

/**
 * Build security assessment JSON with vulnerability analysis
 */
String buildSecurityAssessmentJson(ReconState& reconState);

/**
 * Build replay slots status JSON
 */
String buildReplaySlotsJson(ReconState& reconState);

/**
 * Build diagnostics JSON from TextPacketDiagnostic stats
 */
String buildDiagnosticsJson();

// Internal helpers
namespace Internal {

/**
 * Convert ReconMode enum to string
 */
const char* modeToString(uint8_t mode);

/**
 * Fill a JsonObject with device data
 */
void fillDevice(JsonObject& obj, const TargetableDevice& dev, uint8_t index);

} // namespace Internal

} // namespace JsonBuilders

#endif // JSON_BUILDERS_H
