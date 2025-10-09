// Advanced OLED troubleshooting for Heltec V3
// Tests multiple initialization sequences to find what works
// Based on Meshtastic's actual implementation

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <esp_task_wdt.h>

// Try multiple pin configurations
struct PinConfig {
    const char* name;
    int sda;
    int scl;
    int vext;
    int rst;
    bool vextActiveLow;
};

PinConfig configs[] = {
    {"Heltec V3 Standard",  17, 18, 36, -1, true},   // Most common V3
    {"Heltec V3 Alt RST",   17, 18, 36, 21, true},   // V3 with reset
    {"Heltec V3 No Vext",   17, 18, -1, -1, true},   // V3 without Vext control
    {"Heltec V2",            4, 15, 21, 16, true},   // V2 configuration
    {"Generic SSD1306",     21, 22, -1, -1, false},  // Generic ESP32 pins
};

void testConfig(PinConfig& cfg) {
    Serial.printf("\n=== Testing: %s ===\n", cfg.name);
    Serial.printf("  SDA=%d, SCL=%d, Vext=%d, RST=%d\n", 
                  cfg.sda, cfg.scl, cfg.vext, cfg.rst);
    
    // Step 1: Enable Vext if available
    if (cfg.vext >= 0) {
        pinMode(cfg.vext, OUTPUT);
        if (cfg.vextActiveLow) {
            digitalWrite(cfg.vext, LOW);   // Active LOW
            Serial.println("  Vext: LOW (power ON)");
        } else {
            digitalWrite(cfg.vext, HIGH);  // Active HIGH
            Serial.println("  Vext: HIGH (power ON)");
        }
        delay(100);  // Wait for power stabilization
    }
    
    // Step 2: Reset pulse if RST available
    if (cfg.rst >= 0) {
        pinMode(cfg.rst, OUTPUT);
        digitalWrite(cfg.rst, LOW);
        delay(10);
        digitalWrite(cfg.rst, HIGH);
        delay(10);
        Serial.println("  Reset pulse sent");
    }
    
    // Step 3: Initialize I2C
    Wire.end();  // End any previous I2C
    Wire.begin(cfg.sda, cfg.scl);
    Wire.setClock(100000);  // 100kHz
    delay(50);
    Serial.println("  I2C initialized");
    
    // Step 4: Scan I2C
    bool found = false;
    uint8_t foundAddr = 0;
    for (uint8_t addr = 0x3C; addr <= 0x3D; addr++) {
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();
        if (error == 0) {
            Serial.printf("  ✓ FOUND at 0x%02X!\n", addr);
            found = true;
            foundAddr = addr;
            break;
        }
    }
    
    if (!found) {
        Serial.println("  ✗ No I2C device found");
        // Power off before next test
        if (cfg.vext >= 0) {
            digitalWrite(cfg.vext, cfg.vextActiveLow ? HIGH : LOW);
        }
        return;
    }
    
    // Step 5: Try to initialize display
    Serial.println("  Attempting U8g2 initialization...");
    
    // Create display with hardware I2C
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
    
    display.setBusClock(100000);
    if (display.begin()) {
        Serial.println("  ✓✓✓ SUCCESS! Display initialized!");
        
        // Draw test pattern
        display.clearBuffer();
        display.setFont(u8g2_font_ncenB08_tr);
        display.drawStr(0, 15, "SUCCESS!");
        display.drawStr(0, 30, cfg.name);
        display.drawStr(0, 45, "Display Working!");
        display.sendBuffer();
        
        Serial.println("  Check the display - you should see text!");
        Serial.println("\n*** THIS CONFIGURATION WORKS! ***");
        Serial.printf("*** Use: SDA=%d, SCL=%d, Vext=%d, RST=%d ***\n", 
                      cfg.sda, cfg.scl, cfg.vext, cfg.rst);
        
        // Keep display on and loop
        for (;;) {
            delay(1000);
        }
    } else {
        Serial.println("  ✗ U8g2 begin() failed");
    }
    
    // Power off before next test
    if (cfg.vext >= 0) {
        digitalWrite(cfg.vext, cfg.vextActiveLow ? HIGH : LOW);
    }
    delay(100);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    // Disable watchdog for test (intentionally testing multiple configs)
    esp_task_wdt_delete(NULL);
    
    Serial.println("\n\n╔════════════════════════════════════════╗");
    Serial.println("║  Heltec V3 OLED Advanced Diagnostic   ║");
    Serial.println("║  Testing Multiple Configurations      ║");
    Serial.println("╚════════════════════════════════════════╝\n");
    
    // Try each configuration
    for (int i = 0; i < sizeof(configs) / sizeof(configs[0]); i++) {
        testConfig(configs[i]);
        delay(500);  // Pause between tests
    }
    
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║  All Configurations Tested            ║");
    Serial.println("║  No OLED Found                         ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.println("\nPossibilities:");
    Serial.println("1. OLED is faulty or not actually populated");
    Serial.println("2. Different pin configuration not tested");
    Serial.println("3. Requires specific power sequencing not tried");
    Serial.println("4. Check if there's a solder jumper to enable OLED");
    
    Serial.println("\nWhat to check on the physical board:");
    Serial.println("- Is the OLED screen component actually soldered?");
    Serial.println("- Are there any jumpers labeled 'OLED_EN' or similar?");
    Serial.println("- Check Heltec's pin diagram for your specific board revision");
}

void loop() {
    delay(1000);
}
