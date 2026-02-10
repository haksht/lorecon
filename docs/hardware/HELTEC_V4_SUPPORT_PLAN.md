# Heltec WiFi LoRa 32 V4 Support Plan

**Branch Name:** `feature/heltec-v4-support`  
**Target Version:** v2.4.0  
**Estimated Effort:** Low (pin-compatible with V3 for core LoRa pins)

---

## Executive Summary

The Heltec WiFi LoRa 32 V4 is a drop-in upgrade from V3 with **identical core LoRa pinout** but adds:
- GC1109 RF Front-End Module (PA + LNA) for 28dBm TX power
- GNSS interface (optional L76K GPS module)
- 16MB Flash + 2MB PSRAM (vs 8MB/0 on V3)
- Removed CP2102 (native USB-CDC only)

For **receive-only passive reconnaissance**, V4 is essentially identical to V3. The PA control pins are only needed for transmission, which this firmware doesn't do.

---

## Hardware Comparison

| Feature | V3 | V4 | Impact on Firmware |
|---------|-----|-----|-------------------|
| **MCU** | ESP32-S3FN8 | ESP32-S3R2 | None (same instruction set) |
| **Flash** | 8MB (integrated) | 16MB (external) | More LittleFS space available |
| **PSRAM** | None | 2MB | Can enable for larger buffers if needed |
| **SX1262 Pins** | Same | Same | ✅ **No changes required** |
| **OLED Pins** | SDA:17, SCL:18, RST:21 | SDA:17, SCL:18, RST:21 | ✅ **No changes required** |
| **Vext (OLED power)** | GPIO 36 | GPIO 36 | ✅ **No changes required** |
| **USB-CDC** | CP2102 chip | Native USB | May need `ARDUINO_USB_CDC_ON_BOOT=1` |
| **PA (GC1109)** | N/A | GPIO 2, 7, 46 | Optional - not used in RX-only mode |
| **GNSS Interface** | N/A | GPIO 34, 38-42 | Optional - future enhancement |

### LoRa Pin Map (V3 vs V4 - IDENTICAL)

```
SX1262 Pin    V3 GPIO    V4 GPIO    Notes
-----------------------------------------
NSS/CS        8          8          SPI chip select
DIO1/IRQ      14         14         Interrupt pin
RESET         12         12         Reset pin
BUSY          13         13         Busy indicator
SCK           9          9          SPI clock
MISO          11         11         SPI data in
MOSI          10         10         SPI data out
```

---

## Implementation Tasks

### Phase 1: Minimal V4 Support (Required) ✅ Simple

**Estimated Time:** 1-2 hours

1. **Add V4 pin definitions in `config.h`**
   
   Since pins are identical to V3, we can alias:
   ```cpp
   #elif defined(BOARD_HELTEC_V4)
       // ========================================================================
       // Heltec WiFi LoRa 32 V4 Pin Configuration
       // ========================================================================
       // Same core pins as V3 - compatible for receive-only operation
       // V4 adds: GC1109 PA (GPIO 2,7,46), GNSS (GPIO 34,38-42), 2MB PSRAM
       
       // SX1262 LoRa Radio Pins (identical to V3)
       constexpr uint8_t LORA_NSS = 8;
       constexpr uint8_t LORA_DIO1 = 14;
       constexpr uint8_t LORA_RST = 12;
       constexpr uint8_t LORA_BUSY = 13;
       constexpr uint8_t SPI_SCK = 9;
       constexpr uint8_t SPI_MISO = 11;
       constexpr uint8_t SPI_MOSI = 10;
       
       // OLED pins (identical to V3)
       constexpr uint8_t OLED_SDA = 17;
       constexpr uint8_t OLED_SCL = 18;
       constexpr uint8_t OLED_RST = 21;
       constexpr uint8_t OLED_VEXT = 36;
       
       // GC1109 PA Control (optional - for future TX support)
       constexpr uint8_t LORA_PA_POWER = 7;   // VFEM LDO enable
       constexpr uint8_t LORA_PA_EN = 2;      // CSD - chip enable
       constexpr uint8_t LORA_PA_TX_EN = 46;  // CPS - PA mode select
       
       // Battery monitoring
       constexpr uint8_t VBAT_ADC_PIN = 1;
       constexpr uint8_t VBAT_CTRL_PIN = 37;
       constexpr float VBAT_SCALE = 4.9f;
       
       // SD Card (external, same as V3)
       constexpr uint8_t SD_CS = 5;
       constexpr uint8_t SD_SCK = 9;
       constexpr uint8_t SD_MISO = 11;
       constexpr uint8_t SD_MOSI = 10;
       
       // User Interface
       constexpr uint8_t USER_BUTTON = 0;
       constexpr uint8_t USER_LED = -1;  // No LED on V4 (LED_BUILTIN undefined)
   #endif
   ```

2. **Add V4 environment in `platformio.ini`**
   
   ```ini
   [env:heltec_v4]
   platform = espressif32@^6.4.0
   board = esp32-s3-devkitc-1
   framework = arduino
   monitor_speed = 115200
   board_build.filesystem = littlefs
   build_src_filter = ${env:heltec_v3.build_src_filter}
   build_flags =
       -DARDUINO_USB_CDC_ON_BOOT=1  ; V4 uses native USB-CDC
       -DBOARD_HELTEC_V4
       -DHAS_OLED_DISPLAY
       -DHAS_PSRAM                   ; V4 has 2MB PSRAM
       -DPRODUCTION_BUILD
       -DCONFIG_LITTLEFS_FOR_IDF_3_2
       -DCONFIG_ESP32S3_BROWNOUT_DET_LVL_SEL_7=1
       -O2
       -Wall -Wextra
       -Wno-unused-function
       -Wno-unused-variable
       -fstack-protector
   build_src_flags = -Werror
   lib_deps = ${env:heltec_v3.lib_deps}
   ```

3. **Update OLED display header conditionals**
   
   In `oled_display.h`:
   ```cpp
   #if defined(BOARD_HELTEC_V3) || defined(BOARD_HELTEC_V4) || defined(BOARD_T3_S3)
   ```
   
   Add V4 block (can copy V3 since pins identical):
   ```cpp
   #elif defined(BOARD_HELTEC_V4)
       #define OLED_SDA    17
       #define OLED_SCL    18
       #define OLED_RST    21
       #define OLED_VEXT   36
   #endif
   ```

4. **Update board check in config.h**
   
   ```cpp
   #else
       #error "No board type defined! Define BOARD_T3_S3, BOARD_HELTEC_V3, or BOARD_HELTEC_V4 in platformio.ini"
   #endif
   ```

5. **Test compilation and flash to V4 hardware**

---

### Phase 2: PA Control (Optional - For Future TX) ⏳ Defer

Only needed if firmware later adds packet replay/TX functionality.

```cpp
// In radio_controller.cpp (hypothetical TX mode)
#ifdef BOARD_HELTEC_V4
void RadioController::enablePA() {
    pinMode(Config::Hardware::LORA_PA_POWER, OUTPUT);
    pinMode(Config::Hardware::LORA_PA_EN, OUTPUT);
    pinMode(Config::Hardware::LORA_PA_TX_EN, OUTPUT);
    
    digitalWrite(LORA_PA_POWER, HIGH);  // Enable VFEM LDO
    digitalWrite(LORA_PA_EN, HIGH);     // Enable GC1109 chip
    // LORA_PA_TX_EN is controlled dynamically per TX/RX
}

void RadioController::setTxMode(bool tx) {
    digitalWrite(LORA_PA_TX_EN, tx ? HIGH : LOW);  // HIGH=PA, LOW=bypass
}
#endif
```

---

### Phase 3: GPS Support (Optional - Future Enhancement) ⏳ Defer

V4 has a dedicated GNSS interface (1.25-8Pin) for L76K GPS module.

**Pin Mapping:**
| Function | GPIO | Description |
|----------|------|-------------|
| GPS_EN | 34 | Power enable (active LOW) |
| GPS_TX | 38 | Data to CPU |
| GPS_RX | 39 | Data to GPS |
| GPS_STANDBY | 40 | Wake control |
| GPS_PPS | 41 | Pulse-per-second |
| GPS_RESET | 42 | Reset (active LOW) |

**Future Implementation:**
- Add `GpsController` class with L76K protocol parsing
- Display GPS coordinates on OLED
- Add to geo_intelligence.cpp for position correlation
- Log positions to SD card for mapping

---

### Phase 4: PSRAM Utilization (Optional) ⏳ Defer

V4 has 2MB PSRAM that could be used for:
- Larger packet queue (increase from 100 to 500+ packets)
- Packet history buffer for replay analysis
- Extended device tracking (>50 devices)

```cpp
#ifdef HAS_PSRAM
    // Allocate packet queue in PSRAM
    packetQueue = (QueuedPacket*)ps_malloc(sizeof(QueuedPacket) * 500);
#endif
```

---

## Files to Modify

| File | Changes Required | Priority |
|------|-----------------|----------|
| `config.h` | Add `BOARD_HELTEC_V4` section | P0 |
| `platformio.ini` | Add `[env:heltec_v4]` | P0 |
| `oled_display.h` | Add V4 to conditionals | P0 |
| `oled_display.cpp` | Add V4 to conditionals | P0 |
| `README.md` | Document V4 support | P1 |
| `docs/hardware/` | Add V4 guide | P2 |

---

## Testing Checklist

- [ ] Flash to Heltec V4 hardware
- [ ] OLED display initializes correctly
- [ ] LoRa radio receives Meshtastic packets
- [ ] Web server accessible via WiFi AP
- [ ] Serial commands functional
- [ ] PSK decryption tests pass at boot
- [ ] Device tracking works
- [ ] WebSocket streaming works
- [ ] No compile warnings

---

## Git Workflow

```bash
# Create feature branch
git checkout -b feature/heltec-v4-support

# Make changes to config.h, platformio.ini, oled_display.h/cpp

# Build and test
pio run -e heltec_v4

# Flash to V4 hardware
pio run -e heltec_v4 -t upload

# Upload filesystem
pio run -e heltec_v4 -t uploadfs

# Verify operation
pio device monitor

# Commit
git add .
git commit -m "feat: Add Heltec WiFi LoRa 32 V4 support

- Add BOARD_HELTEC_V4 pin definitions (same core pins as V3)
- Add [env:heltec_v4] PlatformIO environment
- Enable ARDUINO_USB_CDC_ON_BOOT for native USB
- Add HAS_PSRAM flag for future optimization
- Document GC1109 PA pins for future TX support
- Document GNSS pins for future GPS support"

git push origin feature/heltec-v4-support
```

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Pin incompatibility | Very Low | High | Meshtastic confirms identical pins |
| USB-CDC not working | Low | Medium | Test with ARDUINO_USB_CDC_ON_BOOT=0 fallback |
| OLED variant differences | Very Low | Low | Same SSD1306 I2C display |
| PSRAM allocation issues | N/A | N/A | Defer PSRAM use to Phase 4 |

---

## References

- [Heltec V4 Product Page](https://heltec.org/project/wifi-lora-32-v4/)
- [Heltec V4 Datasheet](https://resource.heltec.cn/download/WiFi_LoRa_32_V4/datasheet/WiFi_LoRa_32_V4.2.0.pdf)
- [Heltec V4 Pin Map](https://resource.heltec.cn/download/WiFi_LoRa_32_V4/Pinmap/V4_pinmap.png)
- [Meshtastic V4 variant.h](https://github.com/meshtastic/firmware/blob/master/variants/esp32s3/heltec_v4/variant.h)
- [Meshtastic V3 variant.h](https://github.com/meshtastic/firmware/blob/master/variants/esp32s3/heltec_v3/variant.h)
