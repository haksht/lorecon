# Button and Display Feature - Branch: buttondisplay

## Overview

This branch adds OLED display support and user button control to the ESP32 LoRa Reconnaissance Tool.

## Hardware Requirements

- **Heltec WiFi LoRa 32 V3** board with:
  - Built-in 128x64 OLED display (I2C)
  - User button (PRG button on GPIO 0)

## Pin Configuration

### OLED Display (I2C)
- **SDA**: GPIO 17
- **SCL**: GPIO 18
- **RST**: GPIO 21

### User Button
- **GPIO 0** (PRG button, active low with internal pullup)

## Features

### Button Control
- **Short Press** (< 3 seconds): Toggle display on/off
- **Long Press** (≥ 3 seconds): Initiate shutdown sequence

### Display Modes

The OLED automatically shows different screens based on device state:

1. **Welcome Screen** - Shown during initialization
2. **Scanning Status** - Shows current frequency, SF, config index, packet count
3. **Packet Info** - Shows last received packet (protocol, node ID, RSSI, SNR)
4. **Device List** - Shows count of tracked devices (RF activity, nodes, targets)
5. **Targeting Mode** - Shows target info and capture statistics
6. **Shutdown** - Shows safe shutdown message

### Auto-Off Feature
- Display automatically turns off after **30 seconds** of inactivity (configurable)
- Press button to wake up and view current status
- Activity resets timer:
  - Packet reception
  - Mode changes
  - Button presses

## Code Changes

### New Files
- `firmware/src/oled_display.h` - Display manager class
- `firmware/src/oled_display.cpp` - Display implementation

### Modified Files
- `platformio.ini` - Added U8g2 library dependency
- `firmware/src/lora_recon_tool.h` - Added button pin, display integration
- `firmware/src/lora_recon_tool.cpp` - Button handler, display updates

### Library Dependencies
```ini
lib_deps =
    jgromes/RadioLib@^6.4.2
    bblanchon/ArduinoJson@^7.0.4
    olikraus/U8g2@^2.35.9      # New: OLED display library
```

## Usage

### Normal Operation
1. Device boots and shows welcome screen
2. Display shows scanning status with current frequency
3. When packet received, display shows packet info
4. Display auto-dims after 30 seconds

### Toggle Display
- **Press button briefly** - Display toggles on/off
- When turned back on, shows current scanning status

### Shutdown Device
1. **Hold button for 3 seconds**
2. Display shows "SHUTDOWN" message
3. Radio stops safely
4. Device enters deep sleep
5. To restart: Press reset button or cycle power

## API Reference

### OLEDDisplay Class

```cpp
// Lifecycle
bool initialize();                    // Init display hardware
void update();                        // Refresh display (call in loop)

// Display control
void turnOn();                        // Turn display on
void turnOff();                       // Turn display off
void toggle();                        // Toggle on/off
bool isOn() const;                    // Check if display is on

// Content updates
void showWelcome();
void showScanningStatus(const char* frequency, uint8_t sf, 
                        uint8_t configIndex, uint8_t totalConfigs);
void showPacketReceived(float rssi, float snr, 
                        const char* protocol, uint32_t nodeId);
void showDeviceCount(uint8_t rfActivity, uint8_t trackedNodes, 
                     uint8_t targetableDevices);
void showTargetingMode(const char* targetInfo);
void showShutdown();

// Auto-off timer
void resetAutoOffTimer();             // Reset inactivity timer
void setAutoOffTimeout(uint32_t ms);  // Set timeout (0=disabled)
```

## Building

```bash
# Build with display support (default environment)
pio run

# Upload to device
pio run -t upload

# Monitor output
pio run -t monitor
```

## Troubleshooting

### Display Not Working
1. **Check connections**: Verify SDA (17), SCL (18), RST (21)
2. **Check I2C address**: Default is 0x3C for SSD1306
3. **Monitor serial output**: Look for `[DISPLAY] Initializing OLED...`
4. If init fails, code continues without display

### Button Not Responding
1. **Check GPIO 0**: Ensure PRG button is connected
2. **Monitor serial**: Look for `[BUTTON] Button pressed` messages
3. **Test with LED**: Button should also trigger GPIO 0 events

### Display Auto-Off Too Fast/Slow
```cpp
// In lora_recon_tool.cpp, modify OLEDDisplay constructor
oledDisplay->setAutoOffTimeout(60000);  // 60 seconds
oledDisplay->setAutoOffTimeout(0);      // Disable auto-off
```

## Design Notes

### Why Deep Sleep?
- Deep sleep draws minimal power (~10 μA vs ~80 mA active)
- Effectively "off" - requires reset to wake
- Preserves hardware (no continuous power draw)

### Why Toggle Instead of Always On?
- Saves power during long monitoring sessions
- Reduces OLED burn-in risk
- Quick wake with button press

### Display Update Strategy
- Updates only when display is on (saves CPU)
- Non-blocking - doesn't interfere with packet reception
- Cached info - efficient rendering

## Future Enhancements

Potential additions:
- [ ] Brightness control (short press cycles brightness)
- [ ] Multiple info screens (cycle through on short press)
- [ ] Battery voltage display
- [ ] Signal strength meter graphic
- [ ] Uptime display
- [ ] Custom splash screen

## Merging to Main

To merge this branch into main:

```bash
# From main branch
git checkout main
git merge buttondisplay

# Or create pull request on GitHub
```

Before merging, test:
- ✅ Display initialization
- ✅ Button short press (toggle)
- ✅ Button long press (shutdown)
- ✅ Display updates during scanning
- ✅ Display updates on packet reception
- ✅ Auto-off timer works
- ✅ Device operation without display (if init fails)

---

**Branch**: `buttondisplay`  
**Created**: October 5, 2025  
**Status**: Ready for testing  
**Hardware**: Heltec WiFi LoRa 32 V3
