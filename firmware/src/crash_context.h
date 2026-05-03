/**
 * Crash Context - NVS-backed crash state persistence
 *
 * Saves mode, uptime, and heap to NVS so post-mortem analysis is possible
 * after unexpected reboots. Rate-limited to 5-minute write intervals to
 * prevent NVS flash wear (10-second interval caused ~41K writes/day,
 * which corrupted the NVS partition after ~23 hours).
 */
#pragma once

#include <Preferences.h>
#include <esp_system.h>

namespace CrashContext {
    void setLastAction(const char* action);
    void saveState(uint8_t mode, uint32_t uptimeSec, uint32_t freeHeap);
    void saveResetReason(esp_reset_reason_t reason, const char* reasonStr);
    void loadAndReport();
    void printResetInfo();
    const char* getBootResetReasonStr();
    const char* getBootLastActionStr();  // RTC last-action snapshot from pre-crash boot
}
