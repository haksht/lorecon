# LilyGO T-Deck Port Guide

## 📱 LilyGO T-Deck Compatibility Analysis

### Hardware Comparison
| Component | Current (Heltec V3) | LilyGO T-Deck | Compatible |
|-----------|-------------------|---------------|------------|
| Processor | ESP32-S3 | ESP32-S3 | ✅ Same |
| LoRa Radio | SX1262 | SX1262 | ✅ Same |
| Frequency | 902-928 MHz | 902-928 MHz | ✅ Same |
| Framework | Arduino | Arduino | ✅ Same |

### Pin Mapping Changes Required

#### Current Heltec V3 Pins:
```cpp
#define SS          8    // SPI Chip Select
#define RST         12   // Reset  
#define DIO1        14   // Interrupt
#define BUSY        13   // Busy status
#define SCK         9    // SPI Clock
#define MISO        11   // SPI MISO
#define MOSI        10   // SPI MOSI
```

#### LilyGO T-Deck Pins (Typical):
```cpp
#define SS          18   // SPI Chip Select (LoRa)
#define RST         23   // Reset
#define DIO1        26   // Interrupt  
#define BUSY        32   // Busy status
#define SCK         5    // SPI Clock
#define MISO        19   // SPI MISO  
#define MOSI        27   // SPI MOSI
```

**Note:** Pin assignments vary by T-Deck version. Always verify with your specific board schematic.

## 🔧 Required Code Modifications

### 1. Pin Definitions Update
Replace the pin definitions in `main.cpp`:

```cpp
// LilyGO T-Deck pins (verify with your board!)
#define SS          18
#define RST         23  
#define DIO1        26
#define BUSY        32
#define SCK         5
#define MISO        19
#define MOSI        27
```

### 2. PlatformIO Configuration
The `platformio.ini` can stay mostly the same:

```ini
[env:lilygo_tdeck]
platform = espressif32@^6.4.0
board = esp32-s3-devkitc-1  ; Keep generic board
framework = arduino
monitor_speed = 115200
build_flags = -DARDUINO_USB_CDC_ON_BOOT=0
lib_deps = jgromes/RadioLib@^6.4.2
```

### 3. Optional: T-Deck Specific Features
The T-Deck has additional hardware you could utilize:
- **Screen**: TFT display for packet visualization
- **Keyboard**: Input for configuration
- **Trackball**: Navigation interface
- **SD Card**: Storage for captured packets

## ⚠️ Critical Verification Steps

### Before Flashing:
1. **Verify Pin Assignments**: T-Deck variants have different pin mappings
2. **Check Schematic**: Always reference your specific T-Deck version
3. **Test SPI Communication**: Ensure LoRa module responds correctly
4. **Validate Power Supply**: Confirm adequate power for transmission

### Pin Verification Methods:
```cpp
// Add this to setup() for pin testing
Serial.printf("Testing SX1262 on pins: SS=%d, RST=%d, DIO1=%d, BUSY=%d\n", 
              SS, RST, DIO1, BUSY);
```

## 🚀 Porting Process

### Step 1: Create T-Deck Environment
Add new environment to `platformio.ini`:
```ini
[env:tdeck]
platform = espressif32@^6.4.0
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
build_flags = 
    -DARDUINO_USB_CDC_ON_BOOT=0
    -DTDECK_VARIANT=1  ; Custom flag for T-Deck specific code
lib_deps = jgromes/RadioLib@^6.4.2
```

### Step 2: Update Pin Definitions
Create conditional compilation:
```cpp
#ifdef TDECK_VARIANT
  // T-Deck pins
  #define SS          18
  #define RST         23  
  #define DIO1        26
  #define BUSY        32
  #define SCK         5
  #define MISO        19
  #define MOSI        27
#else
  // Heltec V3 pins (default)
  #define SS          8
  #define RST         12  
  #define DIO1        14
  #define BUSY        13
  #define SCK         9
  #define MISO        11
  #define MOSI        10
#endif
```

### Step 3: Test and Validate
1. Flash firmware with T-Deck pin configuration
2. Monitor serial output for SX1262 initialization
3. Test packet reception with known Meshtastic traffic
4. Verify signal quality (RSSI/SNR) matches expectations

## 💡 T-Deck Advantages

### Additional Capabilities:
- **Built-in Display**: Real-time packet visualization without computer
- **Keyboard Interface**: Local configuration and control  
- **Portable Operation**: Self-contained sniffer device
- **SD Card Storage**: Local packet capture storage
- **Battery Operation**: Truly portable reconnaissance tool

### Enhanced Use Cases:
- **Field Operations**: Portable Meshtastic monitoring
- **Standalone Logging**: Capture sessions without laptop
- **Interactive Analysis**: On-device packet inspection
- **Covert Monitoring**: Discrete surveillance capability

## ⚠️ Potential Challenges

### Hardware Variations:
- **Multiple T-Deck Versions**: Pin assignments vary between revisions
- **LoRa Module Variants**: Some use SX1276 instead of SX1262
- **Power Management**: Different power circuits may affect RF performance
- **Antenna Differences**: Built-in vs external antenna considerations

### Software Considerations:
- **Memory Usage**: Additional components may impact available RAM
- **GPIO Conflicts**: Screen/keyboard pins may interfere with LoRa
- **Power Management**: Sleep modes may affect continuous monitoring
- **Library Compatibility**: T-Deck specific libraries may conflict

## 🎯 Recommended Approach

### Phase 1: Basic Port
1. Update pin definitions for your T-Deck version
2. Test basic packet capture functionality
3. Validate signal quality and performance
4. Ensure stable operation

### Phase 2: T-Deck Integration  
1. Add display support for packet visualization
2. Implement keyboard controls for configuration
3. Add SD card logging for persistent storage
4. Create standalone operation mode

### Phase 3: Enhanced Features
1. Touch interface for packet analysis
2. Real-time spectrum display on screen
3. Local web server for mobile access
4. Advanced filtering and search

---

**Compatibility**: ✅ **EXCELLENT** - Same core hardware (ESP32-S3 + SX1262)  
**Effort Required**: 🔧 **MINIMAL** - Mainly pin definition updates  
**Advantages**: 📱 **SIGNIFICANT** - Portable, self-contained operation  
**Recommendation**: **HIGHLY RECOMMENDED** for portable reconnaissance  

The generic board approach used in this project makes T-Deck porting straightforward!