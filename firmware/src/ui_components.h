/**
 * UI Display Components - Extracted for Web App Transition
 * 
 * These functions are separated for future web interface integration.
 * Serial output can be replaced with JSON/REST API responses.
 */

#ifndef UI_COMPONENTS_H
#define UI_COMPONENTS_H

#include <Arduino.h>
#include "data_structures.h"
#include "recon_state.h"

// Device list display
void displayDeviceList();
void displayDeviceSummary(const TargetableDevice& device, uint8_t index);

// Activity displays
void displayRFActivityTable();
void displayActivitySummary();

// Helper functions
String formatRSSI(float rssi);
String formatDuration(uint32_t seconds);
String getSignalQuality(float rssi);

#endif // UI_COMPONENTS_H
