// Teensy 4.1 + KY-040 Rotary Encoder Test

#define CLK 4
#define DT  5
#define SW  6

volatile long encoderCount = 0;
volatile bool encoderMoved = false;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(CLK, INPUT_PULLUP);
  pinMode(DT, INPUT_PULLUP);
  pinMode(SW, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(CLK), readEncoder, CHANGE);

  Serial.println("KY-040 Rotary Encoder Test");
}

void loop() {
  static long lastCount = 0;

  if (encoderMoved) {
    noInterrupts();
    long currentCount = encoderCount;
    encoderMoved = false;
    interrupts();

    if (currentCount != lastCount) {
      Serial.print("Position: ");
      Serial.println(currentCount);
      lastCount = currentCount;
    }
  }

  // Button press detection
  if (digitalRead(SW) == LOW) {
    Serial.println("Button Pressed");
    delay(300); // simple debounce
  }
}

void readEncoder() {
  bool clkState = digitalRead(CLK);
  bool dtState = digitalRead(DT);

  if (clkState == dtState) {
    encoderCount++;
  } else {
    encoderCount--;
  }

  encoderMoved = true;
}

