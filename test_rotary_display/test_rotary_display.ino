#include <Encoder.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <Control_Surface.h>

// Rotary Encoder Pins
#define ENCODER_CLK 4
#define ENCODER_DT  5
#define ENCODER_SW  6

// OLED Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

// Use I2C bus #2
#define OLED_WIRE Wire2

USBMIDI_Interface midi;
CCPotentiometer expression(A1, {MIDI_CC::Expression_Controller, Channel_1});

Encoder myEncoder(ENCODER_CLK, ENCODER_DT);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &OLED_WIRE, OLED_RESET);

long lastPosition = -999;
bool lastButtonState = HIGH;
uint8_t lastExprValue = 255;

void setup() {
  Serial.begin(115200);

  // A0 high (TRS ring/sense held high → expression pedal detected)
  // Pin 0 low (plug-detect held low → no unplug interrupts)
  pinMode(A0, OUTPUT); digitalWrite(A0, HIGH);
  pinMode(0,  OUTPUT); digitalWrite(0,  LOW);

  analogReadResolution(12);
  Control_Surface.begin();

  pinMode(ENCODER_SW, INPUT_PULLUP);

  // Start I2C bus #2
  OLED_WIRE.begin();

  // Initialize display
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 allocation failed");
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Encoder");
  display.display();

  delay(1000);
}

void loop() {
  Control_Surface.loop();

  long position = myEncoder.read() / 4;  
  // Divide by 4 because KY-040 generates 4 counts per detent

  bool buttonState = digitalRead(ENCODER_SW);
  uint8_t exprValue = expression.getValue();

  if (position != lastPosition || buttonState != lastButtonState || exprValue != lastExprValue) {

    display.clearDisplay();

    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print("Value:");
    display.setCursor(0, 24);
    display.print(position);

    display.setTextSize(1);
    display.setCursor(0, 48);
    display.print("Expr: ");
    display.print(exprValue);

    display.setCursor(0, 57);
    display.print("Button: ");
    display.print(buttonState == LOW ? "Pressed" : "Released");

    display.display();

    lastPosition = position;
    lastButtonState = buttonState;
    lastExprValue = exprValue;
  }
}

