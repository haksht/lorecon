/**
 * Crash Context - NVS-backed crash state persistence
 */

#include "crash_context.h"
#include "logger.h"
#include <Arduino.h>
#include <string.h>

// RTC slow memory: survives software reset, panic, WDT, brownout, deepsleep.
// Lost only on cold power-off / hard reset. Zero flash wear, no NVS commit
// needed. Perfect for "what was the firmware doing right before it died".
RTC_NOINIT_ATTR static char rtcLastAction[32];
RTC_NOINIT_ATTR static uint32_t rtcLastActionMagic;
static constexpr uint32_t RTC_LAST_ACTION_MAGIC = 0xC0DECAFE;

namespace CrashContext {
    static Preferences prefs;
    static constexpr const char* NVS_NAMESPACE = "crash";
    static const char* lastAction = "boot";
    static uint32_t sessionSaveCount = 0;  // Per-session counter (RAM only)

    // Cached reset reason from current boot (persists in RAM for serial queries)
    static esp_reset_reason_t bootResetReason = ESP_RST_UNKNOWN;
    static const char* bootResetReasonStr = "Unknown";

    void setLastAction(const char* action) {
        lastAction = action;
        if (action) {
            strncpy(rtcLastAction, action, sizeof(rtcLastAction) - 1);
            rtcLastAction[sizeof(rtcLastAction) - 1] = '\0';
            rtcLastActionMagic = RTC_LAST_ACTION_MAGIC;
        }
    }

    void saveState(uint8_t mode, uint32_t uptimeSec, uint32_t freeHeap) {
        if (!prefs.begin(NVS_NAMESPACE, false)) return;
        prefs.putUChar("lastMode", mode);
        prefs.putULong("lastUptime", uptimeSec);
        prefs.putULong("lastHeap", freeHeap);
        prefs.putULong("saveCount", prefs.getULong("saveCount", 0) + 1);
        prefs.putString("lastAction", lastAction);
        prefs.end();
        sessionSaveCount++;
    }

    const char* getBootResetReasonStr() {
        return bootResetReasonStr;
    }

    // Save the reset reason to NVS so it survives across subsequent resets.
    // Called once at boot before the crash context gets overwritten.
    void saveResetReason(esp_reset_reason_t reason, const char* reasonStr) {
        bootResetReason = reason;
        bootResetReasonStr = reasonStr;
        if (!prefs.begin(NVS_NAMESPACE, false)) return;
        prefs.putUChar("rstReason", (uint8_t)reason);
        prefs.putString("rstStr", reasonStr);
        prefs.end();
    }

    // Returns the RTC-stored last action if the magic word survived this boot.
    // Returns nullptr on cold power-on (RTC memory uninitialized) or after a
    // hard reset where RTC slow memory got zeroed.
    static const char* getRtcLastAction() {
        if (rtcLastActionMagic != RTC_LAST_ACTION_MAGIC) return nullptr;
        // Defensive: ensure null termination in case of partial corruption
        rtcLastAction[sizeof(rtcLastAction) - 1] = '\0';
        if (rtcLastAction[0] == '\0') return nullptr;
        return rtcLastAction;
    }

    void loadAndReport() {
        if (!prefs.begin(NVS_NAMESPACE, true)) return;
        uint8_t lastMode = prefs.getUChar("lastMode", 255);
        uint32_t lastUptime = prefs.getULong("lastUptime", 0);
        uint32_t lastHeap = prefs.getULong("lastHeap", 0);
        uint32_t saveCount = prefs.getULong("saveCount", 0);
        String lastActionStr = prefs.getString("lastAction", "unknown");
        uint8_t prevReason = prefs.getUChar("rstReason", 255);
        String prevReasonStr = prefs.getString("rstStr", "unknown");
        prefs.end();

        // RTC lastAction is the most recent setLastAction() call before reset
        // (survives panic/WDT/brownout, lost on cold power). NVS lastAction is
        // up to 5 minutes stale because saveState only commits every 300s.
        const char* rtcAction = getRtcLastAction();

        if (lastMode != 255 && lastUptime > 0) {
            const char* modeStr = (lastMode == 0) ? "RECON" :
                                  (lastMode == 1) ? "TARGETED" :
                                  (lastMode == 2) ? "MENU" :
                                  (lastMode == 3) ? "REPLAY" : "UNKNOWN";
            LOG_INFO("Pre-crash context: mode=%s, uptime=%lu sec, heap=%lu bytes",
                     modeStr, lastUptime, lastHeap);
            LOG_INFO("   Last action (NVS, <=5 min stale): %s", lastActionStr.c_str());
            if (rtcAction) {
                LOG_INFO("   Last action (RTC, immediate): %s", rtcAction);
            }
            if (prevReason != 255) {
                LOG_INFO("   Previous boot reason: %s (code %d)", prevReasonStr.c_str(), prevReason);
            }
            LOG_INFO("   (NVS saves since flash: %lu  -  interval is 5 min)", saveCount);
        } else if (rtcAction) {
            // No NVS context (first boot since flash) but RTC survived
            LOG_INFO("Pre-crash context (RTC only): last action = %s", rtcAction);
        }
    }

    // Print current reset reason (callable from serial command 'i')
    void printResetInfo() {
        Serial.println("\n=== RESET / HEALTH INFO ===");
        Serial.printf("Current boot reason: %s (code %d)\n", bootResetReasonStr, (int)bootResetReason);
        Serial.printf("Uptime: %lu seconds (%.1f hours)\n", millis() / 1000, millis() / 3600000.0f);
        Serial.printf("Free heap: %lu bytes\n", (unsigned long)ESP.getFreeHeap());
        Serial.printf("Min free heap: %lu bytes\n", (unsigned long)ESP.getMinFreeHeap());

        const char* rtcAction = getRtcLastAction();
        if (rtcAction) {
            Serial.printf("Last action (RTC, immediate): %s\n", rtcAction);
        }

        if (prefs.begin(NVS_NAMESPACE, true)) {
            uint8_t prevReason = prefs.getUChar("rstReason", 255);
            String prevReasonStr = prefs.getString("rstStr", "unknown");
            uint8_t lastMode = prefs.getUChar("lastMode", 255);
            uint32_t lastUptime = prefs.getULong("lastUptime", 0);
            uint32_t lastHeap = prefs.getULong("lastHeap", 0);
            uint32_t saveCount = prefs.getULong("saveCount", 0);
            String lastActionStr = prefs.getString("lastAction", "unknown");
            prefs.end();

            if (prevReason != 255) {
                Serial.printf("Previous boot reason: %s (code %d)\n", prevReasonStr.c_str(), prevReason);
            }
            if (lastMode != 255 && lastUptime > 0) {
                const char* modeStr = (lastMode == 0) ? "RECON" :
                                      (lastMode == 1) ? "TARGETED" :
                                      (lastMode == 2) ? "MENU" :
                                      (lastMode == 3) ? "REPLAY" : "UNKNOWN";
                Serial.printf("Last crash context: mode=%s, uptime=%u sec, heap=%u bytes\n",
                             modeStr, (unsigned)lastUptime, (unsigned)lastHeap);
                Serial.printf("   Last action (NVS, <=5 min stale): %s\n", lastActionStr.c_str());
            }
            Serial.printf("NVS saves this session: %u (cumulative since flash: %u)\n",
                         (unsigned)sessionSaveCount, (unsigned)saveCount);
        }
        Serial.println("===========================\n");
    }
}
