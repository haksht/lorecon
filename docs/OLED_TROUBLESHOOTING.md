# OLED Display Troubleshooting Guide

## Issue: "No OLED detected on I2C bus"

### Heltec WiFi LoRa 32 Pin Configurations

Different Heltec board versions use different pins:

#### **V3 (ESP32-S3)** - Current Configuration
```cpp
#define OLED_SDA    17
#define OLED_SCL    18
#define OLED_RST    21
#define OLED_VEXT   36  // Power control, active LOW
```

#### **V2 (ESP32)** - If you have V2 board
```cpp
#define OLED_SDA    4
#define OLED_SCL    15
#define OLED_RST    16
#define OLED_VEXT   21  // Power control
```

#### **V1 (Original ESP32)**
```cpp
#define OLED_SDA    4
#define OLED_SCL    15
#define OLED_RST    16
// V1 doesn't have Vext power control
```

### Diagnostic Steps

1. **Identify Your Board Version**
   - Check the silkscreen on the board for "V3", "V2", etc.
   - V3 uses ESP32-S3 chip
   - V2 uses ESP32 (original)

2. **Check Serial Output**
   The improved diagnostic code will show:
   ```
   [DISPLAY] Scanning I2C bus...
   [DISPLAY]   Address 0x3C: FOUND!
   ```
   or
   ```
   [DISPLAY]   Address 0x3C: not found (error 2)
   [DISPLAY]   Address 0x3D: not found (error 2)
   ```

3. **I2C Error Codes**
   - `error 0`: Success (device found)
   - `error 2`: Address not acknowledged (no device at this address)
   - `error 3`: Data not acknowledged
   - `error 4`: Other error
   - `error 5`: Timeout

4. **Common Issues**

   **Wrong Board Version Pins**
   - If you have V2 board, edit `oled_display.h`:
     ```cpp
     #define OLED_SDA    4
     #define OLED_SCL    15
     #define OLED_RST    16
     #define OLED_VEXT   21
     ```

   **Vext Polarity**
   - Most V3 boards: Vext LOW = power ON
   - Some variants: Vext HIGH = power ON
   - The diagnostic code now tries both automatically

   **Physical Connection**
   - Verify OLED is properly seated in connector
   - Check for bent pins or poor contact

   **Factory Configuration**
   - Some Heltec boards come with OLED disabled in hardware
   - Check if there's a jumper or solder bridge that needs to be set

### Manual Testing

If automatic detection fails, you can manually test I2C:

1. **Full I2C Bus Scan** - Add to your code:
   ```cpp
   Serial.println("Scanning I2C bus 0x01-0x7F...");
   for (uint8_t addr = 1; addr < 127; addr++) {
     Wire.beginTransmission(addr);
     if (Wire.endTransmission() == 0) {
       Serial.printf("Device found at 0x%02X\n", addr);
     }
   }
   ```

2. **Test Different Pin Combinations**
   Try these common alternatives in `oled_display.h`:
   - SDA=4, SCL=15 (V2)
   - SDA=21, SCL=22 (Some V3 variants)
   - SDA=17, SCL=18 (Current V3)

3. **Check Vext Pin**
   Some boards use different Vext pins:
   - GPIO 36 (current)
   - GPIO 21 (V2)
   - GPIO 37 (some variants)

### If Still Not Working

1. **Verify Hardware**
   - Use a multimeter to check if display is getting 3.3V power
   - Measure voltage on Vext pin when LOW and HIGH

2. **Try External OLED**
   - Connect a known-good SSD1306 OLED module to:
     - VCC → 3.3V
     - GND → GND
     - SDA → GPIO 17
     - SCL → GPIO 18

3. **Board Without OLED**
   - Some Heltec boards are sold without the OLED populated
   - The code handles this gracefully - it will continue to work without display

### Successful Output

When working correctly, you should see:
```
[DISPLAY] Initializing OLED...
[DISPLAY] Configuring Vext pin 36 (trying LOW first)...
[DISPLAY] Initializing I2C: SDA=17, SCL=18
[DISPLAY] Scanning I2C bus...
[DISPLAY]   Address 0x3C: FOUND!
[DISPLAY] ✓ Display found at 0x3C, initializing... OK
[DISPLAY] OLED ready (128x64, I2C: SDA=17, SCL=18, Vext=36)
```
