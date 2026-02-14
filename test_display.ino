#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

// Use Wire2 for I2C #2 on Teensy 4.1
#define OLED_RESET -1  // No reset pin
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire2, OLED_RESET);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("SSD1306 Test on Wire2...");

  // Start I2C #2
  Wire2.begin();

  // Initialize display
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 allocation failed");
    while (1); // Stop here if display not found
  }

  display.clearDisplay();

  // Text test
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Teensy 4.1");
  display.println("SSD1306 128x64");
  display.println("I2C #2 (Wire2)");
  display.display();

  delay(2000);

  // Graphics test
  display.clearDisplay();
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.drawLine(0, 0, 127, 63, SSD1306_WHITE);
  display.drawCircle(64, 32, 15, SSD1306_WHITE);
  display.display();

  delay(2000);
}

void loop() {
  // Simple pixel animation
  display.clearDisplay();
  for (int x = 0; x < SCREEN_WIDTH; x++) {
    display.drawPixel(x, 32, SSD1306_WHITE);
    display.display();
    delay(5);
    display.drawPixel(x, 32, SSD1306_BLACK);
  }
}

