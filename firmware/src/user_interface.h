/**
 * User Interface Module
 * 
 * Handles all display functions, menus, and user interaction for the 
 * ESP32 LoRa Reconnaissance Tool. Separated from main.cpp for better
 * code organization and maintainability.
 * 
 * This module provides:
 * - Reconnaissance results display with separated activity/device tracking
 * - Interactive menu system 
 * - Device type analysis and statistics
 * - Activity monitoring displays
 * - Command handling interface
 * 
 * Note: Currently uses global state variables - future refactoring should
 * eliminate these dependencies for better testability and modularity.
 */

#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

#include <Arduino.h>
#include "data_structures.h"

// UI Display Functions
void showReconResults();
void showDeviceTypeSummary();
void showActivityDetails();
void showSecurityAssessment();
void showFrequencyTargetingMenu();
void handleFrequencyTargetingInput();
void startFrequencyTargeting(uint8_t configIndex);

// Statistics and Analysis Functions  
void printStats();

// Utility Functions
void displayWelcomeMessage();
void displayReconStartMessage();

// State management - now using ReconState class instead of globals
#include "recon_state.h"
extern ReconState reconState;

#ifdef ENABLE_PSK_TESTING
extern PSKStats pskStats;
#endif

#endif // USER_INTERFACE_H