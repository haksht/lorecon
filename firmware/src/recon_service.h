#ifndef RECON_SERVICE_H
#define RECON_SERVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "irecon_tool.h"
#include "recon_state.h"
#include "geo_intelligence.h"
#include "text_packet_diagnostic.h"

class ReconService {
public:
    static void initialize(IReconTool* tool);
    static bool isInitialized();

    static bool startTargetedCaptureByNodeId(uint32_t nodeId, String& outMessage);
    static bool startTargetedCaptureByIndex(uint8_t deviceIndex, String& outMessage);
    static bool stopCapture(String& outMessage);

    static String buildDevicesJson();
    static String buildDeviceJson(uint32_t nodeId);
    static String buildStatusJson();
    static String buildStatisticsJson();
    static String buildActivityJson();
    static String buildPositionsJson();
    static String buildGeoJson();
    static String buildKml();
    static String buildReconSummaryJson();
    static String buildDeviceTypeSummaryJson();
    static String buildSecurityAssessmentJson();
    static String buildReplaySlotsJson();
    static bool clearReplaySlots(String& outMessage);
    static bool startFrequencyTargeting(uint8_t configIndex, String& outMessage);
    static String buildDiagnosticsJson();
    static bool setVerboseMode(bool enableVerbose, String& outMessage);

private:
    static IReconTool* reconTool;

    static TargetableDevice* findTargetableDevice(uint32_t nodeId);
    static const char* modeToString(OperationMode mode);
    static uint8_t findDeviceIndex(uint32_t nodeId);
    static void fillDevice(JsonObject& obj, const TargetableDevice& device, uint8_t index);
};

#endif // RECON_SERVICE_H
