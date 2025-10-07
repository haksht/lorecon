// Simple OLED test for Heltec WiFi LoRa 32 V3
// Use this to verify OLED hardware before complex RadioLib integration
// 
// To use: 
// 1. Comment out main.cpp in platformio.ini or temporarily rename it
// 2. Add Adafruit SSD1306 library to platformio.ini
// 3. Build and upload this test

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Heltec WiFi LoRa 32 V3 pins
#define I2C_SDA 17      
#define I2C_SCL 18
#define VEXT_PIN 36     // Vext control, active LOW

Adafruit_SSD1306 display(128, 64, &Wire, -1);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== Heltec V3 OLED Test ===");

  // Step 1: Enable Vext power (CRITICAL!)
  Serial.println("Step 1: Enabling Vext (active LOW)...");
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);   // turn Vext ON (active low)
  delay(50);                     // let OLED power stabilize
  Serial.println("  Vext enabled, waiting for power stabilization...");

  // Step 2: Initialize I2C with correct pins
  Serial.printf("Step 2: Initializing I2C on SDA=%d, SCL=%d...\n", I2C_SDA, I2C_SCL);
  Wire.begin(I2C_SDA, I2C_SCL);  // critical on V3
  Wire.setClock(100000);         // 100kHz for reliability
  Serial.println("  I2C initialized");

  // Step 3: Scan I2C bus
  Serial.println("Step 3: Scanning I2C bus...");
  bool found = false;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  ✓ I2C device found at 0x%02X\n", addr);
      found = true;
    }
  }
  
  if (!found) {
    Serial.println("  ❌ NO I2C devices found!");
    Serial.println("\nPossible causes:");
    Serial.println("  - OLED not populated on this board variant");
    Serial.println("  - Wrong pin numbers (check your board revision)");
    Serial.println("  - Hardware defect");
    Serial.println("\nTry V2 pins? (SDA=4, SCL=15, Vext=21)");
    for (;;) { delay(1000); }
  }

  // Step 4: Initialize display
  Serial.println("Step 4: Initializing SSD1306 display at 0x3C...");
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("  ❌ SSD1306 init failed!");
    Serial.println("  Even though I2C device detected, display.begin() failed.");
    Serial.println("  This might be a library or display type mismatch.");
    for (;;) { delay(1000); }
  }
  
  Serial.println("  ✓ Display initialized successfully!");

  // Step 5: Display test pattern
  Serial.println("Step 5: Drawing test pattern...");
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Heltec");
  display.println("V3 OLED");
  display.setTextSize(1);
  display.println("Test OK!");
  display.display();
  
  Serial.println("  ✓ Test pattern drawn");
  Serial.println("\n=== SUCCESS! ===");
  Serial.println("OLED is working correctly!");
  Serial.println("If you see text on the display, hardware is good.");
}

void loop() {
  // Animate to prove it's updating
  static int counter = 0;
  delay(1000);
  
  display.fillRect(0, 56, 128, 8, SSD1306_BLACK); // Clear bottom line
  display.setCursor(0, 56);
  display.setTextSize(1);
  display.printf("Count: %d", counter++);
  display.display();
}
